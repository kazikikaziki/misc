#pragma once

#define NO_TASKBAR
#define NO_AVI
#define NO_TSF
#define NO_STB_TRUETYPE


#include <windows.h>
#include <vector>
#include <unordered_map>


#ifndef NO_TASKBAR
#	include <shobjidl.h> // ITaskbarList3
#	pragma comment(lib, "comctl32.lib")
#endif


#ifndef NO_AVI
#	include <vfw.h> // IAVIFile, IAVIStream
#	pragma comment(lib, "vfw32.lib")
#endif


#ifndef NO_TSF
#	include <msctf.h> // ITfThreadMgr, CLSID_TF_ThreadMgr
#	include <ctffunc.h> // ITfFnReconversion, IID_ITfFnReconversion
#endif



/// ウィンドウメッセージの引数と、プロシージャによる処理結果をまとめたもの。
/// メッセージ処理関数の呼び出し側はメッセージ引数を指定し、lResult, bShouldReturn を 0 に初期化した状態で呼び出す。
/// 呼び出し終了後、bShouldReturn の値が TRUE になっていたら lResult の値を戻す。そうでなければ DefWindowProc や
/// CallWindowProc などでメッセージチェーンを継続する。
///
/// LRESULT MyWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
///   SWin32Msg msg = {hWnd, uMsg, wParam, lParam, 0, FALSE };
///   MyWindowProc2(&msg); // call your own procedure
///   if (msg.bShouldReturn) {
///     return msg.lResult;
///   } else {
///     return DefWindowProc(hWnd, uMsg, wParam, lParam);
///   }
/// }
///
struct SWin32Msg {
	/// ウィンドウメッセージの引数
	HWND    hWnd;
	UINT    uMsg;
	WPARAM  wParam;
	LPARAM  lParam;
	
	/// [OUT]
	/// このメッセージの戻り値。
	/// bShouldReturn が TRUE の場合にのみ使う
	LRESULT lResult;

	/// [OUT]
	/// メッセージ処理関数の呼び出し元に対する要求で、
	/// ここでメッセージチェーンを終わらせ、関数を return するかどうかを指定する。
	/// この値が TRUE だった場合、メッセージ処理関数の呼び出し元はメッセージを次に伝達せずに return lResult する。
	/// この値がFALSE だった場合、メッセージ処理関数の呼び出し元はメッセージを DefWindowProc などに渡してメッセージチェーンを続行する。
	BOOL bShouldReturn;
};



/// HICON から HBITMAP を作成する
HBITMAP Win32_CreateBitmapFromIcon(HICON hIcon);

/// RGBA32ビットで表現されたピクセル配列を指定して HICON を作成する
/// w, h, pixels: 画像データ。RGBAの順に1ピクセル4バイトで表現
HICON Win32_CreateIconFromPixelsRGBA32(int w, int h, const uint8_t *pixels_rgba32);

/// ウィンドウメッセージを処理する
bool Win32_ProcessEvents();

/// ウィンドウメッセージを処理する。hWnd 宛のメッセージの場合には IsDialogMessage 関数を通す。
/// これはモードレスウィンドウで TAB キーを処理したい場合に必要だが、その代償として WM_CHAR が受け取れなくなることに注意。
/// （WM_CHAR は WM_KEYDOWN が TranslateMessage に渡された時に発生するため）
bool Win32_ProcessEventsWithDialog(HWND hWnd);

void Win32_ComputeWindowSize(HWND hWnd, int cw, int ch, int *req_w, int *req_h);

/// ALT または F10 を押したときにウィンドウが応答停止するのを無効化する。
/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
bool Win32_KillMessage_ALT_F10(SWin32Msg *msg);

/// スクリーンセーバーの起動を阻害する
/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
bool Win32_KillMessage_ScreenSaver(SWin32Msg *msg);

/// モニターのオートパワーオフを阻害する
/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
bool Win32_KillMessage_MonitorOff(SWin32Msg *msg);

/// uMsg がリサイズに関係するメッセージだった場合、アスペクト比を固定するためにイベントに介入する
/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
bool Win32_ProcMessage_KeepAspect(SWin32Msg *msg, int base_w, int base_h, int *new_cw, int *new_ch);

/// CWin32DropFiles の簡易版。
/// ドロップされたファイルを一つだけ得る
/// ドロップされたファイルを検出した場合はファイル名を path にセットして true を返す
/// 無関係なメッセージだった場合は何もせずに false を返す
/// @note 事前に DragAcceptFiles を呼んでおかないとファイルのドロップが有効にならない
/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
bool Win32_ProcMessage_DropFile(SWin32Msg *msg, wchar_t *path, int maxwchars);


/// 指定されたキーが押されているかどうか
bool Win32_IsKeyDown(int vk);

/// 次にマウスがウィンドウを離れたときに、WM_MOUSELEAVE メッセージを受け取るように設定する。
/// この設定はイベント発生のたびにリセットされ無効になるので注意する事
void Win32_SetMouseLeaveEvent(HWND hWnd);

/// DefWindowProc を SWin32Msg 引数で呼べるようにしたもの
LRESULT Win32_DefWindowProc(const SWin32Msg *msg);

void Win32_CalcTextSize(HWND hWnd, int *w, int *h);
void Win32_CalcTextSize(HWND hWnd, const wchar_t *text, int *w, int *h);
void Win32_CalcCheckBoxSize(HWND hWnd, int *w, int *h);
void Win32_CalcEditBoxSize(HWND hWnd, int *w, int *h);
void Win32_SetFontSize(HWND hWnd, int size);
void Win32_SetToolHint(HWND hWnd, HWND hCtrl, const wchar_t *text);
void Win32_SetPos(HWND hWnd, int x, int y);
void Win32_SetSize(HWND hWnd, int w, int h);
void Win32_GetSize(HWND hWnd, int *w, int *h);
void Win32_GetRectInParent(HWND hWnd, RECT *rect);
HWND Win32_CreateLabel(HWND hParent, int id, const wchar_t *text);
HWND Win32_CreateEdit(HWND hParent, int id, const wchar_t *text, bool readonly, bool multilines);
HWND Win32_CreateButton(HWND hParent, int id, const wchar_t *text);
HWND Win32_CreateCheckBox(HWND hParent, int id, const wchar_t *text);
HWND Win32_CreateListBox(HWND hParent, int id);
HWND Win32_CreateDropDown(HWND hParent, int id);
HWND Win32_CreateDropDownList(HWND hParent, int id);
HWND Win32_CreateHorzLine(HWND hParent, int id);
HWND Win32_CreateVertLine(HWND hParent, int id);
bool Win32_IsPushButton(HWND hWnd);
bool Win32_IsCheckBox(HWND hWnd);
bool Win32_IsSingleEdit(HWND hWnd);
void Win32_UpdateListBox(HWND hWnd, int selected, const wchar_t *items[], int count);
void Win32_UpdateDropDownList(HWND hWnd, int selected, const wchar_t *items[], int count);
void Win32_UpdateDropDownEdit(HWND hWnd, int selected, const wchar_t *items[], int count, const wchar_t *text);
bool Win32CheckBox_IsChecked(HWND hWnd);
void Win32CheckBox_SetChecked(HWND hWnd, bool checked);
HFONT Win32_CreateSystemUIFont(HWND hWnd, int size);
int Win32_FontPointToPixel(int pt);
void Win32_Print(const wchar_t *s);
void Win32_Printf(const wchar_t *fmt, ...);
void Win32_PrintHResult(HRESULT hr);
void Win32Console_Open();
void Win32Console_Close();
HANDLE Win32Console_GetHandle();
void Win32Console_ClearScreen(HANDLE hConsole); // 画面消去
void Win32Console_ClearToEndOfLine(HANDLE hConsole); // 行末まで削除
void Win32Console_MoveToHome(HANDLE hConsole); // 行頭に移動
void Win32Console_MoveTo(HANDLE hConsole, int row, int col); // 指定行列に移動
void Win32Console_SetFgColor(HANDLE hConsole, int red, int green, int blue, int intensity);
void Win32Console_SetBgColor(HANDLE hConsole, int red, int green, int blue, int intensity);
void Win32_GetErrorString(HRESULT hr, char *msg, int size);


class CWin32StyleMgr {
public:
	CWin32StyleMgr();
	/// hWnd で指定されたウィンドウの位置やスタイルを、ウィンドウモード用のものに変更する。
	/// 必ずグラフィックデバイス側のフルスクリーン状態を解除してから実行すること
	void setWindowedStyle(HWND hWnd);
	/// hWnd で指定されたウィンドウの位置やスタイルを、フルスクリーン用のものに変更する。
	/// 必ずグラフィックデバイス側をフルスクリーン状態にしてから実行すること
	void setFullScreenStyle(HWND hWnd);
private:
	LONG oldStyle_;
	LONG oldStyleEx_;
	RECT  oldRect_;
};


/// ウィンドウをフルスクリーン表示に適したスタイルに変更する。
/// oldStyle, oldStyleEx, oldRect に状態変更前の値をセットし、ウィンドウをフルスクリーン化する。
/// これは最大化とは異なり、SW_RESTORE では元の表示に戻らないため、
/// ウィンドウモードに戻すときには oldStyle, oldStyleEx, oldRect の各値を利用する
void Win32_SetFullScreenStyle(HWND hWnd, LONG *oldStyle, LONG *oldStyleEx, RECT *oldRect);

/// ウィンドウをウィンドウモードに適した表示状態にする
void Win32_SetWindowedStyle(HWND hWnd, LONG style, LONG styleEx, const RECT *rect);

bool Win32_SetClipboardText(HWND hWnd, const std::wstring &text);
bool Win32_GetClipboardText(HWND hWnd, std::wstring &text);


void Win32Menu_AddItem(HMENU hMenu, const wchar_t *text, UINT id, UINT userdata);
int  Win32Menu_GetIndexOfId(HMENU hMenu, UINT id);
void Win32Menu_SetBitmap(HMENU hMenu, int item, bool by_position, const wchar_t *filename);
void Win32Menu_SetEnabled(HMENU hMenu, int item, bool by_position, bool value);
void Win32Menu_SetChecked(HMENU hMenu, int item, bool by_position, bool value);
bool Win32Menu_IsEnabled(HMENU hMenu, int item, bool by_position);
bool Win32Menu_IsChecked(HMENU hMenu, int item, bool by_position);
void Win32Menu_SetSubMenu(HMENU hMenu, int item, bool by_position, HMENU hSubMenu);







typedef std::vector<HICON> std_vector_HICON;

/// 実行ファイルに埋め込まれたアイコンを得る
class CWin32ResourceIcons {
public:
	CWin32ResourceIcons();
	int getCount() const;
	HICON getIcon(int index) const;

private:
	std_vector_HICON icons_;
};


/// 現在利用可能なフォント名を得る。
/// フォントダイアログで表示されるものとだいたい同じ
class CWin32Fonts {
public:
	void update();
	void print();

	const wchar_t * getFace(size_t index);
	int findFace(const wchar_t *face);
	size_t getCount();
private:
	static int CALLBACK fontEnumProc(const ENUMLOGFONTEXW *font, const NEWTEXTMETRICEXW *metrix, DWORD type, LPARAM user);
	void onFontEnum(const ENUMLOGFONTEXW *font, const NEWTEXTMETRICEXW *metrix, DWORD type);
	std::vector<std::wstring> items_;
};


/// 利用可能なすべてのフォントを調べる
/// （レジストリに登録されている情報から調べる）
class CWin32FontFiles {
public:
	struct ITEM {
		wchar_t facename_jp[MAX_PATH];
		wchar_t facename_en[MAX_PATH];
		wchar_t filename[MAX_PATH];
		int index; /// フォント内でのインデックス
	};

	void update();
	void print(bool strip_dir=true);
	const ITEM * getItem(size_t index); 
	size_t getCount();
	int findFile(const wchar_t *filename, bool ignore_dir=true, bool ignore_case=true);
	int findFace(const wchar_t *face);

private:
	void readFile(const wchar_t *filename);
	std::vector<ITEM> items_;
};


/// PCMサウンドを再生する
class CWin32PCMPlayer {
public:
	CWin32PCMPlayer();

	/// サウンドを再生可能な状態にする
	/// wf  サウンド情報
	/// data PCMデータ
	/// size PCMデータサイズ（バイト単位）
	bool open(const WAVEFORMATEX *wf, const void *data, int size);
	
	/// サウンドデータを閉じる
	void close();

	bool isPlaying();
	void play();
	void pause();
	void resume();
	void getpos(int *samples, float *seconds);
	void getvol(float *lvol, float *rvol);
	void setvol(float lvol, float rvol);
	void setpitch(float pitch);

private:
	static void callback(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	static void _err(MMRESULT mr);
	void on_done(HWAVEOUT wo);

	WAVEHDR whdr_;
	WAVEFORMATEX wfmt_;
	HWAVEOUT wout_;
	bool playing_;
};


/// .wav フォーマットのファイルからデータを読み取る
class CWin32PCMWave {
public:
	CWin32PCMWave();

	/// サウンドをファイルから開く
	bool openFromFile(const wchar_t *filename);

	/// サウンドをメモリから開く
	bool openFromMemory(const void *data, int size);

	/// サウンドを HMMIO から開く。
	bool openFromMmio(HMMIO hMmio);

	/// サウンドを閉じる
	void close();

	const WAVEFORMATEX * getFormat();

	/// ブロック単位で読み取る（1ブロック＝チャンネル数 x サンプリングバイト数）
	/// 読み取ったブロック数を返す
	int pcmRead(void *buf, int num_blocks);

	/// ブロック単位でシークする
	int pcmSeek(int num_blocks);

	/// ブロック単位での位置を得る
	int pcmTell();

	/// ブロック単位での全体長さを得る
	int pcmCount();

	/// 1ブロック当たりのバイト数を得る
	int pcmBlockSize();

private:
	HMMIO mmio_;
	WAVEFORMATEX wfmt_;
	MMCKINFO chunk_;

	/// 1ブロックあたりのバイト数（1ブロック＝チャンネル数 x サンプリングバイト数）
	/// 1チャンネル16ビットサウンドなら 2
	/// 2チャンネル16ビットサウンドなら 4
	int block_bytes_;
};


/// ファイルを列挙する
class CWin32FilesInDirecrory {
public:
	int scan(const wchar_t *dir);
	int getCount();
	const WIN32_FIND_DATAW * getData(int index);

private:
	std::vector<WIN32_FIND_DATAW> items_;
};


class CWin32ResourceFiles {
public:
	struct ITEM {
		wchar_t name[MAX_PATH];
		HMODULE hModule;
		HRSRC hRsrc;
	};

	/// この実行フィアルに埋め込まれているリソースデータの一覧を得る
	void update();

	/// リソースデータの個数
	int getCount() const;

	/// リソース情報
	const ITEM * getItem(int index) const;

	/// リソースデータの先頭アドレスとサイズを取得する。
	/// ここで取得できるアポインタはアプリケーション終了時まで常に有効である
	const void * getData(int index, int *size) const;

private:
	static BOOL CALLBACK nameCallback(HMODULE module, LPCWSTR type, LPWSTR name, LONG user);
	static BOOL CALLBACK typeCallback(HMODULE module, LPWSTR type, LONG user);
	void onEnum(HMODULE module, LPCWSTR type, LPWSTR name);
	std::vector<ITEM> items_;
};

class CWin32Menu;
class IWin32MenuCallback {
public:
	virtual void on_win32menu_click(SWin32Msg *msg, CWin32Menu *menu, int index) = 0;
	virtual void on_win32menu_select(SWin32Msg *msg, CWin32Menu *menu, int index) = 0;
	virtual void on_win32menu_open(SWin32Msg *msg, CWin32Menu *menu) = 0;
	virtual void on_win32menu_close(SWin32Msg *msg, CWin32Menu *menu) = 0;
};



struct SWin32MenuItem {
	wchar_t text[256];
	wchar_t icon[256];
	bool checked;
	bool enabled;
	HMENU submenu;
	UINT id;
	UINT userdata;
};

class CWin32Menu {
public:
	CWin32Menu(HMENU hMenu=NULL);
	void create();
	void destroy();
	void setCallback(IWin32MenuCallback *cb);
	HMENU getHandle();
	void setHandle(HMENU hMenu);
	void addItem(const wchar_t *text, UINT id=0, UINT userdata=0);
	int getCount();
	int getIndexOfMenuId(UINT id);
	bool hasMenuId(UINT id);

	bool getItemByIndex(int index, SWin32MenuItem *item);
	bool getItemById(UINT id, SWin32MenuItem *item);
	void setItemByIndex(int index, const SWin32MenuItem *item);
	void setItemById(UINT id, const SWin32MenuItem *item);

	/// メッセージを処理する。
	/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
	/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
	bool processMessage(SWin32Msg *msg);

protected:
	IWin32MenuCallback *cb_;
	HMENU hMenu_;
	bool isopen_;
};

class CWin32MainMenu: public CWin32Menu {
public:
	CWin32MainMenu();
	void createMainMenu(HWND hWnd);
	void updateMainMenu();
private:
	HWND hMainMenuWnd_; ///< メニューを所有しているウィンドウ。createMainMenu で作成した場合のみ有効
};

class CWin32PopupMenu: public CWin32Menu {
public:
	void createPopup();
	/// メニューを現在のカーソル位置にポップアップさせる。
	/// メニュー選択後、選択されたメニュー項目のID を返す
	int popup(HWND hWnd);
};


class CWin32Window {
public:
	CWin32Window();
	void create(WNDPROC wndproc=NULL);
	void destroy();
	void setHandle(HWND hWnd);
	void setMenu(HMENU hMenu);
	HWND getHandle();
	HMENU getMenu();
	HMENU getSystemMenu();
	void setUserData(LONG data);
	LONG getUserData();
	void setWndProc(WNDPROC wp);
	void setClientSize(int cw, int ch);
	void show(int nCmdShow); // SW_SHOW, SW_HIDE, SW_MINIMIZE など
	bool isMaximized();
	bool isIconified();
	bool isFocused();
	bool isVisible();
	void setPosition(int x, int y);
	void getPosition(int *x, int *y);
	void getClientSize(int *cw, int *ch);
	void getWholeSize(int *w, int *h);
	void setIcon(HICON hIcon);
	void setStyle(LONG style); // WS_CAPTION, WS_THICKFRAME, WS_MINIMIZEBOX, WS_MAXIMIZEBOX, WS_SYSMENU など
	void setExStyle(LONG exStyle); // WS_EX_ACCEPTFILES など
	LONG getStyle();
	LONG getExStyle();
	void screenToClient(int *x, int *y);
	void clientToScreen(int *x, int *y);
	void setTitle(const wchar_t *s);

	/// 次にマウスポインタがウィンドウを離れたとき、一度だけ WM_MOUSE_LEAVE メッセージが送られるようにする。
	/// この設定はイベント発生のたびにリセットされることに注意。WM_MOUSE_LEAVE が必要になるたびに有効化しないといけない
	void enable_WM_MOUSE_LEAVE();

private:
	HWND hWnd_;
};


#ifndef NO_TASKBAR
class CWin32SystemTray {
public:
	/// hWnd 関連付けるウィンドウ
	/// hIcon システムトレイのアイコン
	/// uMsg システムトレイでイベントが発生したときに hWnd のプロシージャに送られてくるメッセージ. 例えば WM_USER + 1 など。
	/// hint システムトレイのツールチップヒント
	void init(HWND hWnd, HICON hIcon, UINT uMsg, const wchar_t *hint);
	void addIcon();
	void delIcon();

	/// 「システムトレイ上で左クリックした」メッセージならば true を返す
	bool isLClickMessage(const SWin32Msg *msg);

	/// 「システムトレイ上で右クリックした」メッセージならば true を返す
	bool isRClickMessage(const SWin32Msg *msg);

	/// システムトレイでコンテキストメニューを表示する
	/// hPopupMenu には CreatePopupMenu で作成されたメニューを指定する
	/// 選択されたメニュー項目のメニューIDを返す
	UINT showMenu(HMENU hPopupMenu);

private:
	NOTIFYICONDATAW icondata_;
	HMENU hMenu_;
};

class CWin32TaskBar {
public:
	CWin32TaskBar();
	~CWin32TaskBar();
	void setProgressValue(int done, int total);
	void setProgressState(TBPFLAG state);
private:
	ITaskbarList3 *task3_;
};
#endif // NO_TASKBAR


#ifndef NO_AVI
class CWin32AviWriter {
public:
	CWin32AviWriter();
	~CWin32AviWriter();
	/// 書き込み用にAVIファイルを開く
	bool open(const wchar_t *filename, int width, int height, int fps);
	void close();

	/// AVIファイルにBMP画像を追記する。
	/// 画像サイズは open 時に指定した値と同一でないといけない
	bool writeFromPixels(const void *pixels, size_t size);

	/// AVIファイルにBMPファイルの画像を追記する。
	/// 画像サイズは open 時に指定した値と同一でないといけない
	bool writeFromBitmapFile(const wchar_t *filename);
private:
	BITMAPFILEHEADER bmp_filehdr_;
	BITMAPINFOHEADER bmp_infohdr_;
	AVISTREAMINFOW avi_strminfo_;
	IAVIFile *avi_file_;
	IAVIStream *avi_strm_;
	int index_;
	int width_;
	int height_;
	void *buf_;
	size_t bufsize_;
};
#endif // NO_AVI




class IWin32MouseCallback {
public:
	virtual void on_win32mouse_down(int btn) = 0;
	virtual void on_win32mouse_up(int btn) = 0;
	virtual void on_win32mouse_move(int x, int y) = 0;
	virtual void on_win32mouse_enter() = 0;
	virtual void on_win32mouse_leave() = 0;
	virtual void on_win32mouse_wheel(int delta) = 0;
	virtual void on_win32mouse_horz_wheel(int delta) = 0;
};

class CWin32Mouse {
public:
	CWin32Mouse();
	void init(HWND hWnd, IWin32MouseCallback *cb);
	bool getButton(int btn);
	void getPositionInScreen(int *x, int *y);
	void getPosition(int *x, int *y);
	int getWheel();
	int getHorzWheel();

	/// メッセージを処理する。
	/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
	/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
	bool processMessage(SWin32Msg *msg);

private:
	void _MouseMove(LPARAM lParam);
	void _MouseWheel(WPARAM wParam, bool is_horz);
	void _MouseLeave();
	IWin32MouseCallback *cb_;
	HWND hWnd_;
	int wheel_;
	int horz_wheel_;
	bool entered_;
};


class IWin32KeyboardCallback {
public:
	virtual void on_win32kb_keydown(int vk) = 0;
	virtual void on_win32kb_keyup(int vk) = 0;
	virtual void on_win32kb_keychar(wchar_t c) = 0;
};

class CWin32Keyboard {
public:
	CWin32Keyboard();
	void init(HWND hWnd, IWin32KeyboardCallback *cb);
	void poll();

	/// メッセージを処理する。
	/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
	/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
	bool processMessage(SWin32Msg *msg);

	IWin32KeyboardCallback *cb_;
	HWND hWnd_;
	std::wstring text_;
	uint8_t keys_[256];
	bool key_ctrl_;
	bool key_shift_;
	bool key_alt_;
};


/// ファイルのドロップメッセージを処理し、ドロップされたファイル名を取得する
/// @note 事前に DragAcceptFiles を呼んでおかないとファイルのドロップが有効にならない
class CWin32DropFiles {
public:
	/// メッセージを処理する。
	/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
	/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
	bool processMessage(SWin32Msg *msg);

	/// ファイルリストを消す
	void clear();

	/// ファイル名を返す
	const wchar_t * getName(int index);

	/// ファイル数を返す
	int getCount();

private:
	std::vector<std::wstring> names_;
};


/// 特定のディレクトリ内のファイル変更を監視する
/// 変更があったことは分かるが、どのファイルが変更されたのかまでは分からない
class CWin32WatchDir {
public:
	static const DWORD DEF_FILTER = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE;

	CWin32WatchDir();
	~CWin32WatchDir();

	/// 監視を開始する。
	/// dir 監視するディレクトリ
	/// subdir サブディレクトリ内の変更も監視するかどうか
	/// filter 監視する項目の組み合わせを指定する。省略すると以下の値になる:
	///     FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE
	///     フラグの詳細は FindFirstChangeNotification  のマニュアルを参照
	bool start(const wchar_t *dir, bool subdir, DWORD filter=DEF_FILTER);

	/// 監視終了する
	void end();

	/// 前回の poll 呼び出し以降に変更が発生したかどうか調べる。
	/// ディレクトリ内のファイルが変更されていれば true を返す。
	/// 分かるのは変更の有無だけで、どのファイルに何の変更が発生したのかは分からない。
	/// 詳しい変更内容までチェックしたい時は CWin32WatchDirEx を使う
	bool poll();

private:
	HANDLE hNotif_;
};


/// 特定のディレクトリ内のファイル変更を監視する
/// 具体的に度のファイルが変更されたのかを知ることができる
class CWin32WatchDirEx {
public:
	static const DWORD DEF_FILTER = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE;

	CWin32WatchDirEx();
	~CWin32WatchDirEx();

	/// 監視を開始する
	/// dir 監視するディレクトリ
	/// subdir サブディレクトリ内の変更も監視するかどうか
	/// filter 監視する項目の組み合わせを指定する。省略すると以下の値になる:
	///     FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE
	///     フラグの詳細は FindFirstChangeNotification  のマニュアルを参照
	bool start(const wchar_t *dir, bool subdir, DWORD filter=DEF_FILTER);

	/// 監視終了する
	void end();

	/// 前回の poll 呼び出し以降に発生した内容変更をチェックする
	/// 変更があった場合は true を返す。poll が true を返した場合、
	/// read でその内容を読み取ることができる。
	bool poll();

	/// 変更内容を逐次取得する
	///
	/// データがある場合はその内容をセットして true を返す。
	/// データの終端に達している場合は false を返す
	///
	/// filename ファイル名
	/// maxlen ファイル名の最大文字数
	/// action 変更内容
	///    FILE_ACTION_ADDED
	///    FILE_ACTION_REMOVED
	///    FILE_ACTION_MODIFIED
	///    FILE_ACTION_RENAMED_OLD_NAME
	///    FILE_ACTION_RENAMED_NEW_NAME
	bool read(wchar_t *filename, int maxlen, int *action);

private:
	void _clear();
	std::string buf_;
	HANDLE hDir_;
	HANDLE hEvent_;
	DWORD filter_;
	OVERLAPPED overlap_;
	DWORD pos_;
	DWORD size_;
	bool subdir_;
	bool should_reset_;
};



class CWin32Ini {
public:
	CWin32Ini();
	CWin32Ini(const wchar_t *filename);
	void setFileName(const wchar_t *filename);
	void setString(const wchar_t *section, const wchar_t *key, const wchar_t *value);
	void setBinary(const wchar_t *section, const wchar_t *key, const void *buf, int size);
	void setInt(const wchar_t *section, const wchar_t *key, int value);
	void setFloat(const wchar_t *section, const wchar_t *key, float value);
	std::wstring getString(const wchar_t *section, const wchar_t *key);
	bool getBinary(const wchar_t *section, const wchar_t *key, void *buf, int size);
	int getInt(const wchar_t *section, const wchar_t *key, int def=0);
	float getFloat(const wchar_t *section, const wchar_t *key, float def=0.0f);

private:
	std::wstring fullpath_;
};




















class CBoxLayout {
public:
	struct CUR {
		/// 親ボックス内側の余白
		int margin_x;
		int margin_y;

		/// ボックス同士の間隔
		int space_x;
		int space_y;

		/// 次にボックスを置くべき座標
		int next_x, next_y;

		/// 何も配置しない行の高さ
		int min_row_h;

		/// 現在の配置行のサイズ
		int row_w;
		int row_h;

		/// 配置済みボックスの外接矩形
		int bound_w, bound_h;

		/// 親ボックスに必要なサイズ。
		/// 配置済みボックスの外接矩形にマージンを加えたもの
		int full_w, full_h;

		void *userdata;
	};

	CBoxLayout();
	void reset();

	/// ボックスを追加
	void addBox(int w, int h);

	/// 次の配置行へ移動
	void newLine();

	/// ネスト開始
	void push();

	/// ネスト終了
	/// ネスト内側のボックスサイズを返す
	void pop();

	CUR * cur();

	int x() { return cur_->next_x; }
	int y() { return cur_->next_y; }
	int abs_x() {
		int xx = 0;
		for (int i=depth_; i>=0; i--) {
			xx += stack_[i].next_x;
		}
		return xx;
	}
	int abs_y() {
		int yy = 0;
		for (int i=depth_; i>=0; i--) {
			yy += stack_[i].next_y;
		}
		return yy;
	}
	int bound_w() { return cur_->bound_w; }
	int bound_h() { return cur_->bound_h; }
	int full_w() { return cur_->full_w; }
	int full_h() { return cur_->full_h; }

private:
	static const int MAX_DEPTH = 8;
	CUR *cur_;
	CUR stack_[MAX_DEPTH];
	int depth_;
};


class CWin32DialogLayouter {
public:
	void init(HWND hWnd);

	/// ラベルを追加する
	HWND addLabel(int id, const wchar_t *text);

	/// ボタンを追加する
	HWND addButton(int id, const wchar_t *text);

	/// チェックボックスを追加する。value に初期値を指定する
	HWND addCheckBox(int id, const wchar_t *text, bool value);

	HWND addListBox(int id, int w, int h, int selected, const wchar_t *items[], int count);
	HWND addDropDownEdit(int id, int w, int h, int selected, const wchar_t *items[], int count, const wchar_t *text);
	HWND addDropDownList(int id, int w, int h, int selected, const wchar_t *items[], int count);

	HWND addEdit(int id, int w, int h, const wchar_t *text, bool readonly, bool multilines);

	HWND beginGroup();
	HWND beginGroup(int id, const wchar_t *text);
	void endGroup();

	void addHorzLine(int w);
	void addVertLine(int h);

	/// 任意のコントロールを追加する
	HWND makeControl(int id, const wchar_t *klass, const wchar_t *text, DWORD style, int w=0, int h=0);
	void addControl(HWND hCtrl, bool xFit, bool yFit);

	/// ツールヒントを追加する
	void setToolTip(int id, const wchar_t *text);

	void newLine();
	void space(int w, int h);
	void resizeToFit();
	HWND getCtrl(int id);

	void _autoResize(HWND hCtrl, bool xFit, bool yFit);

	HWND hTop_;
	HWND cur_hWnd();
#if 1
	int cur_x() { return lay_.abs_x(); }
	int cur_y() { return lay_.abs_y(); }
#else
	int cur_x() { return lay_.x(); }
	int cur_y() { return lay_.y(); }
#endif
	CBoxLayout lay_;
	HINSTANCE hInst_;

	/// コントロールのフォントサイズ
	int fontsize_;

	/// コントロール内側の文字と。コントロール境界の隙間
	int padding_x_;
	int padding_y_;

	/// コントロールの大きさを自動計算する
	bool auto_sizing_;
	bool add_to_bottom_; // コントロールを手前から奥に向かって追加する

	/// コントロールを配置するべき座標を、常にトップウィンドウの座標系で返す
	bool flatten_grouping_;
};


class CWin32Ctrls {
public:
	CWin32Ctrls();
	void add(HWND hWnd);
	bool isClicked(HWND hWnd, bool keep_flag=false);

	/// メッセージを処理する。
	/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
	/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
	bool onMessage(SWin32Msg *msg);

private:
	std::unordered_map<HWND, LONG> items_;
};

bool Win32CheckBox_IsChecked(HWND hWnd);
void Win32CheckBox_SetChecked(HWND hWnd, bool checked);


class IWin32ImmCallback {
public:
	virtual void on_win32imm_query_text_position(POINT *pos) = 0;
	virtual void on_win32imm_insert_char(wchar_t chr) = 0;
};

/// IMMメッセージを処理する。
/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
bool Win32_ProcMessage_Imm(SWin32Msg *msg, IWin32ImmCallback *cb);



/// ウィンドウまたはウィンドウコントロールをサブクラス化する。
/// 要するに、独自のウィンドウプロシージャを適用する。
class CWin32Subclass {
public:
	CWin32Subclass();

	/// HWND に新しいメッセージプロシージャを割り当てる。
	/// 元々設定されていたプロシージャは自動的に退避する。
	/// 退避したプロシージャは getPrevProc() で取得できる
	void attachWindow(HWND hWnd);

	/// HWND に対するメッセージプロシージャを解除し、
	/// 元々設定されていたプロシージャに戻す
	void detachWindow(HWND hWnd);

	/// 元々設定されていたプロシージャを返す
	WNDPROC getPrevProc();

	/// このサブクラスで処理するメッセージ
	/// メッセージを処理した場合は true を返し、何もしなかった場合は false を返す
	/// （呼出し後、必ず msg の bShouldReturn, lResult を確認すること。詳細は SWin32Msg を参照）
	virtual bool onMessage(SWin32Msg *msg) = 0;

private:
	static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	WNDPROC prev_proc_;
};




#ifndef NO_TSF
/// IMM/TSF 側からテキスト入力コントロールへの各種問い合わせのためのインターフェース
class ITsfCallback {
public:
	/// 選択範囲の開始またはキャレットの文字インデックス
	virtual int on_tsf_selstart() = 0;

	/// 選択範囲の終端またはキャレットの文字インデックス
	virtual int on_tsf_selend() = 0;

	/// NULL終端テキストを返す
	virtual const wchar_t * on_tsf_text() = 0;

	/// テキストの文字数
	virtual int on_tsf_textsize() = 0;

	/// 選択範囲を設定する。start == end の場合は、単なるキャレットの移動と同義
	virtual void on_tsf_setselection(int start, int end) = 0;

	/// 指定範囲のテキストを含む矩形を返す（スクリーン座標系）
	/// 矩形がウィンドウ外にはみ出ている場合（＝クリッピング状態にある場合）は is_clipped に 1 をセットする
	virtual void on_tsf_gettextrect(int start, int end, RECT *rect, int *is_clipped) = 0;

	/// 編集エリア全体の矩形を返す（スクリーン座標系）
	virtual void on_tsf_getwholerect(RECT *rect) = 0;

	/// ウィンドウハンドル
	virtual HWND on_tsf_gethandle() = 0;

	/// 編集位置に変換結果または変換途中のテキストを挿入する
	virtual void on_tsf_InsertText(const wchar_t *text) = 0;

	/// その他の通知
	virtual void on_tsf_Notify(const wchar_t *n) = 0;
};



class CWin32TextStoreACP: public ITextStoreACP, public ITfContextOwnerCompositionSink {
public:
	CWin32TextStoreACP();
	~CWin32TextStoreACP();
	bool Init();
	void Shutdown();
	void SetEdit(ITsfCallback *edit); // エディタを関連付ける。これをしないと何もできないので注意！
	void SetFocus(); // 入力フォーカスをセットする。これを忘れるとウィンドウ位置が同期できないので注意！
	void UpdateSize(); // ウィンドウサイズが変化したときなど、入力エリアのサイズが変わったときに呼ぶ
	bool DebugPrintCandidates();// 変換候補を表示
	bool GetCharAttr(int index, TF_DISPLAYATTRIBUTE *pDispAttr);

	//IUnknown methods
	STDMETHOD_ (DWORD, AddRef)() override;
	STDMETHOD_ (DWORD, Release)() override;
	STDMETHOD (QueryInterface)(REFIID riid, void** ppReturn) override;

	// ITextStoreACP methods
	STDMETHODIMP AdviseSink(REFIID riid, IUnknown *pUnknown, DWORD dwMask) override;
	STDMETHODIMP UnadviseSink(IUnknown* pUnknown) override;
	STDMETHODIMP RequestLock(DWORD flags, HRESULT *p_hr) override;
	STDMETHODIMP GetStatus(TS_STATUS *status) override;
	STDMETHODIMP QueryInsert(LONG start, LONG end, ULONG len, LONG *out_start, LONG *out_end) override;
	STDMETHODIMP GetSelection(ULONG ulIndex, ULONG ulCount, TS_SELECTION_ACP *pSelection, ULONG *pFetched) override;
	STDMETHODIMP SetSelection(ULONG numSelections, const TS_SELECTION_ACP *selections) override;
	STDMETHODIMP GetText(LONG acpStart, LONG acpEnd, PWSTR pchPlain, ULONG cchPlainReq, ULONG* pcchPlainOut, TS_RUNINFO* prgRunInfo, ULONG ulRunInfoReq, ULONG* pulRunInfoOut, LONG* pacpNext) override;
	STDMETHODIMP SetText(DWORD dwFlags, LONG acpStart, LONG acpEnd, PCWSTR pchText, ULONG cch, TS_TEXTCHANGE* pChange) override;
	STDMETHODIMP GetFormattedText(LONG acpStart, LONG acpEnd, IDataObject** ppDataObject) override;
	STDMETHODIMP InsertEmbeddedAtSelection(DWORD dwFlags, IDataObject* pDataObject, LONG* pacpStart, LONG* pacpEnd, TS_TEXTCHANGE* pChange) override;
	STDMETHODIMP GetEmbedded(LONG acpPos, REFGUID rguidService, REFIID riid, IUnknown** ppunk) override;
	STDMETHODIMP QueryInsertEmbedded(GUID const* pguidService, FORMATETC const* pFormatEtc, BOOL* pfInsertable) override;
	STDMETHODIMP InsertEmbedded(DWORD dwFlags, LONG acpStart, LONG acpEnd, IDataObject* pDataObject, TS_TEXTCHANGE* pChange) override;
	STDMETHODIMP RequestSupportedAttrs(DWORD dwFlags, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs) override;
	STDMETHODIMP RequestAttrsAtPosition(LONG acpPos, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs, DWORD dwFlags) override;
	STDMETHODIMP RequestAttrsTransitioningAtPosition(LONG acpPos, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs, DWORD dwFlags) override;
	STDMETHODIMP FindNextAttrTransition(LONG acpStart, LONG acpHalt, ULONG cFilterAttrs, TS_ATTRID const* paFilterAttrs, DWORD dwFlags, LONG* pacpNext, BOOL* pfFound, LONG* plFoundOffset) override;
	STDMETHODIMP RetrieveRequestedAttrs(ULONG count, TS_ATTRVAL *pAttrVals, ULONG *pFetched) override;
	STDMETHODIMP GetEndACP(LONG *pEnd) override;
	STDMETHODIMP GetActiveView(TsViewCookie* pvcView) override;
	STDMETHODIMP GetACPFromPoint(TsViewCookie vcView, POINT const* pt, DWORD dwFlags, LONG* pacp) override;
	STDMETHODIMP GetTextExt(TsViewCookie vcView, LONG acpStart, LONG acpEnd, RECT *pRect, BOOL *pIsClipped) override;
	STDMETHODIMP GetScreenExt(TsViewCookie vcView, RECT *pRect) override;
	STDMETHODIMP GetWnd(TsViewCookie vcView, HWND *p_hWnd) override;
	STDMETHODIMP InsertTextAtSelection(DWORD dwFlags, PCWSTR pwszText, ULONG cch, LONG* pacpStart, LONG* pacpEnd, TS_TEXTCHANGE* pChange) override;

	// ITfContextOwnerCompositionSink methods
	virtual HRESULT STDMETHODCALLTYPE OnStartComposition(ITfCompositionView *pComposition, BOOL *pfOk) override;
	virtual HRESULT STDMETHODCALLTYPE OnUpdateComposition(ITfCompositionView *pComposition, ITfRange *pRangeNew) override;
	virtual HRESULT STDMETHODCALLTYPE OnEndComposition(ITfCompositionView *pComposition) override;

private:
	bool MakeCharRange(int index, ITfRange **ppRange);
	bool IsLocked(DWORD lockType);
	BOOL LockDocument(DWORD lockFlags);
	void UnlockDocument();
	int SelStart();
	int SelEnd();
	const wchar_t * Text();
	int TextSize();

	ITsfCallback *mEdit;
	TfClientId mClientID;
	ITfThreadMgr *mThreadMgr;
	ITfDocumentMgr *mDocMgr;
	ITfContext *mContext;
	TfEditCookie mEditCookie;
	ITextStoreACPSink *mSink;
	DWORD mSinkMask;
	DWORD mRefCnt;
	DWORD mDocLocked;
	ULONG mOldTextSize;
	TsActiveSelEnd mActiveSelEnd; // 選択状態のとき、カーソルが前後どちらに存在するか
	ITfFunctionProvider *mFuncProv;
	ITfFnReconversion *mReconv;
	ITfCategoryMgr *mCategoryMgr;
	ITfDisplayAttributeMgr *mDispMgr;
	ITfCompositionView *mCompositionView;
	bool mIsPendingLockUpgrade; // 保留中の非同期ロックの有無
	bool mIsLayoutChanged; // 保留中のレイアウト変化通知の有無
	bool mIsInterimChar; // IME処理中の文字が存在するかどうか
};
#endif // NO_TSF
