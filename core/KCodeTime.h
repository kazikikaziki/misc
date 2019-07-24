#pragma once
#include "KClock.h"
#include <unordered_map>
#include <string>
#include <stack>

// 簡易時間計測
// 使い方は、計測したい部分のコードを MO_CODETIME_BEGIN と MO_CODETIME_END で挟むだけ
#if 1
#	define MO_CODETIME_BEGIN   mo::KCodeTime::begin(__FILE__, __LINE__)
#	define MO_CODETIME_END     mo::KCodeTime::end()
#else
#	define MO_CODETIME_BEGIN
#	define MO_CODETIME_END
#endif

namespace mo {

class KCodeTime {
public:
	static void begin(const char *file, int line);
	static void end();
};

}
