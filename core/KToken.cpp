#include "KToken.h"

#define BLANK_CHARS "\n\r\t "

#if 1
#	include "KLog.h"
#	define TOKASSERT(x)  Log_assert(x)
#else
#	include <assert.h>
#	define TOKASSERT(x)  assert(x)
#endif

namespace mo {

static int Tok_isblank(int c) {
	return isascii(c) && isblank(c);
}

// s で最初に現れる非空白文字に移動する
static char * Tok_skip_blank(char *s) {
	return s + strspn(s, BLANK_CHARS);
}
static const char * Tok_skip_blank(const char *s) {
	return s + strspn(s, BLANK_CHARS);
}

// 範囲 [s..e] の文字列の末尾にある空白文字を '\0' で上書きする。
// 新しい末尾位置（ヌル文字）ポインタを返す
static char * Tok_trim_rihgt_blanks(char *s, char *e) {
	while (s < e && Tok_isblank(e[-1])) {
		e[-1] = '\0';
		e--;
	}
	return e;
}

// "\r\n" ===> "\n"
static void Tok_convert_newlines(std::string &out, const char *s) {
	out.reserve(strlen(s)); // 少なくとも元の文字列よりかは短くなるので strlen(s) だけ確保しておけばよい
	for (const char *c=s; *c; /*none*/) {
		if (c[0] == '\r' && c[1] == '\n') {
			out.push_back('\n');
			c += 2;
		} else if (c[0] == '\n') {
			out.push_back('\n');
			c++;
		} else {
			out.push_back(c[0]);
			c++;
		}
	}
}





KToken::KToken() {
}
KToken::KToken(const char *text, const char *delims, bool condense_delims, size_t maxdiv) {
	tokenize(text, delims, condense_delims, maxdiv);
}

void KToken::trim(_Range &str) {
	TOKASSERT(str.s);
	TOKASSERT(str.e);
	while (isascii(*str.s) && isblank(*str.s)) {
		str.s++;
	}
	while (str.s < str.e && isascii(str.e[-1]) && isblank(str.e[-1])) {
		str.e--;
	}
}

void KToken::tokenize_raw(const char *text, const char *delims, bool condense_delims, int maxdiv, std::vector<_Range> &ranges) {
	TOKASSERT(text);
	TOKASSERT(delims);
	TOKASSERT(ranges.empty());
	char *s = (char*)text; // トークン始点
	char *p = s;
	while (*p) {
		if (strchr(delims, *p)) {
			// デリミタが見つかった（文字 *p が delims に含まれている）

			// トークンを登録。
			// デリミタ圧縮が有効な場合はトークン長さをチェックする（長さが 0 ならデリミタが連続しているのでトークン追加しない）
			// デリミタ圧縮が無効ならば長さ 0 のトークンでも有効なトークンとして追加する
			if (!condense_delims || s < p) {
				_Range tok = {s, p};
				ranges.push_back(tok);
			}

			// デリミタをスキップして次トークンの先頭に移動する
			// condense_delims がセットされているならば連続するデリミタを1つの塊として扱う
			size_t span = 1;
			if (condense_delims) {
				span = strspn(p, delims);
			}
			TOKASSERT(span > 0);
			p += span;
			s = p;

			// トークン分割制限がある場合は、トークン数が分割数-1に達したら終了する。
			// -1しているのは、最後のトークンとして「残りの文字列全て」を追加するため
			if (maxdiv == 0 && ranges.size() + 1 == maxdiv) {
				break;
			}
		} else {
			p++;
		}
	}
	// 末尾トークンを登録
	if (*s) {
		if (!condense_delims || s < p) {
			_Range tok = {s, p};
			ranges.push_back(tok);
		}
	}
	TOKASSERT(maxdiv == 0 || (int)ranges.size() <= maxdiv);
}

size_t KToken::tokenize(const char *text, const char *delims, bool condense_delims, size_t maxdiv) {
	s_.clear();
	tok_.clear();
	if (text == NULL || text[0] == '\0') return 0;
	if (delims == NULL || delims[0] == '\0') return 0;

	// strtok による文字列破壊に備え、入力文字列をコピーする
	s_.assign(text);

	// デリミタに応じて素直に分割する
	tokenize_raw(s_.c_str(), delims, condense_delims, maxdiv, tok_);

	// 各トークンの前後の空白を取り除く（トークン範囲を変更するだけ）
	for (auto it=tok_.begin(); it!=tok_.end(); it++) {
		trim(*it);
	}
	
	// トークンの終端にヌル文字を置く
	// 元々 ranges には this->s_ のアドレスが入っているため、
	// ここでは this->s_ の内容が書き換わることになる
	for (auto it=tok_.begin(); it!=tok_.end(); it++) {
		*(char*)(it->e) = '\0';
	}
	return tok_.size();
}
size_t KToken::splitLines(const char *text, bool with_empty_line) {
	s_.clear();
	tok_.clear();
	if (text == NULL || text[0] == '\0') return 0;

	// 処理を簡単かつ確実にするため、１文字で改行を表すようにしておく
	std::string cpy;
	Tok_convert_newlines(cpy, text);

	// 空白行を認識させるなら、連続する改行文字を結合してはいけない
	bool condense_delims = !with_empty_line;

	// 改行文字を区切りにして分割する
	return tokenize(cpy.c_str(), "\n", condense_delims);
}
size_t KToken::split(const char *text, const char *delims) {
	return tokenize(text, delims, true, 2);
}
const char * KToken::operator[](size_t index) const { 
	return tok_[index].s;
}
const char * KToken::get(size_t index, const char *def) const {
	if (index < (int)tok_.size()) {
		return tok_[index].s;
	} else {
		return def;
	}
}
int KToken::compare(size_t index, const char *str) const {
	TOKASSERT(str);
	const char *s = get(index, NULL);
	if (s) {
		return strcmp(s, str);
	} else {
		return strcmp("", str);
	}
}
size_t KToken::size() const { 
	return tok_.size();
}

} // namespace
