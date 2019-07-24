#include "KPath.h"
#include "KConv.h"
#include <stdarg.h>

#ifdef _WIN32
#	include <Shlwapi.h>
#	pragma comment(lib, "shlwapi.lib")
#endif

#if 1
#	include "KLog.h"
#	define PATHASSERT(x)   Log_assert(x)
#	define PATHFATAL(x)    Log_fatal(x)
#else
#	include <assert.h>
#	define PATHASSERT(x)  assert(x)
#	define PATHFATAL(x)   _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, x)
#endif

#define STRICMP _stricmp
//#define STRICMP stricmp
//#define STRICMP strcasecmp


// 格納可能な文字数を超えたら、もう何もできない。
// 下手に長さを切り詰めたまま処理を続行すると、
// 意図しないファイル名やディレクトリになったりして
// 危険すぎるため、ここで強制終了するのが良い
#define TOO_LONG_PATH_ERROR()  PATHFATAL("TOO LONG PATH STRING!!")




#define SLASH  '/'



namespace mo {

static void Path_replace_char(char *s, char before, char after) {
	PATHASSERT(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}

// 末尾の区切り文字を取り除き、適切な区切り文字に変換した文字列を得る
static void Path_normalize(char *dest, const char *path, int maxlen, char olddelim, char newdelim) {
	PATHASSERT(dest);
	PATHASSERT(path);
	PATHASSERT(maxlen > 0);
	PATHASSERT(isprint(olddelim));
	PATHASSERT(isprint(newdelim));
	// 前後の空白をスキップ
	int s = 0;
	int e = strlen(path);
	while (isascii(path[s]) && isblank(path[s])) {
		s++;
	}
	while (e > 0 && isascii(path[e-1]) && isblank(path[e-1])) {
		e--;
	}

	// 文字数
	int n = e - s;

	// 長さチェック
	if (n >= maxlen) {
		TOO_LONG_PATH_ERROR();
		return;
	}

	// コピー
	strncpy(dest, path + s, n);
	dest[n] = '\0';

	if (n > 0) {
		// 区切り文字を置換
		for (int i=0; i<n; i++) {
			if (path[i] == olddelim) {
				dest[i] = newdelim;
			}
		}
		// 最後の区切り文字は削除する
		if (dest[n-1] == newdelim) {
			dest[n-1] = '\0';
		}
	}
}

/// ファイル名 glob パターンと一致しているかどうかを調べる
/// テストコードは以下の通り
/// @code
/// assert(Path_glob("abc", "*") == true);
/// assert(Path_glob("abc", "a*") == true);
/// assert(Path_glob("abc", "ab*") == true);
/// assert(Path_glob("abc", "abc*") == true);
/// assert(Path_glob("abc", "a*c") == true);
/// assert(Path_glob("abc", "*abc") == true);
/// assert(Path_glob("abc", "*bc") == true);
/// assert(Path_glob("abc", "*c") == true);
/// assert(Path_glob("abc", "*a*b*c*") == true);
/// assert(Path_glob("abc", "*bc*") == true);
/// assert(Path_glob("abc", "*c*") == true);
/// assert(Path_glob("aaa/bbb.ext", "a*.ext") == false); // ワイルドカード * はパス区切り文字を含まない
/// assert(Path_glob("aaa/bbb.ext", "a*/*.ext") == true);
/// assert(Path_glob("aaa/bbb.ext", "a*/*.*t") == true);
/// assert(Path_glob("aaa/bbb.ext", "aaa/*.ext") == true);
/// assert(Path_glob("aaa/bbb.ext", "aaa/bbb*ext") == true);
/// assert(Path_glob("aaa/bbb.ext", "aaa*bbb*ext") == false);
/// assert(Path_glob("aaa/bbb/ccc.ext", "aaa/*/ccc.ext") == true);
/// assert(Path_glob("aaa/bbb/ccc.ext", "aaa/*.ext") == false);
/// assert(Path_glob("aaa/bbb.ext", "*.aaa") == false);
/// assert(Path_glob("aaa/bbb.ext", "aaa*bbb") == false);
/// @endcode
static bool Path_glob(const char *path, const char *glob) {
	if (path == NULL || path[0] == '\0') {
		return false;
	}
	if (glob == NULL || glob[0] == '\0') {
		return false;
	}
	if (glob[0]=='*' && glob[1]=='\0') {
		return true;
	}
	int p = 0;
	int g = 0;
	while (1) {
		const char *sp = path + p;
		const char *sg = glob + g;
		if (*sg == '*') {
			if (/*strcmp(sg, "*") == 0*/sg[0]=='*' && sg[1]=='\0') {
				return true; // 末尾が * だった場合は全てにマッチする
			}
			const char *subglob = sg + 1; // '*' の次の文字列
			for (const char *subpath=sp; *subpath; subpath++) {
				if (subpath[0] == SLASH && subglob[0] != SLASH) {
					return false; // 区切り文字を跨いで判定しない
				}
				if (Path_glob(subpath, subglob)) {
					return true;
				}
			}
			return false;
		}
		if (*sp == SLASH && *sg == '\0') {
			return true; // パス単位で一致しているならOK
		}
		if (*sp == L'\0' && *sg == '\0') {
			return true;
		}
		if (*sp != *sg) {
			return false;
		}
		p++;
		g++;
	}
	return true;
}


#pragma region Path
const Path Path::Empty = Path();


Path Path::fromAnsi(const char *mb, const char *locale) {
	PATHASSERT(mb);
	PATHASSERT(locale);
	return Path(KConv::ansiToUtf8(mb, locale).c_str());
}
Path Path::fromUtf8(const char *u8) {
	return Path(u8 ? u8 : "");
}
Path Path::fromFormat(const char *fmt, ...) {
	char tmp[SIZE];
	va_list args;
	va_start(args, fmt);
	vsprintf(tmp, fmt, args);
	va_end(args);
	return Path(tmp);
}
Path::Path() {
	clear();
}
Path::Path(const char *u8) {
	PATHASSERT(u8);
	if (u8 && u8[0]) {
		u8 = KConv::skipBOM(u8); // BOMをスキップ
		Path_normalize(path_, u8, Path::SIZE, NATIVE_DELIM, SLASH);
	} else {
		path_[0] = 0;
	}
	hash_ = (uint32_t)(-1);
}
Path::Path(const std::string &u8) {
	if (!u8.empty()) {
		Path_normalize(path_, u8.c_str(), Path::SIZE, NATIVE_DELIM, SLASH);
	} else {
		path_[0] = 0;
	}
	hash_ = (uint32_t)(-1);
}
Path::Path(const wchar_t *ws) {
	if (ws && ws[0]) {
		Path_normalize(path_, KConv::wideToUtf8(ws).c_str(), Path::SIZE, NATIVE_DELIM, SLASH);
	} else {
		path_[0] = 0;
	}
	hash_ = (uint32_t)(-1);
}
Path::Path(const std::wstring &ws) {
	if (!ws.empty()) {
		Path_normalize(path_, KConv::wideToUtf8(ws).c_str(), Path::SIZE, NATIVE_DELIM, SLASH);
	} else {
		path_[0] = 0;
	}
	hash_ = (uint32_t)(-1);
}
Path::Path(const Path &other) {
	memcpy(path_, other.path_, sizeof(path_));
	hash_ = (uint32_t)(-1);
}
void Path::clear() {
	path_[0] = 0;
	hash_ = (uint32_t)(-1);
}
bool Path::empty() const {
	return path_[0] == 0;
}
size_t Path::hash() const {
	if (hash_ == (uint32_t)(-1)) {
		hash_ = std::hash<std::string>()(path_);
	}
	return hash_;
}
Path Path::directory() const {
	char tmp[SIZE] = {0};
	strcpy(tmp, path_);
	char *delim = strrchr(tmp, SLASH);
	if (delim) {
		// パス区切りが見つかった。それよりも前の部分を返す
		*delim = '\0';
		return Path(tmp);
	} else {
		// パス区切り無し。ディレクトリ部は存在しないので空のパスを返す
		return Path::Empty;
	}
}
Path Path::filename() const {
	const char *s = strrchr(path_, SLASH);
	if (s) {
		// パス区切りあり。それよりも後ろを返す
		return Path(s+1);
	} else {
		// パス区切り無し。そのまま返す
		return *this;
	}
}
Path Path::extension(bool strip_period) const {
	const char *s = strrchr(path_, '.');
	if (s) {
		if (strip_period && s[0] == '.') {
			return Path(s + 1);
		} else {
			return Path(s);
		}
	} 
	return Path::Empty;
}
Path Path::joinString(const Path &s) const {
	return joinString(s.path_);
}
Path Path::joinString(const char *s) const {
	Path ret;
	size_t a = strlen(path_);
	size_t b = strlen(s);
	if (a + b + 1 >= SIZE) {
		TOO_LONG_PATH_ERROR();
		return Path::Empty;
	}
	strcpy(ret.path_, path_);
	strcat(ret.path_, s);
	return ret;
}
size_t Path::split(PathList *list) const {
	if (list) list->clear();
	size_t num = 0;
	std::string tmp;
	for (size_t i=0; i<strlen(path_); i++) {
		if (path_[i] == SLASH) {
			if (list) list->push_back(tmp);
			num++;
			tmp.clear();
		} else {
			tmp.push_back(path_[i]);
		}
	}
	if (!tmp.empty()) {
		if (list) list->push_back(tmp);
		num++;
	}
	return num;
}
Path Path::getCommonPath(const Path &other) const {
	Path ret;
	PathList list;
	if (getCommonPath(other, &list)) {
		ret.join(list);
	}
	return ret;
}
int Path::getCommonPath(const Path &other, PathList *list) const {
	PathList common;

	// パス区切り文字で区切る
	PathList tok_this;
	PathList tok_other;
	this->split(&tok_this);
	other.split(&tok_other);

	// 先頭の共通パスを得る
	size_t i = 0;
	while (1) {
		if (i >= tok_this.size()) break;
		if (i >= tok_other.size()) break;
		if (tok_this[i] != tok_other[i]) break;
		common.push_back(tok_this[i]);
		i++;
	}

	if (list) *list = common;
	return common.size();
}

Path Path::getRelativePathFrom(const Path &other) const {
	// パス区切り文字で区切る
	PathList tok_this;
	PathList tok_other;
	this->split(&tok_this);
	other.split(&tok_other);

	// 先頭の共通パス部分を削除する
	while (1) {
		if (tok_this.empty()) break;
		if (tok_other.empty()) break;
		if (tok_this[0] != tok_other[0]) break;
		tok_this.erase(tok_this.begin());
		tok_other.erase(tok_other.begin());
	}

	// 必要な数だけ ".." で上に行く
	Path rel;
	for (size_t i=0; i<tok_other.size(); i++) {
		rel = rel.join("..");
	}

	// 登り切ったので、今度は目的地までパスを下げていく
	for (size_t i=0; i<tok_this.size(); i++) {
		rel = rel.join(tok_this[i]);
	}
	return rel;
}
Path Path::join(const Path &more) const {
	if (path_[0] == '\0') {
		return more;
	}
	if (more.path_[0] == '\0') {
		return *this;
	}
	char tmp[SIZE] = {'\0'};
	strcpy(tmp, path_);

	size_t n = strlen(path_);
	strncpy(tmp, path_, n);
	tmp[n] = SLASH;
	strcpy(tmp + n + 1, more.path_);
	return Path(tmp);
}
Path Path::join(const PathList &more) const {
	Path ret;
	for (auto it=more.begin(); it!=more.end(); ++it) {
		ret = ret.join(*it);
	}
	return ret;
}


Path Path::changeExtension(const Path &ext) const {
	return changeExtension(ext.path_);
}
Path Path::changeExtension(const char *ext) const {
	PATHASSERT(ext);
	char tmp[SIZE];
	strcpy(tmp, path_);
	if (ext[0] == '\0') {
		// ext が空文字列の場合は、単に拡張子を削除する
		char *s = strrchr(tmp, '.');
		if (s) *s = '\0';

	} else if (ext[0] == '.') {
		// ext がドットで始まっている場合
		char *s = strrchr(tmp, '.');
		if (s) {
			strcpy(s, ext);
		} else {
			strcat(tmp, ext);
		}

	} else {
		// ext がドットで始まっていない場合
		char *s = strrchr(tmp, '.');
		if (s) {
			strcpy(s+1, ext);
		} else {
			strcat(tmp, ".");
			strcat(tmp, ext);
		}
	}
	return Path(tmp);
}
bool Path::hasExtension(const Path &ext) const {
	return extension().compare(ext) == 0;
}
bool Path::hasExtension(const char *ext) const {
	return extension().compare_str(ext) == 0;
}
bool Path::endsWithPath(const Path &sub) const {
	if (sub.path_[0] == '\0') return true; // String::endsWith と同じ挙動。空文字列εは全ての文字列末尾にマッチする
	size_t sublen = strlen(sub.path_);
	size_t thislen = strlen(path_);
	if (thislen < sublen) return false;
	int idx = (int)(thislen - sublen);
	PATHASSERT(0 <= idx && idx < SIZE);
	if (idx > 0 && path_[idx-1] != SLASH) return false; // パス区切り単位でないとダメ
	if (strcmp(path_ + idx, sub.path_) != 0) return false;
	return true;
}
bool Path::startsWithPath(const Path &sub) const {
	if (sub.path_[0] == '\0') return true; // String::startsWith と同じ挙動。空文字列εは全ての文字列先頭にマッチする
	size_t sublen = strlen(sub.path_);
	size_t thislen = strlen(path_);
	if (thislen < sublen) return false;
	char c = path_[sublen];
	if (c != '\0' && c != SLASH) return false; // パス区切り単位でないとダメ
	if (strncmp(path_, sub.path_, sublen) != 0) return false;
	return true;
}
bool Path::startsWithString(const char *sub) const {
	if (sub == NULL) return true; // NULL の場合は空文字列とみなす。空文字列は常に文字列先頭にマッチする
	size_t sublen = strlen(sub);
	size_t thislen = strlen(path_);
	if (thislen < sublen) return false; // sub文字列の方が長い場合は決してマッチしない
	return strncmp(path_, sub, sublen) == 0;
}
bool Path::endsWithString(const char *sub) const {
	if (sub == NULL) return true; // NULL の場合は空文字列とみなす。空文字列は常に文字列末尾にマッチする
	size_t sublen = strlen(sub);
	size_t thislen = strlen(path_);
	if (thislen < sublen) return false; // sub文字列の方が長い場合は決してマッチしない
	return strncmp(path_, sub, sublen) == 0;
}
bool Path::operator ==(const Path &p) const {
	return compare(p) == 0;
}
bool Path::operator !=(const Path &p) const {
	return compare(p) != 0;
}
bool Path::operator < (const Path &p) const {
	return compare(p) < 0;
}
int Path::compare_str(const char *p) const {
	PATHASSERT(p);
	return strcmp(path_, p);
}
int Path::compare_str(const char *p, bool ignore_case, bool ignore_path) const {
	PATHASSERT(p);
	if (ignore_path) {
		const char *s1 = strrchr(path_, SLASH);
		const char *s2 = strrchr(p,     SLASH);
		if (s1) { s1++; /* SLASHの次の文字へ */ } else { s1 = path_; }
		if (s2) { s2++; /* SLASHの次の文字へ */ } else { s2 = p; }
		return ignore_case ? STRICMP(s1, s2) : strcmp(s1, s2);
	
	} else {
		const char *s1 = path_;
		const char *s2 = p;
		return ignore_case ? STRICMP(s1, s2) : strcmp(s1, s2);
	}
}
int Path::compare(const Path &p) const {
	return compare_str(p.path_);
}
int Path::compare(const Path &p, bool ignore_case, bool ignore_path) const {
	return compare_str(p.path_, ignore_case, ignore_path);
}
bool Path::glob(const Path &pattern) const {
	return Path_glob(path_, pattern.path_);
}
const char * Path::u8() const {
	return path_;
}

Path Path::getFullPath() const {
	std::wstring ws = toWideString(NATIVE_DELIM);
	wchar_t wout[SIZE] = {0};
	GetFullPathNameW(ws.c_str(), SIZE, wout, NULL);
	return Path(wout);
}
bool Path::isRelative() const {
	std::wstring ws = toWideString(NATIVE_DELIM);
	return PathIsRelativeW(ws.c_str());
}
std::wstring Path::toWideString(char sep) const {
	char tmp[SIZE];
	strcpy(tmp, path_);
	Path_replace_char(tmp, SLASH, sep);
	return KConv::utf8ToWide(tmp);
}
std::string Path::toAnsiString(char sep, const char *_locale) const {
	PATHASSERT(_locale);
	char tmp[SIZE];
	strcpy(tmp, path_);
	Path_replace_char(tmp, SLASH, sep);
	return KConv::utf8ToAnsi(tmp, _locale);
}
std::string Path::toUtf8(char sep) const {
	std::string u8 = path_;

	// 区切り文字を置換
	for (size_t i=0; u8[i]; i++) {
		if (u8[i] == SLASH) {
			u8[i] = sep;
		}
	}
	return u8;
}
#pragma endregion // Path



void Test_pathstring() {
	// なんとcrc32が同値になる (crc32b の場合）
	//PATHASSERT(Path("girl/186/05(cn).sprite").hash() == Path("effect/fire_wide_05.mesh").hash());
	{
		PATHASSERT(Path(".").compare(".") == 0); // 特殊な文字が省略されない
		PATHASSERT(Path("..").compare("..") == 0); // 特殊な文字が省略されない
		PATHASSERT(Path("../a").compare("../a") == 0); // 特殊な文字が省略されない
		PATHASSERT(Path("aaa/..").join("bbb").compare("aaa/../bbb") == 0); // 相対パス指定でも素直にパス結合できる
		PATHASSERT(Path("..").join("aaa").compare("../aaa") == 0); // 相対パス指定でも素直にパス結合できる
		PATHASSERT(Path("..").join("..").compare("../..") == 0); // 相対パス指定でも素直にパス結合できる
		PATHASSERT(Path("AAA").compare_str("AAA") == 0);
		PATHASSERT(Path("AAA\\bbb").compare_str("AAA/bbb") == 0); // compare_str は内部の文字列と直接比較する。内部ではデリミタが変換されているので注意
		PATHASSERT(Path("aaa/bbb").compare_str("bbb", false, true) == 0); // パスを無視して比較
		PATHASSERT(Path("aaa/bbb").compare_str("BBB", true, true) == 0); // 大小文字とパスを無視して比較
		PATHASSERT(Path("").directory() == "");
		PATHASSERT(Path("").filename() == "");
		PATHASSERT(Path("").extension() == "");
		PATHASSERT(Path("").isRelative());
		PATHASSERT(Path("aaa").isRelative());
		PATHASSERT(Path("../aaa").isRelative());
		PATHASSERT(!Path("c:").isRelative());
		PATHASSERT(!Path("c:\\aaa").isRelative());

		PATHASSERT(Path("").join("") == "");
		PATHASSERT(Path("").join("aaa") == "aaa");
		PATHASSERT(Path("aaa").join("bbb") == "aaa/bbb");
		PATHASSERT(Path("aaa").join("") == "aaa");

		PATHASSERT(Path("").directory() == "");
		PATHASSERT(Path("aaa.txt").directory() == "");
		PATHASSERT(Path("aaa\\bbb.txt").directory() == "aaa");
		PATHASSERT(Path("aaa\\bbb\\ccc.txt").directory() == "aaa/bbb");

		PATHASSERT(Path("aaa.txt").filename() == "aaa.txt");
		PATHASSERT(Path("aaa\\bbb.txt").filename() == "bbb.txt");
		PATHASSERT(Path("aaa\\bbb\\ccc.txt").filename() == "ccc.txt");
		PATHASSERT(Path("aaa\\bbb.zzz\\ccc.txt").filename() == "ccc.txt");

		PATHASSERT(Path("q/aaa/bbb.c").getRelativePathFrom("q/aaa"    ).compare("bbb.c") == 0);
		PATHASSERT(Path("q/aaa/bbb.c").getRelativePathFrom("q/aaa/ccc").compare("../bbb.c") == 0);
		PATHASSERT(Path("q/aaa/bbb.c").getRelativePathFrom("q/bbb"    ).compare("../aaa/bbb.c") == 0);
		PATHASSERT(Path("q/aaa/bbb.c").getRelativePathFrom("q/bbb/ccc").compare("../../aaa/bbb.c") == 0);

		PATHASSERT(Path("q/aaa/bbb.c").changeExtension(".txt").compare("q/aaa/bbb.txt") == 0);
		PATHASSERT(Path("q/aaa/bbb.c").changeExtension( "txt").compare("q/aaa/bbb.txt") == 0);
		PATHASSERT(Path("q/aaa/bbb"  ).changeExtension(".txt").compare("q/aaa/bbb.txt") == 0);
		PATHASSERT(Path("q/aaa/bbb"  ).changeExtension( "txt").compare("q/aaa/bbb.txt") == 0);
		PATHASSERT(Path("q/aaa/bbb.c").changeExtension("").compare("q/aaa/bbb") == 0);
		PATHASSERT(Path("q/aaa/bbb"  ).changeExtension("").compare("q/aaa/bbb") == 0);

		PathList list;
		Path("aaa/bbb/ccc").split(&list);
		PATHASSERT(list.size() == 3);
		PATHASSERT(list[0] == "aaa");
		PATHASSERT(list[1] == "bbb");
		PATHASSERT(list[2] == "ccc");

		Path("/").split(&list);
		PATHASSERT(list.empty());

		Path("c:/aaa/bbb/").split(&list);
		PATHASSERT(list.size() == 3);
		PATHASSERT(list[0] == "c:");
		PATHASSERT(list[1] == "aaa");
		PATHASSERT(list[2] == "bbb");
	}
	{
		PATHASSERT(Path("abc").glob("*") == true);
		PATHASSERT(Path("abc").glob("a*") == true);
		PATHASSERT(Path("abc").glob("ab*") == true);
		PATHASSERT(Path("abc").glob("abc*") == true);
		PATHASSERT(Path("abc").glob("a*c") == true);
		PATHASSERT(Path("abc").glob("*abc") == true);
		PATHASSERT(Path("abc").glob("*bc") == true);
		PATHASSERT(Path("abc").glob("*c") == true);
		PATHASSERT(Path("abc").glob("*a*b*c*") == true);
		PATHASSERT(Path("abc").glob("*bc*") == true);
		PATHASSERT(Path("abc").glob("*c*") == true);
		PATHASSERT(Path("aaa/bbb.ext").glob("a*.ext") == false);
		PATHASSERT(Path("aaa/bbb.ext").glob("a*/*.ext") == true);
		PATHASSERT(Path("aaa/bbb.ext").glob("a*/*.*t") == true);
		PATHASSERT(Path("aaa/bbb.ext").glob("aaa/*.ext") == true);
		PATHASSERT(Path("aaa/bbb.ext").glob("aaa/bbb*ext") == true);
		PATHASSERT(Path("aaa/bbb.ext").glob("aaa*bbb*ext") == false);
		PATHASSERT(Path("aaa/bbb/ccc.ext").glob("aaa/*/ccc.ext") == true);
		PATHASSERT(Path("aaa/bbb/ccc.ext").glob("aaa/*.ext") == false);
		PATHASSERT(Path("aaa/bbb.ext").glob("*.aaa") == false);
		PATHASSERT(Path("aaa/bbb.ext").glob("aaa*bbb") == false);
	}
	{
		char loc[32];
		strcpy(loc, setlocale(LC_CTYPE, NULL));
		PATHASSERT(Path(u8"表計算\\ソフト\\能力.txt").directory() == Path(u8"表計算/ソフト"));
		PATHASSERT(Path(u8"表計算\\ソフト\\能力.txt").filename()  == Path(u8"能力.txt"));
		PATHASSERT(Path(u8"表計算\\ソフト\\能力.txt").extension().compare(".txt") == 0);
		PATHASSERT(Path(u8"表計算\\ソフト\\能力.txt").changeExtension(".bmp") == Path(u8"表計算\\ソフト\\能力.bmp"));
		PATHASSERT(Path(u8"表計算\\ソフト\\能力.txt").changeExtension("") == Path(u8"表計算\\ソフト\\能力"));
		setlocale(LC_CTYPE, loc);
	}
}

} // namespace
