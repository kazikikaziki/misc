#pragma once

namespace mo {

struct __SOUNDID {
	int dummy;
};
typedef __SOUNDID * SOUNDID;

class Path;
class FileInput;

class Sound {
public:
	static void init(void *hWnd);
	static void shutdown();
	static bool isInitialized();

	/// サウンドファイルをロードし、playPooledSound で再生可能な状態にする
	static void poolSound(const Path &name, FileInput &file);
	static void poolSound(const Path &name, const Path &filename);

	/// サウンドファイルがロード済みであれば true を返す
	static bool isPooled(const Path &name);

	/// ロード済みのサウンドファイルを削除する
	static void deletePooledSound(const Path &name);

	/// ロード済みオーディオクリップを再生し、サウンドハンドルを返す。
	/// サウンドはあらかじめ poolSound でロードしておかなければならない。
	/// ロード済みでなかったり、再生に失敗した場合は 0 を返す
	static SOUNDID playPooledSound(const Path &name);

	/// オーディオクリップをストリーミング再生する
	/// name -- オーディオ名。デバッグ用途でのみ使用する
	/// file -- オーディオデーターへのアクセス用
	/// offset -- 再生開始位置（秒）
	/// looping -- ループ再生するかどうか
	/// lstart -- ループ範囲の始点（秒）
	/// lend   -- ループ範囲の終点（秒）
	static SOUNDID playStreamingSound(const Path &name, FileInput &file, float offsetSeconds=0.0f, bool looping=false, float loopStartSeconds=0.0f, float loopEndSeconds=0.0f);
	static SOUNDID playStreamingSound(const Path &filename, float offsetSeconds=0.0f, bool looping=false, float loopStartSeconds=0.0f, float loopEndSeconds=0.0f);

	/// 再生中のサウンドを一時停止する
	static void pause(SOUNDID snd);

	/// 一時停止状態にあるサウンドを再開する
	static void resume(SOUNDID snd);

	/// サウンド識別子を削除し、関連付けられていた内部インスタンスを削除する
	static void deleteHandle(SOUNDID snd);

	/// 再生位置を秒単位で取得する
	static float getPositionTime(SOUNDID snd);

	/// 再生位置を設定する（秒単位）
	static void setPositionTime(SOUNDID snd, float seconds);

	/// 再生時間を取得する
	static float getLengthTime(SOUNDID snd);

	/// 再生速度を取得する
	static float getPitch(SOUNDID snd);

	/// 再生速度を設定する。1.0 で通常速度
	static void setPitch(SOUNDID snd, float value);

	/// 音像定位を取得する
	static float getPan(SOUNDID snd);

	/// 音像定位を設定する。-1.0 で最も左、0.0で中央, 1.0 で最も右
	static void setPan(SOUNDID snd, float value);

	/// 音量を取得する
	static float getVolume(SOUNDID snd);

	/// 音量を設定する。0.0 で無音、1.0 で最大音量
	static void setVolume(SOUNDID snd, float value);

	/// リピート再生の設定を取得する
	static bool isLooping(SOUNDID snd);

	/// リピート再生を設定する
	static void setLooping(SOUNDID snd, bool value);

	/// 再生中ならば true を返す
	static bool isPlaying(SOUNDID snd);

	/// 再生時に指定されたファイル名を返す
	static const Path & getSourceName(SOUNDID snd);

	/// 3Dサウンドにおける、聞き手の絶対座標を指定する
	static void setListenerPosition(const float *pos_xyz);

	/// 3Dサウンドにおける、音源の座標を指定する
	static void setSourcePosition(SOUNDID snd, const float *pos_xyz);

	static void command(const char *cmd, int *args);
};

void Test_wav(const wchar_t *filename=0);


} // namespace

