#include "KLocalTime.h"
#include "KStd.h"

namespace mo {

KLocalTime KLocalTime::now() {
	return KLocalTime(::time(NULL));
}
KLocalTime::KLocalTime() {
	// time_t に変換したときに 0 になるように初期化する。
	// ただし、各フィールドに直接 1970/1/1 0:00 の値を入れてはいけない。
	// このクラスはローカルタイムを表すものなので、time_t を 0 にするためには時差を考慮しないといけない。
	// 日本で実行するなら 1970/1/1 9:00 の値をセットしないと time_t が 0 にならず、
	// 1970/1/1 0:00 にしてしまうと世界時間が 1969/12/31 15:00 になってしまい、time_t に変換できなくなる。
	// 初期化に必要な値はロケールに依存するので、各フィールドを直接指定してはいけない
	// year   = 1970;
	// month  = 1; // monthは1起算
	// day    = 1; // dayは1起算
	// hour   = 0;
	// minute = 0;
	// second = 0;
	// msec   = 0;
	set_time(0);
}
KLocalTime::KLocalTime(time_t t) {
	set_time(t);
}
KLocalTime::KLocalTime(const char *s, const char *fmt) {
	set_time(0); // reset
	parse(s, fmt);
}
struct tm KLocalTime::get_tm() const {
	struct tm _tm;
	memset(&_tm, 0, sizeof(_tm));
	if (year >= 1900) {
		_tm.tm_year = year - 1900;
		_tm.tm_mon  = month - 1; // tm の month は 0 起算なので調整
		_tm.tm_mday = day; // tm の day は 1 起算なのでそのまま指定
		_tm.tm_hour = hour;
		_tm.tm_min  = minute;
		_tm.tm_sec  = second;
	}
	return _tm;
}
void KLocalTime::set_time(time_t gmt) {
	struct tm lt;
	K_localtime_s(&lt, gmt);
	year   = lt.tm_year + 1900;
	month  = lt.tm_mon + 1; // tm の month は 0 起算なので調整
	day    = lt.tm_mday;    // tm の day は 1 起算なのでそのまま使う
	hour   = lt.tm_hour;
	minute = lt.tm_min;
	second = lt.tm_sec;
	msec   = 0;
}
time_t KLocalTime::get_time() const {
	struct tm _tm = get_tm();
	time_t t = mktime(&_tm);
	return t;
}
std::string KLocalTime::format(const char *fmt) const {
	const char *f = fmt ? fmt : DEFAULT_TIME_FORMAT;
	char s[256];
	struct tm _tm = get_tm();
	strftime(s, sizeof(s), f, &_tm);
	return s;
}
bool KLocalTime::parse(const char *str, const char *fmt, const char *loc) {
	const char *f = fmt ? fmt : DEFAULT_TIME_FORMAT;
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	if (K_strptime_l(str, f, &tm, loc)) {
		time_t t = mktime(&tm);
		set_time(t);
		return true;
	}
	return false;
}



void Test_localtime() {
	// 作成直後はエポックタイム0を表す
	K_assert(KLocalTime().get_time() == 0);

	{
		// 時刻値 --> time_t
		KLocalTime t;
		t.year = 2018;
		t.month = 12;
		t.day = 17;
		t.hour = 8;
		t.minute = 15;
		t.second = 0;
		K_assert(t.get_time() == 1545002100);
	}
	{
		// time_t --> 時刻値
		KLocalTime t;
		t.set_time(1545002100);
		K_assert(t.year == 2018);
		K_assert(t.month == 12);
		K_assert(t.day == 17);
		K_assert(t.hour == 8);
		K_assert(t.minute == 15);
		K_assert(t.second == 0);
	}
	{
		// 文字列 --> time_t
		KLocalTime t;
		t.parse("18-12-17 8:15:00");
		K_assert(t.get_time() == 1545002100);
	}
}


} // namespace
