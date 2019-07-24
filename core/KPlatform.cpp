#include "KClock.h"
#include "KCrc32.h"
#include "KDirScan.h"
#include "KFile.h"
#include "KFileTime.h"
#include "KLog.h"
#include "KPlatform.h"
#include "KPath.h"
#include "KStd.h"
#include "KSysInfo.h"
#include "KConv.h"
#include "KNum.h"
#ifdef _WIN32
#	include <Shlobj.h> // File paths
#	include <Shlwapi.h> // File paths
#else
#	include <sys/stat.h> // stat
#	include <unistd.h> // getcwd, access, getpid, chdir
#endif
#include <algorithm> // std::sort
#define MAX_REG_TEXT_LEN    256


#define TOO_DEEP_SUB_FOLDERS_ERROR() Log_fatal_error("DIRECTORY_TOO_DEEP!!\n")


namespace mo {


/// Path を、Win32形式のヌル終端文字列に変換する
void path_normalize(wchar_t *s, const Path &path) {
	std::wstring ws = path.toWideString('\\');
	K_assert(ws.length() < Path::SIZE);
	wcscpy(s, ws.c_str());
}


struct _FontInfo {
	Path facename;
	Path filename;
};
static std::vector<_FontInfo> s_fonts;

#ifdef _WIN32
static int CALLBACK _MSFontCallback(const ENUMLOGFONTEXW *font, const NEWTEXTMETRICEXW *metrix, DWORD type, LPARAM user) {
	if (font->elfLogFont.lfFaceName[0] == '@') {
		return 1; // 横向きフォントはダメ
	}
	if (type != TRUETYPE_FONTTYPE) {
		return 1; // True Type でないとダメ
	}
	switch (font->elfLogFont.lfCharSet) {
//	case ANSI_CHARSET:
	case SHIFTJIS_CHARSET:
		break; // OK
	default:
		return 1; // 日本語または英語対応フォントでないとダメ
	}
	_FontInfo info;
	info.facename = font->elfLogFont.lfFaceName;
	s_fonts.push_back(info);
	return 1; // continue
}
static int _MSFindRegKey(HKEY hKey, const Path &s, Path *key, Path *val) {
	std::wstring ws = s.toWideString();
	for (int i=0; /*SKIP*/; i++) {
		wchar_t name[MAX_REG_TEXT_LEN];
		DWORD namelen = MAX_REG_TEXT_LEN;
		DWORD type = REG_SZ;
		BYTE data[MAX_REG_TEXT_LEN];
		DWORD datalen = MAX_REG_TEXT_LEN;
		memset(name, 0, sizeof(name));
		memset(data, 0, sizeof(data));
		if (RegEnumValueW(hKey, i, name, &namelen, NULL, &type, data, &datalen) != ERROR_SUCCESS) {
			return -1; // NOT FOUND
		}
		std::wstring wname = name;
		if (wname.find(ws) != std::wstring::npos) {
			// レジストリキーが文字列 ws を含んでいる
			if (key) *key = wname;
			if (val) *val = (const wchar_t *)data;
			return i;
		}
	}
	return -1;
}
static void _MSUpdateFontList() {
	s_fonts.clear();

	// フォントファミリー名を列挙
	HWND hWnd = NULL;
	HDC hdc = GetDC(hWnd);
	LOGFONTW font;
	font.lfCharSet = DEFAULT_CHARSET; // 全フォントを検索対象にする。コールバック関数内でフィルタリングする
	font.lfFaceName[0] = 0;
	font.lfPitchAndFamily = 0;
	// フォントの列挙
	// http://vanillasky-room.cocolog-nifty.com/blog/2007/06/post-bb27.html
	EnumFontFamiliesExW(hdc, &font, (FONTENUMPROCW)_MSFontCallback, NULL, 0);
	ReleaseDC(hWnd, hdc);

	// フォントのディレクトリ名を得る
	wchar_t _fdir[Path::SIZE];
	::SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, _fdir);
	Path font_dir = _fdir;

	// 実際のファイル名を調べる
	HKEY hKey;
	Log_info("FIND FILE IN \"HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts\"");
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
		for (int i=0; i<(int)s_fonts.size(); i++) {
			_FontInfo *info = &s_fonts[i];
			// フォント名を含むキーを持つレジストリ項目を探す
			// キーは、"Calibri Light Italic (TrueType)" とか
			// "Meiryo & Meiryo Italic & Meiryo UI & Meiryo UI Italic (TrueType)" などとなっているので、
			// かならず部分一致で探す
			Path key;
			Path val;
			if (_MSFindRegKey(hKey, info->facename, &key, &val) >= 0) {
				Log_infof("[%d] \"%s\" ==> KEY: \"%s\" FILE: \"%s\"", i, info->facename.u8(), key.u8(), val.u8());
				info->filename = font_dir.join(val);
			} else {
				Log_infof("[%d] \"%s\" ==> KEY: (NOT FOUND)", i, info->facename.u8());
			}
		}
		RegCloseKey(hKey);
	}
}
static void _MSGetInfo(PlatformExtraInfo *info) {
	K_assert(info);
	info->max_window_w = GetSystemMetrics(SM_CXMAXIMIZED);
	info->max_window_h = GetSystemMetrics(SM_CYMAXIMIZED);
	info->max_window_client_w = GetSystemMetrics(SM_CXFULLSCREEN);
	info->max_window_client_h = GetSystemMetrics(SM_CYFULLSCREEN);
	info->screen_w = GetSystemMetrics(SM_CXSCREEN);
	info->screen_h = GetSystemMetrics(SM_CYSCREEN);

	// key repeating
	info->keyrepeat_delay_msec = 0;
	info->keyrepeat_interval_msec = 0;
	{
		int del = -1;
		int rep = -1;
		int val = 0;
		// https://msdn.microsoft.com/library/cc429946.aspx
		if (SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &val, 0)) {
			// リピート遅延: 0～3 の値。0 で約250ms, 3で約1000msになる（入力機器によって誤差あり）
			del = 250 + 250 * val; 
		}
		if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &val, 0)) {
			// リピート速度: 0～31 の値。0 で秒間2.5回、31で秒間約30回になる（入力機器によって誤差あり）
			const float FREQ_MIN = 2.5f;
			const float FREQ_MAX = 30.0f;
			float k = Num_normalize(val, 0, 31);
			float freq = Num_lerp(FREQ_MIN, FREQ_MAX, k);
			rep = (int)(1000.0f / freq);
		}
		if (del >= 0 && rep >= 0) {
			info->keyrepeat_delay_msec = del;
			info->keyrepeat_interval_msec = rep;
		}
	}

	// double click
	{
		// https://msdn.microsoft.com/library/cc429812.aspx
		// https://msdn.microsoft.com/library/cc364628.aspx
		info->double_click_dx = GetSystemMetrics(SM_CXDOUBLECLK);
		info->double_click_dy = GetSystemMetrics(SM_CYDOUBLECLK);
		info->double_click_msec = (int)GetDoubleClickTime();
	}
}

class CAppMutex {
	HANDLE hMSP_; // 多重起動の確認用
public:
	CAppMutex() {
		hMSP_ = NULL;
	}
	~CAppMutex() {
		::ReleaseMutex(hMSP_);
		::CloseHandle(hMSP_);
	}
	bool tryLock() {
		// ここでミューテックスの作成を試みる
		// （他方のアプリがこの関数を通している場合にだけ、二重起動が検出できる）

		// 同じライブラリを使っている別アプリと競合せず、自分自身とだけ比較できるよう、
		// ビルド日時文字列を識別子として使う。
		const char *signature = __DATE__ __TIME__;

		hMSP_ = ::CreateMutexA(NULL, TRUE, signature);
		// 既に起動中であればエラーコードが返ってくる
		HRESULT hr = ::GetLastError();
		return (hr != ERROR_ALREADY_EXISTS);
	}
};

static CAppMutex s_app_mutex;
#endif // _WIN32

int Platform::getFileSize(const Path &filename) {
#ifdef _WIN32
	wchar_t wpath[Path::SIZE];
	path_normalize(wpath, filename);
	HANDLE hFile = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD dwSize = GetFileSize(hFile, NULL);
		CloseHandle(hFile);
		if (dwSize == (DWORD)(-1)) {
			return 0;
		}
		return (int)dwSize;
	}
	return 0;
#else
	return 0;
#endif
}

bool Platform::getPhysicalMemorySize(int *total_KB, int *avail_KB, int *used_KB) {
#ifdef _WIN32
	MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
	if (GlobalMemoryStatusEx(&ms)) {
		if (total_KB) *total_KB = (int)(ms.ullTotalPhys / 1024);
		if (avail_KB) *avail_KB = (int)(ms.ullAvailPhys / 1024);
		if (used_KB ) *used_KB  = (int)((ms.ullTotalPhys - ms.ullAvailPhys) / 1024);
		return true;
	}
	return false;
#else
	return false;
#endif
}
Path Platform::getCurrentDir() {
	char s[Path::SIZE] = {'\0'};
	K_getcwd_u8(s, Path::SIZE);
	return Path(s);
}
bool Platform::setCurrentDir(const Path &path) {
	return K_chdir_u8(path.u8()) == 0;
}
bool Platform::fileExists(const Path &path) {
#ifdef _WIN32
	if (! path.empty()) {
		wchar_t wpath[Path::SIZE];
		path_normalize(wpath, path);
		if (PathFileExistsW(wpath)) {
			return true;
		}
	}
	return false;
#else
	if (! path.empty()) {
		if (::access(path.u8(), 0) == 0) {
			return true;
		}
	}
	return false;
#endif
}
bool Platform::directoryExists(const Path &path) {
#ifdef _WIN32
	if (! path.empty()) {
		wchar_t wpath[Path::SIZE];
		path_normalize(wpath, path);
		if (PathIsDirectoryW(wpath)) {
			return true;
		}
	}
	return false;
#else
	if (! path.empty()) {
		struct stat st;
		if (::stat(path.u8(), &st) == 0) {
			if ((st.st_mode & S_IFMT) == S_IFDIR) {
				return true;
			}
		}
	}
	return false;
#endif
}
bool Platform::fileCopy(const Path &src, const Path &dst, bool overwrite) {
#ifdef _WIN32
	wchar_t wsrc[Path::SIZE];
	wchar_t wdst[Path::SIZE];
	path_normalize(wsrc, src);
	path_normalize(wdst, dst);

	BOOL fail_if_exists = !overwrite;
	if (CopyFileW(wsrc, wdst, fail_if_exists)) {
		return true;
	}
	return false;
#else
	if (! overwrite) {
		if (Platform::fileExists(dst)) {
			return false; // already exists
		}
	}
	std::string mb_dst = dst.toAnsiString('/', "");
	std::string mb_src = src.toAnsiString('/', "");

	FILE *fdest = fopen(mb_dst.c_str(), "wb");
	if (fdest == NULL) {
		return false; // cannot open the destination file
	}
	FILE *fsrc = fopen(mb_src.c_str(), "rb");
	if (fsrc == NULL) {
		fclose(fdest);
		return false; // cannot open the source file
	}
	uint8_t buf[1024*4];
	size_t n = 0;
	while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
		fwrite(buf, 1, n, fdest);
	}
	fclose(fdest);
	fclose(fsrc);
	return true;
#endif
}
bool Platform::makeDirectory(const Path &dir, int *err) {
	if (directoryExists(dir)) {
		return true;
	}
#ifdef _WIN32
	wchar_t wdir[Path::SIZE];
	path_normalize(wdir, dir);
	if (CreateDirectoryW(wdir, NULL)) {
		return true;
	}
	uint32_t hr = GetLastError();
	#if MKDIR_TEST
	if (hr == ERROR_ACCESS_DENIED) {
		// アクセス拒否。すこし時間を置けば成功する可能性がある
		if (1) {
			// 少し待ってみる
			int i = 0;
			DWORD timelimit = GetTickCount() + 5000;
			while (GetTickCount() < timelimit) {
				// 少しだけ待つ
				Sleep(500);
				// 再試行
				CreateDirectoryW(wdir, NULL);
				hr = GetLastError();
				SetLastError(0);
				if (hr == S_OK) {
					return true; // うまくいった
				}
				Log_warningf("Failed to make directory '%s'. [Retry %d]", dir.u8(), ++i);
			}
		}
	}
	if (1) {
		Log_warningf("CreateDirectory() returned an error: %s", Win32_GetErrorString(hr).u8());
	}
	#else // MKDIR_TEST
	if (1) {
		Log_warningf("CreateDirectory() returned an error: %d (0x%x)", hr, hr);
	}
	if (hr == ERROR_ACCESS_DENIED) {
		// アクセス拒否。すこし時間を置けば成功する可能性がある
		if (err) *err = 1;
	} else {
		// それ以外の理由でのエラー
		if (err) *err = 2;
	}
	#endif // !MKDIR_TEST
#else
	if (mkdir(dir.u8(), 0777) == 0) { // !! 0777 is not the same as 777 !!
		return true;
	}
#endif
	return false;
}
bool Platform::removeFile(const Path &path) {
	// 初めからパスが存在しない場合は、削除に成功したものとする
	if (! fileExists(path)) return true;

	// ディレクトリは削除できない
	if (directoryExists(path)) return false;

#ifdef _WIN32
	wchar_t wpath[Path::SIZE];
	path_normalize(wpath, path);
	return DeleteFileW(wpath) != FALSE;
#else
	Log_errorf("NOT IMPLEMENTED: %s", __FUNCTION__);
	return false;
#endif
}
bool Platform::isRemovableDirectory(const Path &dir) {
	// 絶対パスは指定できない（間違ってルートディレクトリを指定する事故の軽減）
	if (! dir.isRelative()) {
		Log_errorf("E_REMOVE_DIR_FILES: path is absolute: %s", dir.u8());
		return false;
	}
	// カレントディレクトリそのものは指定できない
	if (dir.empty() || dir == L"." || dir == L"./" || dir == L".\\") {
		Log_errorf("E_REMOVE_DIR_FILES: path includes myself: %s", dir.u8());
		return false;
	}
	// ディレクトリ階層を登るような相対パスは指定できない（意図しないフォルダを消す事故の軽減）
	// 念のため、文字列 ".." を含むパスを一律禁止する
	if (strstr(dir.u8(), "..") != NULL) {
		Log_errorf("E_REMOVE_DIR_FILES: path includes parent directory: %s", dir.u8());
		return false;
	}
	// ディレクトリではなくファイル名だったらダメ
	if (fileExists(dir) && !directoryExists(dir)) {
		Log_errorf("E_REMOVE_DIR_FILES: path is not a directory: %s", dir.u8());
		return false;
	}

	return true;
}

bool Platform::removeEmptyDirectory(const Path &dir, bool subdir, bool subdir_only) {
	if (! isRemovableDirectory(dir)) {
		return false;
	}
#ifdef _WIN32
	// 文字順でソートされたディレクトリリストを得る。
	// これを逆順に辿ると、深いディレクトリから順に処理できるはず
	KDirScan dirscan;
	if (subdir) {
		dirscan.scanRecursive(dir);
	} else {
		dirscan.scanFlat(dir);
	}
	// サブディレクトリを削除
	BOOL ok = TRUE; // 全てのファイルの削除に成功した時のみ true. 初めからファイルが存在しない場合は成功とみなす
	for (int i=dirscan.size()-1; i>=0; i--) {
		const KDirScan::Item &item = dirscan.items(i);
		if (item.is_directory) {
			std::wstring namew = dir.join(item.name).toWideString(Path::NATIVE_DELIM);
			ok &= RemoveDirectoryW(namew.c_str());
		}
	}
	// メインディレクトリを削除
	if (!subdir_only) {
		wchar_t wdir[Path::SIZE];
		path_normalize(wdir, dir);
		ok &= RemoveDirectoryW(wdir);
	}
	return ok != FALSE; // すべて削除できた場合のみ true を返す
#else
	Log_errorf("NOT IMPLEMENTED: %s", __FUNCTION__);
	return false;
#endif
}
bool Platform::removeFilesInDirectory(const Path &dir, bool subdir) {
	if (! isRemovableDirectory(dir)) return false;

	KDirScan dirscan;
	if (subdir) {
		dirscan.scanRecursive(dir);
	} else {
		dirscan.scanFlat(dir);
	}
#ifdef _WIN32
	BOOL ok = TRUE; // 全てのファイルの削除に成功した時のみ true. 初めからファイルが存在しない場合は成功とみなす
	for (int i=0; i<dirscan.size(); i++) {
		const KDirScan::Item &item = dirscan.items(i);
		if (!item.is_directory) {
			std::wstring namew = dir.join(item.name).toWideString(Path::NATIVE_DELIM);
			ok &= DeleteFileW(namew.c_str());
		}
	}
	return ok; // すべて削除できた場合のみ true を返す
#else
	return false;
#endif
}
time_t Platform::getLastModifiedFileTime(const Path &filename) {
	time_t time_cma[] = {0, 0, 0};
	FileTime::getTimeStamp(filename, time_cma);
	return time_cma[1];
}
uint32_t Platform::getFileCRC32(const Path &filename) {
	uint32_t result = KCrc32::INIT;
	FileInput file;
	if (file.open(filename)) {
		uint8_t tmp[1024];
		while (1) {
			int len = file.read(tmp, sizeof(tmp));
			if (len <= 0) break;
			result = KCrc32::compute(tmp, len, result);
			if (len < sizeof(tmp)) break;
		}
	}
	return result;
}
void Platform::getExtraInfo(PlatformExtraInfo *info) {
	if (info == NULL) return;
	memset(info, 0, sizeof(PlatformExtraInfo));
#ifdef _WIN32
	_MSGetInfo(info);
#endif
}
bool Platform::rectIntersectAnyScreen(int x, int y, int w, int h, int *intersection_w, int *intersection_h) {
#ifdef _WIN32
	RECT rect = {x, y, x+w, y+h};
	HMONITOR monitor = ::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	::GetMonitorInfoA(monitor, &info);
	RECT intersection;
	if (::IntersectRect(&intersection, &rect, &info.rcMonitor)) {
		if (intersection_w) *intersection_w = intersection.right - intersection.left;
		if (intersection_h) *intersection_h = intersection.bottom - intersection.top;
		return true;
	}
	return false;
#else
	return true;
#endif
}

void Platform::getScreenSizeByWindow(void *_hWnd, int *w, int *h) {
#ifdef _WIN32
	// hWnd が NULL なら、現在のスレッドのトップウィンドウを得る
	HWND hWnd = _hWnd ? (HWND)_hWnd : GetActiveWindow();

	// ウィンドウを含んでいるモニターを得る
	HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);

	// モニター情報
	MONITORINFO info;
	info.cbSize = sizeof(info);
	GetMonitorInfo(hMon, &info);
	if (w) *w = info.rcWork.right - info.rcWork.left;
	if (h) *h = info.rcWork.bottom - info.rcWork.top;
#endif
}


Path Platform::getSystemFontFileName(const Path &filename) {
#ifdef _WIN32
	wchar_t dir[Path::SIZE];
	::SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, dir);
	return Path(dir).join(filename);
#else
	Log_errorf("INVALID OPERATION: %s", __FUNCTION__);
	return filename;
#endif
}
bool Platform::isApplicatoinAlreadyRunning() {
#ifdef _WIN32
	return !s_app_mutex.tryLock();
#else
	return false;
#endif
}
bool Platform::getFontInfo(int index, Path *face, Path *path) {
#ifdef _WIN32
	if (s_fonts.empty()) {
		_MSUpdateFontList();
	}
#endif
	if (0 <= index && index < (int)s_fonts.size()) {
		if (face) *face = s_fonts[index].facename;
		if (path) *path = s_fonts[index].filename;
		return true;
	}
	return false;
}
Path Platform::getSystemFontDirectory() {
#ifdef _WIN32
	wchar_t ws[Path::SIZE] = {L'\0'};
	SHGetSpecialFolderPathW(NULL, ws, CSIDL_FONTS, FALSE);
	return Path(ws);
#else
	Log_errorf("NOT IMPLEMENTED: %s", __FUNCTION__);
	return Path::Empty;
#endif
}
bool Platform::openFolderInFileBrowser(const Path &path) {
#ifdef _WIN32
	wchar_t wpath[Path::SIZE];
	path_normalize(wpath, path);
	int h;
	if (PathIsDirectoryW(wpath)) {
		h = (int)ShellExecuteW(NULL, L"OPEN", wpath, L"", L"", SW_SHOW);
	} else {
		wchar_t wdir[Path::SIZE];
		path_normalize(wdir, path.directory());
		h = (int)ShellExecuteW(NULL, L"OPEN", wdir, L"", L"", SW_SHOW);
	}
	return h > 32; // ShellExecute は成功すると 32 より大きい値を返す
#else
	Log_errorf("NOT IMPLEMENTED: %s", __FUNCTION__);
	return false;
#endif
}
bool Platform::openFileInSystem(const Path &path) {
#ifdef _WIN32
	wchar_t wpath[Path::SIZE];
	path_normalize(wpath, path);
	if (1) {
		// すごく重い時がある
		int h = (int)ShellExecuteW(NULL, L"OPEN", wpath, L"", L"", SW_SHOW);
		return h > 32; // ShellExecute は成功すると 32 より大きい値を返す
	} else {
		SHELLEXECUTEINFOW info;
		ZeroMemory(&info, sizeof(info));
		info.cbSize = sizeof(info);
		info.lpFile = wpath;
		info.nShow = SW_NORMAL;
		info.fMask = SEE_MASK_ASYNCOK; // 待たずに処理を戻させるが、ファイルを開いてくれない場合がある？
		BOOL b = ShellExecuteExW(&info);
		return b != FALSE;
	}
#else
	Log_errorf("NOT IMPLEMENTED: %s", __FUNCTION__);
	return false;
#endif
}
Path Platform::getSelfFileName() {
#ifdef _WIN32
	wchar_t ws[Path::SIZE] = {L'\0'};
	GetModuleFileNameW(NULL, ws, Path::SIZE);
	return Path(ws);
#else
	/*
	char tmp[Path::SIZE];
	if (_NSGetExecutablePath(tmp, sizeof(Path::SIZE)) > 0) {
		::realpath(tmp, dest);
		return "";
	}
	*/
	Log_errorf("NOT IMPLEMENTED: %s", __FUNCTION__);
	return "";
#endif
}
uint32_t Platform::getSelfProcessId() {
#ifdef _WIN32
	return GetCurrentProcessId();
#else
	return ::getpid();
#endif
}
std::string Platform::getLocalTimeString() {
	time_t gmt = time(NULL);
	KLocalTime localtime(gmt);
	return localtime.format("%y/%m/%d %H:%M:%S");
}
KLocalTime Platform::getLocalTime() {
#ifdef _WIN32
	KLocalTime T;
	SYSTEMTIME st;
	::GetLocalTime(&st);
	T.year   = st.wYear;
	T.month  = st.wMonth;
	T.day    = st.wDay;
	T.hour   = st.wHour;
	T.minute = st.wMinute;
	T.second = st.wSecond;
	T.msec   = st.wMilliseconds;
	return T;
#else
	time_t gmt = ::time(NULL);
	return KLocalTime(gmt);
#endif
}
void Platform::dialog(const char *text_u8) {
	Log_text(text_u8);
#ifdef _WIN32
	// 実行中のスレッドのトップにあるウィンドウを得る
	// (GetForegroundWindow と混同しないこと)
	HWND hWnd = GetActiveWindow();
	std::wstring wname = getSelfFileName().filename().toWideString();
	std::wstring wmsg = KConv::utf8ToWide(text_u8);
	MessageBoxW(hWnd, wmsg.c_str(), wname.c_str(), MB_OK);
#endif
#ifdef __APPLE__
	macos_ShowDialog(text_u8);
#endif
}
void Platform::killSelf() {
#ifdef _WIN32
	if (1) {
		// 黙って強制終了
		// exit() とは違い、この方法で終了すると例外発生ウィンドウが出ない
		TerminateProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId()), 0);
	}
#endif
	exit(-1);
}


} // namespace
