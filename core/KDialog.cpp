#include "KDialog.h"

#ifdef _WIN32
#define USE_COMMONCTRL 1 // コモンコントロールを使う？

#include "KConv.h"
#include "KStd.h"
#include <windows.h>
#include <unordered_map>

#if USE_COMMONCTRL
#include <commctrl.h> // LINK: comctl32.lib
#endif

#define ZFLAG_SEPARATOR 1
#define ZFLAG_EXPAND_X  2

// そもそも TCHAR を全く使っていないため、UNICODE マクロが定義されているかどうかに関係になく、
// 常にワイド文字版を使いたい。そうでないと勝手に char と wchar_t が切り替わってしまう可能性がある。
// そのため、関数マクロを使わずにわざわざ末尾に W が付いているものを使っているわけだが、
// IDC_ARROW は WinUser.h 内で
//
// #define IDC_ARROW           MAKEINTRESOURCE(32512)
//
// と定義されていて、その MAKEINTRESOURCE は
//
// #ifdef UNICODE
// #define MAKEINTRESOURCE  MAKEINTRESOURCEW
// #else
// #define MAKEINTRESOURCE  MAKEINTRESOURCEA
//
// と定義されているため、
// IDC_ARROW の Ansi版 IDC_ARROWA や Wide版の IDC_ARROWW は初めから存在しない。
// 
// そこで、UNICODE の設定とは無関係に常にワイド文字を使うように、自前でWide版の IDC_ARROW を定義しておく
//
#define K_IDC_ARROWW   MAKEINTRESOURCEW(32512)


#define _MAX(a, b) ((a) > (b) ? (a) : (b))

namespace mo {

static void _MoveToCenterInParent(HWND hWnd, HWND hParent) {
	if (hParent == NULL) hParent = GetDesktopWindow();
	RECT parentRect;
	RECT wndRect;
	GetWindowRect(hParent, &parentRect);
	GetWindowRect(hWnd, &wndRect);
	int x = (parentRect.left + parentRect.right ) / 2 - (wndRect.right + wndRect.left) / 2;
	int y = (parentRect.top  + parentRect.bottom) / 2 - (wndRect.bottom + wndRect.top) / 2;
	SetWindowPos(hWnd, NULL, x, y, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
}
static void _ComputeTextSize(HWND hWnd, const wchar_t *text, int *w, int *h) {
	RECT rect = {0, 0, 0, 0};
	HDC hDC = GetDC(hWnd);
	SelectObject(hDC, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
	DrawTextW(hDC, text, -1, &rect, DT_CALCRECT);
	ReleaseDC(hWnd, hDC);
	K_assert(w);
	K_assert(h);
	*w = rect.right - rect.left;
	*h = rect.bottom - rect.top;
}
static void _ComputeTextSize(HWND hWnd, int *w, int *h) {
	static const int SIZE = 1024;
	wchar_t text[SIZE] = {0};
	GetWindowTextW(hWnd, text, SIZE);
	_ComputeTextSize(hWnd, text, w, h);
}
static void _ConvertLineCode(std::wstring &s) {
	for (size_t i=0; i<s.size(); i++) {
		if (s[i]=='\r' && s[i+1]=='\n') { i++; break; }
		if (s[i]=='\n') { s.insert(i, 1, '\r'); i++; }
	}
}

// EnumResourceNames のコールバック用。最初に見つかった HICON を得る
static BOOL CALLBACK icon_cb(HINSTANCE instance, LPCTSTR type, LPTSTR name, void *data) {
	HICON icon = LoadIcon(instance, name);
	if (icon) {
		*(HICON*)data = icon; 
		return FALSE;
	}
	return TRUE; // FIND NEXT
}

LRESULT CALLBACK KDialog::_wndproc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	KDialog *dialog;
	zid id;
	switch (msg) {
	case WM_CREATE:
		DragAcceptFiles(hWnd, TRUE);
		break;
	case WM_CLOSE:
		dialog = reinterpret_cast<KDialog *>(GetWindowLongW(hWnd, GWL_USERDATA));
		dialog->on_WM_CLOSE();
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_COMMAND:
		id = (zid)LOWORD(wp);
		dialog = reinterpret_cast<KDialog *>(GetWindowLongW(hWnd, GWL_USERDATA));
		dialog->on_WM_COMMAND(id);
		break;
	}
	return DefWindowProcW(hWnd, msg, wp, lp);
}
KDialog::KDialog() {
	clear();
}
KDialog::~KDialog() {
	close();
}
void KDialog::create(const char *text_u8) {
	create(0, 0, text_u8);
}
void KDialog::create(int w, int h, const char *text_u8) {
	clear();

#if USE_COMMONCTRL
	InitCommonControls();
#endif

	// 現在アクティブなウィンドウを親とする
	hParent_ = GetActiveWindow();
	hInstance_ = (HINSTANCE)GetModuleHandle(NULL);

	// 最初に見つかったアイコン（＝アプリケーションのアイコンと同じ）をウィンドウのアイコンとして取得する
	HICON hIcon = NULL; // LoadIconW(NULL, IDI_ERROR);
	EnumResourceNames(hInstance_, RT_GROUP_ICON, (ENUMRESNAMEPROC)icon_cb, (LONG_PTR)&hIcon);

	// ウィンドウクラスの登録
	WNDCLASSEXW wc;
	{
		ZeroMemory(&wc, sizeof(WNDCLASSEXW));
		wc.cbSize        = sizeof(WNDCLASSEXW);
		wc.lpszClassName = L"ZIALOG";
		wc.lpfnWndProc   = _wndproc;
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE +1);
		wc.hIcon         = hIcon;
		wc.hCursor       = LoadCursorW(NULL, K_IDC_ARROWW);
		RegisterClassExW(&wc);
	}

	// ウィンドウ作成
	std::wstring ws = KConv::utf8ToWide(text_u8);
	hWnd_ = CreateWindowExW(0, wc.lpszClassName, ws.c_str(), WS_POPUPWINDOW|WS_CAPTION, CW_USEDEFAULT, CW_USEDEFAULT, w, h, hParent_, NULL, NULL, NULL);

	// ウィンドウメッセージハンドラから this を参照できるように、ユーザーデータとしてウィンドウと this を関連付けておく
	SetWindowLongW(hWnd_, GWL_USERDATA, (LONG)this);

	cb_ = NULL;
	auto_id_ = 0;
	cur_.margin_x = 16;
	cur_.margin_y = 16;
	cur_.padding_x = 8;
	cur_.padding_y = 6;
	cur_.curr_line_h = 0;
	cur_.next_x = cur_.margin_x;
	cur_.next_y = cur_.margin_y;
	cur_.next_w = 120;
	cur_.next_h = 12;
	cur_.line_min_h = 8;
	cur_.auto_sizing = true;
}
void KDialog::clear() {
	hInstance_ = NULL;
	hWnd_ = NULL;
	hParent_ = NULL;
	cb_ = NULL;
	ZeroMemory(&cur_, sizeof(cur_));
	auto_id_ = 0;
}
void KDialog::close() {
	// 処理の一貫性を保つため必ず WM_CLOSE を発行する
	// 注意： CloseWindow(hWnd) を使うと WM_CLOSE が来ないまま終了する。
	// そのためか、CloseWindow でウィンドウを閉じたときは、閉じた後にも WM_MOVE などが来てしまう。
	// WM_CLOSE で終了処理をしていたり、WM_MOVE のタイミングでウィンドウ位置を保存とかやっているとおかしな事になる。
	// トラブルを避けるために、明示的に SendMessage(WM_CLOSE, ...) でメッセージを流す。
	SendMessageW(hWnd_, WM_CLOSE, 0, 0);
	DestroyWindow(hWnd_);
	clear(); // ここで hWnd_ がクリアされるため、ウィンドウ破棄関連のメッセージを _wndproc で処理できなくなる可能性に注意
}
void KDialog::getLayoutCursor(KDialogLayoutCursor *cur) const {
	*cur = cur_;
}
void KDialog::setLayoutCursor(const KDialogLayoutCursor *cur) {
	cur_ = *cur;
}
zid KDialog::addLabel(zid id, const char *text_u8) {
	std::wstring ws = KConv::utf8ToWide(text_u8);
	HWND hItem = CreateWindowExW(0, L"STATIC", ws.c_str(),
		WS_CHILD|WS_VISIBLE,
		cur_.next_x, cur_.next_y, cur_.next_w, cur_.next_h,
		hWnd_, 0, hInstance_, NULL);
	SendMessage(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
	if (cur_.auto_sizing) setBestItemSize(hItem);
	moveCursorToNext(hItem);
	return id;
}
zid KDialog::addEdit(zid id, const char *text_u8) {
	std::wstring ws = KConv::utf8ToWide(text_u8);
	HWND hItem = CreateWindowExW(0, L"EDIT", ws.c_str(),
		WS_CHILD|WS_VISIBLE|WS_BORDER|ES_LEFT,
		cur_.next_x, cur_.next_y, cur_.next_w, cur_.next_h,
		hWnd_, NULL, hInstance_, NULL);
	SendMessage(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
	if (cur_.auto_sizing) setBestItemSize(hItem);
	moveCursorToNext(hItem);
	return id;
}
zid KDialog::addMemo(zid id, const char *text_u8, bool wrap, bool readonly) {
	std::wstring ws = KConv::utf8ToWide(text_u8);
	_ConvertLineCode(ws); // "\n" --> "\r\n"
	DWORD rw = wrap ? 0 : ES_AUTOHSCROLL;
	DWORD ro = readonly ? ES_READONLY : 0;
	HWND hItem = CreateWindowExW(0, L"EDIT", ws.c_str(),
		WS_CHILD|WS_VISIBLE|WS_BORDER|WS_HSCROLL|WS_VSCROLL|
		ES_AUTOVSCROLL|ES_LEFT|ES_MULTILINE|ES_NOHIDESEL|rw|ro,
		cur_.next_x, cur_.next_y, cur_.next_w, cur_.next_h,
		hWnd_, NULL, hInstance_, NULL);
	SendMessage(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
	if (cur_.auto_sizing) setBestItemSize(hItem);
	moveCursorToNext(hItem);
	return id;
}
zid KDialog::addCheckBox(zid id, const char *text_u8, bool value) {
	std::wstring ws = KConv::utf8ToWide(text_u8);
	HWND hItem = CreateWindowExW(0, L"BUTTON", ws.c_str(),
		WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX|BS_AUTOCHECKBOX,
		cur_.next_x, cur_.next_y, cur_.next_w, cur_.next_h,
		hWnd_, (HMENU)id, hInstance_, NULL);
	SendMessage(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
	if (cur_.auto_sizing) setBestItemSize(hItem, 2, 0);
	moveCursorToNext(hItem);
	return id;
}
zid KDialog::addButton(zid id, const char *text_u8) {
	std::wstring ws = KConv::utf8ToWide(text_u8);
	HWND hItem = CreateWindowExW(0, L"BUTTON", ws.c_str(),
		WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
		cur_.next_x, cur_.next_y, cur_.next_w, cur_.next_h,
		hWnd_, (HMENU)id, hInstance_, NULL);
	SendMessage(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
	if (cur_.auto_sizing) setBestItemSize(hItem);
	moveCursorToNext(hItem);
	return id;
}
void _SetToolTip(HWND hItem, const char *text_u8) {
#if USE_COMMONCTRL
	std::wstring ws = KConv::utf8ToWide(text_u8);
	HWND hTool = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL,
		TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		hItem, NULL, (HINSTANCE)GetWindowLong(hItem, GWL_HINSTANCE), NULL);
	TTTOOLINFOW ti;
	ZeroMemory(&ti, sizeof(ti));
	ti.cbSize = sizeof(ti);
	ti.uFlags = TTF_SUBCLASS;
	ti.hwnd = hItem;
	ti.lpszText = const_cast<wchar_t*>(ws.c_str());
	GetClientRect(hItem, &ti.rect);
	BOOL ok = SendMessage(hTool, TTM_ADDTOOL, 0, (LPARAM)&ti);
	if (!ok) {
		// ここで失敗した場合は以下の原因を疑う
		//
		// ツールチップ と コモンコントロール のバージョン差異による落とし穴
		// https://ameblo.jp/blueskyame/entry-10398978729.html
		//
		// Adding tooltip controls in unicode fails
		// https://social.msdn.microsoft.com/Forums/vstudio/en-US/5cc9a772-5174-4180-a1ca-173dc81886d9/adding-tooltip-controls-in-unicode-fails?forum=vclanguage
		//
		ti.cbSize = sizeof(ti) - sizeof(void*); // <== ここで小細工
		SendMessage(hTool, TTM_ADDTOOL, 0, (LPARAM)&ti);
	}
	SendMessage(hTool, TTM_SETMAXTIPWIDTH, 0, 640); // 複数行に対応させる。指定幅または改行文字で改行されるようになる
	//	リサイズなどで領域が変化した場合は領域を更新すること
	//	GetClientRect(hWnd , &ti.rect);
	//	SendMessage(hTool , TTM_NEWTOOLRECT , 0 , (LPARAM)&ti);
#endif
}
HWND KDialog::getLastItem() {
	// 最後に作成したコントロールを得る。
	// hWnd_ の子要素のうち、Zオーダーが一番最後になっているものを得る
	return GetWindow(GetWindow(hWnd_, GW_CHILD), GW_HWNDLAST);
}
void KDialog::setFocus() {
	SetFocus(getLastItem());
}
void KDialog::setHint(const char *hint_u8) {
	_SetToolTip(getLastItem(), hint_u8);
}
void KDialog::setExpand() {
	SetWindowLong(getLastItem(), GWL_USERDATA, ZFLAG_EXPAND_X);
}
void KDialog::separate() {
	newLine();
	HWND hItem = CreateWindowExW(0, L"STATIC", L"",
		WS_CHILD|WS_VISIBLE|SS_SUNKEN, 0, cur_.next_y, 0, 0, // サイズは後で決める
		hWnd_, NULL, hInstance_, NULL);
	SetWindowLong(hItem, GWL_USERDATA, ZFLAG_SEPARATOR);
	newLine();
}
void KDialog::newLine() {
	// カーソルを次の行の先頭に移動
	cur_.next_x = cur_.margin_x;
	cur_.next_y += cur_.curr_line_h + cur_.padding_y;
	cur_.curr_line_h = cur_.line_min_h;

	// コントロール矩形が収まるように、ウィンドウサイズを更新
	cur_._max_x = _MAX(cur_._max_x, cur_.next_x);
	cur_._max_y = _MAX(cur_._max_y, cur_.next_y);
}
void KDialog::setInt(zid id, int value) {
	HWND hItem = GetDlgItem(hWnd_, id);
	SendMessage(hItem, BM_SETCHECK, 0, value);
}
void KDialog::setString(zid id, const char *text_u8) {
	HWND hItem = GetDlgItem(hWnd_, id);
	std::wstring ws = KConv::utf8ToWide(text_u8);
	_ConvertLineCode(ws);
	SetWindowTextW(hItem, ws.c_str());
}
int KDialog::getInt(zid id) {
	HWND hItem = GetDlgItem(hWnd_, id);
	return SendMessage(hItem, BM_GETCHECK, 0, 0);
}
void KDialog::getString(zid id, char *text_u8, int maxbytes) {
	HWND hItem = GetDlgItem(hWnd_, id);
	std::wstring ws(maxbytes, L'\0'); // ワイド文字数が maxbytes より大きくなることはないため、とりあえず maxbytes 文字分だけ確保しておく
	GetWindowTextW(hItem, &ws[0], ws.size());
	std::string u8 = KConv::wideToUtf8(ws.c_str());
	strcpy_s(text_u8, maxbytes, u8.c_str());
}
int KDialog::run(KDialogCallback *cb) {
	cb_ = cb;
	if (cur_.auto_sizing) setBestWindowSize(); // ウィンドウのサイズを自動決定する
	updateItemSize(); // 各コントロールのサイズを更新

	// 画面中心にウィンドウを表示
	_MoveToCenterInParent(hWnd_, NULL);
	ShowWindow(hWnd_, SW_SHOW);
	UpdateWindow(hWnd_);

	// 生成のタイミングによっては、他のウィンドウの裏側に表示されてしまう場合があるため
	// 明示的にフォアグラウンドに移動する
	HWND hOldFg = GetForegroundWindow();
	SetForegroundWindow(hWnd_);

	{
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			if (IsDialogMessageW(hWnd_, &msg)) {
				// Tab によるコントロールのフォーカス移動を実現するには
				// IsDialogMessage を呼ぶ必要がある。これが true を返した場合、
				// そのメッセージは既に処理済みなので TranslateMessage に渡してはいけない
				continue;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	// フォアグラウンドを戻す
	SetForegroundWindow(hOldFg);

	cb_ = NULL;
	return 0;
}
void KDialog::setBestItemSize(HWND hItem, int moreW, int moreH) {
	// このコントロールが文字を含むと仮定し、
	// その文字を描画するために必要なサイズを得る
	int w, h;
	_ComputeTextSize(hItem, &w, &h);
	w += moreW;
	h += moreH;
	// スクロールバーを考慮
	if (GetWindowLong(hItem, GWL_STYLE) & WS_VSCROLL) w += GetSystemMetrics(SM_CXHSCROLL); // 垂直バーの幅
	if (GetWindowLong(hItem, GWL_STYLE) & WS_HSCROLL) h += GetSystemMetrics(SM_CYHSCROLL); // 水平バーの高さ
	// 余白を追加する
	w += cur_.padding_x * 2; // 左右で2個分カウントする
	h += cur_.padding_y * 2; // 上下で2個分カウントする
	// コントロールのサイズが指定されている場合は、
	// その値よりも大きくなった場合にだけサイズ変更する
	if (cur_.next_w > 0) w = _MAX(w, cur_.next_w);
	if (cur_.next_h > 0) h = _MAX(h, cur_.next_h);
	// リサイズ
	SetWindowPos(hItem, NULL, 0, 0, w, h, SWP_NOZORDER|SWP_NOMOVE);
}
void KDialog::setBestWindowSize() {
	// 必要なクライアントサイズ
	int cw = cur_.content_w() + cur_.margin_x * 2;
	int ch = cur_.content_h() + cur_.margin_y * 2;

	// 必要なウィンドウサイズを計算
	RECT rect = {0, 0, cw, ch};
	AdjustWindowRect(&rect, GetWindowLong(hWnd_, GWL_STYLE), FALSE);

	// リサイズ
	int W = rect.right - rect.left;
	int H = rect.bottom - rect.top;
	SetWindowPos(hWnd_, NULL, 0, 0, W, H, SWP_NOZORDER|SWP_NOMOVE);
}
void KDialog::updateItemSize() {
	// 各コントロールのサイズを調整する
	HWND hItem = NULL;
	while ((hItem=FindWindowExW(hWnd_, hItem, NULL, NULL)) != NULL) {
		int flags = GetWindowLong(hItem, GWL_USERDATA);
		if (flags & ZFLAG_EXPAND_X) {
			// EXPAND 設定されている。
			// コントロール矩形の右端をウィンドウ右端に合わせる（余白を考慮）
			RECT rc;
			getItemRect(hItem, &rc);
			SetWindowPos(hItem, NULL, 0, 0, cur_.content_w(), rc.bottom-rc.top, SWP_NOZORDER|SWP_NOMOVE);
		}
		if (flags & ZFLAG_SEPARATOR) {
			// 区切り用の水平線。
			// コントロール右端をウィンドウ右端いっぱいまで広げる
			SetWindowPos(hItem, NULL, 0, 0, 10000/*適当な巨大値*/, 2, SWP_NOZORDER|SWP_NOMOVE);
		}
	}
}
void KDialog::getItemRect(HWND hItem, RECT *rect) {
	RECT s;
	GetWindowRect(hItem, &s); // item rect in screen

	POINT p0 = {s.left, s.top};
	POINT p1 = {s.right, s.bottom};
	ScreenToClient(hWnd_, &p0);
	ScreenToClient(hWnd_, &p1);
		
	rect->left   = p0.x;
	rect->top    = p0.y;
	rect->right  = p1.x;
	rect->bottom = p1.y;
}
void KDialog::moveCursorToNext(HWND hItem) {
	// コントロールの位置とサイズ
	RECT rect;
	getItemRect(hItem, &rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
		
	// カーソルをコントロールの右側に移動
	cur_.next_x += w + cur_.padding_x;

	// コントロール配置行の高さを更新
	cur_.curr_line_h = _MAX(cur_.curr_line_h, h);

	// コントロール矩形が収まるように、ウィンドウサイズを更新
	cur_._max_x = _MAX(cur_._max_x, (int)rect.right);
	cur_._max_y = _MAX(cur_._max_y, (int)rect.bottom);
}
void KDialog::on_WM_CLOSE() {
	if (cb_) cb_->onClose(this);
}
void KDialog::on_WM_COMMAND(zid id) {
	if (cb_) cb_->onClick(this, id);
}

} // namespace


#else // _WIN32

namespace mo {

KDialog::KDialog() {}
KDialog::~KDialog() {}
void KDialog::create(const char *text_u8) {}
void KDialog::create(int w, int h, const char *text_u8) {}
void KDialog::close() {}
zid KDialog::addLabel(zid id, const char *text_u8) { return 0; }
zid KDialog::addCheckBox(zid id, const char *text_u8, bool value) { return 0; }
zid KDialog::addButton(zid id, const char *text_u8) { return 0; }
zid KDialog::addEdit(zid id, const char *text_u8) { return 0; }
zid KDialog::addMemo(zid id, const char *text_u8, bool wrap, bool readonly) { return 0; }
void KDialog::setInt(zid id, int value) {}
void KDialog::setString(zid id, const char *text_u8) {}
int KDialog::getInt(zid id) { return 0; }
void KDialog::getString(zid id, char *text_u8, int maxbytes) {}
void KDialog::getLayoutCursor(KDialogLayoutCursor *cur) const {}
void KDialog::setLayoutCursor(const KDialogLayoutCursor *cur) {}
void KDialog::newLine() {}
void KDialog::separate() {}
void KDialog::setFocus() {}
void KDialog::setHint(const char *hint_u8) {}
void KDialog::setExpand() {}
void KDialog::setBestWindowSize() {}
int KDialog::run(KDialogCallback *cb) { return 0; }
}

#endif // !_WIN32
