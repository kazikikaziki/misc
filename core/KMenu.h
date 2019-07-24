#pragma once

namespace mo {

class KMenu;

class KMenuCallback {
public:
	virtual void on_menu_click(KMenu *menu, int index) = 0;
	virtual void on_menu_select(KMenu *menu, int index) = 0;
	virtual void on_menu_open(KMenu *menu) = 0;
};
class KMenu {
public:
	KMenu();
	void create();
	void createPopup();
	void createSystem(void *_hWnd);
	void destroy();
	void setCallback(KMenuCallback *cb);
	void * getHandle(); // HMENU
	void setHandle(void *hMenu);

	/// メニュー項目を追加する
	int addItem(const wchar_t *text, int command_id=0);

	/// セパレータを追加する
	int addSeparator();

	/// メニュー項目数を返す
	int getItemCount();

	/// コマンドIDを指定して、そのメニューインデックスを得る。見つからない場合は -1 を返す
	int getIndexOfCommandId(int id);

	/// メニュー項目にチェックマークがついているかどうか
	bool isItemChecked(int index);

	/// メニュー項目がアクティブかどうか
	bool isItemEnabled(int index);

	/// メニュー項目のユーザー定義データを得る
	void * getItemUserData(int index);

	/// メニュー項目のコマンドIDを得る
	int getItemCommandId(int index);

	/// メニュー項目のチェックマークを設定する
	void setItemChecked(int index, bool value);

	/// メニュー項目の Enabled を設定する
	void setItemEnabled(int index, bool value);

	/// メニュー項目にユーザー定義の値をセットする
	void setItemUserData(int index, void *data);

	/// メニュー項目にアイコンをつける
	/// index 項目インデックス
	/// icon_res アイコンのリソース名
	void setItemBitmapFromResource(int index, const wchar_t *icon_res);

	/// メニュー項目にアイコンをつける
	/// index 項目インデックス
	/// w, h: アイコンのサイズ。普通は 16
	/// pixels_rgba32: R8G8B8A8 フォーマットでのピクセル配列。（アルファは 0 または 255 のみ）
	void setItemBitmapFromPixels(int index, int w, int h, const void *pixels_rgba32);

	/// メッセージを処理する。
	/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
	/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
	bool processMessage(void *_hWnd, unsigned int _uMsg, long _wp, long _lp);

	/// メニューを現在のカーソル位置にポップアップさせる。
	/// メニュー選択後、選択されたメニュー項目のID を返す
	int popup(void *hWnd);

protected:
	KMenuCallback *cb_;
	void *hMenu_;
};

}
