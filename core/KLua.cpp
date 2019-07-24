#include "KLua.h"
#include "KLog.h"

namespace mo {

/// Lua API の lua_dump() に指定するためのコールバック関数
/// @see http://milkpot.sakura.ne.jp/lua/lua52_manual_ja.html#lua_load
static int KLua__writer(lua_State *ls, const void *p, size_t sz, void *ud) {
	Log_assert(ls);
	Log_assert(p);
	Log_assert(ud);
	std::string *bin = reinterpret_cast<std::string *>(ud);
	size_t off = bin->size();
	bin->resize(off + sz);
	memcpy(&bin[off], p, sz);
	return LUA_OK;
}

/// スクリプト実行中にエラーが発生したときのコールバック関数
static int KLua__err(lua_State *ls) {
	Log_assert(ls);

	int top = lua_gettop(ls);

	// スタックトップにはエラーメッセージが積まれているはずなので、それを取得
	const char *errmsg_u8 = lua_tostring(ls, top);

	if (errmsg_u8) {
		// エラーメッセージがある
		// 呼び出しスタック情報とエラーメッセージを設定する
		luaL_traceback(ls, ls, errmsg_u8, 1);
		
		// traceback 情報を push したことによりスタックサイズが変化してしまっているので、
		// 最初に積んであったエラーメッセージを削除する
		// （エラーメッセージは stacktrace にも入っている）
		lua_remove(ls, top);
		
	} else {
		// スタックトップにあるオブジェクトの文字列表現を得る
		// 可能であれば文字列化のメタメソッドを使用するため、lua_tostring ではなく luaL_tolstring をつかう
		// （スタックサイズは変化しない）
		luaL_tolstring(ls, top, NULL);
	}

	// この関数の呼び出し前後でスタックサイズが変わっていないことを確認
	Log_assert(lua_gettop(ls) == top);
	
	return 1;
}
const char * KLua_typename(lua_State *ls, int idx) {
	int tp = lua_type(ls, idx);
	return lua_typename(ls, tp);
}
int KLua_toint(lua_State *ls, int idx) {
	if (lua_isnumber(ls, idx)) {
		// デフォルトでは 実数に対して lua_tointeger を実行すると失敗する。
		// この動作を変えるためには LUA_FLOORN2I = 1 を定義すればよいが、
		// 面倒なのでコード側で処理する
		// return (int)lua_tointeger(ls, idx); <-- lua_tointeger の時点で失敗するのでダメ
		return (int)lua_tonumber(ls, idx); // 一度 number で受けてから整数化する
	}

	// boolean に対する lua_tointeger は失敗するため、こちらで対処する
	if (lua_isboolean(ls, idx)) {
		return (int)lua_toboolean(ls, idx);
	}

	// number でも boolean でもない
	luaL_error(ls, "invalid integer value");
	return 0;
}
float KLua_tofloat(lua_State *ls, int idx) {
	return (float)lua_tonumber(ls, idx);
}
int KLua_toboolean(lua_State *ls, int idx) {
	if (lua_isboolean(ls, idx)) {
		return lua_toboolean(ls, idx);
	}
	if (lua_isnumber(ls, idx)) {
		// number に対する lua_toboolean は
		// 常に true を返すので自前で処理する
		return lua_tonumber(ls, idx) != 0.0f;
	}

	// number でも boolean でもない
	luaL_error(ls, "invalid boolean value");
	return 0;
}

int KLua_dump(lua_State *ls, std::string *bin) {
	if (lua_dump(ls, KLua__writer, bin, 0) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		Log_errorf("E_LUA_DUMP: %s", msg);
	}
	return LUA_OK;
}
int KLua_call_yieldable(lua_State *ls, int nargs, char *errmsg, int errsize) {
	Log_assert(ls);
	Log_assert(nargs >= 0);
	// 実行＆エラー処理
	// スクリプト内から自分自身が呼ばれる再入可能性に注意
	int res = lua_resume(ls, NULL, nargs);
	if (res == LUA_OK) {
		return LUA_OK;
	}
	if (res == LUA_YIELD) {
		return LUA_YIELD;
	}
	// エラーが発生した。
	// エラーメッセージを得る。
	// lua_resume はスタックを巻き戻さないので、そのままスタックトレース関数が使える
	if (errmsg) {
		KLua__err(ls);
		const char *msg = luaL_optstring(ls, -1, "");
		strncpy(errmsg, msg, errsize);
		errmsg[errsize-1] = '\0';
	}
	lua_settop(ls, 0); // スタックを戻す
	return res;
}
int KLua_call(lua_State *ls, int nargs, int ret_args, char *errmsg, int errsize) {
	Log_assert(ls);
	Log_assert(nargs >= 0);
	Log_assert(ret_args >= 0);
#if 1
	// http://bocchies.hatenablog.com/entry/2013/10/29/003014
	// pcall でのエラー取得用コールバック関数を登録しておく
	// pcall はエラーが発生した場合に積まれるトレースバックをスタックから取り除いてしまう
	// ゆえに、lua_pcall から制御が戻って来た時には既にトレースバックは失われている
	// トレースバック情報が知りたければ、コールバック関数で情報を横取りする必要がある
	lua_pushcfunction(ls, KLua__err);

	// この時点でのスタックはトップから順に
	// [エラーハンドラ] [narg個の引数] [関数]
	// になっているはず。
	// ここでスタックトップにあるエラーハンドラが pcall の邪魔になるため、これを末尾に移動する
	lua_insert(ls, -2-nargs);

	// 実行＆エラー処理
	// スクリプト内から自分自身が呼ばれる再入可能性に注意
	int res = lua_pcall(ls, nargs, ret_args, -2-nargs);
	if (res != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, "");
		if (errmsg) {
			strncpy(errmsg, msg, errsize);
			errmsg[errsize-1] = '\0';
		} else {
			Log_errorf("E_LUA: %s", msg);
		}
		lua_pop(ls, 1); // メッセージを取り除く
		lua_pop(ls, 1); // エラーハンドラがまだ残っているので、これも取り除く
	} else {
		// この時点でのスタックはトップから順に
		// [ret_args個の戻り値] [エラーハンドラ]
		// になっているはず。エラーハンドラはもはや不要なので取り除いておく
		lua_remove(ls, -1-ret_args);
	}
#else
	int res = lua_pcall(ls, nargs, ret_args, 0);
	if (res != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, "");
		if (errmsg) {
			strcpy_s(errmsg, errsize, msg);
		} else {
			Log_errorf("E_LUA: %s", msg);
		}
		lua_pop(ls, 1);
	}
#endif
	return res;
}
int KLua_enum_table(lua_State *ls, int idx, void (*iter)(lua_State *, void *), void *user) {
	if (!lua_istable(ls, idx)) return 0;
	lua_pushvalue(ls, idx);
	lua_pushnil(ls); // 最初の lua_next のための nil をセット
	while (lua_next(ls, -2)) { // この時点で、スタック状態は [tbl][key][val] になっている
		// key は次の lua_next のためにキレイな形でスタックトップに残しておく必要があるため、コピーを積んでしておく。
		// （型が勝手に文字列に代わってしまうため、キーに対して直接 lua_tostring してはいけない）
		lua_pushvalue(ls, -2);
		iter(ls, user); // この時点で、スタック状態は [tbl][key][val][key] になっている
		lua_pop(ls, 2);
	}
	lua_pop(ls, 1); // pop table
	return 0;
}
int KLua_callstack(lua_State *ls, int level, char *file, int *line) {
	lua_Debug ar;
	memset(&ar, 0, sizeof(ar));
	if (lua_getstack(ls, level, &ar)) {
		lua_getinfo(ls, "nSl", &ar);
		if (file) strcpy(file, ar.source);
		if (line) *line = ar.currentline;
		return LUA_OK;
	}
	return 1; // ERR
}
int KLua_load(lua_State *ls, const char *code, int size, const char *debug_name, int no_call) {
	if (luaL_loadbuffer(ls, code, size, debug_name) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		Log_errorf("E_LUA: %s", msg);
		return 1; // ERR
	}
	if (!no_call && lua_pcall(ls, 0, 0, 0) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		Log_errorf("E_LUA: %s", msg);
		return 1; // ERR
	}
	return LUA_OK;
}

} // namespace
