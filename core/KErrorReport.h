#pragma once
#include <inttypes.h>
#include <string>

namespace mo {

std::wstring Win32_FindErrorLog(int limit_seconds, const wchar_t *keyword, intptr_t *fault_addr=NULL);

void Win32_InstallExeptionHandler();

bool Win32_ErrorCheckProcess(const char *args, const char *comment_u8);

/// 起動時パラメータの確認と、例外フックの設定を両方行う。
/// 例外フックが呼ばれた場合、例外確認オプションを指定して自分自身を二重起動する。
/// 起動時パラメータ args を確認し、例外確認オプションで起動されている場合は
/// Windows のエラーログに登録されているであろう例外イベントをチェックし、その中身を表示する。
/// 要するに、WinMain の一番最初でこの関数を呼ぶだけでよい。
/// 例外確認モードで起動されていた場合、Win32_ErrorChecker は必要な処理を行って true を返す。
/// その時は何もせずに正に終了させること。
bool Win32_ErrorChecker(const char *args, const char *comment_u8);

} // namespace
