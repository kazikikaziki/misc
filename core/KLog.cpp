#include "KLog.h"
#include <stdlib.h> // exit()
#include <stdarg.h>
#include <ctype.h>
#include <time.h> // timespec
#include <mutex>

#ifdef _WIN32
#include <Windows.h>
#endif


#if 1
#	include "KStd.h"
#	define LOGASSERT(x)          K_assert(x)
#	define FOPEN_U8(f, m)        K_fopen_u8(f, m)
#	define GETTIME_MSEC()        K_gettime_msec()
#	define GETPID()              K_getpid()
#	define GETLOCALTIME(t, ms)   K_current_localtime(t, ms)
#else
#	include <assert.h> 
#	define LOGASSERT(x)          assert(x)
#	define FOPEN_U8(f, m)        fopen(f, m)
#	define GETTIME_MSEC()        GetTickCount()
#	define GETPID()              GetCurrentProcessId()
#	define GETLOCALTIME(t, ms)   { time_t gmt=time(NULL); *(t) = *localtime(&gmt); *(ms) = 0; }
#endif


// ログ自身の詳細ログを取る
#ifndef LOGLOG
#	define LOGLOG 0
#endif

#if LOGLOG
#	define LOGLOG_BEGIN(type, u8) _LogLogBegin(__FUNCTION__, type, u8)
#	define LOGLOG_END()           _LogLogEnd(__FUNCTION__)
#else
#	define LOGLOG_BEGIN(type, u8)  /* empty */
#	define LOGLOG_END()            /* empty */
#endif


#define SPRINTF_BUFFER_SIZE 1024


// CUTLINEには末尾の改行を含めないようにする
// 改行文字 \n が \r\n に変換されてしまう可能性があるため、CUTLINE で検索したときに不一致になり見つけられない可能性があるため
#define CUTLINE  "-------------------------------------------------------------------"


static void _LogLogBegin(const char *func, char type, const char *u8) {
	// ログの詳細ログ。目印として行頭を # にしておく
	KLog::loglog(
		"# %s [%c] \"%s\"",
		func ? func : "(NULL)",
		isprint(type) ? type : ' ',
		u8
	);
}
static void _LogLogEnd(const char *func) {
	// ログの詳細ログ。目印として行頭を # にしておく
	KLog::loglog(
		"# %s [END]",
		func ? func : "(NULL)"
	);
}


struct SMessageItem {
	SMessageItem() {
		memset(&localtime, 0, sizeof(localtime));
		localtime_tm_msec = 0;
		app_msec = 0;
		type = 0;
		text_u8 = 0;
	}


	// ローカル時刻（秒単位まで）
	struct tm localtime;
	
	// ローカル時刻のミリ秒部分(0-999)
	int localtime_tm_msec;

	// アプリケーション起動からの経過時間をミリ秒単位で表した値
	int app_msec;

	// メッセージ属性
	// KLog::CHAR_ERR や KLog::CHAR_INFO など
	char type;

	// メッセージ文字列
	const char *text_u8;
};


#ifdef _WIN32
class CNative {
	// コンソールウィンドウの文字属性コード
	enum COLORCODE {
		BG_BLACK  = 0,
		BG_PURPLE = BACKGROUND_RED|BACKGROUND_BLUE,
		BG_WHITE  = BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY,
		BG_RED    = BACKGROUND_RED|BACKGROUND_INTENSITY,
		BG_YELLOW = BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_INTENSITY,
		BG_LIME   = BACKGROUND_GREEN|BACKGROUND_INTENSITY,
		BG_SILVER = BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE,
		BG_TEAL   = BACKGROUND_GREEN|BACKGROUND_BLUE,
		BG_GREEN  = BACKGROUND_GREEN,
		BG_BLUE   = BACKGROUND_BLUE|BACKGROUND_INTENSITY,

		FG_BLACK  = 0,
		FG_PURPLE = FOREGROUND_RED|FOREGROUND_BLUE,
		FG_WHITE  = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY,
		FG_RED    = FOREGROUND_RED|FOREGROUND_INTENSITY,
		FG_YELLOW = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,
		FG_LIME   = FOREGROUND_GREEN|FOREGROUND_INTENSITY,
		FG_SILVER = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE,
		FG_TEAL   = FOREGROUND_GREEN|FOREGROUND_BLUE,
		FG_GREEN  = FOREGROUND_GREEN,
		FG_BLUE   = FOREGROUND_BLUE|FOREGROUND_INTENSITY,
	};

	// UTF8からワイド文字列への変換
	static void utf8ToWide(std::wstring &out, const char *u8) {
		LOGASSERT(u8);
		int outlen = MultiByteToWideChar(CP_UTF8, 0, u8, -1, NULL, 0); // 変換後の文字数（終端文字含む）
		if (outlen > 0) {
			out.resize(outlen+8); // 念のため、サイズに余裕を持たせる
			MultiByteToWideChar(CP_UTF8, 0, u8, -1, &out[0], out.size());
			size_t size = wcslen(out.c_str());
			out.resize(size); // wstring::size と wcslen を一致させる
		}
	}
	
	// ワイド文字からANSI文字列への変換
	static void wideToAnsi(std::string &out, const wchar_t *ws) {
		LOGASSERT(ws);
		int outlen = WideCharToMultiByte(CP_ACP, 0, ws, -1, NULL, 0, NULL, NULL); // 変換後の文字数（終端文字含む）
		if (outlen > 0) {
			out.resize(outlen+8); // 念のため、サイズに余裕を持たせる

			// Wide --> Ansi 変換
			// フラグ（第二引数）に注意。変換失敗してもエラーにせず無視する。
			// ログ出力のプロセスをなるべく邪魔しないようにする
			WideCharToMultiByte(CP_ACP, 0, ws, -1, &out[0], out.size(), NULL, NULL);
			size_t size = strlen(out.c_str());
			out.resize(size); // string::size と strlen を一致させる
		}
	}

	// コンソールウィンドウの文字属性コードを返す
	static uint16_t getConsoleAttr() {
		CONSOLE_SCREEN_BUFFER_INFO info;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
		return info.wAttributes;
	}

	// コンソールウィンドウの文字属性コードを設定する
	static void setConsoleAttr(uint16_t flags) {
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), flags);
	}

public:
	static void showConsoleWindow() {
		AllocConsole();
		freopen("CON", "w", stdout);
		freopen("CON", "r", stdin);
	}
	static void hideConsoleWindow() {
		// TIPS:
		// freopen でコンソールウィンドウに関連付けた stdin, stdout を戻しておかないと、
		// FreeConsole を呼んでもウィンドウが閉じない
		freopen("NUL", "w", stdout);
		freopen("NUL", "r", stdin);
		FreeConsole();
	}
	static void printDebugger(const char *text_u8) {
		LOGASSERT(text_u8);
		std::wstring ws;
		utf8ToWide(ws, text_u8);
		OutputDebugStringW(ws.c_str());
	}

	// メッセージ属性に応じた色の組み合わせを得る。
	// ※この色の組み合わせの元ネタ
	// https://bitbucket.org/brunobraga/logcat-colorize
	//
	// type 属性
	// typecolor 属性文字列の前景＆背景色
	// msgcolor  メッセージ文字列の前景＆背景色
	static void getConsoleColorCode(char type, uint16_t *typecolor, uint16_t *msgcolor) {
		uint16_t f1 = 0;
		uint16_t f2 = 0;
		switch (type) {
		case KLog::CHAR_ERR:
			f1 = BG_RED|FG_WHITE;
			f2 = BG_BLACK|FG_RED;
			break;

		case KLog::CHAR_WRN:
			f1 = BG_YELLOW|FG_BLACK;
			f2 = BG_BLACK|FG_YELLOW;
			break;

		case KLog::CHAR_INF:
			f1 = BG_TEAL|FG_WHITE;
			f2 = BG_BLACK|FG_TEAL;
			break;

		case KLog::CHAR_DBG:
			f1 = BG_PURPLE|FG_WHITE;
			f2 = BG_BLACK|FG_PURPLE;
			break;

		case KLog::CHAR_VRB:
			f1 = BG_BLUE|FG_WHITE;
			f2 = BG_BLACK|FG_BLUE;
			break;
		}
		*typecolor = f1;
		*msgcolor = f2;
	}
	static void printConsole(const SMessageItem &msg) {

		// 現在の設定を退避
		uint16_t restore_flags = getConsoleAttr();

		// 時刻
		{
			setConsoleAttr(BG_BLACK|FG_PURPLE);
			printf("%02d:%02d:%02d.%03d (%d) ",
				msg.localtime.tm_hour,
				msg.localtime.tm_min,
				msg.localtime.tm_sec,
				msg.localtime_tm_msec,
				msg.app_msec
			);
		}

		// 色を取得
		uint16_t typecolor = 0;
		uint16_t msgcolor = 0;
		getConsoleColorCode(msg.type, &typecolor, &msgcolor);

		// メッセージ属性
		if (msg.type != KLog::CHAR_NUL) {
			char tp = (char)(msg.type & 0x7F); // 念のため ascii 範囲に収まるようにしておく
			setConsoleAttr(typecolor);
			printf(" %c ", tp);
		}

		// メッセージ文字列
		{
			std::string mb; // 現ロケールでのマルチバイト文字列
			std::wstring ws;
			utf8ToWide(ws, msg.text_u8); // UTF8-->ANSIの直接変換はできないので一度ワイド文字を経由する
			wideToAnsi(mb, ws.c_str()); // 現ロケールでのマルチバイト文字列
			setConsoleAttr(msgcolor);
			printf(" %s\n", mb.c_str());
		}

		// 文字属性を元に戻す
		setConsoleAttr(restore_flags);
	}
	static void printConsole(const char *u8) {
		LOGASSERT(u8);
		// 現ロケールでのマルチバイト文字列で出力
		std::wstring ws;
		std::string mb;
		utf8ToWide(ws, u8);
		wideToAnsi(mb, ws.c_str());
		fputs(mb.c_str(), stdout);
		fputs("\n", stdout);
	}
	static void showDialog(const char *title_u8, const char *msg_u8) {
		std::wstring wtitle;
		std::wstring wmsg;
		utf8ToWide(wtitle, title_u8);
		utf8ToWide(wmsg, msg_u8);
		int btn = MessageBoxW(NULL, wmsg.c_str(), wtitle.c_str(), MB_ICONSTOP|MB_ABORTRETRYIGNORE);
		if (btn == IDABORT) exit(-1);
		if (btn == IDRETRY) DebugBreak();
	}
};
#endif // _WIN32













class CLogContext {
public:
	KLogCallback *callback_;
	uint32_t start_msec_;
	FILE *file_;
	bool debugger_;
	bool console_;
	bool info_enabled_;
	bool debug_enabled_;
	bool verbose_enabled_;
	bool dialog_enabled_;

	CLogContext() {
		callback_ = NULL;
		start_msec_ = GETTIME_MSEC();
		file_ = NULL;
		debugger_ = true;
		console_ = false;
		info_enabled_ = true;
		debug_enabled_ = true;
		verbose_enabled_ = false;
		dialog_enabled_ = true;
	}

	bool isLevelEnabled(char type) const {
		if (type==KLog::CHAR_VRB && !verbose_enabled_) return false;
		if (type==KLog::CHAR_DBG && !debug_enabled_) return false;
		if (type==KLog::CHAR_INF && !info_enabled_) return false;
		return true;
	}
};

static CLogContext s_ctx;
static std::recursive_mutex s_mtx;




class CLogFile {
public:
	// ファイルから文字列をロードする。Utf8であることを期待して処理する
	static void load(const char *filename_u8, std::string &bin) {
		LOGASSERT(filename_u8);
		FILE *file = FOPEN_U8(filename_u8, "r");
		if (file) {
			char buf[1024];
			while (1) {
				int sz = fread(buf, 1, sizeof(buf), file);
				if (sz == 0) break;
				bin.append(buf, sz);
			}
			fclose(file);
		}
	}
	// 指定されたメッセージをファイルに書き出す
	static void save(const char *filename_u8, const char *bin) {
		LOGASSERT(filename_u8);
		LOGASSERT(bin);
		FILE *file = FOPEN_U8(filename_u8, "w");
		if (file) {
			fwrite(bin, strlen(bin), 1, file);
			fclose(file);
		}
	}
	// 区切り線を探す
	// number には区切り線のインデックスを指定する。（負の値を指定した場合は末尾から数える）
	static int findCutline(const char *text, size_t size, int number) {
		LOGASSERT(text);
		// 既存のログファイルのエンコードがユーザーによって改変されてしまっている可能性に注意。
		// UTF16などでのエンコードに代わっていると、0x00 の部分をヌル終端と判断してしまい、それより先の検索ができなくなる。
		// ある時点でユーザーがログのエンコードを変えて保存してしまったとしても、プログラムによって追記した部分は
		// SJIS などになっているため、途中にある 0x00 を無視して検索をしていけばそのうち CUTLINE に一致する部分が見つかるはずである。
		// （今見つからなかったとしても、プログラムを起動するたびに新しい CUTLINE が追記されるならば、何回目かの起動で見つかるはず）
		// というわけで、ヌル終端には頼らず、必ずバイトサイズを見る
		int txtlen = size; // strlen(text) は使わない。理由は↑を参照
		int sublen = strlen(CUTLINE);
		if (number >= 0) {
			// 先頭から [number] 番目を探す
			for (int i=0; txtlen-sublen; i++) {
				if (strncmp(text+i, CUTLINE, sublen) == 0) {
					if (number == 0) {
						return i + sublen;
					}
					number--;
				}
			}
		} else {
			// 末尾から [1-number] 番目を探す
			for (int i=txtlen-sublen; i>=0; i--) {
				if (strncmp(text+i, CUTLINE, sublen) == 0) {
					number++;
					if (number == 0) {
						return i + sublen;
					}
				}
			}
		}
		return -1;
	}
};

#pragma region KLog
void KLog::setOutputFile(const char *filename_u8) {
	if (filename_u8 && filename_u8[0]) {
		// 古いログを削除する
		if (1) {
			int N = 3; // 過去 N 回分のログを残し、それより古いものを削除
			clampFileByCutline(filename_u8, -N); // インデックスの指定の仕方に注意。正と負で意味が異なる
		}
		// 指定サイズを超えないように削除する
		if (0) {
			int limitsize = 1024 * 4;
			clampFileBySize(filename_u8, limitsize);
		}
		// 改めてファイルを開く
		s_ctx.file_ = FOPEN_U8(filename_u8, "a");
		if (s_ctx.file_ == NULL) {
			loglog("[Log] ERROR Failed to open log file: '%s'", filename_u8);
		}

	} else {
		// NULLまたは空文字列が指定された。
		// テキストファイルへの出力を終了し、ファイルを閉じる
		if (s_ctx.file_) {
			fflush(s_ctx.file_);
			fclose(s_ctx.file_);
			s_ctx.file_ = NULL;
		}
	}
}
void KLog::setOutputCallback(KLogCallback *cb) {
	s_ctx.callback_ = cb;
}
void KLog::setOutputConsole(bool enabled) {
	if (enabled) {
		// コンソールへの出力を有効にする
		// 順番に注意：有効にしてからログを出す
		CNative::showConsoleWindow();
		s_ctx.console_ = true;
		loglog("[Log] console on");
	} else {
		// コンソールへの出力を停止する。
		// 襦袢に注意：ログを出してから無効にする
		loglog("[Log] console off");
		s_ctx.console_ = false;
	//	CNative::hideConsoleWindow();
	}
}
void KLog::setLevelInfo(bool value) {
	s_ctx.info_enabled_ = value;
	loglog("[Log] level info %s", value ? "on" :"off");
}
void KLog::setLevelDebug(bool value) {
	s_ctx.debug_enabled_ = value;
	loglog("[Log] level debug %s", value ? "on" :"off");
}
void KLog::setLevelVerbose(bool value) {
	s_ctx.verbose_enabled_ = value;
	loglog("[Log] level verbose %s", value ? "on" :"off");;
}
void KLog::setErrorDialog(bool value) {
	s_ctx.dialog_enabled_ = value;
	loglog("[Log] dialog %s", value ? "on" :"off");
}
int KLog::command(const char *cmd) {
	if (cmd == NULL) return 0;
	if (strcmp(cmd, "output_console") == 0) {
		return s_ctx.console_ ? 1 : 0;
	}
	if (strcmp(cmd, "output_file") == 0) {
		return s_ctx.file_ ? 1 : 0;
	}
	if (strcmp(cmd, "output_debugger") == 0) {
		return s_ctx.debugger_ ? 1 : 0;
	}
	if (strcmp(cmd, "level_info") == 0) {
		return s_ctx.info_enabled_ ? 1 : 0;
	}
	if (strcmp(cmd, "level_debug") == 0) {
		return s_ctx.debug_enabled_ ? 1 : 0;
	}
	if (strcmp(cmd, "level_verbose") == 0) {
		return s_ctx.verbose_enabled_ ? 1 : 0;
	}
	loglog("[Log] Unknown log command: '%s'", cmd);
	return 0;
}
bool KLog::clampFileByCutline(const char *filename_u8, int number) {
	// number は負の値でもよい。
	// CLogFile::findCutline を参照

	LOGASSERT(filename_u8);
	std::string text;

	// ログテキストをロード
	CLogFile::load(filename_u8, text);

	// 切り取り線を探す
	// 引数に注意。text はヌル終端文字列ではなく、単なるバイト列として渡す。
	// UTF16などで0x00を含む可能性があるため、ヌル終端を期待すると予期せぬ場所で切られてしまう場合がある
	int pos = CLogFile::findCutline(text.data(), text.size(), number);

	// 切り取り線を含んでいなければ、何もしない
	if (pos < 0) {
		return false;
	}

	// 切り取り線よりも前の部分を削除し、再保存する
	text.erase(0, pos);
	CLogFile::save(filename_u8, text.c_str());
	return true;
}
bool KLog::clampFileBySize(const char *filename_u8, int clamp_size_bytes) {
	LOGASSERT(filename_u8);
	std::string text;
	
	// ログテキストをロード
	CLogFile::load(filename_u8, text);

	// 指定サイズ未満なら何もしない
	if ((int)text.size() <= clamp_size_bytes) {
		return false;
	}

	// 適当な改行位置を境にして古いテキストを削除する
	int findstart = text.size() - clamp_size_bytes;
	int pos = text.find("\n", findstart);
	text.erase(0, pos+1); // 改行文字も消すので pos に +1 する

	// ログ削除メッセージを挿入する
	const char *msg = "=========== OLD LOGS ARE REMOVED ===========\n";
	text.insert(0, msg);

	// 前半削除済みのテキストを書き込む
	CLogFile::save(filename_u8, text.c_str());

	return true;
}
void KLog::emitRaw(const char *raw_text) {
	// 渡されたテキストを無変更、無加工で出力する
	// 文字コード変換も何も行わない
	LOGASSERT(raw_text);
	if (s_ctx.debugger_) {
		CNative::printDebugger(raw_text);
	}
	if (s_ctx.console_) {
		fputs(raw_text, stdout);
	}
	if (s_ctx.file_) {
		fputs(raw_text, s_ctx.file_);
	}
}
void KLog::emit(char type, const char *u8) {
	LOGLOG_BEGIN(type, u8);
	LOGASSERT(u8);
	s_mtx.lock();
	if (type == CHAR_NUL) {
		// 属性なしテキスト
		if (s_ctx.debugger_) {
			CNative::printDebugger(u8);
			CNative::printDebugger("\n");
		}
		if (s_ctx.console_) {
			CNative::printConsole(u8);
		}
		if (s_ctx.file_) {
			fputs(u8, s_ctx.file_);
			fputs("\n", s_ctx.file_);
			fflush(s_ctx.file_);
		}
	} else {
		// 属性付きテキスト
		SMessageItem msg;
		GETLOCALTIME(&msg.localtime, &msg.localtime_tm_msec);
		msg.app_msec = GETTIME_MSEC() - s_ctx.start_msec_;
		msg.type = type;
		msg.text_u8 = u8;
		if (s_ctx.debugger_) {
			// デバッガーへの出力
			char s[SPRINTF_BUFFER_SIZE];
			sprintf(s, "[%c] %s\n", msg.type, msg.text_u8);
			CNative::printDebugger(s);
		}
		if (s_ctx.console_) {
			// コンソールへの出力
			CNative::printConsole(msg);
		}
		if (s_ctx.file_) {
			// テキストファイルへの出力

			// 時刻
			fprintf(s_ctx.file_, "%02d-%02d-%02d ", msg.localtime.tm_year+1900, msg.localtime.tm_mon+1, msg.localtime.tm_mday);
			fprintf(s_ctx.file_, "%02d:%02d:%02d.%03d ", msg.localtime.tm_hour, msg.localtime.tm_min, msg.localtime.tm_sec, msg.localtime_tm_msec);

			// スレッド
			fprintf(s_ctx.file_, "(%6d) @%08x ", msg.app_msec, GETPID());

			// メッセージレベル
			switch (msg.type) {
			case CHAR_AST: fprintf(s_ctx.file_, "[assertion error] "); break;
			case CHAR_ERR: fprintf(s_ctx.file_, "[error] "); break;
			case CHAR_WRN: fprintf(s_ctx.file_, "[warning] "); break;
			case CHAR_INF: fprintf(s_ctx.file_, "[info] "); break;
			case CHAR_DBG: fprintf(s_ctx.file_, "[debug] "); break;
			case CHAR_VRB: fprintf(s_ctx.file_, "[verbose] "); break;
			default:       fprintf(s_ctx.file_, "[%c] ", msg.type); break;
			}

			// メッセージ内容
			fprintf(s_ctx.file_, "%s\n", msg.text_u8);

			// 強制更新
			fflush(s_ctx.file_);
		}
	}
	s_mtx.unlock();
	LOGLOG_END();
}
void KLog::loglog(const char *fmt, ...) {
	char s[SPRINTF_BUFFER_SIZE];
	va_list args;
	va_start(args, fmt);
	vsprintf(s, fmt, args);
	emitRaw(s);
	emitRaw("\n");
	va_end(args);
}
void KLog::report(char type, const char *u8) {
	LOGLOG_BEGIN(type, u8);
	LOGASSERT(u8);
	if (s_ctx.isLevelEnabled(type)) {
		int processed = false;

		// ユーザーによる処理を試みる
		if (s_ctx.callback_) {
			s_ctx.callback_->on_log_output(type, u8, &processed);
		}

		if (processed) {
			// メッセージは処理された。
			// これ以上何もしない

		} else {
			// メッセージが処理されなかった。
			// 既定の処理を行う
			if (s_ctx.dialog_enabled_) {
				if (type == CHAR_AST) {
					CNative::showDialog("ASSERTION ERROR!", u8);
				}
				if (type == CHAR_ERR) {
					CNative::showDialog("ERROR!", u8);
				}
			}
			emit(type, u8);
		}
	}
	LOGLOG_END();
}
void KLog::reportv(char type, const char *fmt, va_list args) {
	LOGLOG_BEGIN(type, fmt);
	LOGASSERT(fmt);
	char tmp[SPRINTF_BUFFER_SIZE];
	vsnprintf(tmp, sizeof(tmp), fmt, args);
	tmp[sizeof(tmp)-1] = '\0'; // 念のため、バッファ最後にヌル文字を入れておく
	report(type, tmp);
	LOGLOG_END();
}
void KLog::reportf(char type, const char *fmt, ...) {
	LOGLOG_BEGIN(type, fmt);
	LOGASSERT(fmt);
	va_list args;
	va_start(args, fmt);
	reportv(type, fmt, args);
	va_end(args);
	LOGLOG_END();
}
void KLog::fatal(const char *msg) {
	emit(CHAR_ERR, msg);
	emitRaw("**** A FATAL ERROR HAS OCCURRED. THIS PROGRAM WILL BE TERMINATED ***\n");
	#ifdef _WIN32
	if (IsDebuggerPresent()) {
		DebugBreak();
	}
	#endif
	exit(-1);
}
void KLog::print(const char *msg) {
	report(CHAR_NUL, msg);
}
void KLog::printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	reportv(CHAR_NUL, fmt, args);
	va_end(args);
}
void KLog::printBinary(const void *data, int size) {
	const unsigned char *p = (const unsigned char *)data;
	print("# dump >>>");
	if (p == NULL) {
		size = 0;
		print("(NULL)");
	} else {
		if (size <= 0) {
			size = (int)strlen((const char *)data);
		}
		std::string txt; // テキスト表記
		std::string bin; // バイナリ表記
		int i = 0;
		for (; i < size; i++) {

			// 1行分の文字列を出力
			if (i>0 && i%16==0) {
				printf("# %s: %s", bin.c_str(), txt.c_str());
				bin.clear();
				txt.clear();
			}

			// 16進数表記を追加
			char s[8];
			sprintf(s, "%02x ", p[i]);
			bin += s;
			if (isprint(p[i])) {
				txt += p[i];
			} else {
				txt += '.';
			}
		}

		// 未出力のバイナリがあれば出力する
		if (!bin.empty()) {
			bin.resize(3 * 16, ' '); // バイナリ16桁の幅に合わせる
			printf("# %s: %s", bin.c_str(), txt.c_str());
		}
	}
	{
		char s[32];
		sprintf(s, "# <<< dump (%d bytes)", size);
		print(s);
	}
}
void KLog::printCutline() {
	print("");
	print("");
	print(CUTLINE);
	print("");
}
void KLog::printTrace(const char *file, int line, const char *func) {
	LOGASSERT(file);
	if (func) {
		reportf(CHAR_NUL, "%s(%d): %s", file, line, func); // msnvの場合、この書式でOutputPrintStringするとダブルクリックで該当箇所へジャンプできる
	} else {
		reportf(CHAR_NUL, "%s(%d)", file, line);
	}
}
void KLog::error(const char *msg) {
	report(CHAR_ERR, msg);
}
void KLog::errorf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	reportv(CHAR_ERR, fmt, args);
	va_end(args);
}
void KLog::warning(const char *msg) {
	report(CHAR_WRN, msg);
}
void KLog::warningf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	reportv(CHAR_WRN, fmt, args);
	va_end(args);
}
void KLog::info(const char *msg) {
	report(CHAR_INF, msg);
}
void KLog::infof(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	reportv(CHAR_INF, fmt, args);
	va_end(args);
}
void KLog::debug(const char *msg) {
	report(CHAR_DBG, msg);
}
void KLog::debugf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	reportv(CHAR_DBG, fmt, args);
	va_end(args);
}
void KLog::verbose(const char *msg) {
	report(CHAR_VRB, msg);
}
void KLog::verbosef(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	reportv(CHAR_VRB, fmt, args);
	va_end(args);
}
void KLog::assertion(const char *file, int line, const char *expr) {
	reportf(CHAR_AST, "Assertion Faied!! %s(%d): %s", file, line, expr);
}


#pragma endregion // KLog






#pragma region KLogScopedLog
KLogScopedLog::KLogScopedLog(const char *file, int line, const char *func) {
	LOGASSERT(file);
	LOGASSERT(line >= 0);
	LOGASSERT(func);
	strcpy(file_, file);
	line_ = line;
	strcpy(func_, func);
	KLog::printf("%s(%d): %s {", file_, line_, func_);
}
KLogScopedLog::~KLogScopedLog() {
	KLog::printf("%s(%d): } // %s end", file_, line_, func_);
}
#pragma endregion // KLogScopedLog





void Test_log() {
	KLog::clampFileByCutline("~test.log", -2); // ~test.log が存在すれば、切り取り線よりも前の部分を削除する

	KLog::setOutputConsole(true);
	KLog::setOutputFile("~test.log");

	KLog::printCutline(); // 切り取り線を出力（テキストファイル）
	KLog::setErrorDialog(false); // エラーダイアログは表示しない
	KLog::setLevelVerbose(true); // 詳細メッセージON
	KLog::setLevelDebug(true); // デバッグメッセージON
	KLog::setLevelInfo(true); // 情報メッセージON
	KLog::verbosef("VERBOSE MESSAGE");
	KLog::debugf("DEBUG MESSAGE: '%s'", "This is a debug message");
	KLog::infof("INFO MESSAGE: '%s'", "This is an information text");
	KLog::warningf("WARNING MESSAGE: '%s'", "Warning!");
	KLog::errorf("ERROR MESSAGE: '%s'", "An error occurred!");
	KLog::printf("PLAIN TEXT: '%s'", "The quick brown fox jumps over the lazy dog"); // 無属性テキストを出力
	char data[256];
	for (int i=0; i<256; i++) data[i]=i;
	KLog::printBinary(data, sizeof(data));
	KLog::printTrace(__FILE__, __LINE__, __FUNCTION__);
}
