#pragma once
#include "KPath.h"
#include "KLocalTime.h"

namespace mo {

struct PlatformExtraInfo {
	/// プライマリモニタの画面全体の幅と高さ
	int screen_w;
	int screen_h;

	/// プライマリモニタの最大化ウィンドウのサイズ
	int max_window_w;
	int max_window_h;

	/// プライマリモニタの最大化ウィンドウのクライアント領域の幅と高さ
	int max_window_client_w;
	int max_window_client_h;

	/// キーリピート認識までの遅延時間と間隔（ミリ秒）
	int keyrepeat_delay_msec;
	int keyrepeat_interval_msec;

	/// ダブルクリックの認識設定
	/// double_click_dx   ２回目のクリックを受け付ける許容X範囲（１回目のクリック位置を基準とする。ピクセル単位）
	/// double_click_dy   ２回目のクリックを受け付ける許容Y範囲
	/// double_click_time ２回目のクリックを受け付ける許容時間（ミリ秒）
	int double_click_dx;
	int double_click_dy;
	int double_click_msec;
};

void path_normalize(wchar_t *s, const Path &path);


/// 実行環境に強く依存する動作や、情報取得などを行うためのクラス
class Platform {
public:
	/// カレントディレクトリを得る
	static Path getCurrentDir();

	/// カレントディレクトリを変更する
	static bool setCurrentDir(const Path &path);

	/// 指定されたパスがディレクトリかどうか
	/// ディレクトリが存在するかどうかを調べる
	///
	/// @retval true  存在する
	/// @retval false 存在しない
	static bool directoryExists(const Path &path);

	/// 指定されたパスが存在するかどうか
	///
	/// @retval true ファイルまたはディレクトリが存在する
	/// @retval false 存在しない
	static bool fileExists(const Path &path);

	/// ファイルをコピーする
	///
	/// @param src コピー元ファイル名
	/// @param dest コピー先ファイル名
	/// @param overwrite 上書きするかどうか
	/// @arg true  コピー先に同名のファイルが存在するなら上書きし、成功したら true を返す
	/// @arg false コピー先に同名のファイルがある場合にはコピーをせず、false を返す
	/// @return 成功したかどうか
	static bool fileCopy(const Path &src, const Path &dest, bool overwrite);

	/// ディレクトリを作成する
	///
	/// 既にディレクトリが存在する場合は成功したとみなす。
	/// ディレクトリが既に存在するかどうかを確認するためには directoryExists() を使う
	static bool makeDirectory(const Path &dir, int *err=NULL);

	/// ファイルを削除する
	///
	/// で指定されたファイルを削除し、削除できたなら true を返す。
	/// 指定されたファイルが初めから存在しない場合は削除に成功したものとして true を返す。
	/// それ以外の場合は false を返す
	/// @attention ディレクトリは削除できない
	static bool removeFile(const Path &path);

	static bool isRemovableDirectory(const Path &dir);

	/// dir ディレクトリ内にある全ての非ディレクトリファイルを削除する
	/// subdir が true の場合は子ディレクトリに対して再帰的に removeFilesInDirectory を実行する
	/// 全てのファイルの削除に成功した時のみ true を返す。初めからファイルが存在しなかった場合は成功とみなす
	static bool removeFilesInDirectory(const Path &dir, bool subdir);

	/// dir ディレクトリ内にファイルが存在しなければ、そのディレクトリを削除する
	/// subdir が true の場合は子ディレクトリに対して再帰的に removeEmptyDirectory を実行する。
	/// subdir_only が true の場合、dir の中身だけを削除して、dir 自体は削除しない
	/// 削除に成功した場合は true を返す。初めからファイルが存在しなかった場合は成功とみなす
	///
	static bool removeEmptyDirectory(const Path &dir, bool subdir, bool subdir_only=false);

	/// ファイルの最終更新日時を得る
	static time_t getLastModifiedFileTime(const Path &filename);

	static int getFileSize(const Path &filename);

	/// ファイルの CRC32 値を計算する。
	/// 計算不能な場合は CRC32 のデフォルト値 mo::crc32_default を返す
	static uint32_t getFileCRC32(const Path &filename);

	/// システム依存の情報を得る。取得できない項目は 0 にセットされる
	static void getExtraInfo(PlatformExtraInfo *info);

	/// 矩形がモニタの表示領域と重なっていれば true を返す。
	/// 矩形がどのモニタの表示領域からも外れていれば false を返す
	/// completely に true を指定した場合、完全に内包している場合のみ true を返す
	/// intersection_w, intersection_h が指定されている場合は、最も大きく重なっている部分のサイズを返す
	/// 複数のモニタにまたがっている場合、最も大きく重なっている部分のサイズを返す
	static bool rectIntersectAnyScreen(int x, int y, int w, int h, int *intersection_w, int *intersection_h);

	/// 指定されたウィンドウを含んでいるモニタの解像度を返す。
	/// ウィンドウ識別子に NULL が指定された場合は、現在のスレッドのアクティブウィンドウを使う。
	/// 現在のスレッドに関連付けられたウィンドウが存在しない場合はプライマリモニタの解像度を返す
	/// @param wid ウィンドウ識別子。Win32 なら HWND と同じ
	static void getScreenSizeByWindow(void *hWnd, int *w, int *h);

	/// 指定されたシステムフォントファイルの絶対パスを返す
	/// 取得できない場合は空文字列 "" を返す
	static Path getSystemFontFileName(const Path &filename);

	/// 同一アプリケーションがすでに起動中かどうか調べる。起動中の場合は true を返す
	static bool isApplicatoinAlreadyRunning();

	/// システムにインストールされているフォントの情報を得る
	/// index フォントのインデックス（内部でのフォントの順序値であり、何らかの方法でソートされているとは限らない）
	/// face  みつかったフォント名（NULLでもよい）
	/// path  みつかったフォントのファイル名（NULLでもよい）
	/// 取得できた場合は face と path に値をセットして true を返す
	/// 取得できない場合は何もせずに false を返す
	static bool getFontInfo(int index, Path *face, Path *path);

	/// システムフォントのインストールディレクトリを得る
	static Path getSystemFontDirectory();

	/// 指定されたフォルダをシステム標準の方法で開く
	static bool openFolderInFileBrowser(const Path &path);

	/// 指定されたファイルをシステム標準の方法で開く
	static bool openFileInSystem(const Path &path);

	/// 自分自身の実行ファイル名を得る
	static Path getSelfFileName();

	/// 自分自身のプロセスIDを得る
	static uint32_t getSelfProcessId();

	/// 現座時刻を得る
	static std::string getLocalTimeString();
	static KLocalTime getLocalTime();

	/// 物理メモリ情報を得る
	/// ※メモリが1TB以上ある場合は KB の値が符号付きintの範囲を超えるため、正しい値を返さない
	/// total 物理メモリ総容量(KB)
	/// avail 物理メモリ空き容量(KB)
	/// used 物理メモリ使用量(KB)
	static bool getPhysicalMemorySize(int *total_KB, int *avail_KB, int *used_KB);

	/// メッセージダイアログを表示する（選択肢なし）
	///
	/// 具体的な動作は実装に依存する。
	/// [OK]ボタン付きのダイアログかもしれないし、バルーン、トーストのような通知かもしれない。
	/// text_u8 -- メッセージ文字列 (UTF8)
	static void dialog(const char *text_u8);

	/// 自分を強制終了する
	static void killSelf();
};



} // namespace
