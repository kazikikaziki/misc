#include "KSoundInput.h"
#include "KFile.h"
#include <assert.h>

#define SNDASSERT assert

#define MO_STBVORBIS


#ifdef _WIN32
	#include <Windows.h>
	#include <mmsystem.h>
#endif

#ifdef MO_LIBVORBIS
	// .ogg オーディオファイルの再生に libogg/libvorbis を使う
	#include <vorbis/codec.h>
	#include <vorbis/vorbisfile.h>
#endif

#ifdef MO_STBVORBIS
	// .ogg オーディオファイルの再生に stb_vorbis を使う
	#define STB_VORBIS_HEADER_ONLY
	#include "stb_vorbis.c"
#endif

#define OGG_BIT_PER_SEC_16      16 // いまのところ、16ビットサウンドにのみ対応させる
#define OGG_BYTES_PER_SAMPLE    2




namespace mo {


/// Implement Interface
class SoundInput::Impl {
public:
	virtual ~Impl() {}
	virtual int sample_rate() const = 0;
	virtual int sample_count() const = 0;
	virtual int channel_count() const = 0;
	virtual int read(int16_t *buffer, int samples) = 0;
	virtual int seek(int samples) = 0;
	virtual int tell() const = 0;
};



#ifdef MO_LIBVORBIS
namespace {
static size_t cb_read(void *ptr, size_t blocksize, size_t numblocks, void *data) {
	FileInput *file = reinterpret_cast<FileInput *>(data);
	const int wsize = file->read(ptr, blocksize * numblocks);
	return (size_t)(wsize / blocksize); // return number of blocks
}
static int cb_seek(void *data, ogg_int64_t offset, int origin) {
	SNDASSERT((offset & 0x7FFFFFFF) == offset) ; // int 範囲のみ対応
	FileInput *file = reinterpret_cast<FileInput *>(data);
	switch (origin) {
	case SEEK_SET:
		file->seek((int)offset);
		break;
	case SEEK_CUR:
		file->seek(file->tell() + (int)offset);
		break;
	case SEEK_END:
		SNDASSERT(offset == 0);
		file->seek(file->size());
		break;
	}
	return 0;
}
static int cb_close(void *data) {
	FileInput *file = reinterpret_cast<FileInput *>(data);
	delete file;
	return 0;
}
static long cb_tell(void *data) {
	FileInput *file = reinterpret_cast<FileInput *>(data);
	return (long)file->tell();
}

} // namesapce
class COggImplOV: public SoundInput::Impl {
	OggVorbis_File ovfile_;
	vorbis_info *vinfo_;
	WAVEFORMATEX fmt_;
	int total_samples_;
public:
	/// file のヘッダが .OGG フォーマットかどうか調べる
	static bool check(FileInput &file) {
		bool result = false;
		int pos = file.tell();

		ov_callbacks callbacks = {
			cb_read,
			cb_seek,
			NULL, // <-- コールバックには file を直接渡すが、これを勝手に delete されては困るので cb_close2 呼ばないようにしておく
			cb_tell,
		};
		OggVorbis_File ovf;
		if (ov_test_callbacks(&file, &ovf, NULL, 0, callbacks) == 0) {
			ov_clear(&ovf);
			result = true;
		}
		file.seek(pos); // 元に戻すのを忘れないように！
		return result;
	}
public:
	COggImplOV() {
		memset(&ovfile_, 0, sizeof(ovfile_));
		memset(&fmt_, 0, sizeof(fmt_));
		vinfo_ = NULL;
		total_samples_ = 0;
	}
	COggImplOV(FileInput &file, int *err) {
		SNDASSERT(err);
		memset(&fmt_, 0, sizeof(fmt_));
		int pos = file.tell();
		ov_callbacks callbacks = {
			cb_read,
			cb_seek,
			cb_close,
			cb_tell,
		};
		FileInput *pFile = new FileInput(); // ov_open_callbacks に FileInput を void* 経由で渡すため、参照カウンタの整合性が取れなくなる。new しておく
		*pFile = file; // ref copy
		if (ov_open_callbacks((void *)pFile, &ovfile_, NULL, 0, callbacks) != 0) {
			// エラー発生。
			// ファイル読み取り位置を元に戻しておく
			file.seek(pos);
			*err = 1;
		}

		// new した FileInput は cb_close() で削除される
		vinfo_ = ov_info(&ovfile_, -1);
		fmt_.wFormatTag      = WAVE_FORMAT_PCM;
		fmt_.nChannels       = vinfo_->channels;
		fmt_.nSamplesPerSec  = vinfo_->rate;
		fmt_.wBitsPerSample  = OGG_BIT_PER_SEC_16;
		fmt_.nBlockAlign     = fmt_.wBitsPerSample * vinfo_->channels / 8;
		fmt_.nAvgBytesPerSec = vinfo_->rate * fmt_.nBlockAlign;
		fmt_.cbSize = 0;
		ov_pcm_seek(&ovfile_, 0);
		total_samples_ = (int)ov_pcm_total(&ovfile_, -1) * vinfo_->channels;
		*err = 0;
	}
	virtual ~COggImplOV() {
		ov_clear(&ovfile_);
	}
	virtual int sample_count() const override {
		return total_samples_;
	}
	virtual int channel_count() const override {
		return fmt_.nChannels;
	}
	virtual int sample_rate() const override {
		return fmt_.nSamplesPerSec;
	}
	virtual int seek(int sample_offset) override {
		int block = sample_offset / fmt_.nChannels;
		return ov_pcm_seek(&ovfile_, block);
	}
	virtual int read(int16_t *buffer, int num_samles) override {
		int count = 0;
		while (count < num_samles) {
			int reqBytes = (num_samles - count) * OGG_BYTES_PER_SAMPLE;
			int gotBytes = ov_read(&ovfile_, (char *)buffer, reqBytes, 0, OGG_BYTES_PER_SAMPLE, 1, NULL);
			if (gotBytes == 0) {
				break; // error or eof
			}
			int gotSamples = gotBytes / OGG_BYTES_PER_SAMPLE;
			count += gotSamples;
			buffer += gotSamples;
		}
		return count;
	}
	virtual int tell() const override {
		ogg_int64_t block = ov_pcm_tell(const_cast<OggVorbis_File*>(&ovfile_));
		return (int)(block * fmt_.nChannels);
	}
};
#endif // MO_LIBVORBIS


#ifdef MO_STBVORBIS
class COggImplStb: public SoundInput::Impl {
	std::string bin_;
	stb_vorbis *vorbis_;
	stb_vorbis_info info_;
	int total_samples_;
public:
	/// file のヘッダが .OGG フォーマットかどうか調べる
	static bool check(FileInput &file) {
		bool check_ok = false;

		// チェックの最初と最後で読み取り位置が変化しないようにする
		int inipos = file.tell();

		// ファイルヘッダを調べる。
		// 最初の5バイトが "OggS\0" なら OK
		{
			char s[5];
			if (file.read(s, 5) == 5) {
				if (memcmp(s, "OggS\0", 5) == 0) {
					check_ok = true;
				}
			}
		}

		file.seek(inipos);
		return check_ok;
	}
public:
	COggImplStb() {
		vorbis_ = NULL;
		memset(&info_, 0, sizeof(info_));
		total_samples_ = 0;
	}
	COggImplStb(FileInput &file, int *err) {
		SNDASSERT(err);
		memset(&info_, 0, sizeof(info_));

		bin_ = file.readBin();
		vorbis_ = stb_vorbis_open_memory((const unsigned char*)bin_.data(), bin_.size(), err, NULL);

	//	vorbis_ = stb_vorbis_open_filename("music/stage1.ogg", err, NULL);

		if (vorbis_ == NULL) {
			// 作成失敗したのにエラーコードが入っていなかったら、とりあえず-1を入れておく。
			// stb_vorbis_open_memory でのエラーコードは必ず正の値になる。
			// のエラーコード詳細については enum STBVorbisError を参照する。
			if (*err == 0) {
				*err = -1;
			}
			return;
		}
		info_ = stb_vorbis_get_info(vorbis_);
		total_samples_ = stb_vorbis_stream_length_in_samples(vorbis_) * info_.channels;
		*err = 0;
	}
	virtual ~COggImplStb() {
		stb_vorbis_close(vorbis_);
		bin_.clear();
	}
	virtual int sample_count() const override {
		return total_samples_;
	}
	virtual int channel_count() const override {
		return info_.channels;
	}
	virtual int sample_rate() const override {
		return info_.sample_rate;
	}
	virtual int seek(int sample_offset) override {
		int block = sample_offset / info_.channels;
		int ok = stb_vorbis_seek(vorbis_, block);
		return ok ? 0 : -1; // 成功なら 0, 失敗なら -1 を返す
	}
	virtual int read(int16_t *buffer, int num_samles) override {
		// 与える数はトータルのサンプル数だが、戻り値は１チャンネルあたりの読み取りサンプル数であることに注意。
		// 正しく読み込まれていれば、戻り値にチャンネル数をかけたものが、引数で与えた読み取りサンプル数と一致する
		int got_samples_per_channel = stb_vorbis_get_samples_short_interleaved(vorbis_, info_.channels, buffer, num_samles);
		return got_samples_per_channel * info_.channels;
	}
	virtual int tell() const override {
		int block = (int)stb_vorbis_get_sample_offset(vorbis_);
		return block * info_.channels;
	}
};
#endif // MO_STBVORBIS


#ifdef _WIN32
class CWavImplWinMM: public SoundInput::Impl {
	HMMIO mmio_;
	WAVEFORMATEX fmt_;
	MMCKINFO chunk_;
	std::string buf_;
	int total_samples_;
	int bytes_per_sample_;
public:
	/// file のヘッダが .WAV フォーマットかどうか調べる。
	/// @attention この関数は読み取り位置を元に戻さない
	static bool check(FileInput &file) {
		bool result = false;
		int pos = file.tell();
		char hdr[12];
		if (file.read(hdr, 12) == sizeof(hdr)) {
			if (strncmp(hdr + 0, "RIFF", 4) == 0) {
				if (strncmp(hdr + 8, "WAVE", 4) == 0) {
					result = true;
				}
			}
		}
		file.seek(pos); // 元に戻すのを忘れないように！
		return result;
	}
public:
	CWavImplWinMM() {
		memset(&fmt_, 0, sizeof(fmt_));
		memset(&chunk_, 0, sizeof(chunk_));
		mmio_ = NULL;
		total_samples_ = 0;
		bytes_per_sample_ = 0;
	}
	CWavImplWinMM(FileInput &file, int *err) {
		SNDASSERT(err);
		int pos = file.tell();
		buf_.clear(); // to fill zero when resize() called.
		buf_.resize(file.size() - pos);
		file.read(&buf_[0], (int)buf_.size());
		// http://hiroshi0945.blog75.fc2.com/?mode=m&no=33
		MMIOINFO info;
		memset(&info, 0, sizeof(MMIOINFO));
		info.pchBuffer = reinterpret_cast<HPSTR>(&buf_[0]);
		info.fccIOProc = FOURCC_MEM; // read from memory
		info.cchBuffer = buf_.size();
		HMMIO hMmio = mmioOpen(NULL, &info, MMIO_ALLOCBUF|MMIO_READ);
		if (hMmio == NULL) {
			// 失敗。ファイル読み取り位置を戻しておく
			file.seek(pos);
			*err = 1;
			return;
		}
		if (!open_mmio(hMmio)) {
			// 失敗。ファイル読み取り位置を戻しておく
			file.seek(pos);
			*err = 1;
			return;
		}
		bytes_per_sample_ = fmt_.wBitsPerSample / 8;
		total_samples_ = (int)buf_.size() / bytes_per_sample_;
		*err = 0;
	}
	virtual ~CWavImplWinMM() {
		mmioClose(mmio_, 0);
	}
	bool open_mmio(HMMIO hMmio) {
		// RIFFチャンク検索
		MMRESULT mmRes;
		MMCKINFO riffChunk;
		riffChunk.fccType = mmioStringToFOURCCA("WAVE", 0);
		mmRes = mmioDescend(hMmio, &riffChunk, NULL, MMIO_FINDRIFF);
		if (mmRes != MMSYSERR_NOERROR) {
			mmioClose(hMmio, 0);
			return false;
		}
		// フォーマットチャンク検索
		MMCKINFO formatChunk;
		formatChunk.ckid = mmioStringToFOURCCA("fmt ", 0);
		mmRes = mmioDescend(hMmio, &formatChunk, &riffChunk, MMIO_FINDCHUNK);
		if (mmRes != MMSYSERR_NOERROR) {
			mmioClose(hMmio, 0);
			return false;
		}
		// WAVEFORMATEX構造体格納
		DWORD fmsize = formatChunk.cksize;
		DWORD size = mmioRead(hMmio, (HPSTR)&fmt_, fmsize);
		if (size != fmsize) {
			mmioClose(hMmio, 0);
			return false;
		}
		// RIFFチャンクに戻る
		mmioAscend(hMmio, &formatChunk, 0);
		// データチャンク検索
		chunk_.ckid = mmioStringToFOURCCA("data ", 0);
		mmRes = mmioDescend(hMmio, &chunk_, &riffChunk, MMIO_FINDCHUNK);
		if (mmRes != MMSYSERR_NOERROR) {
			mmioClose(hMmio, 0);
			return false;
		}
		// いまのところ16ビットサウンドにのみ対応
		if (fmt_.wBitsPerSample != OGG_BIT_PER_SEC_16) {
			mmioClose(hMmio, 0);
			return false;
		}
		mmio_ = hMmio;
		return true;
	}
	virtual int sample_count() const override {
		return total_samples_;
	}
	virtual int channel_count() const override {
		return fmt_.nChannels;
	}
	virtual int sample_rate() const override {
		return fmt_.nSamplesPerSec;
	}
	virtual int seek(int sample_offset) override {
		int num_blocks = sample_offset / fmt_.nChannels;
		int block_bytes = fmt_.nBlockAlign;
		int seekto = chunk_.dwDataOffset + num_blocks * block_bytes;
		int pos = mmioSeek(mmio_, seekto, SEEK_SET);
		if (pos >= 0) {
			SNDASSERT(pos % block_bytes == 0); // 必ず block_bytes 単位でシークする
			return pos / block_bytes;
		}
		return -1;
	}
	virtual int read(int16_t *buffer, int num_samles) override {
		int num_blocks = num_samles / fmt_.nChannels;
		int block_bytes = fmt_.nBlockAlign;
		int gotBytes = mmioRead(mmio_, (HPSTR)buffer, num_blocks * block_bytes);
		return gotBytes / bytes_per_sample_;
	}
	virtual int tell() const override {
		int posBytes = mmioSeek(mmio_, 0, SEEK_CUR);
		return posBytes / SoundInput::BYTES_PER_SAMPLE;
	}
};
#endif // _WIN32


#pragma region SoundInput
SoundInput::SoundInput() {
}
bool SoundInput::open(FileInput &file) {
	impl_ = NULL;

	// .ogg (libogg/libvorbis)
	#ifdef MO_LIBVORBIS
	if (COggImplOV::check(file)) {
		int err = 0;
		auto obj = std::make_shared<COggImplOV>(file, &err);
		if (err == 0) {
			impl_ = obj;
			return true;
		}
	}
	#endif

	// .ogg (stb_vorbis)
	#ifdef MO_STBVORBIS
	if (COggImplStb::check(file)) {
		int err = 0;
		auto obj = std::make_shared<COggImplStb>(file, &err);
		if (err == 0) {
			impl_ = obj;
			return true;
		}
	}
	#endif

	// .wav (winmm)
	#ifdef _WIN32
	if (CWavImplWinMM::check(file)) {
		int err = 0;
		auto obj = std::make_shared<CWavImplWinMM>(file, &err);
		if (err == 0) {
			impl_ = obj;
			return true;
		}
		impl_ = NULL;
	}
	#endif

	return false;
}
int SoundInput::getSampleCount() const {
	if (impl_) {
		return impl_->sample_count();
	}
	return 0;
}
float SoundInput::getTotalSeconds() const {
	int n = getSampleCount() / getChannelCount();
	return (float)n / getSamplesPerSecond();
}
int SoundInput::getChannelCount() const {
	if (impl_) {
		return impl_->channel_count();
	}
	return 0;
}
int SoundInput::getSamplesPerSecond() const {
	if (impl_) {
		return impl_->sample_rate();
	}
	return 0;
}
int SoundInput::seekSamples(int sample_offset) {
	if (impl_) {
		return impl_->seek(sample_offset);
	}
	return 0;
}
int SoundInput::seekSeconds(float sec) {
	int block = (int)(getSamplesPerSecond() * sec);
	int sample = block * getChannelCount();
	return seekSamples(sample);
}
int SoundInput::tellSamples() {
	if (impl_) {
		return impl_->tell();
	}
	return 0;
}
float SoundInput::tellSeconds() {
	int block = tellSamples() / getChannelCount();
	return (float)block / getSamplesPerSecond();
}
int SoundInput::readSamples(int16_t *samples, int max_count) {
	if (impl_) {
		return impl_->read(samples, max_count);
	}
	return 0;
}
int SoundInput::readSamplesLoop(int16_t *samples, int max_count, int loop_start, int loop_end) {
	SNDASSERT(samples);
	SNDASSERT(max_count > 0);
	SNDASSERT(0 <= loop_start);
	SNDASSERT(0 <= loop_end);

	int whole_size = getSampleCount();

	// loop_end が 0 の場合は曲の終端が指定されているものとする
	if (loop_end == 0) loop_end = whole_size;

	// loop_end が曲の終端を超えている場合は修正する
	if (whole_size <= loop_end) loop_end = whole_size;

	int count = 0;
	while (count < max_count) {
		int pos = tellSamples();
		if (pos < loop_end) {
			int avail_r = loop_end - pos; // 読み取り可能なサンプル数（ループ終端までの残りサンプル数）
			int avail_w = max_count - count; // 書き込み可能なサンプル数（バッファ終端までの残りサンプル数）
			int got_samples;
			if (avail_r < avail_w) {
				got_samples = readSamples(samples, avail_r);
			} else {
				got_samples = readSamples(samples, avail_w);
			}
			if (got_samples == 0) {
				break; // error or eof
			}
			count += got_samples;
			samples += got_samples;
		
		} else {
			// ループ終端に達した。先頭に戻る
			seekSamples(loop_start);
		}
	}
	return count;
}


#pragma endregion // SoundInput



} // namespace
