#include "KCodeTime.h"
#include <stdio.h>

#ifdef _WIN32
#	include <Windows.h>
#endif

namespace mo {

namespace {
struct Item {
	std::string name;
	KClock cl;
	float total;
	int count;

	Item() {
		total = 0;
		count = 0;
	}
};
static std::unordered_map<std::string, Item> ct_table;
static std::stack<Item*> ct_stack;
static void ct_begin(const char *file, int line) {
	// 計測個所を識別するためのID
	char id_cstr[256];
	sprintf(id_cstr, "%s(%d)", file, line);

	std::string id(id_cstr);

	// 計測開始
	Item &item = ct_table[id];
	item.name = id;
	item.cl.reset();
	item.count++;

	// スタックに積む
	ct_stack.push(&item);
}
static void ct_print(const Item &item) {
#ifdef _WIN32
	{
		char s[256];
		sprintf(s, "%s: %f[ms]\n", item.name.c_str(), item.total / item.count);
		OutputDebugStringA(s);
	}
#else
	printf("%s: %f[ms]\n", item.name.c_str(), item.total / item.count);
#endif
}
static void ct_end() {
	// スタックトップの計測が終了した
	Item &item = *ct_stack.top();

	// 時刻を更新
	item.total += item.cl.getTimeMsecF();

	// 一定回数ごとに平均値を出力する
	if (item.count % 60 == 0) {
		// プリント
		ct_print(item);
		
		// 時刻と回数をリセット
		item.total = 0;
		item.count = 0;
	}
	ct_stack.pop();
}
} // namespace anonymous


void KCodeTime::begin(const char *file, int line) {
	ct_begin(file, line);
}
void KCodeTime::end() {
	ct_end();
}


}
