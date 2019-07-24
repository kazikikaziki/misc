#pragma once
#include <stdio.h> // FILE


#define __STR(x) #x
#define __STRSTR(x)  __STR(x)

// __LINE__ を文字列化させる
// ダブルクォートで囲まれた行番号を得る
#define __LINE_STR__  __STRSTR(__LINE__)


/// assert の代わり。評価に失敗した場合はエラー処理をする
#ifndef NO_ASSERT
#  define Log_assert(x)  (x) ? (void)0: KLog::assertion(__FILE__, __LINE__, #x)
#else
#  define Log_assert(x)  (void)0
#endif


/// ソースコードの位置を出力する
/// トレースメッセージを出力するかどうかは Log_trace_enabled で設定できる
#ifdef TRACE
#	define Log_trace        Log_TRACE
#	define Log_trace_scope  Log_SCOPE
#else
#	define Log_trace         /* nothing */
#	define Log_trace_scope   /* nothing */
#endif

#define Log_TRACE      KLog::printTrace(__FILE__, __LINE__, __FUNCTION__)
#define Log_SCOPE      KLogScopedLog __log__(__FILE__, __LINE__, __FUNCTION__)
#define Log_text       KLog::print
#define Log_textf      KLog::printf
#define Log_fatal      KLog::fatal
#define Log_error      KLog::error
#define Log_errorf     KLog::errorf
#define Log_warning    KLog::warning
#define Log_warningf   KLog::warningf
#define Log_info       KLog::info
#define Log_infof      KLog::infof
#define Log_debug      KLog::debug
#define Log_debugf     KLog::debugf
#define Log_verbose    KLog::verbose
#define Log_verbosef   KLog::verbosef



/// スコープのトレース用。コンストラクタとデストラクタでログ出力を行う
///
/// コンストラクタおよびデストラクタでログへの出力を行うことで、
/// スコープから抜けたときに自的に記録できる。
/// このクラスを直接使うことはあまりない。基本的に以下のようにしてマクロから使う
/// @code {.cpp}
///		#define TRACE   KLogScopedLog tmp(__FILE__, __LINE__, __FUNCTION__)
///		void sub() {
///			TRACE;
///			...
///		}
/// @endcode
/// @see Log_trace_scope
class KLogScopedLog {
public:
	KLogScopedLog(const char *file, int line, const char *func);
	~KLogScopedLog();
private:
	char file_[256];
	char func_[256];
	int line_;
};


/// KLog のイベントコールバック用のクラス
/// @see KLog::setOutputCallback
class KLogCallback {
public:
	/// ログ出力の直前でコールバックされる。
	/// @param level        ログの属性
	///                     - KLog::CHAR_AST アサーション
	///                     - KLog::CHAR_ERR エラー
	///                     - KLog::CHAR_WRN 警告
	///                     - KLog::CHAR_INF 情報
	///                     - KLog::CHAR_DBG デバッグ
	///                     - KLog::CHAR_VRB 詳細
	///                     - KLog::CHAR_NUL 無属性
	/// @param text_u8     出力しようとしている文字列(utf8)
	/// @param processed   ログの処理が完了したなら 1 をセットする。
	///                    その場合、既定のログ処理は行われず、コンソールやテキストファイルなどにも出力しない
	virtual void on_log_output(char level, const char *text_u8, int *processed) = 0;
};

class KLog {
public:
	enum {
		CHAR_AST = 'A',
		CHAR_ERR = 'E',
		CHAR_WRN = 'W',
		CHAR_INF = 'I',
		CHAR_DBG = 'D',
		CHAR_VRB = 'V',
		CHAR_NUL = '\0',
	};

	/// コマンドを実行し、戻り値を得る
	/// - command("output_console") : コンソールに出力するかどうかを 1 または 0 で返す
	/// - command("output_file") : テキストファイルに出力するかどうかを 1 または 0 で返す
	/// - command("level_info") : Info レベルのログが出力されるかどうかを 1 または 0 で返す
	/// - command("level_debug") : Debug レベルのログが出力されるかどうかを 1 または 0 で返す
	/// - command("level_verbose") : Verbose レベルのログが出力されるかどうかを 1 または 0 で返す
	static int command(const char *cmd);

	/// ログの区切り線よりも前の部分を削除する
	/// @see printCutline
	///
	/// @param file_u8  テキストファイル名(utf8)
	/// @param number   区切り線の番号。0起算での区切り線インデックスか、負のインデックスを指定する。
	///                 正の値 n を指定すると、0 起算で n 本目の区切り線を探し、それより前の部分を削除する。
	///                 （毎回一番最初に区切り線を出力している場合、最初の n 回分のログが削除される）
	///                 負の値 -n を指定すると、末尾から n-1 本目の区切り線を探し、それより前の部分を削除する。
	///                 （毎回一番最初に区切り線を出力している場合、最新の n 回分のログだけ残る）
	static bool clampFileByCutline(const char *file_u8, int number=-1);

	/// ログが指定サイズになるよう、前半部分を削除する
	///
	/// @param file_u8    テキストファイル名(utf8)
	/// @param size_bytes サイズをバイト単位で指定する。
	///                   このサイズに収まるように、適当な改行文字よりも前の部分が削除される
	static bool clampFileBySize(const char *file_u8, int size_bytes);

	/// コールバック関数への出力を設定する。
	static void setOutputCallback(KLogCallback *cb);

	/// テキストファイルへの出力を設定する。
	/// NULLを指定すると出力を停止する
	/// 指定されたファイルを追記モードで開く
	///
	/// @param filename_u8 テキストファイル名(utf8)
	static void setOutputFile(const char *filename_u8);

	/// コンソールウィンドウへの出力を設定する
	static void setOutputConsole(bool value);

	/// Infoレベルのメッセージを表示するかどうか
	static void setLevelInfo(bool value);

	/// Debugレベルのメッセージを表示するかどうか
	static void setLevelDebug(bool value);

	/// Verboseレベルのメッセージを表示するかどうか
	static void setLevelVerbose(bool value);

	/// エラーレベルのメッセージが発生したとき、既定のダイアログを出すかどうか。
	/// エラーが起きた時にすぐに気が付くよう、デフォルトでは true になっている。
	static void setErrorDialog(bool value);

	/// テキストを出力する。
	/// ユーザーによるコールバックを通さず、既定の出力先に直接書き込む。
	/// コールバック内からログを出力したい時など、ユーザーコールバックの再帰呼び出しが邪魔になるときに使う
	static void emit(char type, const char *s);

	/// テキストを完全に無加工で出力する。
	/// コールバックも、文字コード変換も行わず、そのまま fputs などに渡す
	static void emitRaw(const char *raw_text);

	/// テキストを出力する。
	/// type には属性を表す文字を指定する。'E'rror, 'W'arning, 'I'nfo, 'D'ebug, 'V'erbose 無属性は '\0'
	static void report(char type, const char *msg);
	static void reportv(char type, const char *fmt, va_list args);
	static void reportf(char type, const char *fmt, ...);

	/// 無属性テキストを出力する。
	/// 無属性テキストはフィルタリングされず、常に表示される
	static void print(const char *msg);
	static void printf(const char *fmt, ...);

	/// ログの区切り線を無属性テキストとして出力する
	/// この区切り線は古いログを削除するときの境界として使う
	/// @see clampFileByCutline
	static void printCutline();

	/// バイナリデータの16進数表現を無属性テキストとして出力する。
	/// sizeに 0 を指定した場合は、ヌル文字終端の文字列が指定されたとみなす
	static void printBinary(const void *data, int size);

	/// ファイル名、行番号を無属性テキストとして出力する。
	/// このメソッドを直接呼ぶことはあまりない。基本的にはマクロから使う
	/// @code
	/// #define TRACE  KLog::printTree(__FILE__, __LINE__, __FUNCTION__)
	/// @endcode
	static void printTrace(const char *file, int line, const char *func=NULL);

	/// 致命的エラーテキストを表示し、プログラムを exit(-1) で強制終了する。
	/// 続行不可能なエラーが発生したときに使う。
	static void fatal(const char *msg);

	/// エラーテキストを出力する。
	/// setErrorDialog() で true を指定した場合、エラーダイアログが自動的に表示される
	static void error(const char *msg);
	static void errorf(const char *fmt, ...);

	/// 警告テキストを出力する
	static void warning(const char *msg);
	static void warningf(const char *fmt, ...);

	/// 情報テキストを出力する
	static void info(const char *msg);
	static void infof(const char *fmt, ...);

	/// デバッグテキストを出力する
	static void debug(const char *msg);
	static void debugf(const char *fmt, ...);

	/// 詳細なデバッグテキストを出力する
	static void verbose(const char *msg);
	static void verbosef(const char *fmt, ...);

	/// アサーションメッセージを出力する
	static void assertion(const char *file, int line, const char *expr);

	/// ログのログを出力する
	static void loglog(const char *fmt, ...);
};

void Test_log();
