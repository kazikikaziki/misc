#include "KMouse.h"
#ifdef _WIN32
#	include <Windows.h>
#endif

namespace mo {

static int s_Wheel = 0;
static int s_HorzWheel = 0;
static const char *s_Names[] = {
	"Left",
	"Right",
	"Middle",
	"",
};

// マウスホイールのグローバルフックについて。
// http://airiell.blog24.fc2.com/blog-entry-53.html
#define USE_GETMESSAGE 1



#ifdef _WIN32
static HHOOK s_Hook = NULL;
static HWND s_hWnd = NULL;
static LRESULT CALLBACK mouse_hook_cb(int nCode, WPARAM wParam, LPARAM lParam) {
#if USE_GETMESSAGE
	MSG *msg = (MSG*)lParam;
	// 垂直ホイール
	if (msg && msg->message == WM_MOUSEWHEEL) {
		int raw_delta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
		int delta = (raw_delta > 0) ? 1 : -1;
		s_Wheel += delta;
	}
	// 水平ホイール
	if (msg && msg->message == WM_MOUSEHWHEEL) {
		int raw_delta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
		int delta = (raw_delta > 0) ? 1 : -1;
		s_HorzWheel += delta;
	}
#else
	if (nCode >= 0) {
		if (wParam == WM_MOUSEWHEEL) {
			int raw_delta = GET_WHEEL_DELTA_WPARAM(wParam);
			int delta = (raw_delta > 0) ? 1 : -1;
			s_Wheel += delta;
		//	return TRUE;
		}
		if (wParam == WM_MOUSEHWHEEL) {
			int raw_delta = GET_WHEEL_DELTA_WPARAM(wParam);
			int delta = (raw_delta > 0) ? 1 : -1;
			s_HorzWheel += delta;
		//	return TRUE;
		}
	}
#endif
	return CallNextHookEx(s_Hook, nCode, wParam, lParam);
}
static void mouse_hook_start() {
	// ウィンドウが未指定の場合は、現在のスレッドのトップウィンドウを使う
	if (s_hWnd == NULL) {
		s_hWnd = GetActiveWindow();
	}
	HINSTANCE hInst = (HINSTANCE)GetWindowLong(s_hWnd, GWL_HINSTANCE);
#if USE_GETMESSAGE
	s_Hook = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)mouse_hook_cb, hInst, GetCurrentThreadId());
#else
	s_Hook = SetWindowsHookEx(WH_MOUSE, (HOOKPROC)mouse_hook_cb, hInst, GetCurrentThreadId());
#endif
}
static void mouse_hook_end() {
	if (s_Hook) {
		UnhookWindowsHookEx(s_Hook);
		s_Hook = NULL;
	}
}
#endif // _WIN32

void KMouse::setWheelWindow(void *hWnd) {
#ifdef _WIN32
	// 現在設定されているフックを解除する
	mouse_hook_end();

	// ウィンドウハンドルを設定する。
	// 実際にフックが引っ掛けられるのは、最初に問い合わせが発生した時になる
	s_hWnd = (HWND)hWnd;
#endif
}
void KMouse::getPosition(int *x, int *y) {
#ifdef _WIN32
	POINT p;
	GetCursorPos(&p);
	if (x) *x = p.x;
	if (y) *y = p.y;
#endif
}
bool KMouse::isButtonDown(KMouse::Button btn) {
#ifdef _WIN32
	switch (btn) {
	case LEFT:   return (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
	case RIGHT:  return (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
	case MIDDLE: return (GetKeyState(VK_MBUTTON) & 0x8000) != 0;
	}
#endif
	return false;
}
int KMouse::getWheel() {
#ifdef _WIN32
	if (s_Hook == NULL) {
		mouse_hook_start();
	}
#endif
	return s_Wheel;
}
int KMouse::getHorzWheel() {
#ifdef _WIN32
	if (s_Hook == NULL) {
		mouse_hook_start();
	}
#endif
	return s_HorzWheel;
}
const char * KMouse::getButtonName(Button btn) {
#ifdef _WIN32
	switch (btn) {
	case LEFT:   return s_Names[0];
	case RIGHT:  return s_Names[1];
	case MIDDLE: return s_Names[2];
	}
#endif
	return s_Names[3];
}
KMouse::Button KMouse::findButtonByName(const char *name) {
#ifdef _WIN32
	for (int i=0; i<ENUM_MAX; i++) {
		if (_stricmp(s_Names[i], name) == 0) {
			return (KMouse::Button)i;
		}
	}
#endif
	return INVALID;
}

}
