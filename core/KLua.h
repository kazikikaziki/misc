#pragma once
#include "lua/lua.hpp"
#include <string>

namespace mo {

/// バイナリ化したコードを bin に吐き出す。成功すれば LUA_OK を返す
int KLua_dump(lua_State *ls, std::string *bin);

/// lua_pcall と同じ。エラーが発生した場合にスタックトレース付きのメッセージを取得できる。
/// lua_pcall の戻り値をそのまま返す
int KLua_call(lua_State *ls, int nargs, int ret_args, char *errmsg=NULL, int errsize=0);

/// lua_resume と同じ。エラーが発生した場合にエラーメッセージを取得できる。
/// lua_resume の戻り値をそのまま返す
int KLua_call_yieldable(lua_State *ls, int nargs, char *errmsg=NULL, int errsize=0);

/// スタックの idx 番目にテーブルがあると仮定して、その要素を巡回する。
/// コールバック iter を呼び出す。コールバックが呼ばれた時、スタックトップにはキー文字列、その次には値が入っている。
/// なお、コールバックが呼ばれた時にスタックにある2個の値は、lua_tostring しても安全である
int KLua_enum_table(lua_State *ls, int idx, void (*iter)(lua_State *, void *), void *user);

/// 現在の呼び出しスタックを得る
int KLua_callstack(lua_State *ls, int level, char *file, int *line);

int KLua_load(lua_State *ls, const char *code, int size, const char *debug_name, int no_call=0);

/// lua_tointeger の int 版
int KLua_toint(lua_State *ls, int idx);

/// lua_Number の float 版
float KLua_tofloat(lua_State *ls, int idx);

/// lua_toboolean と同じだが、整数表記も受け入れる。
/// ※lua_toboolean は true/false の場合にしか働かない。
int KLua_toboolean(lua_State *ls, int idx);

const char * KLua_typename(lua_State *ls, int idx);
}