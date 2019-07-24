#include "KWindow.h"
#include "KConv.h"
#include "KCursor.h"
#include "KIcon.h"
#include "KMenu.h"
#include "KMouse.h"
#include "KStd.h"
#include <ctype.h>
#include <string>
#include <vector>


#ifdef _WIN32
#	include <Windows.h>
#	include <WindowsX.h> // GET_X_LPARAM
#endif

namespace mo {

#ifdef _WIN32

#define K_WND_CLASS_NAME       L"K_WND_CLASS_NAME"
#define K_WND_HOTKEY_SNAPSHOT_DESKTOP  1
#define K_WND_HOTKEY_SNAPSHOT_CLIENT   2

static void Win32_Full(HWND hWnd, bool val) {
	if (val) {
		SetMenu(hWnd, NULL);
		SetWindowLongW(hWnd, GWL_STYLE, WS_POPUP|WS_VISIBLE);
		MoveWindow(hWnd, GetSystemMetrics(SM_XVIRTUALSCREEN),
				GetSystemMetrics(SM_YVIRTUALSCREEN),
				GetSystemMetrics(SM_CXVIRTUALSCREEN),
				GetSystemMetrics(SM_CYVIRTUALSCREEN), TRUE);
	} else {
		SetWindowLongW(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		ShowWindow(hWnd, SW_RESTORE);
	}
}

static void Win32_SetFullScreen(HWND hWnd, LONG *oldStyle, LONG *oldStyleEx, RECT *oldRect) {
	K_assert(oldStyle && oldStyleEx && oldRect);
	GetWindowRect(hWnd, oldRect);
	*oldStyle = GetWindowLongW(hWnd, GWL_STYLE);
	*oldStyleEx = GetWindowLongW(hWnd, GWL_EXSTYLE);
	ShowWindow(hWnd, SW_MAXIMIZE);
	SetWindowLongW(hWnd, GWL_STYLE, WS_POPUP|WS_VISIBLE);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
}
static void Win32_SetWindowed(HWND hWnd, LONG style, LONG styleEx, const RECT *rect) {
	K_assert(rect);
	SetWindowLongW(hWnd, GWL_STYLE, style);
	SetWindowLongW(hWnd, GWL_EXSTYLE, styleEx);
	ShowWindow(hWnd, SW_RESTORE);
	SetWindowPos(hWnd, HWND_NOTOPMOST, rect->left, rect->top, rect->right-rect->left, rect->bottom-rect->top, SWP_NOSIZE);
}
static void Win32_ComputeWindowSize(HWND hWnd, int cw, int ch, int *req_w, int *req_h) {
	K_assert(cw >= 0);
	K_assert(ch >= 0);
	const BOOL has_menu = GetMenu(hWnd) != NULL;
	const DWORD style = GetWindowLongW(hWnd, GWL_STYLE);
	const DWORD exstyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
	RECT rect = {0, 0, cw, ch};
	AdjustWindowRectEx(&rect, style, has_menu, exstyle);
	if (req_w) *req_w = rect.right - rect.left;
	if (req_h) *req_h = rect.bottom - rect.top;
}
static BOOL Win32_HasWindowStyle(HWND hWnd, LONG style) {
	DWORD s = GetWindowLongA(hWnd, GWL_STYLE);
	return (s & style) != 0;
}
static BOOL Win32_HasWindowStyleEx(HWND hWnd, LONG style) {
	DWORD s = GetWindowLongA(hWnd, GWL_EXSTYLE);
	return (s & style) != 0;
}
static void Win32_SetWindowStyle(HWND hWnd, LONG s, BOOL enabled) {
	LONG style = GetWindowLongA(hWnd, GWL_STYLE);
	if (enabled) {
		style |= s;
	} else {
		style &= ~s;
	}
	SetWindowLongW(hWnd, GWL_STYLE, style);

	// ウィンドウスタイルの変更を反映させる
	SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
		SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOMOVE|
		SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);
}
static void Win32_SetWindowStyleEx(HWND hWnd, LONG s, BOOL enabled) {
	LONG style = GetWindowLongA(hWnd, GWL_EXSTYLE);
	if (enabled) {
		style |= s;
	} else {
		style &= ~s;
	}
	SetWindowLongW(hWnd, GWL_EXSTYLE, style);

	// ウィンドウスタイルの変更を反映させる
	SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
		SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOMOVE|
		SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);
}


struct SWnd {
	std::vector<IWindowCallback *> cblist;
	KCursor cursor;
	KMenu menu;
	HWND hWnd;
	LONG restore_style;
	LONG restore_style_ex;
	RECT restore_rect;
	int horz_wheel;
	int wheel;
	int mouse_last_x;
	int mouse_last_y;
	bool fullscreen;
	bool kill_alt_f10;
	bool kill_monitor_power_off;
	bool kill_screensaver;
	bool kill_snapshot;
	bool mouse_in_window;
	bool menu_opened;

	SWnd() {
		hWnd = NULL;
		restore_style = 0;
		restore_style_ex = 0;
		memset(&restore_rect, 0, sizeof(restore_rect));
		horz_wheel = 0;
		wheel = 0;
		fullscreen = false;
		kill_alt_f10 = false;
		kill_monitor_power_off = false;
		kill_screensaver = false;
		mouse_in_window = false;
		menu_opened = false;
		mouse_last_x = 0;
		mouse_last_y = 0;
		kill_snapshot = false;
	}

	void send(WindowEvent *e) {
		for (auto it=cblist.begin(); it!=cblist.end(); ++it) {
			IWindowCallback *cb = *it;
			cb->onWindowEvent(e);
		}
	}
};

static SWnd g_Wnd;

WindowEvent::WindowEvent() {
	memset(this, 0, sizeof(*this));
}


class CMenuCallback: public KMenuCallback {
public:
	virtual void on_menu_click(KMenu *menu, int index) override {
		WindowEvent e;
		e.type = WindowEvent::MENU_CLICK;
		e.menu_ev.menu = menu;
		e.menu_ev.index = index;
		e.menu_ev.command_id = menu->getItemCommandId(index);
		g_Wnd.send(&e);
	}
	virtual void on_menu_select(KMenu *menu, int index) override {
	}
	virtual void on_menu_open(KMenu *menu) override {
		WindowEvent e;
		e.type = WindowEvent::MENU_OPEN;
		e.menu_ev.menu = menu;
		e.menu_ev.menu_id = 0; /* ? */
		e.menu_ev.index = 0;
		e.menu_ev.command_id = 0;
		g_Wnd.send(&e);
	}
};
static CMenuCallback g_MenuCB;

#pragma region Window

static int _sign(int x) {
	if (x < 0) return -1;
	if (x > 0) return +1;
	return 0;
}

static KKeyboard::ModifierKeys _GetModifers() {
	KKeyboard::ModifierKeys m = 0;
	// 必ず GetKeyState で取得する。
	// GetAsyncKeyState はダメ。
	// 例えばビジー状態の時に Shift + A を押すと、それがキャッチされるのはビジーが解消されてからである。
	// WM_KEYDOWN をキャッチした時点で、既に Shift キーは押していない。
	// このとき GetAsyncKeyState で Shift 判定すると false になるが、
	// GetKeyState で判定すれば WM_KEYDOWN 送出時点での Shift キー状態が分かる
	//
	if ((GetKeyState(VK_SHIFT) & 0x8000) > 0) {
		m |= KKeyboard::M_SHIFT;
	}
	if ((GetKeyState(VK_CONTROL) & 0x8000) > 0) {
		m |= KKeyboard::M_CTRL;
	}
	if ((GetKeyState(VK_MENU) & 0x8000) > 0) {
		m |= KKeyboard::M_ALT;
	}
	return m;
}

static LRESULT CALLBACK _WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (1) {
		WindowEvent e;
		e.type = WindowEvent::WIN32MSG;
		e.win32msg_ev.hWnd = hWnd;
		e.win32msg_ev.msg = msg;
		e.win32msg_ev.wp = wp;
		e.win32msg_ev.lp = lp;
		e.win32msg_ev.lresult = 0;
		g_Wnd.send(&e);
		if (e.win32msg_ev.lresult) {
			return e.win32msg_ev.lresult;
		}
	}
	if (1) {
		g_Wnd.menu.processMessage(hWnd, msg, wp, lp);
	}

	switch (msg) {
	case WM_CREATE:
		DragAcceptFiles(hWnd, TRUE);
		return DefWindowProcW(hWnd, msg, wp, lp);

	case WM_DESTROY:
		PostQuitMessage(0);
		return DefWindowProcW(hWnd, msg, wp, lp);

	case WM_CLOSE:
		// ウィンドウを閉じようとしている
		// ここで return 0 すればウィンドウクローズを拒否できる
		{
			WindowEvent e;
			e.type = WindowEvent::WINDOW_CLOSING;
			e.window_ev.hWnd = hWnd;
			g_Wnd.send(&e);
		}
	//	if (e.dont_close) {
	//		return 0; // キャンセル
	//	}
		break;

	case WM_DROPFILES:
		// ファイルがドラッグドロップされた
		{
			HDROP hDrop = (HDROP)wp;
			UINT numFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
			for (UINT i=0; i<numFiles; i++) {
				wchar_t name[MAX_PATH] = {0};
				DragQueryFileW(hDrop, i, name, MAX_PATH);
				WindowEvent e;
				e.type = WindowEvent::DROPFILE;
				e.drop_ev.index = i;
				e.drop_ev.total = numFiles;
				e.drop_ev.filename = name;
				g_Wnd.send(&e);
			}
			DragFinish(hDrop);
			break;
		}
	case WM_MOVE:
		{
			WindowEvent e;
			e.type = WindowEvent::WINDOW_MOVE;
			e.window_ev.hWnd = hWnd;
			e.window_ev.x = (int)(short)LOWORD(lp);
			e.window_ev.y = (int)(short)HIWORD(lp);
			g_Wnd.send(&e);
		}
		break;

	case WM_SIZE:
		{
			WindowEvent e;
			e.type = WindowEvent::WINDOW_SIZE;
			e.window_ev.hWnd = hWnd;
			e.window_ev.w = (int)(short)LOWORD(lp);
			e.window_ev.h = (int)(short)HIWORD(lp);
			g_Wnd.send(&e);
		}
		break;

	case WM_ACTIVATE:
		if (wp == WA_INACTIVE) {
			WindowEvent e;
			e.type = WindowEvent::WINDOW_INACTIVE;
			g_Wnd.send(&e);
		} else {
			WindowEvent e;
			e.type = WindowEvent::WINDOW_ACTIVE;
			g_Wnd.send(&e);
		}
		break;

	case WM_SETCURSOR:
		#if 0
		if (LOWORD(lp) == HTCLIENT) {
			SetCursor((HCURSOR)g_Wnd.cursor.getHandle()); // 再設定
			return TRUE;
		}
		#endif
		break;

	case WM_MOUSEWHEEL:
		{
			// マウスホイールが回転した
			// ノッチの量は関係なく、符号だけ見る
			int delta = GET_WHEEL_DELTA_WPARAM(wp);
			g_Wnd.wheel += _sign(delta);

			// lp に入っているのはスクリーン座標
			int scr_x = GET_X_LPARAM(lp);
			int scr_y = GET_Y_LPARAM(lp);

			POINT cli = {scr_x, scr_y};
			ScreenToClient(g_Wnd.hWnd, &cli);
			{
				WindowEvent e;
				e.type = WindowEvent::MOUSE_WHEEL;
				e.mouse_ev.wheel_delta = (delta>=0) ? 1 : -1;
				e.mouse_ev.abs_x = scr_x;
				e.mouse_ev.abs_y = scr_y;
				e.mouse_ev.cli_x = cli.x;
				e.mouse_ev.cli_y = cli.y;
				e.mouse_ev.mod_flags = _GetModifers();
				g_Wnd.send(&e);
			}
			break;
		}
	case WM_MOUSEHWHEEL:
		{
			// 水平マウスホイールが回転した
			// ノッチの量は関係なく、符号だけ見る
			int delta = GET_WHEEL_DELTA_WPARAM(wp);
			g_Wnd.horz_wheel += _sign(delta);

			// lp に入っているのはスクリーン座標
			int scr_x = GET_X_LPARAM(lp);
			int scr_y = GET_Y_LPARAM(lp);

			POINT cli = {scr_x, scr_y};
			ScreenToClient(g_Wnd.hWnd, &cli);
			{
				WindowEvent e;
				e.type = WindowEvent::MOUSE_HORZ_WHEEL;
				e.mouse_ev.wheel_delta = (delta>=0) ? 1 : -1;
				e.mouse_ev.abs_x = scr_x;
				e.mouse_ev.abs_y = scr_y;
				e.mouse_ev.cli_x = cli.x;
				e.mouse_ev.cli_y = cli.y;
				e.mouse_ev.mod_flags = _GetModifers();
				g_Wnd.send(&e);
			}
			break;
		}
	case WM_MOUSEMOVE:
		{
			// マウスポインタが移動した

			// lp に入っているのはクライアント座標
			int cli_x = GET_X_LPARAM(lp);
			int cli_y = GET_Y_LPARAM(lp);

			POINT scr;
			GetCursorPos(&scr);

			// マウスポインタ侵入イベントがまだ発生していないなら、発生させる
			if (!g_Wnd.mouse_in_window) {
				g_Wnd.mouse_in_window = true;
				g_Wnd.mouse_last_x = scr.x;
				g_Wnd.mouse_last_y = scr.y;

				// 次回マウスカーソルが離れたときに WM_MOUSELEAVE が発生するようにする。
				// この設定はイベントが発生するたびにリセットされるため、毎回設定しなおす必要がある
				TRACKMOUSEEVENT e;
				e.cbSize = sizeof(e);
				e.hwndTrack = hWnd;
				e.dwFlags = TME_LEAVE;
				e.dwHoverTime = 0;
				TrackMouseEvent(&e);

				// マウスカーソル侵入イベントを発生
				{
					WindowEvent e;
					e.type = WindowEvent::MOUSE_ENTER;
					e.mouse_ev.abs_x = scr.x;
					e.mouse_ev.abs_y = scr.y;
					e.mouse_ev.cli_x = cli_x;
					e.mouse_ev.cli_y = cli_y;
					g_Wnd.send(&e);
				}
			}
			// マウス移動イベント
			{
				// マウス移動量の計算は必ずスクリーン座標系で行う
				// マウスドラッグでウィンドウを移動している最中などに移動量が計算できなくなる
				int delta_x = scr.x - g_Wnd.mouse_last_x;
				int delta_y = scr.y - g_Wnd.mouse_last_y;
				{
					WindowEvent e;
					e.type = WindowEvent::MOUSE_MOVE;
					e.mouse_ev.abs_x = scr.x;
					e.mouse_ev.abs_y = scr.y;
					e.mouse_ev.cli_x = cli_x;
					e.mouse_ev.cli_y = cli_y;
					e.mouse_ev.delta_x = delta_x;
					e.mouse_ev.delta_y = delta_y;
					e.mouse_ev.button = (wp & MK_LBUTTON) ? 1 : (wp==MK_RBUTTON) ? 2 : 3;
					e.mouse_ev.mod_flags = _GetModifers();
					g_Wnd.send(&e);
				}
				g_Wnd.mouse_last_x = scr.x;
				g_Wnd.mouse_last_y = scr.y;
			}
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
		{
			// lp に入っているのはクライアント座標
			int cli_x = GET_X_LPARAM(lp);
			int cli_y = GET_Y_LPARAM(lp);
			POINT scr = {0, 0};
			GetCursorPos(&scr);
			WindowEvent e;
			e.type = WindowEvent::MOUSE_DOWN;
			e.mouse_ev.abs_x = scr.x;
			e.mouse_ev.abs_y = scr.y;
			e.mouse_ev.cli_x = cli_x;
			e.mouse_ev.cli_y = cli_y;
			e.mouse_ev.button = (msg==WM_LBUTTONDOWN) ? 1 : (msg==WM_RBUTTONDOWN) ? 2 : 3;
			e.mouse_ev.mod_flags = _GetModifers();
			g_Wnd.send(&e);
			break;
		}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
		{
			// lp に入っているのはクライアント座標
			int cli_x = GET_X_LPARAM(lp);
			int cli_y = GET_Y_LPARAM(lp);
			POINT scr = {0, 0};
			GetCursorPos(&scr);
			WindowEvent e;
			e.type = WindowEvent::MOUSE_UP;
			e.mouse_ev.abs_x = scr.x;
			e.mouse_ev.abs_y = scr.y;
			e.mouse_ev.cli_x = cli_x;
			e.mouse_ev.cli_y = cli_y;
			e.mouse_ev.button = (msg==WM_LBUTTONUP) ? 1 : (msg==WM_RBUTTONUP) ? 2 : 3;
			e.mouse_ev.mod_flags = _GetModifers();
			g_Wnd.send(&e);
			break;
		}
	case WM_MOUSELEAVE:
		// マウスポインタがウィンドウから離れた。
		// 再び WM_MOUSELEAVE を発生させるには、
		// TrackMouseEvent() で再設定する必要があることに注意
		g_Wnd.mouse_in_window = false;
		{
			POINT scr = {0, 0};
			GetCursorPos(&scr);
			POINT cli = scr;
			ScreenToClient(g_Wnd.hWnd, &cli);
			WindowEvent e;
			e.type = WindowEvent::MOUSE_EXIT;
			e.mouse_ev.abs_x = scr.x;
			e.mouse_ev.abs_y = scr.y;
			e.mouse_ev.cli_x = cli.x;
			e.mouse_ev.cli_y = cli.y;
			g_Wnd.send(&e);
		}
		break;

	case WM_HOTKEY:
		if (wp == K_WND_HOTKEY_SNAPSHOT_DESKTOP) {
			// [PrtScr] キーの WM_KEY_DOWN の代替処理
			{
				WindowEvent e;
				e.type = WindowEvent::KEY_DOWN;
				e.key_ev.key = KKeyboard::SNAPSHOT;
				e.key_ev.mod_flags = _GetModifers();
				g_Wnd.send(&e);
			}
		}
		if (wp == K_WND_HOTKEY_SNAPSHOT_CLIENT) {
			// [Alt + PrtScr] キーの WM_KEY_DOWN の代替処理
			{
				WindowEvent e;
				e.type = WindowEvent::KEY_DOWN;
				e.key_ev.key = KKeyboard::SNAPSHOT;
				e.key_ev.mod_flags = _GetModifers();
				g_Wnd.send(&e);
			}
		}
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		// PrtScr キーは WM_KEYDOWN を送出しないことに注意。代わりに WM_HOTKEY で捕まえている
		{
			WindowEvent e;
			e.type = WindowEvent::KEY_DOWN;
			e.key_ev.key = KKeyboard::getKeyFromVirtualKey(wp);
			e.key_ev.mod_flags = _GetModifers();
			g_Wnd.send(&e);
		}

		// Alt, F10 による一時停止を無効化する。
		// ゲーム中に ALT または F10 を押すと一時停止状態になってしまう。
		// 意図しない一時停止をさせたくない場合に使う
		if (g_Wnd.kill_alt_f10) {
			if (wp == VK_F10 || wp == VK_MENU) {
				return 1; 
			}
		}
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		{
			WindowEvent e;
			e.type = WindowEvent::KEY_UP;
			e.key_ev.key = KKeyboard::getKeyFromVirtualKey(wp);
			e.key_ev.mod_flags = _GetModifers();
			g_Wnd.send(&e);
		}
		break;

	case WM_CHAR:
		{
			WindowEvent e;
			e.type = WindowEvent::KEY_CHAR;
			e.key_ev.chr = (wchar_t)wp;
			g_Wnd.send(&e);
		}
		break;

	case WM_SYSCOMMAND:
		// モニターのオートパワーオフを無効化
		if (g_Wnd.kill_monitor_power_off) {
			if (wp == SC_MONITORPOWER) {
				return 1;
			}
		}
		// スクリーンセーバーの起動を抑制する。
		// ゲームパッドでゲームしていると、キーボードもマウスも操作しない時間が続くために
		// スクリーンセーバーが起動してしまう事がある。それを阻止する
		if (g_Wnd.kill_screensaver) {
			if (wp == SC_SCREENSAVE) {
				return 1;
			}
		}
		break;
	}
	return DefWindowProcW(hWnd, msg, wp, lp);
}


Window::Window() {
}
KMenu * Window::getMenu() {
	return &g_Wnd.menu;
}
bool Window::create(int w, int h, const char *title_u8) {
	g_Wnd.wheel = 0;
	g_Wnd.horz_wheel = 0;
	g_Wnd.mouse_in_window = false;
	g_Wnd.hWnd = NULL;

	WNDCLASSEXW wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEXW));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpszClassName = K_WND_CLASS_NAME;
	wc.lpfnWndProc = _WndProc;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClassExW(&wc);
	g_Wnd.hWnd = CreateWindowExW(0, K_WND_CLASS_NAME, NULL, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, NULL, NULL);
	if (g_Wnd.hWnd == NULL) {
		return false;
	}

	setClientSize(w, h);
	setTitle(title_u8);
	setWindowVisible(true);
	g_Wnd.menu.createSystem(g_Wnd.hWnd);
	g_Wnd.menu.setCallback(&g_MenuCB);
	return true;
}
void Window::destroy() {
	// 処理の一貫性を保つため必ず WM_CLOSE を発行する
	if (IsWindowVisible(g_Wnd.hWnd)) {
		// 注意： CloseWindow(hWnd) を使うと WM_CLOSE が来ないまま終了する。
		// そのためか、CloseWindow でウィンドウを閉じたときは、閉じた後にも WM_MOVE などが来てしまう。
		// WM_CLOSE で終了処理をしていたり、WM_MOVE のタイミングでウィンドウ位置を保存とかやっているとおかしな事になる。
		// トラブルを避けるために、明示的に SendMessage(WM_CLOSE, ...) でメッセージを流すこと。
		SendMessageW(g_Wnd.hWnd, WM_CLOSE, 0, 0);
	}
	DestroyWindow(g_Wnd.hWnd);
	g_Wnd.hWnd = NULL;
}
void Window::addCallback(IWindowCallback *cb) {
	g_Wnd.cblist.push_back(cb);
}
void Window::removeCallback(IWindowCallback *cb) {
	auto it = std::find(g_Wnd.cblist.begin(), g_Wnd.cblist.end(), cb);
	if (it != g_Wnd.cblist.end()) {
		g_Wnd.cblist.erase(it);
	}
}
bool Window::isWindowVisible() const {
	return IsWindowVisible(g_Wnd.hWnd) != 0;
}
void Window::closeWindow() {
	destroy();
}
void * Window::getHandle() const {
	return g_Wnd.hWnd;
}
void Window::getClientSize(int *cw, int *ch) const {
	RECT rc = {0, 0, 0, 0};
	GetClientRect(g_Wnd.hWnd, &rc); // GetClientRect が失敗する可能性に注意
	if (cw) *cw = rc.right - rc.left;
	if (ch) *ch = rc.bottom - rc.top;
}
void Window::getWindowPosition(int *x, int *y) const {
	RECT rect;
	GetWindowRect(g_Wnd.hWnd, &rect);
	if (x) *x = rect.left;
	if (y) *y = rect.top;
}
void Window::screenToClient(int *x, int *y) const {
	POINT p = {*x, *y};
	ScreenToClient(g_Wnd.hWnd, &p);
	if (x) *x = p.x;
	if (y) *y = p.y;
}
void Window::clientToScreen(int *x, int *y) const {
	POINT p = {*x, *y};
	ClientToScreen(g_Wnd.hWnd, &p);
	if (x) *x = p.x;
	if (y) *y = p.y;
}
bool Window::isFullscreen() const {
	return g_Wnd.fullscreen;
}
bool Window::isMaximized() const {
	return IsZoomed(g_Wnd.hWnd) != 0;
}
bool Window::isIconified() const {
	return IsIconic(g_Wnd.hWnd) != 0;
}
bool Window::isWindowFocused() const {
	return GetForegroundWindow() == g_Wnd.hWnd;
}
void Window::fullscreenWindow() {
	// フルスクリーン用のウィンドウ設定に切り替える。
	// 画面をフルスクリーンモードにしてからウィンドウスタイルを適用すること
	Win32_SetFullScreen(g_Wnd.hWnd, &g_Wnd.restore_style, &g_Wnd.restore_style_ex, &g_Wnd.restore_rect);
	g_Wnd.fullscreen = true;
}
void Window::maximizeWindow() {
	ShowWindow(g_Wnd.hWnd, SW_MAXIMIZE);
}
void Window::iconifyWindow() {
	ShowWindow(g_Wnd.hWnd, SW_MINIMIZE);
}
void Window::restoreWindow() {
	if (isFullscreen()) {
		// ウィンドウモードに切り替える
		// フルスクリーン画面を解除してからウィンドウスタイルを適用すること。
		// http://maverickproj.web.fc2.com/pg03.html
		Win32_SetWindowed(g_Wnd.hWnd, g_Wnd.restore_style, g_Wnd.restore_style_ex, &g_Wnd.restore_rect);
		g_Wnd.fullscreen = false;
	}
	ShowWindow(g_Wnd.hWnd, SW_RESTORE);
}
void Window::setWindowVisible(bool value) {
	ShowWindow(g_Wnd.hWnd, value ? SW_SHOW : SW_HIDE);
}
void Window::setWindowPosition(int x, int y) {
	SetWindowPos(g_Wnd.hWnd, NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
}
void Window::setClientSize(int cw, int ch) {
	if (cw < 0) cw = 0;
	if (ch < 0) ch = 0;
	int ww, hh;
	Win32_ComputeWindowSize(g_Wnd.hWnd, cw, ch, &ww, &hh);
	SetWindowPos(g_Wnd.hWnd, NULL, 0, 0, ww, hh, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE);
}
void Window::setTitle(const char *text_u8) {
	if (text_u8) {
		SetWindowTextW(g_Wnd.hWnd, KConv::utf8ToWide(text_u8).c_str());
	}
}
void Window::setIcon(const KIcon &icon) {
	HICON hIcon = (HICON)icon.getHandle();
	SendMessageW(g_Wnd.hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	SendMessageW(g_Wnd.hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
}
void Window::setMouseCursorVisible(bool visible) {
	if (visible) {
		SetCursor((HCURSOR)g_Wnd.cursor.getHandle());
	} else {
		SetCursor(NULL);
	}
}
void Window::setMouseCursor(const KCursor &cursor) {
	g_Wnd.cursor = cursor;
}
bool Window::processEvents() {
	MSG msg;
	if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) return false; // ループ中断
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return true; // ループ継続を許可
}
void Window::getMousePosition(int *x, int *y, bool in_screen) {
	K_assert(x && y);
	POINT p = {0, 0};
	if (in_screen) {
		GetCursorPos(&p);
	} else {
		GetCursorPos(&p);
		ScreenToClient(g_Wnd.hWnd, &p);
	}
	*x = p.x;
	*y = p.y;
}
bool Window::isMouseButtonDown(int btn) {
	if (isWindowFocused()) {
		return KMouse::isButtonDown((KMouse::Button)btn);
	}
	return false;
}
int Window::getMouseWheel() {
	if (isWindowFocused()) {
		return KMouse::getWheel();
	}
	return false;
}
bool Window::isMouseInWindow() {
	return g_Wnd.mouse_in_window;
}
bool Window::isKeyDown(KKeyboard::Key key) {
	if (isWindowFocused()) {
		return KKeyboard::isKeyDown(key);
	}
	return false;
}
int Window::getAttribute(Attribute attr) {
	switch (attr) {
	case HAS_BORDER:          return Win32_HasWindowStyle(g_Wnd.hWnd, WS_CAPTION) && Win32_HasWindowStyle(g_Wnd.hWnd, WS_THICKFRAME);
	case HAS_TITLE:           return Win32_HasWindowStyle(g_Wnd.hWnd, WS_CAPTION);
	case HAS_MAXIMIZE_BUTTON: return Win32_HasWindowStyle(g_Wnd.hWnd, WS_MAXIMIZEBOX);
	case HAS_ICONIFY_BUTTON:  return Win32_HasWindowStyle(g_Wnd.hWnd, WS_MINIMIZEBOX);
	case HAS_CLOSE_BUTTON:    return Win32_HasWindowStyle(g_Wnd.hWnd, WS_SYSMENU);
	case RESIZABLE:           return Win32_HasWindowStyle(g_Wnd.hWnd, WS_THICKFRAME);
	case TOPMOST:             return Win32_HasWindowStyleEx(g_Wnd.hWnd, WS_EX_TOPMOST);
	case KILL_ALT_F10:        return g_Wnd.kill_alt_f10;
	case KILL_SNAPSHOT:       return g_Wnd.kill_snapshot;
	}
	return 0;
}
void Window::setAttribute(Attribute attr, int value) {
	switch (attr) {
	case HAS_BORDER:
		// 境界線の有無は WS_THICKFRAME と WS_CAPTION の値で決まる
		Win32_SetWindowStyle(g_Wnd.hWnd, WS_THICKFRAME, value);
		Win32_SetWindowStyle(g_Wnd.hWnd, WS_CAPTION, value);
		break;

	case HAS_TITLE:
		Win32_SetWindowStyle(g_Wnd.hWnd, WS_CAPTION, value);
		break;

	case RESIZABLE:
		Win32_SetWindowStyle(g_Wnd.hWnd, WS_THICKFRAME, value);
		break;

	case HAS_MAXIMIZE_BUTTON:
		Win32_SetWindowStyle(g_Wnd.hWnd, WS_MAXIMIZEBOX, value);
		break;

	case HAS_ICONIFY_BUTTON:
		Win32_SetWindowStyle(g_Wnd.hWnd, WS_MINIMIZEBOX, value);
		break;

	case HAS_CLOSE_BUTTON:
		Win32_SetWindowStyle(g_Wnd.hWnd, WS_SYSMENU, value);
		break;
	
	case TOPMOST:
		Win32_SetWindowStyleEx(g_Wnd.hWnd, WS_EX_TOPMOST, value);
		SetWindowPos(
			g_Wnd.hWnd, 
			value ? HWND_TOPMOST : HWND_NOTOPMOST,
			0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
		break;

	case FULLFILL:
		Win32_Full(g_Wnd.hWnd, value);
		break;

	case KILL_ALT_F10:
		g_Wnd.kill_alt_f10 = (value != 0);
		break;

	case KILL_SNAPSHOT:
		if (value) {
			g_Wnd.kill_snapshot = true;
			// PrtScr キーは WM_KEYDOWN を送出しない(WM_KEYUPは来る）
			// PtrScr をホットキーとして登録して、WM_HOTKEY 経由で PrtScr の押下イベントを受け取れるようにする
			// https://gamedev.stackexchange.com/questions/20446/does-vk-snapshot-not-send-a-wm-keydown-only-wm-keyup
			RegisterHotKey(g_Wnd.hWnd, K_WND_HOTKEY_SNAPSHOT_DESKTOP, 0, VK_SNAPSHOT);
			RegisterHotKey(g_Wnd.hWnd, K_WND_HOTKEY_SNAPSHOT_CLIENT,  MOD_ALT, VK_SNAPSHOT);
		} else {
			g_Wnd.kill_snapshot = false;
			UnregisterHotKey(g_Wnd.hWnd, K_WND_HOTKEY_SNAPSHOT_DESKTOP);
			UnregisterHotKey(g_Wnd.hWnd, K_WND_HOTKEY_SNAPSHOT_CLIENT);
		}
		break;
	}
}
#pragma endregion // Window


#else // _WIn32

Window::Window() {}
bool Window::create(int w, int h, const char *text_u8) { return false; }
void Window::destroy() {}
void Window::closeWindow() {}
void * Window::getHandle() const { return NULL; }
void Window::getClientSize(int *cw, int *ch) const {}
void Window::getWindowPosition(int *x, int *y) const {}
void Window::screenToClient(int *x, int *y) const {}
void Window::clientToScreen(int *x, int *y) const {}
bool Window::isWindowVisible() const { return false; }
bool Window::isWindowFocused() const { return false; }
bool Window::isIconified() const { return false; }
bool Window::isMaximized() const { return false; }
bool Window::isFullscreen() const { return false; }
void Window::fullscreenWindow() {}
void Window::maximizeWindow() {}
void Window::iconifyWindow() {}
void Window::restoreWindow() {}
void Window::setWindowVisible(bool value) {}
void Window::setWindowPosition(int x, int y) {}
void Window::setClientSize(int cw, int ch) {}
void Window::setTitle(const char *text_u8) {}
void Window::setIcon(const KIcon &icon) {}
void Window::setMouseCursorVisible(bool visible) {}
void Window::setMouseCursor(const KCursor &cursor) {}
bool Window::processEvents() { return false; }
void Window::getMousePosition(int *x, int *y, bool in_screen) {}
bool Window::isMouseButtonDown(int btn) { return false; }
int Window::getMouseWheel() { return 0; }
bool Window::isMouseInWindow() { return false; }
bool Window::isKeyDown(KKeyboard::Key key) { return false; }
int Window::getAttribute(Attribute attr) { return 0; }
void Window::setAttribute(Attribute attr, int value) {}
KMenu * Window::getMenu() { return NULL; }


#endif // _WIN32


} // namespace
