#include "KNumval.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#if 0
#	include "KLog.h"
#	define NUMASSERT(x) Log_assert(x)
#else
#	include <assert.h>
#	define NUMASSERT(x) assert(x)
#endif


namespace mo {

static float __strtof(const char *s) {
	if (s == NULL || s[0] == '\0' || isblank(s[0])) return 0.0f; // 空文字列または空白文字だけで構成されている場合、strtoul はエラーにならない。事前にチェックが必要
	char *err = NULL;
	float value = strtof(s, &err);
	if (err && *err) return 0.0f;
	return value;
}

KNumval::KNumval() {
	clear();
}
KNumval::KNumval(const char *expr) {
	clear();
	parse(expr);
}
void KNumval::clear() {
	num[0] = '\0';
	suf[0] = '\0';
	numf = 0.0f;
}
void KNumval::assign(const KNumval &source) {
	*this = source;
}
bool KNumval::parse(const char *expr) {
	if (expr == NULL || expr[0] == 0) return false;

	// 数字文字列と単位文字列に分割
	int pos = (int)strlen(expr);
	for (int i=(int)strlen(expr)-1; i>=0; i--) {
		NUMASSERT(!isblank(expr[i]));
		if (isdigit(expr[i])) {
			break;
		}
		pos = i;
	}
	strncpy(this->num, expr, pos); // [pos] よりも前を取得
	this->num[pos] = 0;

	strcpy(this->suf, expr+pos); // [pos] よりも後を取得

	// 数字文字列を解析
	this->numf = __strtof(this->num);
	return true;
}
bool KNumval::has_suffix(const char *_suf) const {
	return strcmp(this->suf, _suf) == 0;
}
int KNumval::valuei(int percent_base) const {
	if (has_suffix("%")) {
		return (int)(this->numf * percent_base / 100);
	} else {
		return (int)(this->numf);
	}
}
float KNumval::valuef(float percent_base) const {
	if (has_suffix("%")) {
		return this->numf * percent_base / 100;
	} else {
		return this->numf;
	}
}


void Test_numval() {
	{
		KNumval n("20");
		NUMASSERT(strcmp(n.num, "20") == 0);
		NUMASSERT(strcmp(n.suf, "") == 0);
		NUMASSERT(n.numf == 20);
		NUMASSERT(n.has_suffix("") == true);
		NUMASSERT(n.valuei(0) == 20); // % が付いていないので引数は無視される
		NUMASSERT(n.valuei(100) == 20); // % が付いていないので引数は無視される
	}

	{
		KNumval n("70%");
		NUMASSERT(strcmp(n.num, "70") == 0);
		NUMASSERT(strcmp(n.suf, "%") == 0);
		NUMASSERT(n.numf == 70);
		NUMASSERT(n.has_suffix("") == false);
		NUMASSERT(n.has_suffix("%") == true);
		NUMASSERT(n.valuei(0) == 0); // % が付いている。この場合は 0 の 70% の値を返す
		NUMASSERT(n.valuei(200) == 140); // % が付いている。この場合は 200 の 70% の値を返す
	}

	{
		KNumval n("70pt");
		NUMASSERT(strcmp(n.num, "70") == 0);
		NUMASSERT(strcmp(n.suf, "pt") == 0);
		NUMASSERT(n.numf == 70);
		NUMASSERT(n.has_suffix("") == false);
		NUMASSERT(n.has_suffix("pt") == true);
		NUMASSERT(n.valuei(0) == 70); // % が付いていないので引数は無視される
		NUMASSERT(n.valuei(200) == 70); // % が付いていないので引数は無視される
	}
}

} // namespace
