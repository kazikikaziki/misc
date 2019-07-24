#include "KLuapp.h"
#include "lua/lua.hpp" // www.lua.org

#if 1
#include "KLog.h"
#define LUAPP_ASSERT(expr)     Log_assert(expr)
#define LUAPP_ERRORF(fmt, ...) Log_errorf(fmt, ##__VA_ARGS__)
#else
#include <assert.h>
#define LUAPP_ASSERT(expr)     assert(expr)
#define LUAPP_ERRORF(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

namespace mo {

// 入出力ファイル名を引数に与えると、lua側でファイルを開いて置換処理を行う。
// 処理結果の文字列を返す
static const char *KLuapp_file_source = 
	"function prep(src, dst)\n"
	"	local file = assert(io.open(src))\n"
	"	local chunk = {n=0}\n"
	"	table.insert(chunk, string.format('io.output %q', dst))\n"
	"	for line in file:lines() do\n"
	"		if string.find(line, '^#') then\n"
	"			table.insert(chunk, string.sub(line, 2) .. '\\n')\n"
	"		else\n"
	"			local last = 1\n"
	"			for text, expr, index in string.gmatch(line, '(.-)$(%b())()') do \n"
	"				last = index\n"
	"				if text ~= '' then\n"
	"					table.insert(chunk, string.format('io.write %q;', text))\n"
	"				end\n"
	"				table.insert(chunk, string.format('io.write%s;', expr))\n"
	"			end\n"
	"			table.insert(chunk, string.format('io.write %q\\n', string.sub(line, last)..'\\n'))\n"
	"		end\n"
	"	end\n"
	"	return table.concat(chunk, '\\n')\n"
	"end\n";

// テキストを引数に与えると、lua側でテキストを処理する。
// 処理結果の文字列を返す
static const char *KLuapp_text_prep = 
		"function prep(src)\n"
		"	local chunk = {n=0}\n"
		"	table.insert(chunk, 'function __CLuaPrep__()')\n"
		"	table.insert(chunk, 'local S=[[]]')\n"
		"	for line in string.gmatch(src, '(.-)\\r?\\n') do\n"
		"		if string.find(line, '^#') then\n"
		"			table.insert(chunk, string.sub(line, 2) .. '\\n')\n"
		"		else\n"
		"			local last = 1\n"
		"			for text, expr, index in string.gmatch(line, '(.-)$(%b())()') do \n"
		"				last = index\n"
		"				if text ~= '' then\n"
		"					table.insert(chunk, string.format('S = S .. %q;', text))\n"
		"				end\n"
		"				table.insert(chunk, string.format('S = S .. %s;', expr))\n"
		"			end\n"
		"			table.insert(chunk, string.format('S = S .. %q\\n', string.sub(line, last)..'\\n'))\n"
		"		end\n"
		"	end\n"
		"	table.insert(chunk, 'return S')\n"
		"	table.insert(chunk, 'end')\n"
		"	return table.concat(chunk, '\\n')\n"
		"end\n";

// 文字列で表現された値を適切な型で push する
static void KLuapp__push_typed_value(lua_State *ls, const char *val) {
	LUAPP_ASSERT(ls);
	LUAPP_ASSERT(val);
	std::string str(val);

	char *end_ptr = NULL;

	// 空文字列の場合は無条件で文字列とする
	if (val[0] == '\0') {
		lua_pushstring(ls, "");
		return;
	}

	// 整数表記ならば int として push する
	int i = strtol(val, &end_ptr, 0);
	if (end_ptr[0] == '\0') {
		lua_pushinteger(ls, i);
		return;
	}
	// 実数表記ならば float として push する
	float f = strtof(val, &end_ptr);
	if (end_ptr[0] == '\0') {
		lua_pushnumber(ls, f);
		return;
	}
	// 組み込み値表記を処理
	if (str.compare("true") == 0) {
		lua_pushboolean(ls, 1);
		return;
	}
	if (str.compare("false") == 0) {
		lua_pushboolean(ls, 0);
		return;
	}
	if (str.compare("nil") == 0) {
		lua_pushnil(ls);
		return;
	}
	// 上記どれにも該当しないなら文字列として push する
	lua_pushstring(ls, val);
}

// 注意：
// 組み込み lua で boolean として処理されるためには、defines の setString で "true" とか "false" を指定すること。
// setBool や setInteger だと単なる 0, 1 になってしまい、組み込み lua で 
// if x then
// などのように判定している場合に意図しない動作になる。（lua で偽と判定されるのは nil, false のみ。数値の 0 は真になる）
// "true", "false" は CLuaPrep で boolean として処理される。
// また "nil" は CLuaPrep で nil として処理される。
bool KLuapp_file(const char *srcfile_u8, const char *dstfile_u8, const char *name_u8, const KLuappDef *defines, int numdef) {
	std::string generated_lua;
	bool ok = true;
	if (ok) {
		// 埋め込み lua スクリプトを処理し、テキスト展開用のスクリプトを生成する
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);
		ok &= luaL_loadbuffer(ls, KLuapp_file_source, strlen(KLuapp_file_source), "CLuaPrep_prep") == LUA_OK;
		ok &= lua_pcall(ls, 0, 0, 0) == LUA_OK; // 一番外側のスコープを実行
		lua_getglobal(ls, "prep");
		lua_pushstring(ls, srcfile_u8);
		lua_pushstring(ls, dstfile_u8);
		ok &= lua_pcall(ls, 2, 1, 0) == LUA_OK; // 実行
		generated_lua = lua_tostring(ls, -1);
		lua_close(ls);
	}
	if (ok) {
		// テキスト展開用のスクリプトを処理
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);
		luaL_loadbuffer(ls, generated_lua.c_str(), generated_lua.size(), name_u8);
		// 組み込み定義があるなら、グローバル変数として追加しておく
		if (defines) {
			lua_getglobal(ls, "_G");
			for (int i=0; i<numdef; i++) {
				KLuapp__push_typed_value(ls, defines[i].value);
				lua_setfield(ls, -2, defines[i].name); // pop val
			}
			lua_pop(ls, 1); // _G
		}
		ok &= lua_pcall(ls, 0, 0, 0) == LUA_OK; // 実行（チャンク全体を実行する。特に関数を呼ぶ必要はない）
		lua_close(ls);
	}
	return ok;
}
bool KLuapp_text(std::string *dst_u8, const char *src_u8, const char *name_u8, const KLuappDef *defines, int numdef) {
	LUAPP_ASSERT(dst_u8);
	LUAPP_ASSERT(dst_u8->c_str() != src_u8);
	std::string generated_lua;
	bool ok = true;
	{
		// 埋め込み lua スクリプトを処理し、テキスト展開用のスクリプトを生成する
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);

		// チャンクをロードして評価
		ok &= luaL_loadbuffer(ls, KLuapp_text_prep, strlen(KLuapp_text_prep), "CLuaPrep_prep") == LUA_OK;
		ok &= lua_pcall(ls, 0, 0, 0) == LUA_OK;

		// prep(string.gsub(src_u8, "\r\n", "\n"))
		lua_getglobal(ls, "prep");
		luaL_gsub(ls, src_u8, "\r\n", "\n");
		ok &= lua_pcall(ls, 1, 1, 0) == LUA_OK;

		// lua のスクリプト文字列が戻り値になっているので、それを取得して終了
		generated_lua = lua_tostring(ls, -1);
		lua_close(ls);
	}

	if (ok) {
		// テキスト展開用のスクリプトを処理
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);

		luaL_loadbuffer(ls, generated_lua.c_str(), generated_lua.size(), name_u8);
		// 組み込み定義があるなら、グローバル変数として追加しておく
		if (defines) {
			lua_getglobal(ls, "_G");
			for (int i=0; i<numdef; i++) {
				KLuapp__push_typed_value(ls, defines[i].value);
				lua_setfield(ls, -2, defines[i].name); // pop val
			}
			lua_pop(ls, 1); // _G
		}
		if (ok && lua_pcall(ls, 0, 0, 0)!=LUA_OK) { // 一番外側のスコープを実行
			const char *err = luaL_optstring(ls, -1, "");
			LUAPP_ERRORF("E_LUAPP: %s", err ? err : "???");
			ok = false;
		}
		lua_getglobal(ls, "__CLuaPrep__"); // <-- 関数をゲット
		if (ok && lua_pcall(ls, 0, 1, 0)!=LUA_OK) { // 実行
			const char *err = luaL_optstring(ls, -1, "");
			LUAPP_ERRORF("E_LUAPP: %s", err ? err : "???");
			ok = false;
		}
		if (ok) {
			const char *str = lua_tostring(ls, -1);
			*dst_u8 = str ? str : "";
		}
		lua_close(ls);
	}
	return ok;
}

void Test_luapp() {
#ifdef _DEBUG
	const char *i =
		"#for i=1,3 do\n"
		"data$(i)\n"
		"#end\n";
	const char *m =
		"data1\n"
		"data2\n"
		"data3\n";
	std::string o;
	KLuapp_text(&o, i, "Test1", NULL, 0);
//	mo::string_replace(o, "\r\n", "\n");
	LUAPP_ASSERT(o.compare(m) == 0);
#endif
}

} // namespace
