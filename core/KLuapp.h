#pragma once
#include <string>

namespace mo {

struct KLuappDef {
	const char *name;
	const char *value;
};

/// Luaを使ったテキストファイルのプリプロセッサ
///
/// http://lua-users.org/wiki/SimpleLuaPreprocessor
///
/// テキスト文書に埋め込まれた Lua スクリプトを処理する。
/// # で始まる行を lua のスクリプトコードとみなして実行する。
/// # 以外で始まる行はそのまま出力するが、 $(expr) と書いてある部分は、expr がそのまま Lua スクリプトであるとみなし、その評価結果で置換する。
/// 例えば次の3つの記述はすべて同じ結果になる：
/// <pre>
/// 記述１：
/// #for i=1, 3 do
///   <int>Num$(i)</int>
/// #end
///
/// 記述２：
/// #for i=1, 3 do
///   <int>$(string.format("Num%d", i))</int>
/// #end
///
/// 記述３：
/// #for i=1, 3 do
///   <int>$("Num" .. i)</int>
/// #end
///
/// 結果：
///   <int>Num1</int>
///   <int>Num2</int>
///   <int>Num3</int>
/// </pre>
/// 注意：
/// 組み込み lua で boolean として処理されるためには、defines の setString で "true" とか "false" を指定すること。
/// setBool や setInteger だと単なる 0, 1 になってしまい、組み込み lua で 
/// if x then
/// などのように判定している場合に意図しない動作になる。（lua で偽と判定されるのは nil, false のみ。数値の 0 は真になる）
/// "true", "false" は CLuaPrep で boolean として処理される。
/// また "nil" は CLuaPrep で nil として処理される。
bool KLuapp_file(const char *srcfile_u8, const char *dstfile_u8, const char *name_u8, const KLuappDef *defines, int numdef);
bool KLuapp_text(std::string *dst, const char *src_u8, const char *name_u8, const KLuappDef *defines, int numdef);

void Test_luapp();

}
