#pragma once
#include <inttypes.h>
#include "KKeyboard.h"

namespace mo {

class KCursor;
class KIcon;
class KMenu;

struct WindowEvent {
	WindowEvent();

	enum Type {
		NONE,
		MENU_CLICK,
		MENU_OPEN,
		WINDOW_ACTIVE,
		WINDOW_INACTIVE,
		WINDOW_CLOSING,
		WINDOW_MOVE,
		WINDOW_SIZE,
		MOUSE_WHEEL,
		MOUSE_HORZ_WHEEL,
		MOUSE_ENTER,
		MOUSE_EXIT,
		MOUSE_MOVE,
		MOUSE_DOWN,
		MOUSE_UP,
		KEY_DOWN,
		KEY_UP,
		KEY_CHAR,
		DROPFILE,
		WIN32MSG,
	};
	Type type;
	union {
		struct {
			KMenu *menu;
			int menu_id;
			int index;
			int command_id;
		} menu_ev;

		struct {
			void *hWnd;
			int x;
			int y;
			int w;
			int h;
		} window_ev;

		struct {
			int abs_x; // スクリーン上での座標
			int abs_y;
			int cli_x; // ウィンドウクライアント座標系での座標
			int cli_y;
			int delta_x;
			int delta_y;
			int wheel_delta; // ホイールの回転ノッチ数。1=上回転、-1=下回転
			int button; // 1=Left, 2=Right, 3=Middle
			KKeyboard::ModifierKeys mod_flags;
			bool withLeft()  const { return button == 1; }
			bool withRight() const { return button == 2; }
			bool withMiddle()const { return button == 3; }
			bool withShift() const { return mod_flags & KKeyboard::M_SHIFT; }
			bool withCtrl()  const { return mod_flags & KKeyboard::M_CTRL;  }
			bool withAlt()   const { return mod_flags & KKeyboard::M_ALT;   }
		} mouse_ev;

		struct {
			KKeyboard::Key key;
			KKeyboard::ModifierKeys mod_flags;
			wchar_t chr;
			bool withShift() const { return mod_flags & KKeyboard::M_SHIFT; }
			bool withCtrl()  const { return mod_flags & KKeyboard::M_CTRL;  }
			bool withAlt()   const { return mod_flags & KKeyboard::M_ALT;   }
		} key_ev;

		struct {
			const wchar_t *filename; // ファイル名
			int index; // 処理中のファイルインデックス（0 以上 total 未満）
			int total; // ドロップされたファイル総数
		} drop_ev;

		struct {
			void *hWnd;
			unsigned int msg;
			long wp;
			long lp;
			long lresult;
		} win32msg_ev;
	};
};

class IWindowCallback {
public:
	virtual void onWindowEvent(WindowEvent *e) = 0;
};


class Window {
public:
	enum Attribute {
		RESIZABLE,
		KEEP_ASPECT,
		ASPECT_RATIO_W,
		ASPECT_RATIO_H,
		HAS_BORDER,
		HAS_TITLE,
		HAS_MAXIMIZE_BUTTON, ///< 最大化ボタンを表示（最大化を許可）
		HAS_ICONIFY_BUTTON, ///< 最小化ボタンを表示（最小化を許可）
		HAS_CLOSE_BUTTON,    ///< 閉じるボタンを表示（ユーザーによる終了を許可）
		TOPMOST, ///< 最前面に表示（タスクバーよりも手前）
		FULLFILL, ///< フルスクリーン用ウィンドウ

		/// [WIN32]
		/// ALT キー と F10 キーのイベントを OS に伝達しないようにする。
		/// ALT または F10 を押した場合アプリケーションから制御が奪われ、ゲーム全体が一時停止状態になってしまう
		/// これを回避したい場合は WindowAttr_Kill_ALT_and_F10 を true に設定し、キーイベントを握りつぶす
		KILL_ALT_F10,

		/// SnapShot, PrtScr キーをシステムに任せずに自前で処理する
		KILL_SNAPSHOT,

		ATTR_ENUM_MAX
	};

	Window();
	bool create(int w, int h, const char *text_u8);
	void destroy();

	void addCallback(IWindowCallback *cb);
	void removeCallback(IWindowCallback *cb);

	/// ウィンドウを閉じる
	/// このメソッドが直ちにウィンドウを閉じるとは限らないことに注意。
	/// closeWindow() を呼んだ後も、processEvents() の戻り値をチェックし、
	/// 実際にウィンドウが閉じてアプリケーションを終了してもよい状態かどうかを確認すること
	void closeWindow();

	/// 内部で保持しているウィンドウ識別子を返す。
	/// Windows なら HWND の値になる
	void * getHandle() const;

	/// ウィンドウのクライアント領域のサイズを取得する
	/// 不要なパラメータには NULL を指定してもよい
	void getClientSize(int *cw, int *ch) const;

	/// ウィンドウの位置を得る
	/// 得られる座標は、ウィンドウの最外枠の左上の点の位置を、スクリーン座標系で表したもの
	void getWindowPosition(int *x, int *y) const;

	/// クライアント座標とスクリーン座標を変換する
	void screenToClient(int *x, int *y) const;
	void clientToScreen(int *x, int *y) const;

	/// ウィンドウが表示されているかどうか
	bool isWindowVisible() const;

	/// ウィンドウが入力フォーカスを持っているかどうか
	bool isWindowFocused() const;

	/// ウィンドウがアイコン化されているかどうか
	bool isIconified() const;

	/// ウィンドウが最大化されているかどうか
	bool isMaximized() const;

	/// ウィンドウがフルスクリーンになっているかどうか
	bool isFullscreen() const;

	/// フルスクリーンにする
	/// フルスクリーンモードでは、画面の「すべての領域」がウィンドウによって占有される
	/// ウィンドウの最大化では、OSによって表示されているタスクバーやステータスバーなどを除いた領域内で最大の大きさになる
	void fullscreenWindow();

	/// ウィンドウを最大化する
	/// フルスクリーンモードでは、画面の「すべての領域」がウィンドウによって占有される
	/// ウィンドウの最大化では、OSによって表示されているタスクバーやステータスバーなどを除いた領域内で最大の大きさになる
	void maximizeWindow();

	/// ウィンドウを最小化する
	void iconifyWindow();

	/// 最大化、または最小化されているウィンドウを通常の状態に戻す
	/// ただし、フルスクリーンモードの場合は動作しない。
	/// フルスクリーンを解除するには setFullScreen(false, ...) を呼び出す
	void restoreWindow();

	/// ウィンドウを表示する
	void setWindowVisible(bool value);

	/// ウィンドウ位置を設定する
	/// ウィンドウの最外枠の左上の点の位置を、スクリーン座標系で指定する
	void setWindowPosition(int x, int y);

	/// ウィンドウのクライアント領域のサイズを設定する
	void setClientSize(int cw, int ch);

	/// ウィンドウのタイトル文字列を設定する
	void setTitle(const char *text_u8);

	/// ウィンドウのアプリケーションアイコンを設定する
	void setIcon(const KIcon &icon);

	/// マウスカーソルの表示、非表示を切り替える
	void setMouseCursorVisible(bool visible);

	/// マウスカーソルを変更する
	void setMouseCursor(const KCursor &cursor);

	/// メインループを継続するかどうかの判定と処理を行う
	/// 必ず毎フレームの先頭で呼び出し、
	/// 戻り値をチェックすること。
	/// @retval true  ウィンドウは有効である
	/// @retval false ウィンドウを閉じ、アプリケーション終了の準備が整った
	bool processEvents();

	//////////////////////////////////////////////////////////////

	void getMousePosition(int *x, int *y, bool in_screen);
	bool isMouseButtonDown(int btn);

	/// マウスホイールの積算回転量を返す。
	/// 回転量を知るためには、前回との差を取る必要がある
	int getMouseWheel();

	/// マウスカーソルがウィンドウ内にあるかどうか
	bool isMouseInWindow();

	//////////////////////////////////////////////////////////////

	/// キーの状態を得る。ウィンドウが非アクティブな場合は常に false を返す
	bool isKeyDown(KKeyboard::Key key);

	//////////////////////////////////////////////////////////////

	int getAttribute(Attribute attr);
	void setAttribute(Attribute attr, int value);

	KMenu * getMenu();
};


} // namespace
