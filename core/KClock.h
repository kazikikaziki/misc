#pragma once
#include <inttypes.h>


namespace mo {


/// 時間計測のためのクラス
class KClock {
public:
	/// システム時刻をナノ秒単位で得る
	static uint64_t getSystemTimeNano64();

	/// システム時刻をミリ秒単位で取得する
	static uint64_t getSystemTimeMsec64();
	static int getSystemTimeMsec();
	static float getSystemTimeMsecF();

	/// システム時刻を秒単位で取得する
	static int getSystemTimeSec();
	static float getSystemTimeSecF();

	KClock();

	/// カウンタをゼロにセットする
	/// （ここで前回のリセットからの経過時間を返せば便利そうだが、
	/// ミリ秒なのかナノ秒なのかがあいまいなので何も返さないことにする）
	void reset();

	/// リセットしてからの経過時間をナノ秒単位で返す
	uint64_t getTimeNano64() const;

	/// リセットしてからの経過時間をミリ秒単位で返す
	uint64_t getTimeMsec64() const;

	/// getTimeNano64() と同じだが時刻を int で返す
	int getTimeNano() const;

	/// getTimeMsec64() と同じだが値を int で返す
	int getTimeMsec() const;

	/// getTimeMsec64() と同じだが値をミリ秒単位の実数で返す
	float getTimeMsecF() const;

	/// リセットしてからの経過時間を単位で返す
	float getTimeSecondsF() const;

private:
	uint64_t start_;
};




}