#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

namespace mo {

typedef int zid;

class KDialog;

class KDialogCallback {
public:
	virtual void onClose(KDialog *wnd) {}
	virtual void onClick(KDialog *wnd, zid id) {}
	virtual void onDrop(KDialog *wnd, zid id, const wchar_t *data) {}
};

struct KDialogLayoutCursor {
	/// ウィンドウ内側の余白
	int margin_x;
	int margin_y;

	/// コントロール外側の余白
	int padding_x;
	int padding_y;

	/// コントロール配置座標
	int next_x, next_y;

	/// コントロールの大きさ
	int next_w, next_h;

	/// 何もコントロールを配置しない行の高さ
	int line_min_h;

	/// 現在カーソルがある行の高さ
	int curr_line_h;

	/// コントロールの大きさを自動計算する
	bool auto_sizing;

	/// これまで配置したコントロール矩形のうち一番右下の座標
	int _max_x;
	int _max_y;

	/// これまで配置したコントロールをすべて収めるために必要なサイズ
	int content_w() const { return _max_x - margin_x; }	// _max_x は配置コントロールの右端を指しているため、そこから左側の余白を引く
	int content_h() const { return _max_y - margin_y; }

};

class KDialog {
public:
	KDialog();
	~KDialog();

	/// ウィンドウを作成
	void create(const char *text_u8);
	void create(int w, int h, const char *text_u8);

	/// ウィンドウを閉じる
	void close();

	/// ラベルを追加する
	/// 現在の「コントロール配置カーソル」位置にラベルを配置し、カーソルを次の配置予定位置に進める。
	/// @param id 追加するコントロールの識別番号。0を指定すると自動割り振りになる。負の値は使えない
	/// @return 追加したラベルの識別番号。自動付加した場合は負の値になる。失敗したら 0 を返す
	zid addLabel(zid id, const char *text_u8);

	/// チェックボックスを追加する
	zid addCheckBox(zid id, const char *text_u8, bool value=false);

	/// ボタンを追加する
	zid addButton(zid id, const char *text_u8);

	/// テキスト入力ボックスを追加する
	zid addEdit(zid id, const char *text_u8);

	/// テキスト編集ボックスを追加する（複数行、スクロールバー付き、読み取り専用設定可能）
	zid addMemo(zid id, const char *text_u8, bool wrap, bool readonly);

	/// コントロールが整数値の表示に対応している場合、その値を int で指定する
	void setInt(zid id, int value);

	/// コントロールが文字列の表示に対応している場合、その文字列をセットする (UTF8)
	/// 例: setString(id, u8"こんにちは")
	void setString(zid id, const char *text_u8);

	/// コントロールが何らかの値の表示に対応している場合、その値を int で取得する
	int getInt(zid id);

	/// コントロールが何らかの値の表示に対応している場合、その値を utf8 で取得する
	/// text_u8 取得した文字列の格納先
	/// maxbytes: text_u8 に格納できる最大バイト数
	void getString(zid id, char *text_u8, int maxbytes);

	/// 「コントロール配置カーソル」を取得する
	void getLayoutCursor(KDialogLayoutCursor *cur) const;

	/// 「コントロール配置カーソル」を設定する
	void setLayoutCursor(const KDialogLayoutCursor *cur);

	/// 「コントロール配置カーソル」を「改行」する
	void newLine();

	/// コントロールを視覚的に区切るための水平線を追加する
	void separate();

	/// 直前に追加したコントロールに対してフォーカスを設定する
	void setFocus();

	/// 直前に追加したコントロールに対してツールヒントを設定する
	/// ヒント文字列は UTF8 で指定する
	void setHint(const char *hint_u8);

	/// 直前に追加したコントロールに対して、幅を最大限取るように設定する
	void setExpand();

	/// ウィンドウサイズを自動決定する
	void setBestWindowSize();

	/// ウィンドウ処理を開始する。ウィンドウを閉じるまで制御が戻らないことに注意
	int run(KDialogCallback *cb);

private:
#ifdef _WIN32
	static LRESULT CALLBACK _wndproc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

	void clear();

	/// コントロールのサイズを自動決定する
	void setBestItemSize(HWND hItem, int moreW=0, int moreH=0);

	/// コントロール矩形を、親ウィンドウのクライアント座標系で返す
	void getItemRect(HWND hItem, RECT *rect);

	/// コントロール配置カーソルを次の位置へ移動
	void moveCursorToNext(HWND hItem);

	/// 各コントロールのサイズを更新
	void updateItemSize();

	void on_WM_CLOSE();
	void on_WM_COMMAND(zid id);

	/// 最後に作成したアイテムを得る
	HWND getLastItem();

private:
	HINSTANCE hInstance_;
	HWND hWnd_;
	HWND hParent_;
	KDialogCallback *cb_;
	KDialogLayoutCursor cur_;
	int auto_id_;
#endif // _WIN32
};

} // namespace
