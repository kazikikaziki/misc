#include "KMenu.h"
#include "KStd.h"
#include <stdio.h> // NULL

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef _WIN32
namespace mo {

/// メニュー項目を追加する。
/// 成功すれば、新しいメニュー項目のインデックスを返す。失敗したら -1 を返す
static int Win32_AddItem(HMENU hMenu, const wchar_t *text, UINT wID) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_TYPE|MIIM_ID|MIIM_DATA;
	info.fType = MFT_STRING;
	info.wID = wID;
	info.dwTypeData = (LPWSTR)text;
	info.cch = wcslen(text);
	int index = GetMenuItemCount(hMenu);
	if (InsertMenuItemW(hMenu, index, TRUE, &info)) {
		return index;
	}
	return -1;
}
/// セパレータを追加する。
/// 成功すれば、新しいメニュー項目のインデックスを返す。失敗したら -1 を返す
static int Win32_AddSeparator(HMENU hMenu) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_TYPE;
	info.fType = MFT_SEPARATOR;
	int index = GetMenuItemCount(hMenu);
	if (InsertMenuItemW(hMenu, index, TRUE, &info)) {
		return index;
	}
	return -1;
}
static int Win32_GetIndexOfCommandId(HMENU hMenu, UINT wID) {
	if (wID == 0) return -1; // メニューIDが 0 の場合、インデックスを特定できない
	int num = GetMenuItemCount(hMenu);
	for (int i=0; i<num; i++) {
		UINT m = GetMenuItemID(hMenu, i);
		if (m == wID) return i;
	}
	return -1;
}
static HICON Win32_CreateIconFromPixelsRGBA32(int w, int h, const uint8_t *pixels) {
	uint8_t *buf = (uint8_t *)malloc(w * h * 4);
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
	free(buf);
	return hIcon;
}
/// HICON から HBITMAP を作成する
static HBITMAP Win32_CreateBitmapFromIcon(HICON hIcon) {
	if (hIcon == NULL) return NULL;
	int w = 16;
	int h = 16;
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
static BOOL Win32_IsMenuChecked(HMENU hMenu, UINT index) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STATE;
	if (GetMenuItemInfoW(hMenu, index, TRUE, &info)) {
		return (info.fState & MFS_CHECKED) != 0;
	}
	return FALSE;
}
static BOOL Win32_IsMenuEnabled(HMENU hMenu, UINT index) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STATE;
	if (GetMenuItemInfoW(hMenu, index, TRUE, &info)) {
		return (info.fState & MFS_DISABLED) == 0; // MFS_ENABLED は 0 なので & による比較ができない。MSF_DISABLEで判定する
	}
	return FALSE;
}
static ULONG Win32_GetMenuUserData(HMENU hMenu, UINT index) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_DATA;
	GetMenuItemInfoW(hMenu, index, TRUE, &info);
	return info.dwItemData;
}
static void Win32_SetMenuChecked(HMENU hMenu, UINT index, BOOL value) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STATE;
	info.fState = (value ? MFS_CHECKED : MFS_UNCHECKED);
	SetMenuItemInfoW(hMenu, index, TRUE, &info);
}
static void Win32_SetMenuEnabled(HMENU hMenu, UINT index, BOOL value) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STATE;
	info.fState = (value ? MFS_ENABLED : MFS_DISABLED);
	SetMenuItemInfoW(hMenu, index, TRUE, &info);
}
static void Win32_SetMenuBitmapFromPixelsRGBA32(HMENU hMenu, UINT index, int w, int h, const uint8_t *pixels_rgba32) {
	HICON hIcon = Win32_CreateIconFromPixelsRGBA32(w, h, pixels_rgba32);
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_BITMAP;
	if (hIcon) info.hbmpItem = Win32_CreateBitmapFromIcon(hIcon); // hIcon == NULLの場合はビットマップ無しになる
	SetMenuItemInfoW(hMenu, index, TRUE, &info);
}
static void Win32_SetMenuBitmapFromIconResourceName(HMENU hMenu, UINT index, const wchar_t *icon_res) {
	HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);
	HICON hIcon = (HICON)LoadImageW(hInst, icon_res, IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_BITMAP;
	if (hIcon) info.hbmpItem = Win32_CreateBitmapFromIcon(hIcon); // hIcon == NULLの場合はビットマップ無しになる
	SetMenuItemInfoW(hMenu, index, TRUE, &info);
}
static void Win32_SetMenuUserData(HMENU hMenu, UINT index, ULONG data) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_DATA;
	info.dwItemData = data;
	SetMenuItemInfoW(hMenu, index, TRUE, &info);
}
/*
static void Win32_AddSubMenu(HMENU hMenu, UINT index, HMENU hSubMenu) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_SUBMENU;
	info.hSubMenu = hSubMenu;
	SetMenuItemInfoW(hMenu, index, TRUE, &info);
}
static void Win32_SetMenuText(HMENU hMenu, UINT index, const wchar_t *text) {
	MENUITEMINFOW info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	if (text && wcscmp(text, L"-") == 0) {
		info.fMask = MIIM_FTYPE;
		info.fType = MFT_SEPARATOR;
	} else {
		info.fMask = MIIM_FTYPE|MIIM_STRING;
		info.fType = MFT_STRING;
		info.dwTypeData = (LPWSTR)text;
	}
	SetMenuItemInfoW(hMenu, index, TRUE, &info);
}
*/
/// システムメニューIDとして使える値かどうか調べる
static bool _IsUserSysCommand(UINT wID) {
	// システムメニューIDには16ビット符号なし整数を使う。
	// 定義済みのシステムメニューID (SC_CLOSE とか SC_MAXIMIZE など）はすべて 0xF000 以上の値になっていて、
	// さらに下位4ビットはシステムによって使われるため、0にしておかないといけない
	// つまり 0x0FF0 でマスクしたときに値が変化していなければよい
	return (wID & 0x0FF0) == wID;
}

/// ユーザー定義のコマンドIDを、実際に Win32で利用可能なコマンドIDに変換する
static UINT _ToRawCommandId(int user_id) {
	// システムメニューのIDは下位4ビットが0でないといけないので、4ビットシフトする。
	// システムメニューで利用可能な値については _IsUserSystemMenuId を参照せよ
	UINT wID = user_id << 4;
	K_assert(_IsUserSysCommand(wID)); // 確認
	return wID;
}

/// Win32の生のメニューIDをユーザーが定義したIDに変換する
static int _ToVirtualCommandId(UINT wID) {
	K_assert((wID & 0x000F) == 0);
	return wID >> 4;
}

#pragma region KMenu
KMenu::KMenu() {
	hMenu_ = NULL;
	cb_ = NULL;
}
void * KMenu::getHandle() {
	return hMenu_;
}
void KMenu::setHandle(void *hMenu) {
	K_assert(IsMenu((HMENU)hMenu));
	hMenu_ = hMenu;
}
void KMenu::setCallback(KMenuCallback *cb) {
	cb_ = cb;
}
void KMenu::create() {
	hMenu_ = CreateMenu();
}
void KMenu::createPopup() {
	hMenu_ = CreatePopupMenu();
}
void KMenu::createSystem(void *_hWnd) {
	HWND hWnd = _hWnd ? (HWND)_hWnd : GetActiveWindow();
	hMenu_ = GetSystemMenu(hWnd, FALSE);
}

void KMenu::destroy() {
	DestroyMenu((HMENU)hMenu_);
}
int KMenu::getItemCount() {
	return GetMenuItemCount((HMENU)hMenu_);
}
int KMenu::addItem(const wchar_t *text, int command_id) {
	UINT wID = _ToRawCommandId(command_id);
	return Win32_AddItem((HMENU)hMenu_, text, wID);
}
int KMenu::addSeparator() {
	return Win32_AddSeparator((HMENU)hMenu_);
}
bool KMenu::isItemChecked(int index) {
	return Win32_IsMenuChecked((HMENU)hMenu_, (UINT)index);
}
bool KMenu::isItemEnabled(int index) {
	return Win32_IsMenuEnabled((HMENU)hMenu_, (UINT)index);
}
void * KMenu::getItemUserData(int index) {
	return (void *)Win32_GetMenuUserData((HMENU)hMenu_, (UINT)index);
}
void KMenu::setItemChecked(int index, bool value) {
	Win32_SetMenuChecked((HMENU)hMenu_, (UINT)index, value);
}
void KMenu::setItemEnabled(int index, bool value) {
	Win32_SetMenuEnabled((HMENU)hMenu_, (UINT)index, value);
}
void KMenu::setItemBitmapFromResource(int index, const wchar_t *icon_res) {
	Win32_SetMenuBitmapFromIconResourceName((HMENU)hMenu_, (UINT)index, icon_res);
}
void KMenu::setItemBitmapFromPixels(int index, int w, int h, const void *pixels_rgba32) {
	Win32_SetMenuBitmapFromPixelsRGBA32((HMENU)hMenu_, (UINT)index, w, h, (const uint8_t *)pixels_rgba32);
}
void KMenu::setItemUserData(int index, void *data) {
	Win32_SetMenuUserData((HMENU)hMenu_, (UINT)index, (ULONG)data);
}
int KMenu::getItemCommandId(int index) {
	UINT wID = GetMenuItemID((HMENU)hMenu_, index);
	return _ToVirtualCommandId(wID);
}
int KMenu::getIndexOfCommandId(int id) {
	UINT wID = _ToRawCommandId(id); // Win32の生のメニューIDに変換
	return Win32_GetIndexOfCommandId((HMENU)hMenu_, wID);
}
int KMenu::popup(void * hWnd) {
	POINT cur;
	SetForegroundWindow((HWND)hWnd);
	GetCursorPos(&cur);
	return TrackPopupMenu(
		(HMENU)hMenu_, TPM_RETURNCMD | TPM_NONOTIFY,
		cur.x, cur.y, 
		0, (HWND)hWnd, NULL);
}

bool KMenu::processMessage(void *_hWnd, unsigned int _msg, long _wp, long _lp) {
//	HWND hWnd = (HWND)_hWnd;
	UINT uMsg = _msg;
	WPARAM wParam = _wp;
	LPARAM lParam = _lp;
	// http://wisdom.sakura.ne.jp/system/winapi/win32/win77.html
	// http://www.geocities.jp/asumaroyuumaro/program/winapi/menu.html
	switch (uMsg) {
	case WM_INITMENUPOPUP:
		{
			HMENU hMenu = (HMENU)wParam;
			if (hMenu == (HMENU)hMenu_) {
				if (cb_) cb_->on_menu_open(this);
				return true;
			}
			break;
		}
	case WM_SYSCOMMAND:
		{
			DWORD uCmdType = wParam & 0xFFF0;
			if (uCmdType == SC_MOUSEMENU) {
				// システムメニューを開こうとしている
				if (cb_) cb_->on_menu_open(this);
				break;
			}
			if (_IsUserSysCommand(uCmdType)) {
				// ユーザー定義のシステムメニューが選択された
				UINT wID = uCmdType;
				int index = Win32_GetIndexOfCommandId((HMENU)hMenu_, wID);
				if (index >= 0) {
					if (cb_) cb_->on_menu_click(this, index);
				}
				break;
			}
			break;
		}
	case WM_COMMAND:
		{
			// メニュー項目が決定された。
			// マウスクリックやキーボードー操作でのENTERキーに相当する
			if (lParam == 0) {
				UINT wID = LOWORD(wParam);
				int index = Win32_GetIndexOfCommandId((HMENU)hMenu_, wID);
				if (index >= 0) {
					if (cb_) cb_->on_menu_click(this, index);
					return true;
				}
			}
			break;
		}
	case WM_MENUSELECT:
		{
			// メニュー項目が選択状態になった。
			// メニュー項目の上でマウスを動かしたり、キーボード操作での上下キーに相当する
			HMENU hMenu = (HMENU)lParam;
			if (hMenu == NULL && HIWORD(wParam) == 0xFFFF) {
				return true;
			}
			if (hMenu == hMenu_) {
				int index;
				UINT uFlags = HIWORD(wParam);
				if (uFlags & MF_POPUP) {
					// このメニューはサブメニューを持っている。
					// wParam 下位ワードはメニューインデックスを表す
					index = (int)LOWORD(wParam);
				} else {
					// このメニューはサブメニューを持たない。
					// wParam 下位ワードはメニューIDを表す
					UINT wID = LOWORD(wParam);
					index = Win32_GetIndexOfCommandId((HMENU)hMenu_, wID);
				}
				if (cb_) cb_->on_menu_select(this, index);
				return true;
			}
			break;
		}
	}
	return false;
}

#pragma endregion // CWin32PopupMenu



} // namespace
#else // _WIN32

namespace mo {

KMenu::KMenu() {}
void KMenu::create() {}
void KMenu::createPopup() {}
void KMenu::createSystem(void *_hWnd) {}
void KMenu::destroy() {}
void KMenu::setCallback(KMenuCallback *cb) {}
void * KMenu::getHandle() {}
void KMenu::setHandle(void *hMenu) {}
int KMenu::addItem(const wchar_t *text, int command_id) { return 0; }
int KMenu::addSeparator() { return 0; }
int KMenu::getItemCount() { return 0; }
int KMenu::getIndexOfCommandId(int id) { return 0; }
bool KMenu::isItemChecked(int index) { return false; }
bool KMenu::isItemEnabled(int index) { return false; }
void * KMenu::getItemUserData(int index) { return NULL; }
int KMenu::getItemCommandId(int index) { return 0; }
void KMenu::setItemChecked(int index, bool value) {}
void KMenu::setItemEnabled(int index, bool value) {}
void KMenu::setItemUserData(int index, void *data) {}
void KMenu::setItemBitmap(int index, const wchar_t *icon_res) {}
bool KMenu::processMessage(void *_hWnd, unsigned int _uMsg, long _wp, long _lp) { return false; }
int KMenu::popup(void *hWnd) { return 0; }

} // namespace

#endif // !_WIN32
