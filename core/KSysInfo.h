#pragma once

namespace mo {

struct SystemInfo {
	static const int MAXSIZE = 260;

	/// CPUの名前
	char cpuname[MAXSIZE];

	/// CPUのベンダ名
	char cpuvendor[MAXSIZE];

	/// ローカル日時と時刻
	char localtime[MAXSIZE];

	/// OSの名前 (utf8)
	char productname[MAXSIZE];

	/// 自分自身（実行ファイル）のパス(utf8)
	char exepath[MAXSIZE];

	/// カレントディレクトリ(utf8)
	char curdir[MAXSIZE];

	/// システムフォントディレクトリ(utf8)
	char fontdir[MAXSIZE];

	int mem_total_kb;
	int mem_avail_kb;

	/// システムの言語ID
	///
	/// 0x0C09 : 英語
	/// 0x0411 : 日本語
	/// 0x0404 : 中国語（台湾）
	/// 0x0804 : 中国語（中国）
	/// 0x0C04 : 中国語（香港）
	/// 0x1004 : 中国語（シンガポール）
	/// 0x1404 : 中国語（マカオ）
	///
	/// ロケール ID (LCID) の一覧
	/// "https://docs.microsoft.com/ja-jp/previous-versions/windows/scripting/cc392381(v=msdn.10)"
	///
	int langid;

	/// SSE2に対応しているかどうか
	///
	/// Intel は Pentium4 (2000年) 以降、AMD は Athlon64 (2004年) 以降に対応している。
	/// ちなみにSSE2非対応のPCはWin7の更新対象から外れされた (2018年3月) ほか、
	/// Google Chrome は2014年5月から、
	/// Firefox は2016年4月以降からSSE2必須になった (SSE2非対応マシンでは起動できない）
	/// https://security.srad.jp/story/18/06/21/0850222/
	/// https://rockridge.hatenablog.com/entry/2016/05/29/223926
	/// https://ameblo.jp/miyou55mane/entry-12272544473.html
	///
	bool has_sse2;
};

void getSystemInfo(SystemInfo *info);


}
