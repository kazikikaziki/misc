#include <assert.h>
#include <intrin.h> // __cpuid
#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <string>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <Windows.h>
#include <WindowsX.h> // GET_X_LPARAM
#include "Win32.h"

#ifndef NO_STB_TRUETYPE
//#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif

#define K_MALLOC   malloc
#define K_REALLOC  realloc
#define K_FREE     free
#define K_ASSERT   assert
#define K_WCSICMP  _wcsicmp

#ifdef NOMINMAX
#	define K_MIN std::min
#	define K_MAX std::max
#else
#	define K_MIN min
#	define K_MAX max
#endif

#define K_RELEASE(x) if (x) { (x)->Release(); (x)=NULL; }
#define K_SMALL_ICON_SIZE_W  16
#define K_SMALL_ICON_SIZE_H  16
#define K_LANGID_USER_DEFAULT  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)             // 0x0400
#define K_LANGID_ENGLISH       MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)          // 0x0C09


#pragma region Win32 Functions
void Win32_PrintA(const char *s) {
	OutputDebugStringA(s);
}
void Win32_Print(const wchar_t *s) {
	OutputDebugStringW(s);
}
void Win32_Printf(const wchar_t *fmt, ...) {
	wchar_t s[1024];
	va_list args;
	va_start(args, fmt);
	wvsprintfW(s, fmt, args);
	Win32_Print(s);
	va_end(args);
}

#define LID_ENGLISH 0x0C09

void Win32_GetErrorString(HRESULT hr, char *msg, int size) {
	// 海外で実行してエラーが発生した場合、その国の言語でエラーログを出力したものを報告されても分からないので
	// 実行中の言語とは無関係に常に英語で取得する
	char *buf = NULL;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, hr, LID_ENGLISH, buf, size, NULL
	);
	if (buf) {
		strcpy_s(msg, size, buf);
		LocalFree(buf);
	}
}

void Win32_PrintHResult(HRESULT hr) {
	char msg[1024] = {0};
	Win32_GetErrorString(hr, msg, sizeof(msg));
	Win32_PrintA(msg);
	Win32_PrintA("\n");
}


/// HICON から HBITMAP を作成する
HBITMAP Win32_CreateBitmapFromIcon(HICON hIcon) {
	int w = K_SMALL_ICON_SIZE_W;
	int h = K_SMALL_ICON_SIZE_H;
	void *dummy;
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(BITMAPINFO));
	bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth    = w;
	bmi.bmiHeader.biHeight   = h;
	bmi.bmiHeader.biPlanes   = 1;
	bmi.bmiHeader.biBitCount = 32;
	HBITMAP hBmp = CreateDIBSection(NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &dummy, NULL, 0);
    HDC hDC = CreateCompatibleDC(NULL);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hBmp);
    DrawIconEx(hDC, 0, 0, hIcon, w, h, 0, NULL, DI_NORMAL);
    SelectObject(hDC, hOldBmp);
    DeleteDC(hDC);
	return hBmp;
}
HICON Win32_CreateIconFromPixelsRGBA32(int w, int h, const uint8_t *pixels) {
	uint8_t *buf = (uint8_t *)K_MALLOC(w * h * 4);
	const uint8_t *src = pixels;
	for (int i=0; i<w * h; i++) {
		uint8_t r = src[i * 4 + 0];
		uint8_t g = src[i * 4 + 1];
		uint8_t b = src[i * 4 + 2];
		uint8_t a = src[i * 4 + 3];
		buf[i * 4 + 0] = b;
		buf[i * 4 + 1] = g;
		buf[i * 4 + 2] = r;
		buf[i * 4 + 3] = a;
	}
	HICON hIcon = CreateIcon(GetModuleHandleW(NULL), w, h, 1, 32, NULL, buf);
	K_FREE(buf);
	return hIcon;
}
bool Win32_SetClipboardText(HWND hWnd, const std::wstring &text) {
	bool result = false;
	if (OpenClipboard(hWnd)) {
		EmptyClipboard();
		int whole_bytes = sizeof(wchar_t) * (text.size() + 1); // with null terminator
		HGLOBAL hBuf = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, whole_bytes);
		wchar_t *strBuf = (wchar_t *)GlobalLock(hBuf);
		if (strBuf) {
			wcscpy(strBuf, text.c_str());
			GlobalUnlock(hBuf);
			SetClipboardData(CF_UNICODETEXT, strBuf);
			result = true;
		}
		CloseClipboard();
	}
	return result;
}
bool Win32_GetClipboardText(HWND hWnd, std::wstring &text) {
	bool result = false;
	if (OpenClipboard(hWnd)) {
		HGLOBAL hBuf = GetClipboardData(CF_UNICODETEXT);
		if (hBuf) {
			wchar_t *data = (wchar_t *)GlobalLock(hBuf);
			if (data) {
				text = data;
				GlobalUnlock(hBuf);
				result = true;
			}
		}
		CloseClipboard();
	}
	return result;
};

bool Win32_ProcessEvents() {
	MSG msg;
	if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) return false; // ループ中断
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return true; // ループ継続を許可
}

bool Win32_ProcessEventsWithDialog(HWND hWnd) {
	MSG msg;
	if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) return false; // ループ中断
		if (IsDialogMessageW(hWnd, &msg)) {
			// メッセージは既に処理済みなので TranslateMessage に渡してはいけない
		} else {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	return true; // ループ継続を許可
}
void Win32_ComputeWindowSize(HWND hWnd, int cw, int ch, int *req_w, int *req_h) {
	const BOOL has_menu = GetMenu(hWnd) != NULL;
	const DWORD style = GetWindowLongW(hWnd, GWL_STYLE);
	const DWORD exstyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
	RECT rect = {0, 0, cw, ch};
	AdjustWindowRectEx(&rect, style, has_menu, exstyle);
	if (req_w) *req_w = rect.right - rect.left;
	if (req_h) *req_h = rect.bottom - rect.top;
}
bool Win32_KillMessage_ALT_F10(SWin32Msg *msg) {
	switch (msg->uMsg) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (msg->wParam == VK_MENU || msg->wParam == VK_F10) { //<-- このキーは WM_SYSKEYDOWN でしか流れてこないことに注意
			msg->lResult = 1;
			msg->bShouldReturn = true;
			return true;
		}
		break;

	case WM_SYSCOMMAND:
		if (msg->wParam == SC_KEYMENU) { // ALTによる一時停止を無効化
			msg->lResult = 1;
			msg->bShouldReturn = true;
			return true;
		}
		break;
	}
	return false;
}
bool Win32_KillMessage_ScreenSaver(SWin32Msg *msg) {
	if (msg->uMsg == WM_SYSCOMMAND && msg->wParam == SC_SCREENSAVE) {
		msg->lResult = 1;
		msg->bShouldReturn = true;
		return true;
	}
	return false;
}
bool Win32_KillMessage_MonitorOff(SWin32Msg *msg) {
	if (msg->uMsg == WM_SYSCOMMAND && msg->wParam == SC_MONITORPOWER) {
		msg->lResult = 1;
		msg->bShouldReturn = true;
		return true;
	}
	return false;
}
bool Win32_ProcMessage_KeepAspect(SWin32Msg *msg, int aspect_w, int aspect_h, int *new_cw, int *new_ch) {
	if (msg->uMsg != WM_SIZING) return false;
	if (aspect_w <= 0) return false;
	if (aspect_h <= 0) return false;
	int cli_w, cli_h;
	{
		RECT rc = {0, 0, 0, 0};
		GetClientRect(msg->hWnd, &rc); // GetClientRect が失敗する可能性に注意
		cli_w = rc.right - rc.left;
		cli_h = rc.bottom - rc.top;
	}
	int wnd_w, wnd_h;
	RECT *pRect = (RECT *)msg->lParam;
	switch (msg->wParam) {
	case WMSZ_TOP:
	case WMSZ_BOTTOM:
		// 上または下をドラッグしている
		// アスペクト比を保つために必要なクライアント幅を計算する
		cli_w = cli_h * aspect_w / aspect_h;
		Win32_ComputeWindowSize(msg->hWnd, cli_w, cli_h, &wnd_w, NULL);
		pRect->right = pRect->left + wnd_w;
		if (new_cw) *new_cw = cli_w;
		if (new_ch) *new_ch = cli_h;
		return true;

	case WMSZ_LEFT:
	case WMSZ_TOPLEFT:
	case WMSZ_BOTTOMLEFT:
	case WMSZ_RIGHT:
	case WMSZ_TOPRIGHT:
	case WMSZ_BOTTOMRIGHT:
		// 左右または四隅をドラッグしている
		// アスペクト比を保つために必要なクライアント高さを計算する
		cli_h = cli_w * aspect_h / aspect_w;
		Win32_ComputeWindowSize(msg->hWnd, cli_w, cli_h, NULL, &wnd_h);
		pRect->bottom = pRect->top + wnd_h;
		if (new_cw) *new_cw = cli_w;
		if (new_ch) *new_ch = cli_h;
		return true;
	}
	return false;
}
bool Win32_ProcMessage_DropFile(SWin32Msg *msg, wchar_t *path, int maxwchars) {
	if (msg->uMsg != WM_DROPFILES) return false;
	DragQueryFileW((HDROP)msg->wParam, 0, path, maxwchars);
	DragFinish((HDROP)msg->wParam);
	return true;
}
void Win32_SetFontSize(HWND hWnd, int size) {
	HFONT hDefFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	LOGFONTW lf = {0};
	GetObjectW(hDefFont, sizeof(lf), &lf);
	lf.lfHeight = size;
	HFONT hMyFont = CreateFontIndirectW(&lf);
	SendMessageW(hWnd, WM_SETFONT, (WPARAM)hMyFont, MAKELPARAM(TRUE, 0));
}
void Win32_CalcTextSize(HWND hWnd, const wchar_t *text, int *w, int *h) {
	static const int MAXSIZE = 1024;
	RECT rect = {0, 0, 0, 0};

	HDC hDC = GetDC(hWnd);
	SelectObject(hDC, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
	DrawTextW(hDC, text, -1, &rect, DT_CALCRECT|DT_LEFT/*|DT_SINGLELINE*/);
	ReleaseDC(hWnd, hDC);

	if (w) *w = rect.right - rect.left;
	if (h) *h = rect.bottom - rect.top;
}
void Win32_CalcTextSize(HWND hWnd, int *w, int *h) {
	static const int MAXSIZE = 1024;
	wchar_t text[MAXSIZE] = {0};
	RECT rect = {0, 0, 0, 0};
	GetWindowTextW(hWnd, text, MAXSIZE);
	if (text[0]) {
		Win32_CalcTextSize(hWnd, text, w, h);
	} else {
		// 文字がない。ダミー文字で計算する。
		// 横幅はゼロにしておく
		Win32_CalcTextSize(hWnd, L"M", w, h);
		if (w) *w = 0;
	}
}
void Win32_CalcCheckBoxSize(HWND hWnd, int *w, int *h) {
	int text_w, text_h;
	Win32_CalcTextSize(hWnd, &text_w, &text_h);
	int text_space = 0;
	int box_w = GetSystemMetrics(SM_CXMENUCHECK);
	int box_h = GetSystemMetrics(SM_CYMENUCHECK);
	int edge_w = GetSystemMetrics(SM_CXEDGE);
	int edge_h = GetSystemMetrics(SM_CYEDGE);
	if (w) *w = edge_w + box_w + edge_w + text_space + text_w;
	if (h) *h = K_MAX(edge_h + box_h + edge_h, text_h);
}
void Win32_CalcEditBoxSize(HWND hWnd, int *w, int *h) {
	int text_w, text_h;
	Win32_CalcTextSize(hWnd, &text_w, &text_h);
	int edge_w = GetSystemMetrics(SM_CXEDGE);
	int edge_h = GetSystemMetrics(SM_CYEDGE);
	if (w) *w = edge_w + text_w + edge_w;
	if (h) *h = edge_h + text_h + edge_h;
}
void Win32_SetToolHint(HWND hWnd, HWND hCtrl, const wchar_t *text) {
	HINSTANCE hInst = (HINSTANCE)GetModuleHandleW(NULL);
	HWND hToolTip = CreateWindowExW(
		0, TOOLTIPS_CLASSW, NULL,
		TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		hWnd, NULL, hInst, NULL
	);
	TOOLINFOW info;
	ZeroMemory(&info, sizeof(info));
	//
	// cbSize の設定に注意！！
	// 素直に sizeof(TOOLINFOW) とかやってもダメ
	//
	// ツールチップ と コモンコントロール のバージョン差異による落とし穴
	// https://ameblo.jp/blueskyame/entry-10398978729.html
	//
	// 構造体が拡張されたときの動作
	// http://d.hatena.ne.jp/firewood/20080304/1204560137
	//
	info.cbSize = TTTOOLINFOW_V2_SIZE; // sizeof(info)
	info.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
	info.hwnd = hWnd;
	info.uId = (UINT_PTR)hCtrl;
	info.lpszText = (LPWSTR)text;
	GetClientRect(hWnd, &info.rect);
	BOOL result_of_TTM_ADDTOOL = SendMessageW(hToolTip, TTM_ADDTOOL, 0, (LPARAM)&info);
	K_ASSERT(result_of_TTM_ADDTOOL); // ここで失敗した場合、info.cbSize を確認せよ
}
void Win32_SetFullScreenStyle(HWND hWnd, LONG *oldStyle, LONG *oldStyleEx, RECT *oldRect) {
	K_ASSERT(oldStyle && oldStyleEx && oldRect);
	GetWindowRect(hWnd, oldRect);
	*oldStyle = GetWindowLongW(hWnd, GWL_STYLE);
	*oldStyleEx = GetWindowLongW(hWnd, GWL_EXSTYLE);
	ShowWindow(hWnd, SW_MAXIMIZE);
	SetWindowLongW(hWnd, GWL_STYLE, WS_POPUP|WS_VISIBLE);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
}
void Win32_SetWindowedStyle(HWND hWnd, LONG style, LONG styleEx, const RECT *rect) {
	K_ASSERT(rect);
	SetWindowLongW(hWnd, GWL_STYLE, style);
	SetWindowLongW(hWnd, GWL_EXSTYLE, styleEx);
	ShowWindow(hWnd, SW_RESTORE);
	SetWindowPos(hWnd, HWND_NOTOPMOST, rect->left, rect->top, rect->right-rect->left, rect->bottom-rect->top, SWP_NOSIZE);
}


CWin32StyleMgr::CWin32StyleMgr() {
	oldStyle_ = 0;
	oldStyleEx_ = 0;
	ZeroMemory(&oldRect_, sizeof(oldRect_));
}
void CWin32StyleMgr::setWindowedStyle(HWND hWnd) {
	Win32_SetWindowedStyle(hWnd, oldStyle_, oldStyleEx_, &oldRect_);
}
void CWin32StyleMgr::setFullScreenStyle(HWND hWnd) {
	Win32_SetFullScreenStyle(hWnd, &oldStyle_, &oldStyleEx_, &oldRect_);
}



HWND Win32_CreateLabel(HWND hParent, int id, const wchar_t *text) {
	return CreateWindowExW(
		0, L"STATIC", text,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateEdit(HWND hParent, int id, const wchar_t *text, bool readonly, bool multilines) {
	DWORD mflags = multilines ? (ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_WANTRETURN) : 0;
	DWORD rflags = readonly ? ES_READONLY : 0;
	return CreateWindowExW(
		0, L"EDIT", text, 
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_LEFT | mflags | rflags,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateButton(HWND hParent, int id, const wchar_t *text) {
	DWORD flat = 0;//BS_FLAT;
	return CreateWindowExW(
		0, L"BUTTON", text,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | flat,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateCheckBox(HWND hParent, int id, const wchar_t *text) {
	return CreateWindowExW(
		0, L"BUTTON", text,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_AUTOCHECKBOX,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateListBox(HWND hParent, int id) {
	return CreateWindowExW(
		id, L"LISTBOX", NULL,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_BORDER | WS_VSCROLL,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateDropDown(HWND hParent, int id) {
	return CreateWindowExW(
		id, L"COMBOBOX", NULL,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateDropDownList(HWND hParent, int id) {
	return CreateWindowExW(
		id, L"COMBOBOX", NULL,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateHorzLine(HWND hParent, int id) {
	return CreateWindowExW(
		0, L"STATIC", NULL,
		WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, 2,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
HWND Win32_CreateVertLine(HWND hParent, int id) {
	return CreateWindowExW(
		0, L"STATIC", NULL,
		WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT,
		2, CW_USEDEFAULT,
		hParent,
		(HMENU)id,
		(HINSTANCE)GetModuleHandleW(NULL),
		NULL
	);
}
bool Win32_IsPushButton(HWND hWnd) {
	wchar_t s[MAX_PATH];
	GetClassNameW(hWnd, s, MAX_PATH);
	if (K_WCSICMP(s, L"BUTTON") == 0) {
		DWORD style = GetWindowLongW(hWnd, GWL_STYLE);
		if ((style & BS_TYPEMASK) == BS_PUSHBUTTON) {
			return true;
		}
	}
	return false;
}
bool Win32_IsCheckBox(HWND hWnd) {
	wchar_t s[MAX_PATH];
	GetClassNameW(hWnd, s, MAX_PATH);
	if (K_WCSICMP(s, L"BUTTON") == 0) {
		DWORD style = GetWindowLongW(hWnd, GWL_STYLE);
		if (style & BS_CHECKBOX) {
			return true;
		}
	}
	return false;
}
bool Win32_IsSingleEdit(HWND hWnd) {
	wchar_t s[MAX_PATH];
	GetClassNameW(hWnd, s, MAX_PATH);
	if (K_WCSICMP(s, L"EDIT") == 0) {
		DWORD style = GetWindowLongW(hWnd, GWL_STYLE);
		if ((style & ES_MULTILINE) == 0) {
			return true;
		}
	}
	return false;
}
void Win32_SetPos(HWND hWnd, int x, int y) {
	SetWindowPos(hWnd, NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
}
void Win32_SetSize(HWND hWnd, int w, int h) {
	SetWindowPos(hWnd, NULL, 0, 0, w, h, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE);
}
void Win32_GetSize(HWND hWnd, int *w, int *h) {
	RECT rect = {0, 0, 0, 0};
	GetWindowRect(hWnd, &rect);
	if (w) *w = rect.right - rect.left;
	if (h) *h = rect.bottom - rect.top;
}
/// ウィンドウ矩形を親ウィンドウのクライアント座標系で返す。
/// ちなみに GetWindowRect は常にスクリーン座標系になるのでそのままではダメ
void Win32_GetRectInParent(HWND hWnd, RECT *rect) {
	K_ASSERT(rect);
	RECT s;
	GetWindowRect(hWnd, &s);

	HWND hParent = GetParent(hWnd);
	POINT p0 = {s.left, s.top};
	POINT p1 = {s.right, s.bottom};
	ScreenToClient(hParent, &p0);
	ScreenToClient(hParent, &p1);
	
	rect->left   = p0.x;
	rect->top    = p0.y;
	rect->right  = p1.x;
	rect->bottom = p1.y;
}
void Win32_UpdateListBox(HWND hWnd, int selected, const wchar_t *items[], int count) {
	wchar_t buf[1024];
	int cnt = SendMessageW(hWnd, LB_GETCOUNT, 0, 0);
	int num = K_MIN(cnt, count);
	if (num > 0) {
		K_ASSERT(items);
		for (int i=0; i<num; i++) {
			SendMessageW(hWnd, LB_GETTEXT, i, (LPARAM)buf);
			if (wcscmp(buf, items[i]) != 0) {
				SendMessageW(hWnd, LB_DELETESTRING, i, 0);
				SendMessageW(hWnd, LB_INSERTSTRING, i, (LPARAM)items[i]);
			}
		}
	}
	if (cnt < count) {
		for (int i=cnt; i<count; i++) {
			SendMessageW(hWnd, LB_ADDSTRING, 0, (LPARAM)items[i]);
		}
	}
	if (count < cnt) {
		for (int i=cnt; i<count; i++) {
			SendMessageW(hWnd, LB_DELETESTRING, count, 0);
		}
	}
	int sel = SendMessageW(hWnd, LB_GETCURSEL, 0, 0);
	if (sel != selected) {
		SendMessageW(hWnd, LB_SETCURSEL, selected, 0);
	}
}
void Win32_UpdateDropDownList(HWND hWnd, int selected, const wchar_t *items[], int count) {
	wchar_t buf[1024];
	int cnt = SendMessageW(hWnd, CB_GETCOUNT, 0, 0);
	int num = K_MIN(cnt, count);
	if (num > 0) {
		K_ASSERT(items);
		for (int i=0; i<K_MIN(cnt, count); i++) {
			SendMessageW(hWnd, CB_GETLBTEXT, i, (LPARAM)buf);
			if (wcscmp(buf, items[i]) != 0) {
				SendMessageW(hWnd, CB_DELETESTRING, i, NULL);
				SendMessageW(hWnd, CB_INSERTSTRING, i, (LPARAM)items[i]);
			}
		}
	}
	if (cnt < count) {
		for (int i=cnt; i<count; i++) {
			SendMessageW(hWnd, CB_ADDSTRING, 0, (LPARAM)items[i]);
		}
	}
	if (count < cnt) {
		for (int i=cnt; i<count; i++) {
			SendMessageW(hWnd, CB_DELETESTRING, count, 0);
		}
	}
	int sel = SendMessageW(hWnd, CB_GETCURSEL, 0, 0);
	if (sel != selected) {
		SendMessageW(hWnd, CB_SETCURSEL, selected, 0);
	}
}
void Win32_UpdateDropDownEdit(HWND hWnd, int selected, const wchar_t *items[], int count, const wchar_t *text) {
	Win32_UpdateDropDownList(hWnd, selected, items, count);
	SetWindowTextW(hWnd, text);
}
bool Win32CheckBox_IsChecked(HWND hWnd) {
	return SendMessageW(hWnd, BM_GETCHECK, 0, 0) != 0;
}
void Win32CheckBox_SetChecked(HWND hWnd, bool checked) {
	SendMessageW(hWnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}
HFONT Win32_CreateSystemUIFont(HWND hWnd, int size) {
	HFONT hDefFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	LOGFONTW lf = {0};
	GetObjectW(hDefFont, sizeof(lf), &lf);
	lf.lfHeight = size;
	return CreateFontIndirectW(&lf);
}
int Win32_FontPointToPixel(int pt) {
	return pt * 96 / 72; // 72pt = 96px
}
// 次にマウスがウィンドウを離れたときに、WM_MOUSELEAVE メッセージを受け取るように設定する。
// この設定はイベント発生のたびにリセットされ無効になるので注意する事
void Win32_SetMouseLeaveEvent(HWND hWnd) {
	TRACKMOUSEEVENT e;
	e.cbSize = sizeof(e);
	e.hwndTrack = hWnd;
	e.dwFlags = TME_LEAVE;
	e.dwHoverTime = 0;
	TrackMouseEvent(&e);
}
LRESULT Win32_DefWindowProc(const SWin32Msg *msg) {
	K_ASSERT(msg);
	return DefWindowProcW(msg->hWnd, msg->uMsg, msg->wParam, msg->lParam);
}
void Win32Menu_AddItem(HMENU hMenu, const wchar_t *text, UINT id, UINT userdata) {
	if (wcscmp(text, L"-") == 0) {
		MENUITEMINFOW info;
		ZeroMemory(&info, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = MIIM_TYPE;
		info.fType = MFT_SEPARATOR;
		int index = GetMenuItemCount(hMenu);
		InsertMenuItemW(hMenu, index, TRUE, &info);
	} else {
		MENUITEMINFOW info;
		ZeroMemory(&info, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = MIIM_TYPE|MIIM_ID|MIIM_DATA;
		info.fType = MFT_STRING;
		info.wID = id;
		info.dwTypeData = (LPWSTR)text;
		info.cch = wcslen(text);
		info.dwItemData = userdata;
		int index = GetMenuItemCount(hMenu);
		InsertMenuItemW(hMenu, index, TRUE, &info);
	}
}
int Win32Menu_GetIndexOfId(HMENU hMenu, UINT id) {
	if (id == 0) return -1; // メニューIDが 0 の場合、インデックスを特定できない
	int num = GetMenuItemCount(hMenu);
	for (int i=0; i<num; i++) {
		UINT m = GetMenuItemID(hMenu, i);
		if (m == id) return i;
	}
	return -1;
}
void Win32Menu_SetBitmap(HMENU hMenu, int item, bool by_position, const wchar_t *filename) {
	// SetMenuItemBitmaps も参照
	HBITMAP hBmp = NULL;
	if (filename) {
		HINSTANCE hInstance = (HINSTANCE)GetModuleHandleW(NULL);
		HICON hIcon = (HICON)LoadImageW(hInstance, filename, IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
		hBmp = Win32_CreateBitmapFromIcon(hIcon);
	}
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask |= MIIM_BITMAP;
	info.hbmpItem = hBmp;
	SetMenuItemInfoW(hMenu, item, by_position, &info);
}
void Win32Menu_SetEnabled(HMENU hMenu, int item, bool by_position, bool value) {
	UINT flags = value ? MFS_ENABLED : MFS_GRAYED;
	if (by_position) flags |= MF_BYPOSITION;
	EnableMenuItem(hMenu, item, flags);
}
void Win32Menu_SetChecked(HMENU hMenu, int item, bool by_position, bool value) {
	UINT flags = value ? MFS_CHECKED : MFS_UNCHECKED;
	if (by_position) flags |= MF_BYPOSITION;
	CheckMenuItem(hMenu, item, flags);
}
bool Win32Menu_IsEnabled(HMENU hMenu, int item, bool by_position) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STATE;
	GetMenuItemInfoW(hMenu, item, by_position, &info);
	return (info.fState & MFS_ENABLED) != 0;
}
bool Win32Menu_IsChecked(HMENU hMenu, int item, bool by_position) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STATE;
	GetMenuItemInfoW(hMenu, item, by_position, &info);
	return (info.fState & MFS_CHECKED) != 0;
}
void Win32Menu_SetSubMenu(HMENU hMenu, int item, bool by_position, HMENU hSubMenu) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_SUBMENU;
	info.hSubMenu = hSubMenu;
	SetMenuItemInfoW(hMenu, item, by_position, &info);
}
bool Win32_IsKeyDown(int vk) {
	return (GetKeyState(vk) & 0x8000) != 0;
}

#pragma endregion // Win32 Functions


#pragma region CWin32DropFiles
bool CWin32DropFiles::processMessage(SWin32Msg *msg) {
	K_ASSERT(msg);
	if (msg->uMsg == WM_DROPFILES) {
		names_.clear();
		UINT num = DragQueryFileW((HDROP)msg->wParam, (UINT)(-1), NULL, 0);
		for (UINT i=0; i<num; i++) {
			wchar_t path[MAX_PATH];
			DragQueryFileW((HDROP)msg->wParam, i, path, MAX_PATH);
			names_.push_back(path);
		}
		msg->lResult = 0;
		msg->bShouldReturn = true;
		return true;
	}
	return false;
}
void CWin32DropFiles::clear() {
	return names_.clear();
}
const wchar_t * CWin32DropFiles::getName(int index) {
	return names_[index].c_str();
}
int CWin32DropFiles::getCount() {
	return (int)names_.size();
}
#pragma endregion // CWin32DropFiles


#pragma region CWin32WatchDir
CWin32WatchDir::CWin32WatchDir() {
	hNotif_ = NULL;
}
CWin32WatchDir::~CWin32WatchDir() {
	end();
}
bool CWin32WatchDir::start(const wchar_t *dir, bool subdir, DWORD filter) {
	hNotif_ = FindFirstChangeNotificationW(dir, subdir, filter);
	if (hNotif_ == INVALID_HANDLE_VALUE) {
		return false;
	}
	return true;
}
void CWin32WatchDir::end() {
	FindCloseChangeNotification(hNotif_);
	hNotif_ = NULL;
}
bool CWin32WatchDir::poll() {
	DWORD result = WaitForSingleObject(hNotif_, 0);
	if (result != WAIT_TIMEOUT) {
		FindNextChangeNotification(hNotif_);
		return true;
	}
	return false;
}
#pragma endregion // CWin32WatchDir


#pragma region CWin32WatchDirEx
CWin32WatchDirEx::CWin32WatchDirEx() {
	_clear();
}
CWin32WatchDirEx::~CWin32WatchDirEx() {
	end();
}
void CWin32WatchDirEx::_clear() {
	hDir_ = NULL;
	hEvent_ = NULL;
	filter_ = 0;
	pos_ = 0;
	size_ = 0;
	subdir_ = false;
	should_reset_ = false;
	ZeroMemory(&overlap_, sizeof(overlap_));
	buf_.clear();
}
bool CWin32WatchDirEx::start(const wchar_t *dir, bool subdir, DWORD filter) {
	subdir_ = subdir;
	// 対象のディレクトリを監視用にオープン
	// 共有ディレクトリ使用可、対象フォルダを削除可
	// 非同期I/O
	hDir_ = CreateFileW(
		dir,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // ReadDirectoryChangesW用
		NULL
	);
	if (hDir_ == INVALID_HANDLE_VALUE) {
		_clear();
		return false;
	}
	filter_ = filter;

	// 非同期I/Oの完了待機用(手動リセットモード)
	hEvent_ = CreateEventW(NULL, TRUE, FALSE, NULL);

	buf_.resize(1024 * 4);
	should_reset_ = true;
	return true;
}
bool CWin32WatchDirEx::poll() {
	if (should_reset_) {
		ResetEvent(hEvent_);

		// 非同期IO用
		ZeroMemory(&overlap_, sizeof(overlap_));
		overlap_.hEvent = hEvent_;

		// dir_ の変更を監視
		ReadDirectoryChangesW(hDir_, &buf_[0], buf_.size(), subdir_, filter_, NULL, &overlap_, NULL);
		pos_ = 0;
		size_ = 0;
		should_reset_ = false;
	}

	DWORD waitResult = WaitForSingleObject(hEvent_, 0);
	if (waitResult == WAIT_TIMEOUT) return false;

	// 変更通知あり
	// 非同期I/Oの結果を取得
	GetOverlappedResult(hDir_, &overlap_, &size_, FALSE);
	if (size_ == 0) {
		// バッファが足りなかった。
		// 何か変更があったことは確かだが、その情報を得ることはできない
	}

	// 次に poll を呼んだ時はイベントをリセットする
	should_reset_ = true;

	return true;
}
bool CWin32WatchDirEx::read(wchar_t *filename, int maxlen, int *action) {
	K_ASSERT(filename);
	K_ASSERT(action);
	if (pos_ >= size_) return false;
	FILE_NOTIFY_INFORMATION *pData = (FILE_NOTIFY_INFORMATION *)(&buf_[pos_]);

	if (action) {
		*action = pData->Action;
	}
	if (filename && maxlen > 0) {
		// ファイル名はヌル終端されていない
		int bytes = pData->FileNameLength;
		int cnt = bytes / sizeof(wchar_t);
		if (cnt >= maxlen) cnt = maxlen-1;
		memcpy(filename, pData->FileName, sizeof(wchar_t) * cnt);
		filename[cnt] = L'\0';
	}

	if (pData->NextEntryOffset == 0) {
		pos_ = size_; // EOF
	} else {
		pos_ += pData->NextEntryOffset;
	}
	return true;
}
void CWin32WatchDirEx::end() {
	if (hDir_) {
		CancelIo(hDir_);
		WaitForSingleObject(hEvent_, INFINITE);
		CloseHandle(hEvent_);
		CloseHandle(hDir_);
	}
	_clear();
}
#pragma endregion // CWin32WatchDirEx



#pragma region CWin32Ini
CWin32Ini::CWin32Ini() {
}
CWin32Ini::CWin32Ini(const wchar_t *filename) {
	setFileName(filename);
}
void CWin32Ini::setFileName(const wchar_t *filename) {
	// WritePrivateProfileXXX, GetPrivateProfileXXX 系で指定するファイル名に注意。
	// これらの関数に相対パスを渡した場合、Windows ディレクトリから検索してしまう
	// 相対パスを指定した場合はカレントディレクトリから探す。
	// 絶対パスの場合はそのまま
	if (PathIsRelativeW(filename)) {
		wchar_t tmp[MAX_PATH];
		GetFullPathNameW(filename, MAX_PATH, tmp, NULL);
		fullpath_.assign(tmp);
	} else {
		fullpath_.assign(filename);
	}
}
void CWin32Ini::setString(const wchar_t *section, const wchar_t *key, const wchar_t *value) {
	WritePrivateProfileStringW(section, key, value, fullpath_.c_str());
}
void CWin32Ini::setBinary(const wchar_t *section, const wchar_t *key, const void *buf, int size) {
	WritePrivateProfileStructW(section, key, (LPVOID)buf, size, fullpath_.c_str());
}
void CWin32Ini::setInt(const wchar_t *section, const wchar_t *key, int value) {
	wchar_t s[64];
	_swprintf(s, L"%d", value);
	setString(section, key, s);
}
void CWin32Ini::setFloat(const wchar_t *section, const wchar_t *key, float value) {
	wchar_t s[64];
	_swprintf(s, L"%g", value);
	setString(section, key, s);
}
std::wstring CWin32Ini::getString(const wchar_t *section, const wchar_t *key) {
	const int MAXCHARS = 1024;
	wchar_t buf[MAXCHARS] = {L'\0'};
	GetPrivateProfileStringW(section, key, L"", buf, MAXCHARS, fullpath_.c_str());
	return std::wstring(buf);
}
bool CWin32Ini::getBinary(const wchar_t *section, const wchar_t *key, void *buf, int size) {
	return GetPrivateProfileStructW(section, key, buf, size, fullpath_.c_str()) != 0;
}
int CWin32Ini::getInt(const wchar_t *section, const wchar_t *key, int def) {
	// GetPrivateProfileIntW の戻り値は UINT であることに注意。デフォルト値として負の値を指定できない！
	// UINT val = GetPrivateProfileIntW(section, key, def, fullpath_.c_str());
	std::wstring ws = getString(section, key);
	if (ws.empty()) return def;
	wchar_t *e;
	int val = wcstol(ws.c_str(), &e, 0);
	if (*e != '\0') return def;
	return val;
}
float CWin32Ini::getFloat(const wchar_t *section, const wchar_t *key, float def) {
	std::wstring ws = getString(section, key);
	if (ws.empty()) return def;
	wchar_t *e;
	double val = wcstod(ws.c_str(), &e);
	if (*e != '\0') return def;
	return (float)val;
}
#pragma endregion // CWin32Ini







void Win32_RitchErrorDialog(const wchar_t *title, const wchar_t *text, const wchar_t *detail) {
	CWin32Window wnd;
	wnd.create(NULL);
	wnd.setTitle(title);

	CWin32DialogLayouter layout;
	layout.init(wnd.getHandle());

	layout.addLabel(0, text);
	layout.newLine();

	layout.addEdit(1, 460, 460, detail, true, true);
	layout.newLine();

	layout.space(400, 0);
	layout.auto_sizing_ = false;
	layout.addButton(2, L"OK");

	layout.resizeToFit();

	wnd.show(SW_SHOW);
	while (Win32_ProcessEventsWithDialog(wnd.getHandle())) {
		
	}
	wnd.destroy();
}



#pragma region Win32Console
// エスケープシーケンス
// http://www.isthe.com/chongo/tech/comp/ansi_escapes.html

void Win32Console_Open() {
	AllocConsole();
	freopen("CON", "w", stdout);
	freopen("CON", "r", stdin);
}
void Win32Console_Close() {
	// TIPS:
	// freopen でコンソールウィンドウに関連付けた stdin, stdout を戻しておかないと、
	// FreeConsole を呼んでもウィンドウが閉じない
	freopen("NUL", "w", stdout);
	freopen("NUL", "r", stdin);
	FreeConsole();
}
HANDLE Win32Console_GetHandle() {
	return GetStdHandle(STD_OUTPUT_HANDLE);
}
void Win32Console_ClearScreen(HANDLE hConsole) {
	// 画面消去
	// puts("\x1B[2J");
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(hConsole, &info);
	DWORD length = info.dwSize.X * info.dwSize.Y;
	DWORD dummy;
	COORD pos = {0, 0};
	FillConsoleOutputCharacterA(hConsole, ' ', length, pos, &dummy);
	FillConsoleOutputAttribute(hConsole, info.wAttributes, length, pos, &dummy);
	SetConsoleCursorPosition(hConsole, pos);
}
void Win32Console_ClearToEndOfLine(HANDLE hConsole) {
	// 行末まで削除
	// puts("\x1B[01K");
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(hConsole, &info);
	DWORD length = info.dwSize.X - info.dwCursorPosition.X;
	DWORD dummy;
	COORD pos = {info.dwCursorPosition.X, info.dwCursorPosition.Y};
	FillConsoleOutputCharacterA(hConsole, ' ', length, pos, &dummy);
	FillConsoleOutputAttribute(hConsole, info.wAttributes, length, pos, &dummy);
	SetConsoleCursorPosition(hConsole, pos);
}
void Win32Console_MoveToHome(HANDLE hConsole) {
	// 行頭に移動
	// puts("\r");
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(hConsole, &info);
	Win32Console_MoveTo(hConsole, 0, info.dwCursorPosition.Y);
}
void Win32Console_MoveTo(HANDLE hConsole, int row, int col) {
	// 指定行列に移動
	// printf("\x1B[%d;%dH", row, col);
	COORD pos = {(SHORT)col, (SHORT)row};
	SetConsoleCursorPosition(hConsole, pos);
}
void Win32Console_SetFgColor(HANDLE hConsole, int red, int green, int blue, int intensity) {
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(hConsole, &info);
	WORD attr = info.wAttributes;
	attr &= ~(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
	if (red)       attr |= FOREGROUND_RED;
	if (green)     attr |= FOREGROUND_GREEN;
	if (blue)      attr |= FOREGROUND_BLUE;
	if (intensity) attr |= FOREGROUND_INTENSITY;
	SetConsoleTextAttribute(hConsole, attr);
}
void Win32Console_SetBgColor(HANDLE hConsole, int red, int green, int blue, int intensity) {
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(hConsole, &info);
	WORD attr = info.wAttributes;
	attr &= ~(BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY);
	if (red)       attr |= BACKGROUND_RED;
	if (green)     attr |= BACKGROUND_GREEN;
	if (blue)      attr |= BACKGROUND_BLUE;
	if (intensity) attr |= BACKGROUND_INTENSITY;
	SetConsoleTextAttribute(hConsole, attr);
}
#pragma endregion // Win32Console


#pragma region CWin32ResourceIcons
// LPCTSTR, LPCWSTR, LPCSTR の違いをヘッダファイル側に出したくないので、ソースファイル内で完結するようにクラスとは独立させておく
static BOOL CALLBACK K_EnumResourceNamesCallback(HINSTANCE instance, LPCTSTR type, LPTSTR name, void *std_vector_hicon) {
	std_vector_HICON *icons = reinterpret_cast<std_vector_HICON *>(std_vector_hicon);
	HICON icon = LoadIcon(instance, name);
	if (icon) {
		icons->push_back(icon);
	}
	return TRUE;
}
CWin32ResourceIcons::CWin32ResourceIcons() {
	// この実行ファイルに含まれているアイコンを取得する
	HINSTANCE hInst = (HINSTANCE)GetModuleHandleW(NULL);
	EnumResourceNames(hInst, RT_GROUP_ICON, (ENUMRESNAMEPROC)K_EnumResourceNamesCallback, (LONG_PTR)&icons_);
}
int CWin32ResourceIcons::getCount() const {
	return (int)icons_.size();
}
HICON CWin32ResourceIcons::getIcon(int index) const {
	if (0 <= index && index < (int)icons_.size()) {
		return icons_[index];
	}
	return NULL;
}
#pragma endregion // CWin32ResourceIcons


#pragma region CWin32Fonts
void CWin32Fonts::update() {
	items_.clear();
	{
		// フォントファミリー名を列挙
		HWND hWnd = NULL;
		HDC hdc = GetDC(hWnd);
		LOGFONTW font;
		font.lfCharSet = DEFAULT_CHARSET; // とりあえず全フォントを検索対象にしておき、コールバック関数内でフィルタリングする
		font.lfFaceName[0] = 0;
		font.lfPitchAndFamily = 0;
		// フォントの列挙
		// http://vanillasky-room.cocolog-nifty.com/blog/2007/06/post-bb27.html
		EnumFontFamiliesExW(hdc, &font, (FONTENUMPROCW)fontEnumProc, (LPARAM)this, 0);
		ReleaseDC(hWnd, hdc);
	}
	std::sort(items_.begin(), items_.end());
}
void CWin32Fonts::print() {
	for (size_t i=0; i<items_.size(); i++) {
		Win32_Printf(L"%ls\n", items_[i].c_str());
	}
}
size_t CWin32Fonts::getCount() {
	return items_.size();
}
const wchar_t * CWin32Fonts::getFace(size_t index) {
	return items_[index].c_str();
}
int CWin32Fonts::findFace(const wchar_t *face) {
	for (size_t i=0; i<items_.size(); i++) {
		if (wcscmp(items_[i].c_str(), face) == 0) {
			return (int)i;
		}
	}
	return -1;
}
void CWin32Fonts::onFontEnum(const ENUMLOGFONTEXW *font, const NEWTEXTMETRICEXW *metrix, DWORD type) {
	if (font->elfLogFont.lfFaceName[0] == '@') return; // 横向きフォントはダメ
	if (type != TRUETYPE_FONTTYPE) return; // True Type でないとダメ
	if (font->elfLogFont.lfCharSet != SHIFTJIS_CHARSET) return; // 日本語対応していないとダメ
	items_.push_back(font->elfLogFont.lfFaceName);
}
int CALLBACK CWin32Fonts::fontEnumProc(const ENUMLOGFONTEXW *font, const NEWTEXTMETRICEXW *metrix, DWORD type, LPARAM user) {
	CWin32Fonts *obj = reinterpret_cast<CWin32Fonts *>(user);
	obj->onFontEnum(font, metrix, type);
	return 1; // continue
}
#pragma endregion // CWin32Fonts


#pragma region CWin32FontFiles
void CWin32FontFiles::update() {
	items_.clear();

	wchar_t dir[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, dir);

	HKEY hKey = NULL;
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &hKey);
	wchar_t key[MAX_PATH] = {0};
	BYTE val[MAX_PATH];
	for (int i=0; /*SKIP*/; i++) {
		DWORD keysize = MAX_PATH;
		DWORD valsize = MAX_PATH;
		DWORD type = REG_SZ;
		if (RegEnumValueW(hKey, i, key, &keysize, NULL, &type, val, &valsize) == ERROR_SUCCESS) {
			wchar_t path[MAX_PATH];
			wcscpy(path, dir);
			PathAppendW(path, (const wchar_t *)val);
			readFile(path);
		} else {
			break; // END
		}
	}
	RegCloseKey(hKey);
}
void CWin32FontFiles::print(bool strip_dir) {
	for (auto it=items_.begin(); it!=items_.end(); ++it) {
		const ITEM &item = *it;
		const wchar_t *face = item.facename_jp[0] ? item.facename_jp : item.facename_en;
		const wchar_t *file = strip_dir ? PathFindFileNameW(item.filename) : item.filename;
		Win32_Printf(L"%-40ls | [%d] %ls\n", face, item.index, file);
	}
}

#ifndef NO_STB_TRUETYPE
enum K_Stbtt_NAMEID {
	K_Stbtt_NID_COPYRIGHT  = 0,
	K_Stbtt_NID_FAMILY     = 1, // 例: "MS Gothic", "ＭＳ ゴシック"
	K_Stbtt_NID_SUBFAMILY  = 2, // 例: "Regular", "標準"
	K_Stbtt_NID_FONTID     = 3, // 例: "Microsoft:MS Gothic", "Microsoft:ＭＳ ゴシック"
	K_Stbtt_NID_FULLNAME   = 4, // 例: "MS Gothic", "ＭＳ ゴシック"
	K_Stbtt_NID_POSTSCRIPT = 6, // 例: "MS-Gothic"
};
static void K_Stbtt_getstring(stbtt_fontinfo *font_info, K_Stbtt_NAMEID id, int lang, wchar_t *ws) {
	int u16be_bytes = 0;
	const char *u16be_name = stbtt_GetFontNameString(
		font_info,
		&u16be_bytes,
		STBTT_PLATFORM_ID_MICROSOFT,
		STBTT_MS_EID_UNICODE_BMP,
		lang,
		id);
	int name_len = u16be_bytes / 2;
	for (int i=0; i<name_len; i++) {
		uint8_t hi = u16be_name[i*2];
		uint8_t lo = u16be_name[i*2+1];
		ws[i] = (hi << 8) | lo;
	}
	ws[name_len] = '\0';
}
#endif // !NO_STB_TRUETYPE

void CWin32FontFiles::readFile(const wchar_t *filename) {
	HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return;

	size_t filesize = GetFileSize(hFile, NULL);
	uint8_t *filedata = (uint8_t *)K_MALLOC(filesize);

	DWORD sz = 0;
	ReadFile(hFile, filedata, filesize, &sz, NULL);
	#ifndef NO_STB_TRUETYPE
	{
		int numfonts = stbtt_GetNumberOfFonts(filedata);
		for (int i=0; i<numfonts; i++) {
			int fontpos = stbtt_GetFontOffsetForIndex(filedata, i);
			ITEM item;
			ZeroMemory(&item, sizeof(item));
			stbtt_fontinfo info;
			stbtt_InitFont(&info, filedata, fontpos);
			K_Stbtt_getstring(&info, K_Stbtt_NID_FAMILY, STBTT_MS_LANG_ENGLISH, item.facename_en); // フォント名（英語）
			K_Stbtt_getstring(&info, K_Stbtt_NID_FAMILY, STBTT_MS_LANG_JAPANESE, item.facename_jp); // フォント名（日本語）
			wcscpy(item.filename, filename);
			item.index = i;
			items_.push_back(item);
		}
	}
	#endif
	K_FREE(filedata);
	CloseHandle(hFile);
}
size_t CWin32FontFiles::getCount() {
	return items_.size();
}
const CWin32FontFiles::ITEM * CWin32FontFiles::getItem(size_t index) {
	return &items_[index];
}
int CWin32FontFiles::findFile(const wchar_t *filename, bool ignore_dir, bool ignore_case) {
	const wchar_t *n1 = ignore_dir ? PathFindFileNameW(filename) : filename;
	for (size_t i=0; i<items_.size(); i++) {
		const ITEM &item = items_[i];
		const wchar_t *n2 = ignore_dir ? PathFindFileNameW(item.filename) : item.filename;
		if (ignore_case) {
			if (K_WCSICMP(n1, n2) == 0) {
				return (int)i;
			}
		} else {
			if (wcscmp(n1, n2) == 0) {
				return (int)i;
			}
		}
	}
	return -1;
}
int CWin32FontFiles::findFace(const wchar_t *face) {
	for (size_t i=0; i<items_.size(); i++) {
		const ITEM &item = items_[i];
		if (wcscmp(item.facename_en, face) == 0 || wcscmp(item.facename_jp, face) == 0) {
			return (int)i;
		}
	}
	return -1;
}
#pragma endregion // CWin32FontFiles


#pragma region CWin32PCMWave
CWin32PCMWave::CWin32PCMWave() {
	mmio_ = NULL;
	close(); // init
}
void CWin32PCMWave::close() {
	if (mmio_) mmioClose(mmio_, 0);
	ZeroMemory(&wfmt_, sizeof(wfmt_));
	ZeroMemory(&chunk_, sizeof(chunk_));
	mmio_ = NULL;
	block_bytes_ = 0;
}
bool CWin32PCMWave::openFromFile(const wchar_t *filename) {
	HMMIO hMmio = mmioOpenW((LPWSTR)filename, NULL, MMIO_ALLOCBUF|MMIO_READ);
	return openFromMmio(hMmio);
}
bool CWin32PCMWave::openFromMemory(const void *data, int size) {
	MMIOINFO info;
	ZeroMemory(&info, sizeof(MMIOINFO));
	info.pchBuffer = (HPSTR)data;
	info.fccIOProc = FOURCC_MEM; // read from memory
	info.cchBuffer = size;
	HMMIO hMmio = mmioOpen(NULL, &info, MMIO_ALLOCBUF|MMIO_READ);
	return openFromMmio(hMmio);
}
bool CWin32PCMWave::openFromMmio(HMMIO hMmio) {
	if (hMmio == NULL) {
		return false;
	}
	// RIFFチャンク検索
	MMRESULT mmRes;
	MMCKINFO riffChunk;
	riffChunk.fccType = mmioStringToFOURCCA("WAVE", 0);
	mmRes = mmioDescend(hMmio, &riffChunk, NULL, MMIO_FINDRIFF);
	if (mmRes != MMSYSERR_NOERROR) {
		mmioClose(hMmio, 0);
		return false;
	}
	// フォーマットチャンク検索
	MMCKINFO formatChunk;
	formatChunk.ckid = mmioStringToFOURCCA("fmt ", 0);
	mmRes = mmioDescend(hMmio, &formatChunk, &riffChunk, MMIO_FINDCHUNK);
	if (mmRes != MMSYSERR_NOERROR) {
		mmioClose(hMmio, 0);
		return false;
	}
	// WAVEFORMATEX構造体格納
	DWORD fmsize = formatChunk.cksize;
	DWORD size = mmioRead(hMmio, (HPSTR)&wfmt_, fmsize);
	if (size != fmsize) {
		mmioClose(hMmio, 0);
		return false;
	}
	// RIFFチャンクに戻る
	mmioAscend(hMmio, &formatChunk, 0);
	// データチャンク検索
	chunk_.ckid = mmioStringToFOURCCA("data ", 0);
	mmRes = mmioDescend(hMmio, &chunk_, &riffChunk, MMIO_FINDCHUNK);
	if (mmRes != MMSYSERR_NOERROR) {
		mmioClose(hMmio, 0);
		return false;
	}
	// いまのところ16ビットサウンドにのみ対応
	if (wfmt_.wBitsPerSample != 16) {
		mmioClose(hMmio, 0);
		return false;
	}
	// 必要な情報を計算しておく
	block_bytes_ = wfmt_.wBitsPerSample * wfmt_.nChannels / 8;
	mmio_ = hMmio;
	return true;
}
const WAVEFORMATEX * CWin32PCMWave::getFormat() {
	return &wfmt_;
}
int CWin32PCMWave::pcmBlockSize() {
	return block_bytes_;
}
int CWin32PCMWave::pcmCount() {
	K_ASSERT(block_bytes_ > 0);
	return chunk_.cksize / block_bytes_;
}
int CWin32PCMWave::pcmRead(void *buf, int num_blocks) {
	K_ASSERT(block_bytes_ > 0);
	int read_bytes = mmioRead(mmio_, (HPSTR)buf, num_blocks * block_bytes_);
	if (read_bytes >= 0) {
		K_ASSERT(read_bytes % block_bytes_ == 0); // 必ず block_bytes 単位でシークする
		return read_bytes / block_bytes_;
	}
	return -1;
}
int CWin32PCMWave::pcmSeek(int num_blocks) {
	K_ASSERT(block_bytes_ > 0);
	int seekto = chunk_.dwDataOffset + num_blocks * block_bytes_;
	int pos = mmioSeek(mmio_, seekto, SEEK_SET);
	if (pos >= 0) {
		K_ASSERT(pos % block_bytes_ == 0); // 必ず block_bytes 単位でシークする
		return pos / block_bytes_;
	}
	return -1;
}
int CWin32PCMWave::pcmTell() {
	K_ASSERT(block_bytes_ > 0);
	int bytes_from_file_head = mmioSeek(mmio_, 0, SEEK_CUR);
	int bytes_from_data_head = bytes_from_file_head - chunk_.dwDataOffset;
	K_ASSERT(bytes_from_data_head % block_bytes_ == 0); // 必ず block_bytes 単位でシークする
	return bytes_from_data_head / block_bytes_;
}
#pragma endregion // CWin32PCMWave


#pragma region CWin32PCMPlayer
CWin32PCMPlayer::CWin32PCMPlayer() {
	wout_ = NULL;
	close(); // init
}
void CWin32PCMPlayer::_err(MMRESULT mr) {
	wchar_t ws[MAXERRORLENGTH];
	waveOutGetErrorTextW(mr, ws, MAXERRORLENGTH);
	Win32_Print(ws);
}
void CWin32PCMPlayer::callback(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
	CWin32PCMPlayer *snd = reinterpret_cast<CWin32PCMPlayer *>(dwInstance);
	switch (uMsg) {
	case MM_WOM_DONE:
		snd->on_done(hwo);
		break;
	}
}
bool CWin32PCMPlayer::open(const WAVEFORMATEX *wf, const void *data, int size) {
	MMRESULT mr = waveOutOpen(&wout_, WAVE_MAPPER, wf, (DWORD_PTR)callback, (DWORD_PTR)this, CALLBACK_FUNCTION);
	if (mr) { _err(mr); return false; }
	ZeroMemory(&whdr_, sizeof(whdr_));
	whdr_.lpData = (LPSTR)data;
	whdr_.dwBufferLength = size;
	memcpy(&wfmt_, wf, sizeof(wfmt_));
	return true;
}
void CWin32PCMPlayer::close() {
	waveOutClose(wout_);
	wout_ = NULL;
	ZeroMemory(&whdr_, sizeof(whdr_));
	ZeroMemory(&wfmt_, sizeof(wfmt_));
	playing_ = false;
}
bool CWin32PCMPlayer::isPlaying() {
	return playing_;
}
void CWin32PCMPlayer::on_done(HWAVEOUT wo) {
	if (wo == wout_) {
		playing_ = false;
	}
}
void CWin32PCMPlayer::play() {
	playing_ = false;
	
	MMRESULT mr;
	mr = waveOutPrepareHeader(wout_, &whdr_, sizeof(WAVEHDR));
	if (mr) { _err(mr); return; }

	mr = waveOutWrite(wout_, &whdr_, sizeof(WAVEHDR));
	if (mr) { _err(mr); return; }

	playing_ = true;
}
void CWin32PCMPlayer::pause() {
	waveOutPause(wout_);
}
void CWin32PCMPlayer::resume() {
	waveOutRestart(wout_);
}
void CWin32PCMPlayer::getpos(int *samples, float *seconds) {
	K_ASSERT(wfmt_.nSamplesPerSec > 0);
	MMTIME mt;
	mt.wType = TIME_SAMPLES;
	waveOutGetPosition(wout_, &mt, sizeof(MMTIME));
	// mt.wType で指定した形式がサポートされていない場合は
	// waveOutGetPosition によって勝手に代替形式が設定され、
	// mt.wType にはその代替形式が入ることに注意
	K_ASSERT(mt.wType == TIME_SAMPLES); 
	if (samples) *samples = mt.u.sample;
	if (seconds) *seconds = (float)mt.u.sample / wfmt_.nSamplesPerSec;
}
void CWin32PCMPlayer::getvol(float *lvol, float *rvol) {
	DWORD vol = 0;
	waveOutGetVolume(wout_, &vol);
	WORD L = LOWORD(vol);
	WORD R = HIWORD(vol);
	if (lvol) *lvol = (float)L / 0xFFFF;
	if (rvol) *rvol = (float)R / 0xFFFF;
}
void CWin32PCMPlayer::setvol(float lvol, float rvol) {
	WORD L = (WORD)(0xFFFF * lvol); // HIWORD に左音量を入れる
	WORD R = (WORD)(0xFFFF * rvol); // LOWORD に右音量を入れる
	waveOutSetVolume(wout_, MAKELONG(L, R));
}
void CWin32PCMPlayer::setpitch(float pitch) {
	if (pitch <= 0) return;
	float i;
	float f = modf(pitch, &i);
	WORD H = (WORD)i; // HIWORD に整数部を入れる
	WORD L = (WORD)(0x10000 * f); // LOWORD に小数部を入れる
	MMRESULT mr = waveOutSetPitch(wout_, MAKEWORD(H, L));
	if (mr == MMSYSERR_NOTSUPPORTED) {
		// 未サポート。残念ながら vista 以降では実装されていないらしい…
	}
}
#pragma endregion // CWin32PCMPlayer


#pragma region CWin32AviWriter
#ifndef NO_AVI
#define K_AVI_BYTES_PER_PIXEL  3 // 24 bit color
CWin32AviWriter::CWin32AviWriter() {
	avi_strm_ = NULL;
	avi_file_ = NULL;
	buf_ = NULL;
	close(); // init
	//
	AVIFileInit();
}
CWin32AviWriter::~CWin32AviWriter() {
	close();
	AVIFileExit();
}
bool CWin32AviWriter::open(const wchar_t *filename, int width, int height, int fps) {
	close(); // clear
	width_ = width;
	height_ = height;
	index_ = 0;
	avi_file_ = NULL;
	avi_strm_ = NULL;
	// BITMAPFILEHEADER
	int w_align = (int)ceilf((width_ * K_AVI_BYTES_PER_PIXEL) / 4.0f) * 4;
	ZeroMemory(&bmp_filehdr_, sizeof(bmp_filehdr_));
	bmp_filehdr_.bfType = 0x4D42; // "BM"
	bmp_filehdr_.bfOffBits = sizeof(bmp_filehdr_) + sizeof(bmp_infohdr_);
	bmp_filehdr_.bfSize = bmp_filehdr_.bfOffBits + w_align * height_ * K_AVI_BYTES_PER_PIXEL;
	// BITMAPINFOHEADER
	ZeroMemory(&bmp_infohdr_, sizeof(bmp_infohdr_));
	bmp_infohdr_.biSize = sizeof(bmp_infohdr_);
	bmp_infohdr_.biWidth = width_;
	bmp_infohdr_.biHeight = height_;
	bmp_infohdr_.biPlanes = 1;
	bmp_infohdr_.biBitCount = 8 * K_AVI_BYTES_PER_PIXEL;
	// AVISTREAMINFO
	ZeroMemory(&avi_strminfo_, sizeof(AVISTREAMINFO));
	avi_strminfo_.fccType = streamtypeVIDEO;
	avi_strminfo_.fccHandler = comptypeDIB;
	avi_strminfo_.dwScale = 1;
	avi_strminfo_.dwRate = fps;
	avi_strminfo_.dwLength = 10000; // 全部で何枚になるかわからないので、適当に大きな数を入れておく
	avi_strminfo_.dwQuality = (DWORD)(-1);
	avi_strminfo_.rcFrame.right = width_;
	avi_strminfo_.rcFrame.bottom = height_;
	index_ = 0;

	HRESULT hr;
	hr = AVIFileOpenW(&avi_file_, filename, OF_CREATE|OF_WRITE, NULL);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }
	
	hr = AVIFileCreateStreamW(avi_file_, &avi_strm_, &avi_strminfo_);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = AVIStreamSetFormat(avi_strm_, 0, &bmp_infohdr_, sizeof(bmp_infohdr_));
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }
		
	return true;
}
void CWin32AviWriter::close() {
	if (avi_strm_) AVIStreamRelease(avi_strm_);
	if (avi_file_) AVIFileRelease(avi_file_);
	if (buf_) K_FREE(buf_);
	ZeroMemory(&bmp_filehdr_, sizeof(bmp_filehdr_));
	ZeroMemory(&bmp_infohdr_, sizeof(bmp_infohdr_));
	ZeroMemory(&avi_strminfo_, sizeof(avi_strminfo_));
	avi_strm_ = NULL;
	avi_file_ = NULL;
	buf_ = NULL;
	bufsize_ = 0;
	width_ = 0;
	height_ = 0;
	index_ = 0;
}
bool CWin32AviWriter::writeFromPixels(const void *pixels, size_t size) {
	HRESULT hr = AVIStreamWrite(avi_strm_, index_, 1, (LPVOID)pixels, size, AVIIF_KEYFRAME, NULL, NULL);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }
	index_++;
	return true;
}
bool CWin32AviWriter::writeFromBitmapFile(const wchar_t *filename) {
	// BMPファイルを開く
	HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return false;

	size_t size = bmp_filehdr_.bfSize - bmp_filehdr_.bfOffBits;
	if (bufsize_ < size) {
		buf_ = K_REALLOC(buf_, size);
		bufsize_ = size;
	}
	// 画像データを取得する。BMPファイルのヘッダは open 時に決めたものと全く同一とする
	SetFilePointer(hFile, bmp_filehdr_.bfOffBits, NULL, FILE_BEGIN);
	DWORD sz = 0;
	ReadFile(hFile, buf_, size, &sz, NULL);
	CloseHandle(hFile);

	// ビットマップを追加
	bool result = writeFromPixels(buf_, size);
	K_FREE(buf_);
	return result;
}
#endif // NO_AVI
#pragma endregion // CWin32AviWriter


#pragma region CWin32FilesInDirecrory
int CWin32FilesInDirecrory::getCount() {
	return (int)items_.size();
}
const WIN32_FIND_DATAW * CWin32FilesInDirecrory::getData(int index) {
	return &items_[index];
}
int CWin32FilesInDirecrory::scan(const wchar_t *dir) {
	items_.clear();
	int num = 0;
	wchar_t path[MAX_PATH];
	WIN32_FIND_DATAW fdata;
	wcscpy(path, dir);
	PathAppendW(path, L"*");
	HANDLE hFind = FindFirstFileW(path, &fdata);
	if (hFind == INVALID_HANDLE_VALUE) return 0;
	do {
		if (fdata.cFileName[0] != L'.') {
			items_.push_back(fdata);
			num++;
		}
	} while (FindNextFileW(hFind, &fdata));
	FindClose(hFind);
	return num;
}
#pragma endregion // CWin32FilesInDirecrory


#pragma region CWin32ResourceFiles
BOOL CWin32ResourceFiles::nameCallback(HMODULE module, LPCWSTR type, LPWSTR name, LONG user) {
	CWin32ResourceFiles *obj = reinterpret_cast<CWin32ResourceFiles *>(user);
	obj->onEnum(module, type, name);
	return TRUE;
}
void CWin32ResourceFiles::onEnum(HMODULE module, LPCWSTR type, LPWSTR name) {
	// https://msdn.microsoft.com/ja-jp/library/windows/desktop/ms648038(v=vs.85).aspx
	if (IS_INTRESOURCE(name)) return; // 文字列ではなくリソース番号を表している場合は無視
	if (IS_INTRESOURCE(type)) return; // 文字列ではなくリソース番号を表している場合は無視

	ITEM item;
	wcscpy(item.name, name);
	wcscat(item.name, L".");
	wcscat(item.name, type);
	item.hModule = module;
	item.hRsrc = FindResourceW(module, name, type);
	K_ASSERT(item.hRsrc);
	items_.push_back(item);
}
BOOL CWin32ResourceFiles::typeCallback(HMODULE module, LPWSTR type, LONG user) {
	EnumResourceNamesW(module, type, nameCallback, user);
	return TRUE;
}
void CWin32ResourceFiles::update() {
	items_.clear();
	EnumResourceTypesW(NULL, typeCallback, (LONG_PTR)this);
}
int CWin32ResourceFiles::getCount() const {
	return (int)items_.size();
}
const CWin32ResourceFiles::ITEM * CWin32ResourceFiles::getItem(int index) const {
	return &items_[index];
}
const void * CWin32ResourceFiles::getData(int index, int *size) const {
	if (0 <= index && index < (int)items_.size()) {
		const ITEM &item = items_[index];
		HGLOBAL hGlobal = LoadResource(item.hModule, item.hRsrc);
		if (size) *size = (int)SizeofResource(item.hModule, item.hRsrc);
		return LockResource(hGlobal); // アンロック不要
	}
	return NULL;
}
#pragma endregion // CWin32ResourceFiles


#pragma region CWin32Menu
CWin32Menu::CWin32Menu(HMENU hMenu) {
	hMenu_ = hMenu;
	cb_ = NULL;
	isopen_ = false;
}
HMENU CWin32Menu::getHandle() {
	return hMenu_;
}
void CWin32Menu::setHandle(HMENU hMenu) {
	hMenu_ = hMenu;
	isopen_ = false;
}
void CWin32Menu::setCallback(IWin32MenuCallback *cb) {
	cb_ = cb;
}
void CWin32Menu::create() {
	isopen_ = false;
	hMenu_ = CreateMenu();
}
void CWin32Menu::destroy() {
	DestroyMenu(hMenu_);
}
int CWin32Menu::getCount() {
	return GetMenuItemCount(hMenu_);
}
void CWin32Menu::addItem(const wchar_t *text, UINT id, UINT userdata) {
	Win32Menu_AddItem(hMenu_, text, id, userdata);
}
int CWin32Menu::getIndexOfMenuId(UINT id) {
	return Win32Menu_GetIndexOfId(hMenu_, id);
}
bool CWin32Menu::hasMenuId(UINT id) {
	return getIndexOfMenuId(id) >= 0;
}

static bool K_GetMenuItem(HMENU hMenu, UINT number, bool by_position, SWin32MenuItem *item) {
	K_ASSERT(item);

	ZeroMemory(item, sizeof(SWin32MenuItem));

	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.cch = sizeof(item->text) / sizeof(wchar_t);
	info.dwTypeData = item->text;
	info.fMask = MIIM_STATE|MIIM_ID|MIIM_SUBMENU|MIIM_FTYPE|MIIM_STRING|MIIM_DATA;
	if (GetMenuItemInfoW(hMenu, number, by_position, &info)) {
		item->checked = (info.fState & MFS_CHECKED) != 0;
		item->enabled = (info.fState & MFS_DISABLED) == 0; // <--- MFS_ENABLED は 0 なので & による比較できない！
		item->submenu = info.hSubMenu;
		item->id = info.wID;
		item->userdata = info.dwItemData;
		if (info.fType & MFT_STRING) {
	//		wcscpy(item->text, info.dwTypeData);
		}
		if (info.fType & MFT_SEPARATOR) {
			wcscpy(item->text, L"-");
		}
		return true;
	}
	return false;
}
static bool K_SetMenuItem(HMENU hMenu, UINT number, bool by_position, const SWin32MenuItem *item) {
	K_ASSERT(item);

	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);

	info.fMask |= MIIM_ID;
	info.wID = item->id;

	info.fMask |= MIIM_STATE;
	if (item->checked) {
		info.fState |= MFS_CHECKED;
	} else {
		info.fState |= MFS_UNCHECKED;
	}
	if (item->enabled) {
		info.fState |= MFS_ENABLED;
	} else {
		info.fState |= MFS_DISABLED;
	}

	//if (state->separator) {
	//	info.fMask |= MIIM_TYPE;
	//	info.fType = MFT_SEPARATOR;
	//}

	HICON hIcon = NULL;
	if (item->icon[0]) {
		HINSTANCE inst = (HINSTANCE)GetModuleHandle(NULL);
		hIcon = (HICON)LoadImageW(inst, item->icon, IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	}
	info.fMask |= MIIM_BITMAP;
	if (hIcon) {
		info.hbmpItem = Win32_CreateBitmapFromIcon(hIcon);
	} else {
		info.hbmpItem = NULL;
	}

	info.fMask |= MIIM_SUBMENU;
	info.hSubMenu = item->submenu;

	info.fMask |= MIIM_FTYPE;
	if (wcscmp(item->text, L"-") == 0) {
		info.fType = MFT_SEPARATOR;
	} else {
		info.fMask |= MIIM_STRING;
		info.fType = MFT_STRING;
		info.dwTypeData = (LPWSTR)item->text;
	}
	return SetMenuItemInfoW(hMenu, number, by_position, &info) != 0;
}

bool CWin32Menu::getItemByIndex(int index, SWin32MenuItem *item) {
	return K_GetMenuItem(hMenu_, index, TRUE, item);
}
bool CWin32Menu::getItemById(UINT id, SWin32MenuItem *item) {
	return K_GetMenuItem(hMenu_, id, FALSE, item);
}
void CWin32Menu::setItemByIndex(int index, const SWin32MenuItem *item) {
	K_SetMenuItem(hMenu_, index, TRUE, item);
}
void CWin32Menu::setItemById(UINT id, const SWin32MenuItem *item) {
	K_SetMenuItem(hMenu_, id, FALSE, item);
}

bool CWin32Menu::processMessage(SWin32Msg *msg) {
	// http://wisdom.sakura.ne.jp/system/winapi/win32/win77.html
	// http://www.geocities.jp/asumaroyuumaro/program/winapi/menu.html
	switch (msg->uMsg) {
	case WM_INITMENUPOPUP:
		{
			HMENU hMenu = (HMENU)msg->wParam;
			if (hMenu == hMenu_) {
				if (cb_) cb_->on_win32menu_open(msg, this);
				Win32_Printf(L"%d\n", LOWORD(msg->lParam));
				isopen_ = true;
				return true;
			}
			break;
		}
	case WM_SYSCOMMAND:
		{
			UINT id = LOWORD(msg->wParam);
			int index = getIndexOfMenuId(id);
			if (index >= 0) {
				if (cb_) cb_->on_win32menu_click(msg, this, index);
				return true;
			}
			break;
		}
	case WM_COMMAND:
		{
			if (msg->lParam == 0) {
				UINT id = LOWORD(msg->wParam);
				int index = getIndexOfMenuId(id);
				if (index >= 0) {
					if (cb_) cb_->on_win32menu_click(msg, this, index);
					return true;
				}
			}
			break;
		}
	case WM_MENUSELECT:
		{
			HMENU hMenu = (HMENU)msg->lParam;
			if (hMenu == NULL && HIWORD(msg->wParam) == 0xFFFF && isopen_) {
				isopen_ = false;
				if (cb_) cb_->on_win32menu_close(msg, this);
				return true;
			}
			if (hMenu == hMenu_) {
				int index = -1;
				UINT uItem  = LOWORD(msg->wParam);
				UINT uFlags = HIWORD(msg->wParam);
				// メニューによって wParam の意味が変わるので注意。
				// サブメニューを持つなら wParam 下位ワードはメニュー項目のインデックスを表す
				// サブメニューを持たないなら wParam はメニュー項目のIDを表す
				if (uFlags & MF_POPUP) {
					index = (int)uItem;
				} else {
					index = getIndexOfMenuId(uItem);
				}
				if (cb_) cb_->on_win32menu_select(msg, this, index);
				return true;
			}
			break;
		}
	}
	return false;
}
#pragma endregion // CWin32Menu


#pragma region CWin32MainMenu
CWin32MainMenu::CWin32MainMenu() {
	hMainMenuWnd_ = NULL;
}
void CWin32MainMenu::createMainMenu(HWND hWnd) {
	isopen_ = false;
	hMainMenuWnd_ = hWnd;
	hMenu_ = CreateMenu();
	SetMenu(hWnd, hMenu_);
}
void CWin32MainMenu::updateMainMenu() {
	DrawMenuBar(hMainMenuWnd_);
}
#pragma endregion // CWin32MainMenu


#pragma region CWin32PopupMenu
void CWin32PopupMenu::createPopup() {
	isopen_ = false;
	hMenu_ = CreatePopupMenu();
}
int CWin32PopupMenu::popup(HWND hWnd) {
	POINT cur;
	SetForegroundWindow(hWnd);
	GetCursorPos(&cur);
	return TrackPopupMenu(
		hMenu_, TPM_RETURNCMD | TPM_NONOTIFY,
		cur.x, cur.y, 
		0, hWnd, NULL);
}
#pragma endregion // CWin32PopupMenu



#pragma region CWin32Window
#define K_WND_CLASS_NAME L"K_WND_CLASS_NAME"
static LRESULT CALLBACK K_DefWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_DESTROY) {
		PostQuitMessage(0);
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

CWin32Window::CWin32Window() {
	hWnd_ = NULL;
}
HWND CWin32Window::getHandle() {
	return hWnd_;
}
void CWin32Window::setHandle(HWND hWnd) {
	hWnd_ = hWnd;
}
void CWin32Window::setMenu(HMENU hMenu) {
	SetMenu(hWnd_, hMenu);
}
HMENU CWin32Window::getMenu() {
	return GetMenu(hWnd_);
}
HMENU CWin32Window::getSystemMenu() {
	return GetSystemMenu(hWnd_, FALSE);
}
void CWin32Window::create(WNDPROC wndproc) {
	WNDCLASSEXW wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEXW));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpszClassName = K_WND_CLASS_NAME;
	wc.lpfnWndProc = wndproc ? wndproc : K_DefWndProc;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.cbWndExtra = 0;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClassExW(&wc);
	hWnd_ = CreateWindowExW(
		0, K_WND_CLASS_NAME, NULL, 
		WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
		NULL, NULL, NULL, NULL);
}
void CWin32Window::destroy() {
	// 処理の一貫性を保つため必ず WM_CLOSE を発行する
	if (IsWindowVisible(hWnd_)) {
		// 注意： CloseWindow(hWnd) を使うと WM_CLOSE が来ないまま終了する。
		// そのためか、CloseWindow でウィンドウを閉じたときは、閉じた後にも WM_MOVE などが来てしまう。
		// WM_CLOSE で終了処理をしていたり、WM_MOVE のタイミングでウィンドウ位置を保存とかやっているとおかしな事になる。
		// トラブルを避けるために、明示的に SendMessage(WM_CLOSE, ...) でメッセージを流すこと。
		SendMessageW(hWnd_, WM_CLOSE, 0, 0);
	}
	DestroyWindow(hWnd_);
	hWnd_ = NULL;
}
void CWin32Window::setWndProc(WNDPROC wp) {
	SetWindowLongW(hWnd_, GWL_WNDPROC, (LPARAM)wp);
}
void CWin32Window::setUserData(LONG data) {
	SetWindowLongW(hWnd_, GWL_USERDATA, data);
}
LONG CWin32Window::getUserData() {
	return GetWindowLongW(hWnd_, GWL_USERDATA);
}
void CWin32Window::setClientSize(int cw, int ch) {
	int ww, hh;
	Win32_ComputeWindowSize(hWnd_, cw, ch, &ww, &hh);
	SetWindowPos(hWnd_, NULL, 0, 0, ww, hh, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE);
}
void CWin32Window::show(int nCmdShow) {
	ShowWindow(hWnd_, nCmdShow);
}
bool CWin32Window::isMaximized() {
	return IsZoomed(hWnd_) != 0;
}
bool CWin32Window::isIconified() {
	return IsIconic(hWnd_) != 0;
}
bool CWin32Window::isFocused() {
	return GetForegroundWindow() == hWnd_;
}
bool CWin32Window::isVisible() {
	return IsWindowVisible(hWnd_) != 0;
}
void CWin32Window::setPosition(int x, int y) {
	SetWindowPos(hWnd_, NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
}
void CWin32Window::getPosition(int *x, int *y) {
	K_ASSERT(x && y);
	RECT rect;
	GetWindowRect(hWnd_, &rect);
	*x = rect.left;
	*y = rect.top;
}
void CWin32Window::getClientSize(int *cw, int *ch) {
	K_ASSERT(cw && ch);
	RECT rc = {0, 0, 0, 0};
	GetClientRect(hWnd_, &rc); // GetClientRect が失敗する可能性に注意
	*cw = rc.right - rc.left;
	*ch = rc.bottom - rc.top;
}
void CWin32Window::getWholeSize(int *w, int *h) {
	K_ASSERT(w && h);
	RECT rc = {0, 0, 0, 0};
	GetWindowRect(hWnd_, &rc);
	*w = rc.right - rc.left;
	*h = rc.bottom - rc.top;
}
void CWin32Window::setIcon(HICON hIcon) {
	SendMessageW(hWnd_, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	SendMessageW(hWnd_, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
}
void CWin32Window::screenToClient(int *x, int *y) {
	K_ASSERT(x && y);
	POINT p = {*x, *y};
	ScreenToClient(hWnd_, &p);
	*x = p.x;
	*y = p.y;
}
void CWin32Window::clientToScreen(int *x, int *y) {
	K_ASSERT(x && y);
	POINT p = {*x, *y};
	ClientToScreen(hWnd_, &p);
	*x = p.x;
	*y = p.y;
}
void CWin32Window::setStyle(LONG style) {
	SetWindowLongW(hWnd_, GWL_STYLE, style);
	SetWindowPos(hWnd_, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER); // call SetWindowPos for apply style
}
void CWin32Window::setExStyle(LONG exStyle) {
	SetWindowLongW(hWnd_, GWL_EXSTYLE, exStyle);
	SetWindowPos(hWnd_, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER); // call SetWindowPos for apply style
}
LONG CWin32Window::getStyle() {
	return GetWindowLongW(hWnd_, GWL_STYLE);
}
LONG CWin32Window::getExStyle() {
	return GetWindowLongW(hWnd_, GWL_EXSTYLE);
}
void CWin32Window::enable_WM_MOUSE_LEAVE() {
	TRACKMOUSEEVENT e;
	e.cbSize = sizeof(e);
	e.hwndTrack = hWnd_;
	e.dwFlags = TME_LEAVE;
	e.dwHoverTime = 0;
	TrackMouseEvent(&e);
}
void CWin32Window::setTitle(const wchar_t *s) {
	K_ASSERT(s);
	SetWindowTextW(hWnd_, s);
}
#pragma endregion // CWin32Window


#pragma region CWin32TaskBar
#ifndef NO_TASKBAR
CWin32TaskBar::CWin32TaskBar() {
	CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void**)&task3_);
}
CWin32TaskBar::~CWin32TaskBar() {
	if (task3_) {
		task3_->Release();
		task3_ = NULL;
	}
}
void CWin32TaskBar::setProgressValue(int done, int total) {
	if (task3_) {
		HWND hWnd = GetActiveWindow();
		task3_->SetProgressValue(hWnd, done, total);
	}
}
void CWin32TaskBar::setProgressState(TBPFLAG flag) {
	if (task3_) {
		HWND hWnd = GetActiveWindow();
		task3_->SetProgressState(hWnd, flag);
	}
}
#endif // NO_TASKBAR
#pragma endregion // CWin32TaskBar


#pragma region CWin32SystemTray
#ifndef NO_TASKBAR
void CWin32SystemTray::init(HWND hWnd, HICON hIcon, UINT uMsg, const wchar_t *hint) {
	ZeroMemory(&icondata_, sizeof(icondata_));
	icondata_.cbSize = sizeof(icondata_);
	icondata_.hWnd = hWnd;
//	icondata_.uID = 1000;
	icondata_.hIcon = hIcon;
	icondata_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	icondata_.uCallbackMessage = uMsg;

	size_t maxlen = sizeof(icondata_.szTip) / sizeof(wchar_t);
	K_ASSERT(wcslen(hint) < maxlen);
	wcscpy(icondata_.szTip, hint);
}
void CWin32SystemTray::addIcon() {
	Shell_NotifyIconW(NIM_ADD, &icondata_);
}
void CWin32SystemTray::delIcon() {
	Shell_NotifyIconW(NIM_DELETE, &icondata_);
}
bool CWin32SystemTray::isLClickMessage(const SWin32Msg *msg) {
	if (msg->hWnd == icondata_.hWnd) {
		if (msg->uMsg == icondata_.uCallbackMessage) {
			if (msg->lParam == WM_LBUTTONUP) {
				return true;
			}
		}
	}
	return false;
}
bool CWin32SystemTray::isRClickMessage(const SWin32Msg *msg) {
	if (msg->hWnd == icondata_.hWnd) {
		if (msg->uMsg == icondata_.uCallbackMessage) {
			if (msg->lParam == WM_RBUTTONDOWN) {
				return true;
			}
		}
	}
	return false;
}
UINT CWin32SystemTray::showMenu(HMENU hPopupMenu) {
	POINT cur;
	GetCursorPos(&cur);
	SetForegroundWindow(icondata_.hWnd);
	return TrackPopupMenu(
		hPopupMenu, TPM_RETURNCMD | TPM_NONOTIFY, 
		cur.x, cur.y, 
		0, icondata_.hWnd, NULL);
}
#endif // NO_TASKBAR
#pragma endregion // CWin32SystemTray




#pragma region CWin32Mouse
CWin32Mouse::CWin32Mouse() {
	init(NULL, NULL);
}
void CWin32Mouse::init(HWND hWnd, IWin32MouseCallback *cb) {
	hWnd_ = hWnd;
	cb_ = cb;
	wheel_ = 0;
	horz_wheel_ = 0;
	entered_ = false;
}
bool CWin32Mouse::getButton(int btn) {
	if (btn == 0) return Win32_IsKeyDown(VK_LBUTTON);
	if (btn == 1) return Win32_IsKeyDown(VK_RBUTTON);
	if (btn == 2) return Win32_IsKeyDown(VK_MBUTTON);
	return false;
}
void CWin32Mouse::getPositionInScreen(int *x, int *y) {
	K_ASSERT(x && y);
	POINT p = {0, 0};
	GetCursorPos(&p);
	*x = p.x;
	*y = p.y;
}
void CWin32Mouse::getPosition(int *x, int *y) {
	K_ASSERT(x && y);
	POINT p = {0, 0};
	GetCursorPos(&p);
	ScreenToClient(GetActiveWindow(), &p);
	*x = p.x;
	*y = p.y;
}
int CWin32Mouse::getWheel() {
	return wheel_;
}
int CWin32Mouse::getHorzWheel() {
	return horz_wheel_;
}
void CWin32Mouse::_MouseLeave() {
	// WM_MOUSELEAVE
	// マウスポインタがウィンドウから離れた。
	// 再び WM_MOUSELEAVE を発生させるには、
	// TrackMouseEvent() で再設定する必要があることに注意
	entered_ = false;
	if (cb_) cb_->on_win32mouse_leave();
}
void CWin32Mouse::_MouseMove(LPARAM lParam) {
	if (!entered_) { // マウスカーソルがウィンドウ内に入ってきたとみなす
		entered_ = true;
		if (cb_) cb_->on_win32mouse_enter();
		Win32_SetMouseLeaveEvent(hWnd_); // マウスカーソルが離れたときに WM_MOUSELEAVE が発生するようにする。
	}
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	if (cb_) cb_->on_win32mouse_move(x, y);
}
void CWin32Mouse::_MouseWheel(WPARAM wParam, bool is_horz) {
	int raw_delta = GET_WHEEL_DELTA_WPARAM(wParam);
	int delta = (raw_delta > 0) ? 1 : -1;
	if (is_horz) {
		horz_wheel_ += delta;
		if (cb_) cb_->on_win32mouse_horz_wheel(delta);
	} else {
		wheel_ += delta;
		if (cb_) cb_->on_win32mouse_wheel(delta);
	}
}
bool CWin32Mouse::processMessage(SWin32Msg *msg) {
	K_ASSERT(msg);
	if (msg->hWnd == hWnd_ || hWnd_ == NULL) {
		switch (msg->uMsg) {
		case WM_MOUSEWHEEL:  _MouseWheel(msg->wParam, 0); return true;
		case WM_MOUSEHWHEEL: _MouseWheel(msg->wParam, 1); return true;
		case WM_MOUSEMOVE:   _MouseMove(msg->lParam);     return true;
		case WM_LBUTTONDOWN: { if (cb_) cb_->on_win32mouse_down(0);} return true;
		case WM_RBUTTONDOWN: { if (cb_) cb_->on_win32mouse_down(1);} return true;
		case WM_MBUTTONDOWN: { if (cb_) cb_->on_win32mouse_down(2);} return true;
		case WM_LBUTTONUP:   { if (cb_) cb_->on_win32mouse_up(0);  } return true;
		case WM_RBUTTONUP:   { if (cb_) cb_->on_win32mouse_up(1);  } return true;
		case WM_MBUTTONUP:   { if (cb_) cb_->on_win32mouse_up(2);  } return true;
		case WM_MOUSELEAVE:  _MouseLeave();  return true;
		}
	}
	return false;
}
#pragma endregion // CWin32Mouse



#pragma region CWin32Keyboard
CWin32Keyboard::CWin32Keyboard() {
	init(NULL, NULL);
}
void CWin32Keyboard::init(HWND hWnd, IWin32KeyboardCallback *cb) {
	hWnd_ = hWnd;
	cb_ = cb;
	ZeroMemory(keys_, sizeof(keys_));
	key_ctrl_ = false;
	key_shift_ = false;
	key_alt_ = false;
}
void CWin32Keyboard::poll() {
	GetKeyboardState(keys_);
	key_ctrl_ = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	key_shift_ = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	key_alt_ = (GetKeyState(VK_MENU) & 0x8000) != 0;
}
bool CWin32Keyboard::processMessage(SWin32Msg *msg) {
	if (msg->hWnd == hWnd_ || hWnd_ == NULL) {
		switch (msg->uMsg) {
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			K_ASSERT(msg->wParam < 256);
			keys_[msg->wParam] = 1;
			if (cb_) cb_->on_win32kb_keydown(msg->wParam);
			return true;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			K_ASSERT(msg->wParam < 256);
			keys_[msg->wParam] = 0;
			if (cb_) cb_->on_win32kb_keyup(msg->wParam);
			return true;

		case WM_CHAR:
			text_.push_back((wchar_t)msg->wParam);
			if (cb_) cb_->on_win32kb_keychar((wchar_t)msg->wParam);
			return true;
		}
	}
	return false;
}
#pragma endregion // CWin32Keyboard



bool Win32_ProcMessage_Imm(SWin32Msg *msg, IWin32ImmCallback *cb) {
	K_ASSERT(msg);
	K_ASSERT(cb);
	// IMEハッカーズ☆
	// WM_IME_SETCONTEXT メッセージ
	// http://www.geocities.jp/katayama_hirofumi_mz/imehackerz/ja/WM_IME_SETCONTEXT.html
	//
	switch (msg->uMsg) {
	case WM_IME_SETCONTEXT:
		// IMMウィンドウの ON/OFF 
		Win32_Print(L"WM_IME_SETCONTEXT\n");
		return true;

	case WM_IME_NOTIFY:
		// IMEからの通知
		Win32_Print(L"WM_IME_NOTIFY\n");
	//	if (msg->wParam == IMN_SETCONVERSIONMODE) Win32_Print(L"  入力文字列モードが変化した\n");
	//	if (msg->wParam == IMN_SETOPENSTATUS    ) Win32_Print(L"  IMEのON,OFF状態が変化した\n");
	//	if (msg->wParam == IMN_OPENCANDIDATE    ) Win32_Print(L"  候補ウィンドウ開く\n");
	//	if (msg->wParam == IMN_CLOSECANDIDATE   ) Win32_Print(L"  候補ウィンドウ閉じる\n");
		return true;

	case WM_IME_REQUEST:
		// 情報を要求された
		Win32_Print(L"WM_IME_REQUEST\n");
		return true;

	case WM_IME_STARTCOMPOSITION:
		// これから未確定文字列がやってくるという予告
		Win32_Print(L"WM_IME_STARTCOMPOSITION\n");

		// このタイミングで変換ウィンドウの位置を指定しておく。
		// 選択範囲始点（＝変換範囲の始点）の文字座標を与えればＯＫ。
		// 座標はスクリーンではなくウィンドウのクライアント座標で指定する。
		// 与えた座標がクライアント範囲外だった場合は自動的にスクリーン左上に表示される
		// (この実装のヒントは ImGui の ImeSetInputScreenPosFn_DefaultImpl)
		{
			POINT p = {0, 0};
			cb->on_win32imm_query_text_position(&p);
			HIMC hIMC = ImmGetContext(msg->hWnd);
			COMPOSITIONFORM cf;
			cf.ptCurrentPos.x = p.x;
			cf.ptCurrentPos.y = p.y;
			cf.dwStyle = CFS_FORCE_POSITION;
			ImmSetCompositionWindow(hIMC, &cf);
		}
		return true;

	case WM_IME_COMPOSITION:
		// 未確定文字列がやってきたとき
		// 未確定文字列に変更があったとき
		// 未確定文字列が確定されたとき
		Win32_Print(L"WM_IME_COMPOSITION\n");
		if (msg->lParam & GCS_RESULTSTR) { // http://www.kumei.ne.jp/c_lang/sdk3/sdk_281.htm
			HIMC hIMC = ImmGetContext(msg->hWnd);
			DWORD dwSize = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
			dwSize += sizeof(wchar_t); // for null terminater
			HGLOBAL hStr = GlobalAlloc(GHND, dwSize);
			wchar_t *text = (wchar_t *)GlobalLock(hStr);
			DWORD nBytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, text, dwSize);
			if (nBytes > 0) {
				Win32_Print(text);
				Win32_Print(L"\n");
			}
			GlobalUnlock(hStr);
			GlobalFree(hStr);
			ImmReleaseContext(msg->hWnd, hIMC);
		}
		return true;

	case WM_IME_CHAR:
		// 確定文字が送られてきた
		Win32_Print(L"WM_IME_CHAR\n");
		{
			wchar_t c[] = {(wchar_t)msg->wParam, L'\n', L'\0'};
			Win32_Print(c);
		}
		cb->on_win32imm_insert_char((wchar_t)msg->wParam);
		// 処理済みとしてメッセージチェーンを終了する。
		// そうしないと、さらに WM_CHAR が呼ばれて二重に文字が処理されてしまう
		msg->lResult = 0;
		msg->bShouldReturn = true;
		return true;

	case WM_IME_ENDCOMPOSITION:
		// 変換が完了した
		Win32_Print(L"WM_IME_ENDCOMPOSITION\n");
		return true;
	}

	return false;
}



#pragma region CWin32Subclass
CWin32Subclass::CWin32Subclass() {
	prev_proc_ = NULL;
}
void CWin32Subclass::attachWindow(HWND hWnd) {
	prev_proc_ = (WNDPROC)GetWindowLongW(hWnd, GWL_WNDPROC);

	// WindowLong のユーザー領域に自身を登録するので、この領域が使用中でないことを確認する
	K_ASSERT(GetWindowLongW(hWnd, GWL_USERDATA) == 0);
	SetWindowLongW(hWnd, GWL_WNDPROC, (LONG)wndProc);
	SetWindowLongW(hWnd, GWL_USERDATA, (LONG)this);
}
void CWin32Subclass::detachWindow(HWND hWnd) {
	// 設定を元に戻す
	SetWindowLongW(hWnd, GWL_USERDATA, 0);
	SetWindowLongW(hWnd, GWL_WNDPROC, (LONG)prev_proc_);
	prev_proc_ = NULL;
}
WNDPROC CWin32Subclass::getPrevProc() {
	return prev_proc_;
}
// [STATIC METHOD]
LRESULT CWin32Subclass::wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	LONG ptr = GetWindowLongW(hWnd, GWL_USERDATA);
	CWin32Subclass *klass = reinterpret_cast<CWin32Subclass *>(ptr);
	if (klass) {
		SWin32Msg msg;
		ZeroMemory(&msg, sizeof(msg));
		msg.hWnd = hWnd;
		msg.uMsg = uMsg;
		msg.wParam = wParam;
		msg.lParam = lParam;
		klass->onMessage(&msg);
		if (msg.bShouldReturn) {
			return msg.lResult;
		}
	}
	return CallWindowProcW(klass->prev_proc_, hWnd, uMsg, wParam, lParam);
}
#pragma endregion // CWin32Subclass


#pragma region CBoxLayout
CBoxLayout::CBoxLayout() {
	ZeroMemory(stack_, sizeof(stack_));
	depth_ = 0;
	cur_ = stack_;
}
CBoxLayout::CUR *CBoxLayout::cur() {
	return cur_;
}
void CBoxLayout::reset() {
	cur_->margin_x = 20;
	cur_->margin_y = 20;
	cur_->space_x = 10;
	cur_->space_y = 10;
	cur_->row_w = 0;
	cur_->row_h = 0;
	cur_->next_x = cur_->margin_x;
	cur_->next_y = cur_->margin_y;
	cur_->bound_w = 0;
	cur_->bound_h = 0;
	cur_->full_w = 0;
	cur_->full_h = 0;
	cur_->min_row_h = 8;
	cur_->userdata = NULL;
}
void CBoxLayout::addBox(int w, int h) {
	// カーソルをコントロールの右側に移動
	cur_->next_x += w + cur_->space_x;

	// 行サイズを更新
	cur_->row_w += w;
	cur_->row_h = K_MAX(cur_->row_h, h);

	// 外接矩形を更新
	cur_->bound_w = K_MAX(cur_->bound_w, cur_->row_w);
	cur_->bound_h = K_MAX(cur_->bound_h, cur_->next_y + cur_->row_h - cur_->margin_y); // <-- next_y の初期値は margin_y だけかさ上げされているので、その分を引く

	// 余白も含めたサイズ
	cur_->full_w = K_MAX(cur_->full_w, cur_->margin_x + cur_->bound_w + cur_->margin_x);
	cur_->full_h = K_MAX(cur_->full_h, cur_->margin_y + cur_->bound_h + cur_->margin_y);
}
void CBoxLayout::newLine() {
	// カーソルを次の行の先頭に移動
	cur_->next_x = cur_->margin_x;
	cur_->next_y += cur_->row_h + cur_->space_y;
	cur_->row_w = 0;
	cur_->row_h = cur_->min_row_h;
}
void CBoxLayout::push() {
	// 現在のウィンドウをスタックに退避し、
	// グループウィンドウ内での操作に切り替える
	K_ASSERT(depth_+1 < MAX_DEPTH);
	depth_++;
	cur_ = stack_ + depth_;
	ZeroMemory(cur_, sizeof(CUR));
	reset();
}
void CBoxLayout::pop() {
	K_ASSERT(depth_ > 0);
	depth_--;
	cur_ = stack_ + depth_;
}
#pragma endregion // CBoxLayout


CWin32Ctrls::CWin32Ctrls() {
}
void CWin32Ctrls::add(HWND hWnd) {
	items_[hWnd] = 0;
}
bool CWin32Ctrls::isClicked(HWND hWnd, bool keep_flag) {
	auto it = items_.find(hWnd);
	if (it == items_.end()) return false;

	if (it->second) {
		if (!keep_flag) {
			it->second= 0;
			return true;
		}
	}
	return false;
}
bool CWin32Ctrls::onMessage(SWin32Msg *msg) {
	if (msg->uMsg == WM_COMMAND) {
	//	LONG hCtrlID = LOWORD(wParam);
	//	HWND hCtrl = GetDlgItem(hWnd, hCtrlID);
		HWND hCtrl = (HWND)msg->lParam;
		if (hCtrl && items_.find(hCtrl)!=items_.end()) {
			items_[hCtrl] = 1;
		}
	}
	return true;
}




#pragma region CWin32Layouter
void CWin32DialogLayouter::init(HWND hWnd) {
	hTop_ = hWnd;
	lay_.reset();
	padding_x_ = 6;
	padding_y_ = 6;
	fontsize_ = 12;
	auto_sizing_ = true;
	flatten_grouping_ = false;
	lay_.cur()->userdata = hWnd;
	hInst_ = (HINSTANCE)GetModuleHandleW(NULL);
}
HWND CWin32DialogLayouter::cur_hWnd() {
	return (HWND)lay_.cur()->userdata;
}
HWND CWin32DialogLayouter::getCtrl(int id) {
	return GetDlgItem(cur_hWnd(), id);
}
HWND CWin32DialogLayouter::addLabel(int id, const wchar_t *text) {
	K_ASSERT(text);
	HWND hCtrl = Win32_CreateLabel(hTop_, id, text);
	Win32_SetPos(hCtrl, cur_x(), cur_y());
	addControl(hCtrl, auto_sizing_, auto_sizing_);
	return hCtrl;
}
HWND CWin32DialogLayouter::addButton(int id, const wchar_t *text) {
	K_ASSERT(text);
	HWND hCtrl = Win32_CreateButton(hTop_, id, text);
	Win32_SetPos(hCtrl, cur_x(), cur_y());
	addControl(hCtrl, auto_sizing_, auto_sizing_);
	return hCtrl;
}
HWND CWin32DialogLayouter::addEdit(int id, int w, int h, const wchar_t *text, bool readonly, bool multilines) {
	HWND hCtrl = Win32_CreateEdit(hTop_, id, text, readonly, multilines);
	Win32_SetPos(hCtrl, cur_x(), cur_y());
	Win32_SetSize(hCtrl, w, h);
	addControl(hCtrl, false, auto_sizing_);
	return hCtrl;
}
HWND CWin32DialogLayouter::addCheckBox(int id, const wchar_t *text, bool value) {
	K_ASSERT(text);
	HWND hCtrl = Win32_CreateCheckBox(hTop_, id, text);
	Win32_SetPos(hCtrl, cur_x(), cur_y());
	addControl(hCtrl, auto_sizing_, auto_sizing_);
	return hCtrl;
}
HWND CWin32DialogLayouter::addListBox(int id, int w, int h, int selected, const wchar_t *items[], int count) {
	HWND hCtrl = Win32_CreateListBox(hTop_, id);
	Win32_SetPos(hCtrl, cur_x(), cur_y());
	Win32_SetSize(hCtrl, w, h);
	addControl(hCtrl, false, false);
	Win32_UpdateListBox(hCtrl, selected, items, count);
	return hCtrl;
}
HWND CWin32DialogLayouter::addDropDownEdit(int id, int w, int h, int selected, const wchar_t *items[], int count, const wchar_t *text) {
	HWND hCtrl = Win32_CreateDropDown(hTop_, id);
	Win32_SetPos(hCtrl, cur_x(), cur_y());
	Win32_SetSize(hCtrl, w, h);
	addControl(hCtrl, false, false);
	Win32_UpdateDropDownEdit(hCtrl, selected, items, count, text);
	return hCtrl;
}
HWND CWin32DialogLayouter::addDropDownList(int id, int w, int h, int selected, const wchar_t *items[], int count) {
	HWND hCtrl = Win32_CreateDropDownList(hTop_, id);
	Win32_SetPos(hCtrl, cur_x(), cur_y());
	Win32_SetSize(hCtrl, w, h);
	addControl(hCtrl, false, false);
	Win32_UpdateDropDownList(hCtrl, selected, items, count);
	return hCtrl;
}
HWND CWin32DialogLayouter::makeControl(int id, const wchar_t *klass, const wchar_t *text, DWORD style, int w, int h) {
	HWND hCtrl = CreateWindowExW(
		0, klass, NULL, style, cur_x(), cur_y(), w, h, hTop_, (HMENU)id, hInst_, NULL
	);
	SetWindowTextW(hCtrl, text);
	return hCtrl;
}
void CWin32DialogLayouter::addHorzLine(int w) {
	HWND hCtrl = CreateWindowExW(
		0, L"STATIC", NULL,
		WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
		cur_x(), cur_y(), w, 2,
		hTop_, NULL, hInst_, NULL
	);
	addControl(hCtrl, false, false);
}
void CWin32DialogLayouter::addVertLine(int h) {
	HWND hCtrl = CreateWindowExW(
		0, L"STATIC", NULL,
		WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
		cur_x(), cur_y(), 2, h,
		hTop_, NULL, hInst_, NULL
	);
	addControl(hCtrl, false, false);
}
void CWin32DialogLayouter::addControl(HWND hCtrl, bool xFit, bool yFit) {
	Win32_SetFontSize(hCtrl, fontsize_);
	_autoResize(hCtrl, xFit, yFit);

	// コントロールの位置とサイズ
	RECT rect;
	Win32_GetRectInParent(hCtrl, &rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
	lay_.addBox(w, h);

	if (add_to_bottom_) {
		// tab order の調整
		// タブオーダーは手前から奥に向かって移動する。
		// 一方、コントロールは奥から手前に向かって追加される。
		// そのため、このままだとコントロールを追加した順番とは逆にタブが移動することになる

		// コントロールの追加順とタブの移動順を同じにするためには、
		// 新しいコントロールを奥に追加していく必要がある
		SetWindowPos(hCtrl, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	}
}
void CWin32DialogLayouter::setToolTip(int id, const wchar_t *text) {
	HWND hCtrl = getCtrl(id);
	Win32_SetToolHint(hTop_, hCtrl, text);
}
HWND CWin32DialogLayouter::beginGroup() {
	return beginGroup(0, NULL);
}
HWND CWin32DialogLayouter::beginGroup(int id, const wchar_t *text) {
	// グループ作成
	HWND hGroup;
	if (text) {
		// GroupBox
		hGroup = CreateWindowExW(
			0, L"BUTTON", text,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_GROUPBOX,
			cur_x(), cur_y(), 0, 0,
			hTop_, (HMENU)id, hInst_, NULL
		);
	} else {
		// Panel
		hGroup = CreateWindowExW(
			0, L"STATIC", NULL,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			cur_x(), cur_y(), 0, 0,
			hTop_, (HMENU)id, hInst_, NULL
		);
	}

	// 現在のウィンドウをスタックに退避し、
	// グループウィンドウ内での操作に切り替える
	lay_.push();
	lay_.cur()->userdata = hGroup;
	return hGroup;
}
void CWin32DialogLayouter::endGroup() {
	// 現在のグループ
	HWND hGroup = cur_hWnd();

	//	// 現在のバウンディングボックス
//	int w = lay_.full_w();
//	int h = lay_.full_h();
//
//	// グループコントロールのサイズを決定
//	Win32_SetSize(hGroup, w, h);

	// 現在のグループまたはウィンドウのサイズを内容物にフィットさせる
	resizeToFit();

	// スタック戻す
	lay_.pop();

	// グループボックスを追加
	addControl(hGroup, false, false);
}
void CWin32DialogLayouter::resizeToFit() {

	// グループコントロールまたはウィンドウのサイズを決定
	HWND hGroup = cur_hWnd();

	// 必要なクライアントサイズ
	int cw = lay_.full_w();
	int ch = lay_.full_h();

	// 必要なウィンドウサイズを計算
	RECT rect = {0, 0, cw, ch};
	AdjustWindowRect(&rect, GetWindowLongW(hGroup, GWL_STYLE), FALSE);

	// リサイズ
	int W = rect.right - rect.left;
	int H = rect.bottom - rect.top;
	SetWindowPos(hGroup, NULL, 0, 0, W, H, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE);
}
void CWin32DialogLayouter::newLine() {
	lay_.newLine();
}
void CWin32DialogLayouter::space(int w, int h) {
	lay_.addBox(w, h);
}
void CWin32DialogLayouter::_autoResize(HWND hCtrl, bool xFit, bool yFit) {
	if (!xFit && !yFit) return;

	// 現在のサイズ
	int old_w = 0;
	int old_h = 0;
	Win32_GetSize(hCtrl, &old_w, &old_h);

	// ぴったりの大きさ
	int fit_w = 0;
	int fit_h = 0;
	{
		if (Win32_IsCheckBox(hCtrl)) {
			Win32_CalcCheckBoxSize(hCtrl, &fit_w, &fit_h);
	
		} else if (Win32_IsSingleEdit(hCtrl)) {
			Win32_CalcEditBoxSize(hCtrl, &fit_w, &fit_h);

		} else if (Win32_IsPushButton(hCtrl)) {
			// ボタンの場合、フォーカス用の点線が
			// テキスト、ボタン境界線と密集してしまうため、少し間隔をあける
			Win32_CalcTextSize(hCtrl, &fit_w, &fit_h);
			fit_w += padding_x_ * 2; // 左右で2個分カウントする
			fit_h += padding_y_ * 2; // 上下で2個分カウントする

			fit_w += padding_x_ * 2; // 左右で2個分カウントする
			fit_h += padding_y_ * 2; // 上下で2個分カウントする

		} else {
			// このコントロールが文字を含むと仮定し、
			// その文字を描画するために必要なサイズを得る
			Win32_CalcTextSize(hCtrl, &fit_w, &fit_h);
			// 余白を追加する
			fit_w += padding_x_ * 2; // 左右で2個分カウントする
			fit_h += padding_y_ * 2; // 上下で2個分カウントする
		}
	}

	// リサイズ
	int new_w = xFit ? fit_w : old_w;
	int new_h = yFit ? fit_h : old_h;
	Win32_SetSize(hCtrl, new_w, new_h);
}

#pragma endregion // CWin32Layouter


#pragma region CWin32TextStoreACP
#ifndef NO_TSF
// Vista で読みから変換候補リストを取得する
// http://hp.vector.co.jp/authors/VA050396/tech_01.html

// IME (Input Method Editor) と Text Services Framework のアクセシビリティ
// https://msdn.microsoft.com/library/cc421572.aspx

// Text Services Framework - テキスト ストア
// https://msdn.microsoft.com/library/ms629043.aspx

// Text Services Framework - 表示属性の使用
// https://msdn.microsoft.com/library/ms629224.aspx

// Webと文字
// C++でWindowsのIMEを作ろう( ﾟ∀ﾟ)　その2
// http://d.hatena.ne.jp/project_the_tower2/20090403

// C++でWindowsのIMEを作ろう( ﾟ∀ﾟ)　その３
// http://d.hatena.ne.jp/project_the_tower2/20090426

// C++でWindowsのIMEを作ろう( ﾟ∀ﾟ)　その4
// http://d.hatena.ne.jp/project_the_tower2/20090506

// TSFに挑戦 (2) 表示属性
// https://dev.activebasic.com/egtra/2009/02/26/194/

// Effects Tutorial Win32 Sample
// ImeUi.cpp
// https://code.msdn.microsoft.com/windowsdesktop/Effects-Tutorial-Win32-b03b8501/sourcecode?fileId=121864&pathId=62325188

// IMEを使う（Windows: TSF編）
// https://qiita.com/496_/items/95acf68eaf31fd57f44e

// WindowsのIME API、TSFのTS_E_NOLAYOUT問題とは
// https://d-toybox.com/studio/weblog/show.php?id=2018021600

// 実装の参考
// https://github.com/Microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/winui/input/tsf/tsfapps/tsfpad-step2/TextStore.cpp

#if 1
#	define K_CONNECT_E_NOCONNECTION   E_FAIL
#	define K_CONNECT_E_ADVISELIMIT    E_FAIL
#else
#	define K_CONNECT_E_NOCONNECTION   CONNECT_E_NOCONNECTION
#	define K_CONNECT_E_ADVISELIMIT    CONNECT_E_ADVISELIMIT
#endif
const TsViewCookie EDIT_VIEW_COOKIE = 0;

CWin32TextStoreACP::CWin32TextStoreACP() {
	mRefCnt = 1;
	mDocLocked = 0;
	mActiveSelEnd = TS_AE_END;
	mIsPendingLockUpgrade = false;
	mIsInterimChar = false;
	mIsLayoutChanged = false;
	mOldTextSize = 0;
	mSink = NULL;
	mSinkMask = 0;
	mFuncProv = NULL;
	mReconv = NULL;
	mContext = NULL;
	mDocMgr = NULL;
	mThreadMgr = NULL;
	mCategoryMgr = NULL;
	mDispMgr = NULL;
	mEdit = NULL;
	mCompositionView = NULL;
}
CWin32TextStoreACP::~CWin32TextStoreACP() {
}
bool CWin32TextStoreACP::Init() {
	AddRef();
	HRESULT hr;
	hr = CoCreateInstance(CLSID_TF_ThreadMgr, 0, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr, (void**)&mThreadMgr);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&mCategoryMgr);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = CoCreateInstance(CLSID_TF_DisplayAttributeMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfDisplayAttributeMgr, (void**)&mDispMgr);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = mThreadMgr->Activate(&mClientID);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = mThreadMgr->CreateDocumentMgr(&mDocMgr);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = mDocMgr->CreateContext(mClientID, 0, (ITextStoreACP*)this, &mContext, &mEditCookie);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = mDocMgr->Push(mContext);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = mThreadMgr->GetFunctionProvider(GUID_SYSTEM_FUNCTIONPROVIDER, &mFuncProv);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	hr = mFuncProv->GetFunction(GUID_NULL, IID_ITfFnReconversion, (IUnknown**)&mReconv);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	return true;
}
void CWin32TextStoreACP::Shutdown() {
	K_RELEASE(mReconv);
	K_RELEASE(mFuncProv);
	K_RELEASE(mContext);
	if (mDocMgr) {
		mDocMgr->Pop(TF_POPF_ALL);
		K_RELEASE(mDocMgr);
	}
	K_RELEASE(mCategoryMgr);
	K_RELEASE(mDispMgr);
	K_RELEASE(mThreadMgr);
	mEditCookie = 0;
	Release();
}
void CWin32TextStoreACP::SetEdit(ITsfCallback *edit) {
	mEdit = edit;
}
int CWin32TextStoreACP::SelStart() {
	return mEdit->on_tsf_selstart();
}
int CWin32TextStoreACP::SelEnd() {
	return mEdit->on_tsf_selend();
}
const wchar_t * CWin32TextStoreACP::Text() {
	return mEdit->on_tsf_text();
}
int CWin32TextStoreACP::TextSize() {
	return mEdit->on_tsf_textsize();
}
STDMETHODIMP_(DWORD) CWin32TextStoreACP::AddRef() {
	return ++mRefCnt;
}
STDMETHODIMP_(DWORD) CWin32TextStoreACP::Release() {
	if (--mRefCnt == 0) {
		delete this;
		return 0;
	}
	return mRefCnt;
}
STDMETHODIMP CWin32TextStoreACP::QueryInterface(REFIID riid, void** ppReturn) {
	*ppReturn = NULL;
	if (riid == IID_IUnknown || riid == IID_ITextStoreACP) {
		*ppReturn = (ITextStoreACP *)this;
		AddRef();
		return S_OK;
	}
	if (riid == IID_ITfContextOwnerCompositionSink) {
		*ppReturn = (ITfContextOwnerCompositionSink *)this;
		AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}
//void ClearText() {
//	if (IsLocked(TS_LF_READ)) return;
//	LockDocument(TS_LF_READWRITE);
//	mSink->OnSelectionChange();
//	mEdit.clearText();
//	EditChanged();
//	UnlockDocument();
//	mSink->OnLayoutChange(TS_LC_CHANGE, EDIT_VIEW_COOKIE);
//}
//void EditChanged() {
//	// テキストに変更があった
//	if (mSink && (mSinkMask & TS_AS_TEXT_CHANGE)) {
//		ULONG cch = TextSize();
//		TS_TEXTCHANGE tc;
//		tc.acpStart = 0;
//		tc.acpOldEnd = mOldTextSize;
//		tc.acpNewEnd = cch;
//		mSink->OnTextChange(0, &tc);
//		mOldTextSize = cch;
//	}
//}
// 変換候補を表示
bool CWin32TextStoreACP::DebugPrintCandidates() {
	bool retval = false;
	HRESULT hr = E_FAIL;

	// 選択範囲の取得
	LockDocument(TS_LF_READ);
	static const int NUMSEL = 10;
	TF_SELECTION selections[NUMSEL] = {0};
	ULONG fetched_count = 0;
	hr = mContext->GetSelection(mEditCookie, TF_DEFAULT_SELECTION, NUMSEL, selections, &fetched_count);
	UnlockDocument();
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	// 変換範囲を取得
	ITfRange *range_cp = NULL;
	BOOL is_converted = FALSE;
	hr = mReconv->QueryRange(selections[0].range, &range_cp, &is_converted);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	// 再変換
	ITfCandidateList *cand_list_cp = NULL;
	hr = mReconv->GetReconversion(selections[0].range, &cand_list_cp);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	// 候補数を取得
	ULONG cand_length = 0;
	hr = cand_list_cp->GetCandidateNum(&cand_length);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }

	// 表示
	for (ULONG i=0; i<cand_length; i++) {
		ITfCandidateString *string_cp = NULL;
		BSTR bstr = NULL;
		cand_list_cp->GetCandidate(i, &string_cp);
		string_cp->GetString(&bstr);
		Win32_Print(bstr);
		Win32_Print(L"\n");
		SysFreeString(bstr);
		K_RELEASE(string_cp);
	}

	// 後始末
	K_RELEASE(cand_list_cp);
	K_RELEASE(range_cp);
	for (int i=0; i<NUMSEL; i++) {
		K_RELEASE(selections[i].range);
	}
	return true;
}

// シンク（イベントの受け取り手）の登録。
// 平たく言えばコールバックの登録
STDMETHODIMP CWin32TextStoreACP::AdviseSink(REFIID riid, IUnknown *pUnknown, DWORD dwMask) {
	Win32_Print(L"AdviseSink\n");

	// 登録できるシンクは ITextStoreACPSink だけ
	if (!IsEqualIID(riid, IID_ITextStoreACPSink)) return E_INVALIDARG;

	// シンクが既に登録されている場合はマスクの更新だけ行う
	if (mSink == pUnknown) {
		mSinkMask = dwMask;
		return S_OK;
	}
	// 違うインスタンスが登録済みならダメ
	if (mSink) {
		return K_CONNECT_E_ADVISELIMIT;
	}

	// 登録可能
	mSinkMask = dwMask;
	return pUnknown->QueryInterface(&mSink);
}

// シンク登録解除
STDMETHODIMP CWin32TextStoreACP::UnadviseSink(IUnknown* pUnknown) {
	Win32_Print(L"UnadviseSink\n");

	// 未登録シンクはダメ
	if (mSink != pUnknown) return K_CONNECT_E_NOCONNECTION;

	K_RELEASE(mSink);
	mSinkMask = 0;
	return S_OK;
}

// ロック要求
// ドキュメントに対してロックを要求し、ロックが成功した場合に登録してあるシンクの OnLockGranted() メソッドを呼ぶ
// キーボードを押しながら音声入力を行った場合など、複数のテキストサービスから同時に入力がくる可能性がある
STDMETHODIMP CWin32TextStoreACP::RequestLock(DWORD flags, HRESULT *p_hr) {
	Win32_Print(L"RequestLock\n");
	if (mSink == NULL) return E_UNEXPECTED;
	if (p_hr == NULL) return E_INVALIDARG;
	*p_hr = E_FAIL;
	if (mDocLocked) {
		// ロック済み
		if (flags & TS_LF_SYNC) {
			*p_hr = TS_E_SYNCHRONOUS; // ロック中に同期ロックは不可
			return S_OK;
		}
		// READロック中に、READ/WRITEロックが要求された場合のみ非同期ロックを許可
		if (((mDocLocked & TS_LF_READWRITE) == TS_LF_READ) &&
			((flags & TS_LF_READWRITE) == TS_LF_READWRITE))
		{
			mIsPendingLockUpgrade = true;
			*p_hr = TS_S_ASYNC;
			return S_OK;
		}
		return E_FAIL;
	}
	LockDocument(flags);
	// ロックできたことを知らせる。
	*p_hr = mSink->OnLockGranted(flags);
	UnlockDocument();
	return S_OK;
}

// ドキュメントのステータスを返す
// 読取専用や処理中、複数の選択を許可するかなどの状態を返す
STDMETHODIMP CWin32TextStoreACP::GetStatus(TS_STATUS *status) {
//	Win32_Print(L"GetStatus\n");
	if (status == NULL) return E_INVALIDARG;
	status->dwDynamicFlags = 0;
	status->dwStaticFlags = TS_SS_NOHIDDENTEXT; //MSDNライブラリ参照
	return S_OK;
}

// テキストの挿入または選択範囲の変更が可能かの問い合わせ
STDMETHODIMP CWin32TextStoreACP::QueryInsert(LONG start, LONG end, ULONG len, LONG *out_start, LONG *out_end) {
	Win32_Print(L"QueryInsert\n");
	if (end < start) return E_INVALIDARG;
	if (end < 0) return E_INVALIDARG;
	if (TextSize() < end) return E_INVALIDARG;
	*out_start = start;
	*out_end = end;
	return S_OK;
}

// ドキュメントの選択範囲を返す
// 開始位置はそのまま選択範囲の開始文字インデックスを、
// 終端位置は選択範囲の終端文字の次のインデックスを表す
STDMETHODIMP CWin32TextStoreACP::GetSelection(ULONG ulIndex, ULONG ulCount, TS_SELECTION_ACP *pSelection, ULONG *pFetched) {
//	Win32_Print(L"GetSelection\n");
	if (pFetched == NULL) return E_INVALIDARG;
	*pFetched = 0;
	if (pSelection == NULL) return E_INVALIDARG;
	if (!IsLocked(TS_LF_READ)) {
		return TS_E_NOLOCK; //読み取りロックが必要
	}
	if (ulIndex == TF_DEFAULT_SELECTION) {
		ulIndex = 0;
	} else if (ulIndex > 1) {
		return E_INVALIDARG; //この実装は複数セレクションに対応していない。
	}
	// 範囲選択時、始点と終点どちらにキャレットが存在するかを表す。
	mActiveSelEnd = TS_AE_END;
	pSelection[0].acpStart = SelStart();
	pSelection[0].acpEnd = SelEnd();
	pSelection[0].style.fInterimChar = mIsInterimChar;
	if (mIsInterimChar) {
		pSelection[0].style.ase = TS_AE_NONE;
	} else {
		pSelection[0].style.ase = mActiveSelEnd;
	}
	*pFetched = 1;
	return S_OK;
}

// 選択位置の設定
STDMETHODIMP CWin32TextStoreACP::SetSelection(ULONG numSelections, const TS_SELECTION_ACP *selections) {
	Win32_Print(L"SetSelection\n");
	if (selections == NULL) return E_INVALIDARG;
	if (numSelections > 1) return E_INVALIDARG; //この実装は複数セレクションに対応していない。
	if (!IsLocked(TS_LF_READWRITE)) return TS_E_NOLOCK; //読み書きロックが必要
	int start = selections[0].acpStart;
	int end = selections[0].acpEnd;
	mIsInterimChar = selections[0].style.fInterimChar != 0;
	if (mIsInterimChar) {
		mActiveSelEnd = TS_AE_NONE;
	} else {
		mActiveSelEnd = selections[0].style.ase;
	}
	if (mActiveSelEnd == TS_AE_START) {
		std::swap(start, end); // 文字列の始点側を選択位置の終わりとする。
	}
	mEdit->on_tsf_setselection(start, end);
	return S_OK;
}

// ドキュメントの指定した範囲の文字列を返す
// 開始インデックスから、終端インデックスの一つ手前の文字を返す。
// 終端インデックスが -1 だった場合、残り全ての文字列を意味する
STDMETHODIMP CWin32TextStoreACP::GetText(LONG acpStart, LONG acpEnd, PWSTR pchPlain, ULONG cchPlainReq, ULONG* pcchPlainOut, TS_RUNINFO* prgRunInfo, ULONG ulRunInfoReq, ULONG* pulRunInfoOut, LONG* pacpNext) {
	Win32_Print(L"GetText\n");
	if (!IsLocked(TS_LF_READ)) {
		return TS_E_NOLOCK; // ロックが必要。
	}
	bool fDoText = cchPlainReq > 0;
	bool fDoRunInfo = ulRunInfoReq > 0;
	if (pcchPlainOut) {
		*pcchPlainOut = 0;
	}
	if (fDoRunInfo) {
		*pulRunInfoOut = 0;
	}
	if (pacpNext) {
		*pacpNext = acpStart;
	}

	// 引数の検証
	if (acpStart < 0 || acpStart > TextSize()) {
		return TS_E_INVALIDPOS;
	}
	if (acpStart == TextSize()) {
		return S_OK; // 始点がテキストの終わりの場合、何もする必要がない。
	}
	if (acpEnd < 0) acpEnd = TextSize();
	ULONG cchReq = acpEnd - acpStart;
	if (fDoText) {
		if (cchReq > cchPlainReq){
			cchReq = cchPlainReq;
		}
		// 必要な場合にだけコピーする。
		if (pchPlain && cchPlainReq && cchReq > 0) {
			// ナル文字終端は不要。
			wmemcpy(pchPlain, Text() + acpStart, cchReq);
		}
	}
	if (pcchPlainOut) {
		*pcchPlainOut = cchReq;// 指定されていれば文字数を返す。
	}
	if (fDoRunInfo){
		//情報設定。すべてプレーンテキスト。
		*pulRunInfoOut = 1;
		prgRunInfo[0].type = TS_RT_PLAIN;
		prgRunInfo[0].uCount = cchReq;
	}
	if (pacpNext){
		*pacpNext = acpStart + cchReq;
	}
	return S_OK;
}
	
// テキスト設定
STDMETHODIMP CWin32TextStoreACP::SetText(DWORD dwFlags, LONG acpStart, LONG acpEnd, PCWSTR pchText, ULONG cch, TS_TEXTCHANGE* pChange) {
	Win32_Print(L"SetText\n");
	if (dwFlags & TS_ST_CORRECTION) {
		Win32_Print(L"\tTS_ST_CORRECTION\n");
	}
	// SetSelectionとInsertTextAtSelectionで実装。
	TS_SELECTION_ACP tsa;
	tsa.acpStart = acpStart;
	tsa.acpEnd = acpEnd;
	tsa.style.ase = TS_AE_END;
	tsa.style.fInterimChar = FALSE;
	HRESULT hr = SetSelection(1, &tsa);
	if (FAILED(hr)) { Win32_PrintHResult(hr); return false; }
	return InsertTextAtSelection(TS_IAS_NOQUERY, pchText, cch, 0, 0, pChange);
}

// フォーマット付きテキスト
STDMETHODIMP CWin32TextStoreACP::GetFormattedText(LONG acpStart, LONG acpEnd, IDataObject** ppDataObject) {
	return E_FAIL; // 非対応
}
STDMETHODIMP CWin32TextStoreACP::InsertEmbeddedAtSelection(DWORD dwFlags, IDataObject* pDataObject, LONG* pacpStart, LONG* pacpEnd, TS_TEXTCHANGE* pChange) {
	return E_NOTIMPL; // 埋め込みオブジェクト非対応
}

STDMETHODIMP CWin32TextStoreACP::GetEmbedded(LONG acpPos, REFGUID rguidService, REFIID riid, IUnknown** ppunk) {
	return E_NOTIMPL; // 埋め込みオブジェクト非対応
}
STDMETHODIMP CWin32TextStoreACP::QueryInsertEmbedded(GUID const* pguidService, FORMATETC const* pFormatEtc, BOOL* pfInsertable) {
	*pfInsertable = FALSE;
	return S_OK; // 埋め込みオブジェクト非対応
}
STDMETHODIMP CWin32TextStoreACP::InsertEmbedded(DWORD dwFlags, LONG acpStart, LONG acpEnd, IDataObject* pDataObject, TS_TEXTCHANGE* pChange) {
	return E_NOTIMPL; // 埋め込みオブジェクト非対応
}
STDMETHODIMP CWin32TextStoreACP::RequestSupportedAttrs(DWORD dwFlags, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs) {
	return S_OK; // 非対応
}
STDMETHODIMP CWin32TextStoreACP::RequestAttrsAtPosition(LONG acpPos, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs, DWORD dwFlags) {
	if(acpPos < 0 || acpPos > TextSize()) {
		return TS_E_INVALIDPOS;
	}
	return S_OK;
}
STDMETHODIMP CWin32TextStoreACP::RequestAttrsTransitioningAtPosition(LONG acpPos, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs, DWORD dwFlags) {
	return E_NOTIMPL; // 非対応
}
STDMETHODIMP CWin32TextStoreACP::FindNextAttrTransition(LONG acpStart, LONG acpHalt, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs, DWORD dwFlags, LONG* pacpNext, BOOL* pfFound, LONG* plFoundOffset) {
	return E_NOTIMPL; // 非対応
}
STDMETHODIMP CWin32TextStoreACP::RetrieveRequestedAttrs(ULONG count, TS_ATTRVAL *pAttrVals, ULONG *pFetched) {
	*pFetched = 0;
	return S_OK; // 非対応
}

// 選択位置の終わりを取得
STDMETHODIMP CWin32TextStoreACP::GetEndACP(LONG *pEnd) {
	Win32_Print(L"GetEndACP\n");
	if (pEnd == NULL) return E_INVALIDARG;
	if (!IsLocked(TS_LF_READWRITE)) return TS_E_NOLOCK;
	*pEnd = SelEnd();
	return S_OK;
}

// アクティブなビュー取得
STDMETHODIMP CWin32TextStoreACP::GetActiveView(TsViewCookie* pvcView) {
	*pvcView = EDIT_VIEW_COOKIE; // このアプリはこれ1つしか持っていない。
	return S_OK;
}
STDMETHODIMP CWin32TextStoreACP::GetACPFromPoint(TsViewCookie vcView, POINT const* pt, DWORD dwFlags, LONG* pacp) {
	Win32_Print(L"GetACPFromPoint\n");
	return E_NOTIMPL;
}
// テキストの含まれる領域を取得
STDMETHODIMP CWin32TextStoreACP::GetTextExt(TsViewCookie vcView, LONG acpStart, LONG acpEnd, RECT *pRect, BOOL *pIsClipped) {
	Win32_Print(L"GetTextExt\n");
	if (pRect == NULL) return E_INVALIDARG;
	if (pIsClipped == NULL) return E_INVALIDARG;
	if (vcView != EDIT_VIEW_COOKIE) return E_INVALIDARG;
	if (!IsLocked(TS_LF_READ)) return TS_E_NOLOCK;
	if (acpEnd < 0) acpEnd = TextSize();
	if (acpStart == acpEnd) return E_INVALIDARG;// 対象範囲が1文字も無い
	if (acpStart > acpEnd) std::swap(acpStart, acpEnd);
	*pIsClipped = FALSE;
	*pRect = RECT(); //0クリア
	mEdit->on_tsf_gettextrect(acpStart, acpEnd, pRect, pIsClipped);
	return S_OK;
}

// 全体の大きさの取得
STDMETHODIMP CWin32TextStoreACP::GetScreenExt(TsViewCookie vcView, RECT *pRect) {
	Win32_Print(L"GetScreenExt\n");
	if (pRect == NULL) return E_INVALIDARG;
	if (vcView != EDIT_VIEW_COOKIE) return E_INVALIDARG;
	*pRect = RECT(); //0クリア
	mEdit->on_tsf_getwholerect(pRect);
	return S_OK; // E_UNEXPECTED;
}

// ウィンドウハンドルの取得。
STDMETHODIMP CWin32TextStoreACP::GetWnd(TsViewCookie vcView, HWND *p_hWnd) {
	if (vcView == EDIT_VIEW_COOKIE && p_hWnd) {
		*p_hWnd = mEdit->on_tsf_gethandle();
		return S_OK;
	}
	return E_INVALIDARG;
}
STDMETHODIMP CWin32TextStoreACP::InsertTextAtSelection(DWORD dwFlags, PCWSTR pwszText, ULONG cch, LONG* pacpStart, LONG* pacpEnd, TS_TEXTCHANGE* pChange) {
	Win32_Print(L"InsertTextAtSelection\n");
	Win32_Printf(L"-- %ls\n", pwszText);

	if (!IsLocked(TS_LF_READWRITE)) return TS_E_NOLOCK;
	if (SelEnd() < SelStart()) return E_INVALIDARG;
	LONG acpOldEnd = SelEnd();
	if (dwFlags & TS_IAS_QUERYONLY) {
		if (pacpStart) *pacpStart = SelStart();
		if (pacpEnd) *pacpEnd = acpOldEnd;
		return S_OK;
	}
	if (pwszText == NULL) return E_INVALIDARG;
	mEdit->on_tsf_InsertText(std::wstring(pwszText, cch).c_str());
	if ((dwFlags & TS_IAS_NOQUERY) == 0) {
		if (pacpStart) *pacpStart = SelStart();
		if (pacpEnd) *pacpEnd = SelEnd();
	}
	pChange->acpStart = SelStart();
	pChange->acpOldEnd = acpOldEnd;
	pChange->acpNewEnd = SelEnd();
	mIsLayoutChanged = true; // ロック解除されてから通知を出す
	return S_OK;
}
// １文字だけを表すレンジを作成する
bool CWin32TextStoreACP::MakeCharRange(int index, ITfRange **ppRange) {
	int start = index;
	int end = index + 1;
	ITfRange *range = NULL;
	mContext->GetStart(mEditCookie, &range); // レンジ[0, 0) を取得
	if (range) {
		range->Collapse(mEditCookie, TF_ANCHOR_START); // レンジを長さゼロに圧縮。多分 [0, 0) と同じ
		LONG lshifted;
		LONG rshifted;
		range->ShiftStart(mEditCookie, start, &lshifted, NULL); // 開始点を start だけシフト。多分[start, 0) になる
		range->ShiftEnd(mEditCookie, end, &rshifted, NULL); // 終端を end だけシフト。多分[start, end) になる
		*ppRange = range;
		return true;
	}
	return false;
}
bool CWin32TextStoreACP::GetCharAttr(int index, TF_DISPLAYATTRIBUTE *pDispAttr) {
	bool result = false;
	HRESULT hr;
	// index 番目の文字を１文字だけ含むレンジを作成
	ITfRange *range = NULL;
	LockDocument(TS_LF_READ);
	if (MakeCharRange(index, &range)) {
		// 未確定文字列の情報
		ITfProperty *pProp = NULL;
		mContext->GetProperty(GUID_PROP_ATTRIBUTE, &pProp);
		if (pProp) {
			VARIANT var;
			VariantInit(&var);
			hr = pProp->GetValue(mEditCookie, range, &var);
			if (hr == S_FALSE) {
				// 指定範囲がプロパティ範囲と一致しなかった。
				// 指定範囲を含むプロパティ範囲を探してみる
				ITfRange *propRange = NULL;
				pProp->FindRange(mEditCookie, range, &propRange, TF_ANCHOR_START);
				if (range) {
					// プロパティ範囲が見つかったので、この範囲を使ってもう一度プロパティを取得する
					hr = pProp->GetValue(mEditCookie, propRange, &var);
					K_RELEASE(propRange);
				}
			}
			if (var.vt == VT_I4) {
				GUID guid;
				hr = mCategoryMgr->GetGUID((TfGuidAtom)var.lVal, &guid);
				ITfDisplayAttributeInfo *pDispInfo;
				hr = mDispMgr->GetDisplayAttributeInfo(guid, &pDispInfo, NULL);
				hr = pDispInfo->GetAttributeInfo(pDispAttr);
				K_RELEASE(pDispInfo);
				result = true;
			}
			VariantClear(&var);
			K_RELEASE(pProp);
		}
		K_RELEASE(range);
	}
	UnlockDocument();
	return result;
}
bool CWin32TextStoreACP::IsLocked(DWORD lockType) {
	return (mDocLocked & lockType) == lockType;
}
BOOL CWin32TextStoreACP::LockDocument(DWORD lockFlags) {
	if (mDocLocked) return FALSE;
	#if 0
	if ((lockFlags & TS_LF_READ) || (lockFlags & TS_LF_READWRITE)) {
		mDocLocked = lockFlags;
		return TRUE;
	}
	return FALSE;
	#endif
	mDocLocked = lockFlags;
	return TRUE;
}
void CWin32TextStoreACP::UnlockDocument() {
	mDocLocked = 0;
	if (mIsPendingLockUpgrade) { // 解除待ちがあれば、今すぐロックを与える
		HRESULT hr;
		mIsPendingLockUpgrade = false;
		RequestLock(TS_LF_READWRITE, &hr);
	}
	if (mIsLayoutChanged) { //ロック中に起きたレイアウト変更を通知。
		mIsLayoutChanged = false;
		mSink->OnLayoutChange(TS_LC_CHANGE, EDIT_VIEW_COOKIE);
	}
}
void CWin32TextStoreACP::SetFocus() {
	mThreadMgr->SetFocus(mDocMgr);
}
void CWin32TextStoreACP::UpdateSize() {
	mSink->OnLayoutChange(TS_LC_CHANGE, EDIT_VIEW_COOKIE);
}

// ITfContextOwnerCompositionSink
HRESULT CWin32TextStoreACP::OnStartComposition(ITfCompositionView *pComposition, BOOL *pfOk) {
	K_RELEASE(mCompositionView);
	mCompositionView = pComposition;
	mCompositionView->AddRef();
	mEdit->on_tsf_Notify(L"composition_start");
	*pfOk = TRUE;
	return S_OK;
}
// ITfContextOwnerCompositionSink
HRESULT CWin32TextStoreACP::OnUpdateComposition(ITfCompositionView *pComposition, ITfRange *pRangeNew) {
	K_RELEASE(mCompositionView);
	mCompositionView = pComposition;
	mCompositionView->AddRef();
	return S_OK;
}
// ITfContextOwnerCompositionSink
HRESULT CWin32TextStoreACP::OnEndComposition(ITfCompositionView *pComposition) {
	K_RELEASE(mCompositionView);
	mEdit->on_tsf_Notify(L"composition_end");
	return S_OK;
}
#endif // NO_TSF
#pragma endregion // CWin32TextStoreACP



