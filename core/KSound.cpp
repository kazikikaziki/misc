#include "KSound.h"
#include "KPath.h"
#ifdef _WIN32
//
#include "KDxGetErrorString.h"
#include "KFile.h"
#include "KRef.h"
#include "KSoundInput.h"
#include <dsound.h>
#include <mmsystem.h> // WAVEFORMATEX
#include <process.h> // _beginthreadex
#include <mutex>
#include <unordered_map>

#if 1
#include "KLog.h"
#define SNDTRACE            Log_trace
#define SNDASSERT(x)        Log_assert(x)
#define SNDERROR(fmt, ...)  Log_errorf(fmt, ##__VA_ARGS__)
#else
#include <assert.h>
#define SNDTRACE            /* nothing */
#define SNDASSERT(x)        assert(x)
#define SNDERROR(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#endif


#define FX_TEST 0
#define STREAMING_BUFFSER_SEC  2.0f // ストリーミングのバッファサイズ（秒単位）

#define DS_RELEASE(x) {if (x) (x)->Release(); x=NULL;}

namespace mo {

namespace {

static void _sleep_about_msec(int msec) {
	// だいたいmsecミリ秒待機する。
	// 精度は問題にせず、適度な時間待機できればそれで良い。
	// エラーによるスリープ中断 (nanosleepの戻り値チェック) も考慮しない
#ifdef _WIN32
	Sleep(msec);
#else
	struct timespec ts;
	ts.tv_sec  = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000 * 1000;
	nanosleep(&ts, &ts);
#endif
}

static float _clamp(float t, float a, float b) {
	if (t < a) return a;
	if (t < b) return t;
	return b;
}
static int _clamp(int t, int a, int b) {
	if (t < a) return a;
	if (t < b) return t;
	return b;
}

#pragma region DS Functions
static float DS_GetVolume(IDirectSoundBuffer8 *dsbuf8) {
	SNDASSERT(dsbuf8);
	LONG dB_cent = 0;
	dsbuf8->GetVolume(&dB_cent); // 1/100 デシベル単位で取得
	float dB = dB_cent * 0.01f;
	float vol = powf(10, dB / 20.0f);
	return _clamp(vol, 0.0f, 1.0f);
}
static void DS_SetVolume(IDirectSoundBuffer8 *dsbuf8, float vol) {
	SNDASSERT(dsbuf8);
	LONG dB_100; // 1/100 dB 単位での値
	if (vol <= 0.0f) {
		dB_100 = DSBVOLUME_MIN;
	} else if (vol >= 1.0f) {
		dB_100 = DSBVOLUME_MAX;
	} else {
		/*
		旧計算。デシベル値としては正しくないが、ボリュームの上がり方的にはこちらの方が…
		float minDb = log10f(1 / 255.0f); // 0でない最小音量のdB
		float maxDb = 0; // 最大音量 dB
		float curDb = log10f(vol); // 指定された vol に対応したdB (1/100デシベル単位)
		LONG dB = (LONG)Num_lerp_clamp(DSBVOLUME_MIN, DSBVOLUME_MAX, minDb, maxDb, curDb);
		*/
		dB_100 = (LONG)(20 * log10f(vol) * 100);
		dB_100 = _clamp(dB_100, DSBVOLUME_MIN, DSBVOLUME_MAX);

	}
	dsbuf8->SetVolume(dB_100);
}
static void DS_SetPan(IDirectSoundBuffer8 *dsbuf8, float pan) {
	SNDASSERT(dsbuf8);
	SNDASSERT(DSBPAN_RIGHT - DSBPAN_CENTER == DSBPAN_CENTER - DSBPAN_LEFT);
	SNDASSERT(DSBPAN_RIGHT > 0);
	const long RANGE = DSBPAN_RIGHT;
	//
	dsbuf8->SetPan((long)(RANGE * pan));
}
static float DS_GetPan(IDirectSoundBuffer8 *dsbuf8) {
	SNDASSERT(dsbuf8);
	SNDASSERT(DSBPAN_RIGHT - DSBPAN_CENTER == DSBPAN_CENTER - DSBPAN_LEFT);
	SNDASSERT(DSBPAN_RIGHT > 0);
	const long RANGE = DSBPAN_RIGHT;
	//
	long dspan = 0;
	dsbuf8->GetPan(&dspan);
	return (float)dspan / RANGE;
}
static void DS_SetPitch(IDirectSoundBuffer8 *dsbuf8, float pitch) {
	SNDASSERT(dsbuf8);
	WAVEFORMATEX wf;
	dsbuf8->GetFormat(&wf, sizeof(wf), NULL);
	DWORD def_freq = wf.nSamplesPerSec;
	DWORD new_freq = (DWORD)(def_freq * pitch);
	dsbuf8->SetFrequency(new_freq);
}
static float DS_GetPitch(IDirectSoundBuffer8 *dsbuf8) {
	SNDASSERT(dsbuf8);
	WAVEFORMATEX wf;
	dsbuf8->GetFormat(&wf, sizeof(wf), NULL);
	DWORD def_freq = wf.nSamplesPerSec;
	DWORD cur_freq = def_freq;
	dsbuf8->GetFrequency(&cur_freq);
	return (float)cur_freq / def_freq;
}
#if FX_TEST
static void DS_Play(IDirectSoundBuffer8 *dsbuf8) {
	dsbuf8->Play(0, 0, 0);
}
static void DS_Stop(IDirectSoundBuffer8 *dsbuf8) {
	dsbuf8->Stop();
}
static bool DS_IsPlaying(IDirectSoundBuffer8 *dsbuf8) {
	DWORD s = 0;
	dsbuf8->GetStatus(&s);
	return s & DSBSTATUS_PLAYING;
}
#endif
#pragma endregion // DS Functions


#pragma region FX Functions
void DS_FxClear(IDirectSoundBuffer8 *dsbuf8) {
	dsbuf8->SetFX(0, NULL, NULL); // ※バッファ再生中、バッファロック中は失敗する
}
void DS_FxEcho(IDirectSoundBuffer8 *dsbuf8) {
	// ※バッファ再生中、バッファロック中は失敗する
	HRESULT hr;
	DWORD dwResults[1];  // エフェクトごとに 1 要素。
 
	DSEFFECTDESC dsEffect;
	memset(&dsEffect, 0, sizeof(DSEFFECTDESC));
	dsEffect.dwSize = sizeof(DSEFFECTDESC);
	dsEffect.dwFlags = 0;
	dsEffect.guidDSFXClass = GUID_DSFX_STANDARD_ECHO;
	hr = dsbuf8->SetFX(1, &dsEffect, dwResults);

	IDirectSoundFXEcho8 *fx = NULL;
	hr = dsbuf8->GetObjectInPath(GUID_DSFX_STANDARD_ECHO, 0, IID_IDirectSoundFXEcho8, (void**)&fx);
	if (fx) {
		DSFXEcho param;
		fx->GetAllParameters(&param);
		param.fWetDryMix = 50;
		param.fFeedback = 10;
		param.fLeftDelay = 100;
		param.fRightDelay = 100;
		param.lPanDelay = 0;
		fx->SetAllParameters(&param);
	}
}
void DS_FxEq(IDirectSoundBuffer8 *dsbuf8) {
	HRESULT hr;
	DWORD dwResults[3]; // エフェクトごとに 1 要素。
 
	DSEFFECTDESC dsEffect[3];
	memset(dsEffect, 0, sizeof(dsEffect));
	dsEffect[0].dwSize = sizeof(DSEFFECTDESC);
	dsEffect[0].dwFlags = 0;
	dsEffect[0].guidDSFXClass = GUID_DSFX_STANDARD_PARAMEQ;

	dsEffect[1].dwSize = sizeof(DSEFFECTDESC);
	dsEffect[1].dwFlags = 0;
	dsEffect[1].guidDSFXClass = GUID_DSFX_STANDARD_PARAMEQ;

	dsEffect[2].dwSize = sizeof(DSEFFECTDESC);
	dsEffect[2].dwFlags = 0;
	dsEffect[2].guidDSFXClass = GUID_DSFX_STANDARD_PARAMEQ;

	hr = dsbuf8->SetFX(3, dsEffect, dwResults);

	IDirectSoundFXParamEq8 *fx = NULL;
	hr = dsbuf8->GetObjectInPath(GUID_DSFX_STANDARD_PARAMEQ, 0, IID_IDirectSoundFXParamEq8, (void**)&fx);
	if (SUCCEEDED(hr)) {
		DSFXParamEq param;
		fx->GetAllParameters(&param);
		param.fCenter = 2000;
		param.fGain = DSFXPARAMEQ_GAIN_MIN;
		param.fBandwidth = 12;
		fx->SetAllParameters(&param);
	} 
	hr = dsbuf8->GetObjectInPath(GUID_DSFX_STANDARD_PARAMEQ, 1, IID_IDirectSoundFXParamEq8, (void**)&fx);
	if (SUCCEEDED(hr)) {
		DSFXParamEq param;
		fx->GetAllParameters(&param);
		param.fCenter = 1000;
		param.fGain = DSFXPARAMEQ_GAIN_MAX;
		param.fBandwidth = 5;
		fx->SetAllParameters(&param);
	} 
	hr = dsbuf8->GetObjectInPath(GUID_DSFX_STANDARD_PARAMEQ, 2, IID_IDirectSoundFXParamEq8, (void**)&fx);
	if (SUCCEEDED(hr)) {
		DSFXParamEq param;
		fx->GetAllParameters(&param);
		param.fCenter = 8000;
		param.fGain = DSFXPARAMEQ_GAIN_MIN;
		param.fBandwidth = 12;
		fx->SetAllParameters(&param);
	} 
}
#pragma endregion // FX Functions

class CSoundDevice {
public:
	IDirectSound8 *ds8_;
	int cnt_;

	CSoundDevice() {
		ds8_ = NULL;
		cnt_ = 0;
	}
	void inc() {
		if (cnt_ == 0) {
			CoInitialize(NULL);
			DirectSoundCreate8(NULL, &ds8_, NULL);
			if (ds8_) {
				HWND hWnd = GetActiveWindow(); // 現在のスレッドのウィンドウを得る
				if (hWnd == NULL) hWnd = GetDesktopWindow(); // 現在のスレッドにウィンドウがなければデスクトップの HWND を使う
				ds8_->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
			}
		}
		cnt_++;
	}
	void dec() {
		cnt_--;
		if (cnt_ == 0) {
			DS_RELEASE(ds8_);
			CoUninitialize();
		}
	}
	IDirectSound8 * getDirectSound8() {
		return ds8_;
	}
};
static CSoundDevice s_dev;

class CSoundBuffer {
public:
	CSoundBuffer() {
		dsbuf8_ = NULL;
		channels_ = 0;
		sample_rate_ = 0;
		total_seconds_ = 0;
		s_dev.inc();
	}
	virtual ~CSoundBuffer() {
		destroy();
		s_dev.dec();
	}
	virtual void play() = 0;
	virtual void stop() = 0;
	virtual float getPosition() = 0;
	virtual void setPosition(float seconds) = 0;
	virtual bool loadSound(SoundInput &snd) = 0;

	void assign(const CSoundBuffer &source, bool clone) {
		// 既存のデータは上書きされるので破棄しておく
		if (dsbuf8_) {
			destroy();
		}

		IDirectSound8 *ds8 = s_dev.getDirectSound8();
		if (ds8 && source.dsbuf8_) {
			if (clone) {
				// 実体コピー
				ds8->DuplicateSoundBuffer(source.dsbuf8_, (IDirectSoundBuffer **)&dsbuf8_);
			} else {
				// 参照コピー
				dsbuf8_ = source.dsbuf8_;
				dsbuf8_->AddRef();
			}
			channels_ = source.channels_;
			sample_rate_ = source.sample_rate_;
			total_seconds_ = source.total_seconds_;
		}
	}
	bool loadFile(FileInput &file) {
		SoundInput snd;
		if (snd.open(file)) {
			loadSound(snd);
			return true;
		}
		return false;
	}
	bool isPlaying() const {
		DWORD status = 0;
		if (dsbuf8_) dsbuf8_->GetStatus(&status);
		return status & DSBSTATUS_PLAYING;
	}
	void setPan(float pan) {
		if (dsbuf8_) DS_SetPan(dsbuf8_, pan);
	}
	float getPan() const {
		return dsbuf8_ ? DS_GetPan(dsbuf8_) : 0.0f;
	}
	void setPitch(float pitch) {
		if (dsbuf8_) DS_SetPitch(dsbuf8_, pitch);
	}
	float getPitch() const {
		return dsbuf8_ ? DS_GetPitch(dsbuf8_) : 1.0f;
	}
	void setVolume(float vol) {
		if (dsbuf8_) DS_SetVolume(dsbuf8_, vol);
	}
	float getVolume() const {
		return dsbuf8_ ? DS_GetVolume(dsbuf8_) : 0.0f;
	}
	float getLength() {
		return total_seconds_;
	}

	IDirectSoundBuffer8 * getDS8Buf() { return dsbuf8_; }

protected:
	bool createBuffer(int numSamples, int channels, int sample_rate) {
		destroy();
		SNDASSERT(numSamples >= 0);
		SNDASSERT(channels == 1 || channels == 2);
		SNDASSERT(sample_rate >= 0);

		WAVEFORMATEX wf;
		ZeroMemory(&wf, sizeof(WAVEFORMATEX));
		wf.wFormatTag      = WAVE_FORMAT_PCM;
		wf.nChannels       = (WORD)channels;
		wf.nSamplesPerSec  = (DWORD)sample_rate;
		wf.wBitsPerSample  = (WORD)(SoundInput::BITS_PER_SAMPLE);
		wf.nBlockAlign     = (WORD)(SoundInput::BITS_PER_SAMPLE / 8 * channels);
		wf.nAvgBytesPerSec = (DWORD)(SoundInput::BITS_PER_SAMPLE / 8 * channels * sample_rate);

		DSBUFFERDESC desc;
		ZeroMemory(&desc, sizeof(DSBUFFERDESC));
		desc.dwSize = sizeof(DSBUFFERDESC);
		desc.dwBufferBytes = numSamples * SoundInput::BYTES_PER_SAMPLE;
		desc.lpwfxFormat = &wf;
		desc.dwFlags =
			DSBCAPS_GETCURRENTPOSITION2 |
			DSBCAPS_CTRLVOLUME          |
			DSBCAPS_CTRLPAN             |
			DSBCAPS_CTRLFREQUENCY       |
			#if FX_TEST
			DSBCAPS_CTRLFX |
			#endif
			DSBCAPS_GLOBALFOCUS; // ウィンドウがフォーカスを失っても再生を継続
		
		IDirectSoundBuffer *dsbuf = NULL;
		IDirectSound8 *ds8 = s_dev.getDirectSound8();
		if (ds8) ds8->CreateSoundBuffer(&desc, &dsbuf, NULL); // <-- ここで失敗した場合、desc.dwFlags の組み合わせを疑う
		if (dsbuf) {
			dsbuf->QueryInterface(IID_IDirectSoundBuffer8, (void**)&dsbuf8_);
			DS_RELEASE(dsbuf);
		}
		if (dsbuf8_) {
			dsbuf8_->SetVolume(0); // 0dB
			channels_ = channels;
			sample_rate_ = sample_rate;
			return true;
		}
		return false;
	}
	void destroy() {
		DS_RELEASE(dsbuf8_);
	}
	int write(int offsetSamples, SoundInput &source, int numSamples) {
		int gotSamples = 0;
		if (dsbuf8_) {
			int16_t *buf;
			DWORD locked;
			DWORD off = offsetSamples * SoundInput::BYTES_PER_SAMPLE;
			DWORD num = numSamples * SoundInput::BYTES_PER_SAMPLE;
			if (SUCCEEDED(dsbuf8_->Lock(off, num, (void**)&buf, &locked, NULL, NULL, 0))) {
				ZeroMemory(buf, locked);
				gotSamples = source.readSamples(buf, numSamples);
				dsbuf8_->Unlock(buf, locked, NULL, 0);
			}
		}
		return gotSamples;
	}
	int writeLoop(int offsetSamples, SoundInput &source, int numSamples, int loopStartSamples, int loopEndSamples) {
		int gotSamples = 0;
		if (dsbuf8_) {
			int16_t *buf;
			DWORD locked;
			DWORD off = offsetSamples * SoundInput::BYTES_PER_SAMPLE;
			DWORD num = numSamples * SoundInput::BYTES_PER_SAMPLE;
			if (SUCCEEDED(dsbuf8_->Lock(off, num, (void**)&buf, &locked, NULL, NULL, 0))) {
				ZeroMemory(buf, locked);
				gotSamples = source.readSamplesLoop(buf, numSamples, loopStartSamples, loopEndSamples);
				dsbuf8_->Unlock(buf, locked, NULL, 0);
			}
		}
		return gotSamples;
	}
	void setBufferPosition(int samples) {
		DWORD pos_bytes = (DWORD)samples * SoundInput::BYTES_PER_SAMPLE;
		if (dsbuf8_) dsbuf8_->SetCurrentPosition(pos_bytes);
	}
	int getBufferPosition() const {
		DWORD pos_bytes = 0;
		if (dsbuf8_) dsbuf8_->GetCurrentPosition(&pos_bytes, 0);
		return (int)pos_bytes / SoundInput::BYTES_PER_SAMPLE; // num samples
	}
	void playBuffer(bool loop) {
		if (dsbuf8_) dsbuf8_->Play(0, 0, loop ? DSBPLAY_LOOPING : 0);
	}
	void stopBuffer() {
		if (dsbuf8_) dsbuf8_->Stop();
	}
protected:
	IDirectSoundBuffer8 *dsbuf8_;
	int channels_;
	int sample_rate_; // samples per second per channel
	float total_seconds_;
};

class CStaticSoundBuffer: public CSoundBuffer {
public:
	CStaticSoundBuffer() {}
	CStaticSoundBuffer(const CStaticSoundBuffer &other, bool clone=false) {
		assign(other, clone);
	}
	CStaticSoundBuffer & operator=(const CStaticSoundBuffer &other) {
		assign(other, false);
		return *this;
	}
	virtual void play() override {
		playBuffer(false);
	}
	virtual void stop() override {
		stopBuffer();
	}
	virtual float getPosition() override {
		return (float)getBufferPosition() / channels_ / sample_rate_;
	}
	virtual void setPosition(float seconds) override {
		setBufferPosition((int)(seconds * sample_rate_) * channels_);
	}
	virtual bool loadSound(SoundInput &sound) override {
		int numsamp = sound.getSampleCount();
		int channels = sound.getChannelCount();
		int rate = sound.getSamplesPerSecond();
		if (createBuffer(numsamp, channels, rate)) {
			total_seconds_ = sound.getTotalSeconds();
			write(0, sound, numsamp);
			return true;
		}
		return false;
	}
};

class Scoped_lock {
public:
	std::recursive_mutex &m_;

	Scoped_lock(std::recursive_mutex &m): m_(m) {
		m_.lock();
	}
	~Scoped_lock() {
		m_.unlock();
	}
};
#define SCOPED_LOCK  Scoped_lock lock__(mutex_)

class CStreamingSoundBuffer: public CSoundBuffer {
public:
	static const int NUM_BLOCKS = 2; // ストリーミング再生用のバッファブロック数


	CStreamingSoundBuffer() {
		loop_start_  = 0;
		loop_end_    = 0;
		loop_        = false;
		stop_next_   = false;
		blocksize_   = 0;
		blockindex_  = -1;
		inputpos_[0] = 0;
		inputpos_[1] = 0;
	}
	CStreamingSoundBuffer(const CStreamingSoundBuffer &other) {
		loop_start_  = other.loop_start_;
		loop_end_    = other.loop_end_;
		loop_        = other.loop_;
		stop_next_   = other.stop_next_;
		blocksize_   = other.blocksize_;
		blockindex_  = other.blockindex_;
		inputpos_[0] = other.inputpos_[0];
		inputpos_[1] = other.inputpos_[1];
	}
	CStreamingSoundBuffer & operator=(const CStreamingSoundBuffer &other) {
		assign(other, false);
		loop_start_  = other.loop_start_;
		loop_end_    = other.loop_end_;
		loop_        = other.loop_;
		stop_next_   = other.stop_next_;
		blocksize_   = other.blocksize_;
		blockindex_  = other.blockindex_;
		inputpos_[0] = other.inputpos_[0];
		inputpos_[1] = other.inputpos_[1];
	}
	virtual void play() override {
		SCOPED_LOCK;
		blockindex_ = 0;
		stop_next_ = false;
		writeToBlock(0); // ブロック前半にデータを転送
		writeToBlock(1); // ブロック後半にデータを転送
		setBufferPosition(0);
		playBuffer(true);
	}
	virtual void stop() override {
		SCOPED_LOCK;
		stopBuffer();
	}
	virtual float getPosition() override {
		SCOPED_LOCK;
		int pos = getBufferPosition();
		SNDASSERT(0 <= pos);

		// 再生中のブロック番号
		int idx = pos / blocksize_;
		SNDASSERT(0 <= idx && idx < NUM_BLOCKS);

		// ブロック先頭からのオフセット
		int off = pos % blocksize_;

		// 入力ストリームの先頭から数えたサンプリング数
		int samples = inputpos_[idx] + off;
		
		return (float)samples / channels_ / sample_rate_;
	}
	virtual void setPosition(float seconds) override {
		SCOPED_LOCK;
		// 入力ストリームの読み取り位置を変更
		int samples = (int)(seconds * sample_rate_) * channels_;
		source_.seekSamples(samples);

		// 再生中であれば改めて play を実行し、バッファ内容を更新しておく
		if (isPlaying()) {
			play();
		}
	}
	virtual bool loadSound(SoundInput &source) override {
		SCOPED_LOCK;
		int src_rate = source.getSamplesPerSecond();
		int src_channels = source.getChannelCount();

		blocksize_ = (int)(src_rate * STREAMING_BUFFSER_SEC); // 1ブロック = STREAMING_BUFFSER_SEC 秒分。作成するバッファは前半用と後半用で2ブロック分になる
		
		if (createBuffer(blocksize_ * NUM_BLOCKS, src_channels, src_rate)) {
			source_ = source;
			stop_next_ = false;
			blockindex_ = -1;
			return true;
		}

		source_ = SoundInput();
		blocksize_ = 0;
		return false;
	}
	void update() {
		SCOPED_LOCK;
		if (blockindex_ < 0) {
			return; // 停止中
		}
		// 再生中のブロック番号を得る
		int idx = getBufferPosition() / blocksize_;
		if (idx == blockindex_) {
			return; // 現在のブロックを再生中
		}
		// ブロックの再生が終わり、次のブロックを再生し始めている。
		// 終了フラグがあるなら再生を停止する
		if (stop_next_) {
			stop();
			blockindex_ = -1;
			stop_next_ = false;
			return;
		}
		// たった今再生が終わったばかりのブロックに次のデータを入れておく
		if (writeToBlock(blockindex_) == 0) {
			stop_next_ = true; // 入力ストリームからのデータが途絶えた
		}
		// ブロック番号を更新
		blockindex_ = (blockindex_ + 1) % NUM_BLOCKS;
	}
	void setLoop(bool value) {
		loop_ = value;
	}
	bool isLoop() const {
		return loop_;
	}
	void setLoopRange(float start_seconds, float end_seconds) {
		SCOPED_LOCK;
		int channels = source_.getChannelCount();
		int rate = source_.getSamplesPerSecond();
		loop_start_ = (int)(rate * start_seconds) * channels;
		loop_end_   = (int)(rate * end_seconds)   * channels;
	}
protected:
	int writeToBlock(int index) {
		SCOPED_LOCK;
		inputpos_[index] = source_.tellSamples();
		if (loop_) {
			return writeLoop(blocksize_*index, source_, blocksize_, loop_start_, loop_end_);
		} else {
			return write(blocksize_*index, source_, blocksize_);
		}
	}
private:
	SoundInput source_;
	std::recursive_mutex mutex_;
	int blocksize_;
	int blockindex_;
	int loop_start_;
	int loop_end_;
	int play_pos_;
	int inputpos_[NUM_BLOCKS];
	bool loop_;
	bool stop_next_;
};

class CSoundImpl {
public:
	std::unordered_map<SOUNDID, CSoundBuffer*> sounds_;
	std::unordered_map<Path, CStaticSoundBuffer> pool_;
	int newid_;
	HANDLE thread_;
	std::recursive_mutex mutex_; // sounds_, pool_ の操作に対するロック用
	bool exit_thread_;

	static unsigned int CALLBACK updating_thread(void *data) {
		CSoundImpl *sp = reinterpret_cast<CSoundImpl*>(data);
		sp->updateThreadProc();
		return 0;
	}
	CSoundImpl() {
		newid_ = 0;
		thread_ = NULL;
		exit_thread_ = false;
	}
	void init(HWND hWnd) {
		// ストリーミング再生用のスレッドを開始する
		thread_ = (HANDLE)_beginthreadex(NULL, 0, updating_thread, this, 0, NULL);
	}
	void shutdown() {
		// ストリーミング再生用のスレッドを停止する
		exit_thread_ = true;
		WaitForSingleObject(thread_, 0);

		// サウンドオブジェクトをすべて削除する
		for (auto it=sounds_.begin(); it!=sounds_.end(); ++it) {
			it->second->stop();
			delete it->second;
		}
		// SCOPED_LOCK もうスレッドからは抜けているはずなので、ロック不要
		sounds_.clear();
		pool_.clear();
	}
	void poolSound(const Path &name, FileInput &file) {
		if (isPooled(name)) {
			return;
		}
		SoundInput snd;
		if (!snd.open(file)) {
			SNDERROR("E_FAIL_OPEN_SOUND: %s", name.u8());
			return;
		}
		CStaticSoundBuffer se;
		if (!se.loadSound(snd)) {
			SNDERROR("E_FAIL_LOAD_SOUND: %s", name.u8());
			return;
		}
		{
			SCOPED_LOCK;
			pool_[name] = se;
		}
	}
	bool isPooled(const Path &name) {
		SCOPED_LOCK;
		auto it = pool_.find(name);
		if (it != pool_.end()) {
			return true;
		}
		return false;
	}
	void deletePooledSound(const Path &name) {
		SCOPED_LOCK;
		pool_.erase(name);
	}
	SOUNDID playPooledSound(const Path &name) {
		SNDASSERT(! name.empty());
		SCOPED_LOCK;
		auto it = pool_.find(name);
		if (it == pool_.end()) {
			return NULL;
		}
		CStaticSoundBuffer *se = new CStaticSoundBuffer(it->second, true);
		se->play();

		SOUNDID id = (SOUNDID)(++newid_);
		sounds_[id] = se;
		return id;
	}
	SOUNDID playStreamingSound(const Path &name, FileInput &file, float offsetSeconds, bool loop, float loopStartSeconds, float loopEndSeconds) {
		SNDASSERT(! name.empty());
		SCOPED_LOCK;
		SoundInput source;
		if (!source.open(file)) {
			SNDERROR("E_FAIL_OPEN_SOUND: %s", name.u8());
			return NULL;
		}

		CStreamingSoundBuffer *music = new CStreamingSoundBuffer;
		if (!music->loadSound(source)) {
			return NULL;
		}

		music->setLoop(loop);
		music->setLoopRange(loopStartSeconds, loopEndSeconds);
		music->setPosition(offsetSeconds);
		#if FX_TEST
		DS_FxEq(music->getDS8Buf());
		#endif
		music->play();

		SOUNDID id = (SOUNDID)(++newid_);
		sounds_[id] = music;
		return id;
	}
	void pause(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			sb->stop();
		}
	}
	void resume(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			sb->play();
		}
	}
	void deleteHandle(SOUNDID id) {
		SCOPED_LOCK;
		SNDTRACE;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			sb->stop();
			delete sb;
			sounds_.erase(it);
		}
	}
	const Path & getSourceName(SOUNDID id) {
		return Path::Empty;
	}
	float getPosition(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			return sb->getPosition();
		}
		return 0.0f;
	}
	float getLength(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			return sb->getLength();
		}
		return 0.0f;
	}
	void setPosition(SOUNDID id, float seconds) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			sb->setPosition(seconds);
		}
	}
	bool isLooping(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			CStreamingSoundBuffer *music = dynamic_cast<CStreamingSoundBuffer*>(sb);
			if (music) {
				return music->isLoop();
			}
		}
		return false;
	}
	bool isPlaying(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			return sb->isPlaying();
		}
		return false;
	}
	void setLooping(SOUNDID id, bool value) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			CStreamingSoundBuffer *music = dynamic_cast<CStreamingSoundBuffer*>(sb);
			if (music) {
				music->setLoop(value);
			}
		}
	}
	void setPan(SOUNDID id, float value) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			sb->setPan(value);
		}
	}
	float getPan(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			return sb->getPan();
		}
		return 0.0f;
	}
	void setVolume(SOUNDID id, float value) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			sb->setVolume(value);
		}
	}
	float getVolume(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			return sb->getVolume();
		}
		return 0.0f;
	}
	void setPitch(SOUNDID id, float value) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			sb->setPitch(value);
		}
	}
	float getPitch(SOUNDID id) {
		SCOPED_LOCK;
		auto it = sounds_.find(id);
		if (it != sounds_.end()) {
			CSoundBuffer *sb = it->second;
			return sb->getPitch();
		}
		return 1.0f;
	}
	void setListenerPosition(const float *pos_xyz) {
	}
	void setSourcePosition(SOUNDID id, const float *pos_xyz) {
	}
	#if FX_TEST
	IDirectSoundBuffer8 *dsBuf(SOUNDID id) {
		CSoundBuffer *buf = sounds_[id];
		return buf ? buf->getDS8Buf() : NULL;
	}
	#endif
	void command(const char *cmd, int *args) {
		#if FX_TEST
		if (strcmp(cmd, "fxecho") == 0) {
			IDirectSoundBuffer8 *buf8 = dsBuf((SOUNDID)args[0]);
			if (buf8) {
				if (DS_IsPlaying(buf8)) {
					DS_Stop(buf8);
					DS_FxEcho(buf8);
					DS_Play(buf8);
				} else {
					DS_FxEcho(buf8);
				}
			}
		}
		if (strcmp(cmd, "fxeq") == 0) {
			IDirectSoundBuffer8 *buf8 = dsBuf((SOUNDID)args[0]);
			if (buf8) {
				if (DS_IsPlaying(buf8)) {
					DS_Stop(buf8);
					DS_FxEq(buf8);
					DS_Play(buf8);
				} else {
					DS_FxEq(buf8);
				}
			}
		}
		if (strcmp(cmd, "fxoff") == 0) {
			IDirectSoundBuffer8 *buf8 = dsBuf((SOUNDID)args[0]);
			if (buf8) {
				if (DS_IsPlaying(buf8)) {
					DS_Stop(buf8);
					DS_FxClear(buf8);
					DS_Play(buf8);
				} else {
					DS_FxClear(buf8);
				}
			}
		}
		#endif
	}

	void updateStreaming() {
		SCOPED_LOCK;
		for (auto it=sounds_.begin(); it!=sounds_.end(); ++it) {
			CSoundBuffer *sb = it->second;
			CStreamingSoundBuffer *music = dynamic_cast<CStreamingSoundBuffer*>(sb);
			if (music) {
				music->update();
			}
		}
	}
	void updateThreadProc() {
		// 適当な間隔で update を繰り返す。
		// 少なくとも STREAMING_BUFFSER_SEC 秒の半分未満の時間間隔で更新すること
		int interval_msec = (int)(STREAMING_BUFFSER_SEC * 1000) / 4;

		while (!exit_thread_) {
			updateStreaming();

			// 適当な時間待つだけ。スリープの精度が低くてもよい
			_sleep_about_msec(interval_msec);
		}
	}
};

static CSoundImpl s_snd;


} // namespace anonymous

#pragma region Sound
void Sound::init(void *hWnd) {
	SNDTRACE;
	SNDASSERT(IsWindow((HWND)hWnd)); // 有効な HWND であることを確認
	s_snd.init((HWND)hWnd);
}
void Sound::shutdown() {
	s_snd.shutdown();
}
bool Sound::isInitialized() {
	return s_snd.thread_ != NULL;
}
void Sound::poolSound(const Path &name, const Path &filename) {
	SNDTRACE;
	FileInput file;
	if (file.open(filename)) {
		poolSound(name, file);
	}
}
void Sound::poolSound(const Path &name, FileInput &file) {
	SNDTRACE;
	s_snd.poolSound(name, file);
}
bool Sound::isPooled(const Path &name) {
	return s_snd.isPooled(name);
}
void Sound::deletePooledSound(const Path &name) {
	SNDTRACE;
	s_snd.deletePooledSound(name);
}
SOUNDID Sound::playPooledSound(const Path &name) {
	SNDTRACE;
	return s_snd.playPooledSound(name);
}
SOUNDID Sound::playStreamingSound(const Path &name, FileInput &file, float offsetSeconds, bool looping, float loopStartSeconds, float loopEndSeconds) {
	SNDTRACE;
	return s_snd.playStreamingSound(name, file, offsetSeconds, looping, loopStartSeconds, loopEndSeconds);
}
SOUNDID Sound::playStreamingSound(const Path &filename, float offsetSeconds, bool looping, float loopStartSeconds, float loopEndSeconds) {
	SNDTRACE;
	SOUNDID sid = 0;
	FileInput file;
	if (file.open(filename)) {
		sid = s_snd.playStreamingSound(filename, file, offsetSeconds, looping, loopStartSeconds, loopEndSeconds);
	}
	return sid;
}
void Sound::pause(SOUNDID snd) {
	SNDTRACE;
	s_snd.pause(snd);
}
void Sound::resume(SOUNDID snd) {
	SNDTRACE;
	s_snd.resume(snd);
}
void Sound::deleteHandle(SOUNDID snd) {
	SNDTRACE;
	s_snd.deleteHandle(snd);
}
float Sound::getPositionTime(SOUNDID snd) {
	return s_snd.getPosition(snd);
}
void Sound::setPositionTime(SOUNDID snd, float pos_seconds) {
	return s_snd.setPosition(snd, pos_seconds);
}
float Sound::getLengthTime(SOUNDID snd) {
	return s_snd.getLength(snd);
}
float Sound::getPitch(SOUNDID snd) {
	return s_snd.getPitch(snd);
}
void Sound::setPitch(SOUNDID snd, float value) {
	s_snd.setPitch(snd, value);
}
float Sound::getPan(SOUNDID snd) {
	return s_snd.getPan(snd);
}
void Sound::setPan(SOUNDID snd, float value) {
	s_snd.setPan(snd, value);
}
float Sound::getVolume(SOUNDID snd) {
	return s_snd.getVolume(snd);
}
void Sound::setVolume(SOUNDID snd, float value) {
	s_snd.setVolume(snd, value);
}
bool Sound::isLooping(SOUNDID snd) {
	return s_snd.isLooping(snd);
}
void Sound::setLooping(SOUNDID snd, bool value) {
	s_snd.setLooping(snd, value);
}
bool Sound::isPlaying(SOUNDID snd) {
	return s_snd.isPlaying(snd);
}
const Path & Sound::getSourceName(SOUNDID snd) {
	return s_snd.getSourceName(snd);
}
void Sound::setListenerPosition(const float *pos_xyz) {
	s_snd.setListenerPosition(pos_xyz);
}
void Sound::setSourcePosition(SOUNDID snd, const float *pos_xyz) {
	s_snd.setSourcePosition(snd, pos_xyz);
}

void Sound::command(const char *cmd, int *args) {
	s_snd.command(cmd, args);
}
#pragma endregion // Sound

} // namespace mo

#else // _WIN32

namespace mo {
#pragma region Sound
void Sound::init(void *hWnd) {}
void Sound::shutdown() {}
void Sound::poolSound(const Path &name, FileInput &file) {}
void Sound::poolSound(const Path &name, const Path &filename) {}
bool Sound::isPooled(const Path &name) { return false; }
void Sound::deletePooledSound(const Path &name) {}
SOUNDID Sound::playPooledSound(const Path &name) { return NULL; }
SOUNDID Sound::playStreamingSound(const Path &name, FileInput &file, float offsetSeconds, bool looping, float loopStartSeconds, float loopEndSeconds) { return NULL; }
SOUNDID Sound::playStreamingSound(const Path &filename, float offsetSeconds, bool looping, float loopStartSeconds, float loopEndSeconds) { return NULL; }
void Sound::pause(SOUNDID snd) {}
void Sound::resume(SOUNDID snd) {}
void Sound::deleteHandle(SOUNDID snd) {}
float Sound::getPositionTime(SOUNDID snd) { return 0.0f; }
void Sound::setPositionTime(SOUNDID snd, float seconds) {}
float Sound::getLengthTime(SOUNDID snd) { return 0.0f; }
float Sound::getPitch(SOUNDID snd) { return 0.0f; }
void Sound::setPitch(SOUNDID snd, float value) {}
float Sound::getPan(SOUNDID snd) { return 0.0f; }
void Sound::setPan(SOUNDID snd, float value) {}
float Sound::getVolume(SOUNDID snd) { return 0.0f; }
void Sound::setVolume(SOUNDID snd, float value) {}
bool Sound::isLooping(SOUNDID snd) { return false; }
void Sound::setLooping(SOUNDID snd, bool value) {}
bool Sound::isPlaying(SOUNDID snd) { return false; }
const Path & Sound::getSourceName(SOUNDID snd) { return Path::Empty; }
void Sound::setListenerPosition(const float *pos_xyz) {}
void Sound::setSourcePosition(SOUNDID snd, const float *pos_xyz) {}
#pragma endregion // Sound
} // namespace

#endif // !_WIN32



namespace mo {
void Test_wav(const wchar_t *filename) {
#ifdef _WIN32
#if 0
	FileInput file;
	if (filename) {
		file.open(filename);
	} else {
		file.open("C:\\Windows\\Media\\Ring02.wav");
	}
	if (1) {
		CStreamingSoundBuffer buffer;
	//	buffer.setLoop(true);
		buffer.loadFile(file);
		buffer.play();
		while (buffer.isPlaying()) {
			Sleep(500);
			buffer.update();
		}

	} else {
		CStaticSoundBuffer buffer;
		buffer.loadFile(file);
		buffer.play();
		while (buffer.isPlaying()) {
			Sleep(500);
		}

	}s
#else
	HWND hWnd = GetTopWindow(NULL);
	Sound::init(hWnd);
	SOUNDID snd;
	if (filename) {
		snd = Sound::playStreamingSound(filename, 0, true);
	} else {
		snd = Sound::playStreamingSound("C:\\Windows\\Media\\Ring08.wav", 0, true);
	}
	while (Sound::isPlaying(snd)) {
		Sleep(1000);
	}
	Sound::shutdown();
#endif

#endif // _WIN32
}


} // namespace mo
