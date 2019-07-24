#include "KFileTime.h"
#include "KPath.h"
#include <inttypes.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

namespace mo {

time_t FileTime::to_time_t(uint64_t ntfs_time64) {
	uint64_t basetime = 116444736000000000; // FILETIME at 1970/1/1 0:00
	uint64_t nsec  = 10000000; // 100nsec --> sec  10 * 1000 * 1000
	return (time_t)((ntfs_time64 - basetime) / nsec);
}
time_t FileTime::to_time_t(const _FILETIME *ft) {
#ifdef _WIN32
	uint64_t ntfs64 = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	return to_time_t(ntfs64);
#else
	return 0;
#endif
}
bool FileTime::getTimeStamp(const Path &filename, time_t *time_cma) {
#ifdef _WIN32
	HANDLE hFile = CreateFileW(filename.toWideString().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return false;
	FILETIME ctm, mtm, atm;
	GetFileTime(hFile, &ctm, &atm, &mtm);
	CloseHandle(hFile);
	time_cma[0] = to_time_t(&ctm); // creation
	time_cma[1] = to_time_t(&mtm); // modify
	time_cma[2] = to_time_t(&atm); // access
	return true;
#else
	struct stat st;
	memset(&st, 0, sizeof(st));
	stat(filename.u8(), &st);
	return st.st_mtime;
#endif
}

}
