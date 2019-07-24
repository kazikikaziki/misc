#pragma once
#include <time.h>
#include <inttypes.h>
struct _FILETIME; // <Windows.h>

namespace mo {

class Path;

class FileTime {
public:
	/// NTFSで使われている64ビットのファイル時刻形式（1601/1/1 0:00から100ナノ秒刻み）から
	/// time_t 形式（1970/1/1 0:00から1秒刻み）を得る
	static time_t to_time_t(uint64_t ntfs_time64);

	/// Win32API の FILETIME (NTFSで使われている64ビットのファイル時刻形式を32bit*2の構造体で表現したもの）形式から
	/// time_t 形式（1970/1/1 0:00から1秒刻み）を得る
	static time_t to_time_t(const _FILETIME *ft);

	/// ファイルのタイムスタンプを得る。
	/// 取得に成功した場合、time_cma に {Creation, Modified, Access} の順で
	/// 3つのtime_tをセットし、trueを返す
	static bool getTimeStamp(const Path &filename, time_t *time_cma);
};

}
