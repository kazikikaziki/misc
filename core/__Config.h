#pragma once

/// 公開用にビルドするか
#ifndef MO_PUBLISHING_BUILD
#	define MO_PUBLISHING_BUILD 0
#endif



/// デバッグ用の機能を有効にしてビルドするかどうか
/// @arg 1 -- デバッグモードで動作する
/// @arg 0 -- 製品モードで動作する
#ifndef MO_BUILD_WITH_DEBUG
#    if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
#        define MO_BUILD_WITH_DEBUG 1
#    else
#        define MO_BUILD_WITH_DEBUG 0
#    endif
#endif


/// トレースログを有効にする（デバッグ用）
#ifndef MO_DEBUG_TRACE
#	define MO_DEBUG_TRACE (MO_BUILD_WITH_DEBUG && 0)
#endif

/// 細かいエラーチェックを行う（デバッグ用）
#ifndef MO_STRICT_CHECK
#	define MO_STRICT_CHECK (MO_BUILD_WITH_DEBUG && 1)
#endif


/// @def MO_BUILD_WITH_WIN32
/// Win32API をリンクしてビルドするかどうか
#ifndef MO_BUILD_WITH_WIN32
#	ifdef _MSC_VER
#		define MO_BUILD_WITH_WIN32  1
#	else
#		define MO_BUILD_WITH_WIN32  0
#	endif
#endif


/// DirectX9.0を使うか
#ifndef MO_BUILD_WITH_DX9
#	define MO_BUILD_WITH_DX9 1
#endif

/// Ogg/Vorbis を使うかどうか
#ifndef MO_BUILD_WITH_OGG
#	define MO_BUILD_WITH_OGG 1
#endif


#if MO_BUILD_WITH_WIN32
	// Windows ライブラリ
	#pragma comment(lib, "winmm")    // timeGetTime
	#pragma comment(lib, "imm32")    // ImmDisableIME
	#pragma comment(lib, "shlwapi")  // File path functions
	#pragma comment(lib, "comctl32") // commctrl.h
#endif


#if MO_BUILD_WITH_DX9
	// DirectX 9.0
	#pragma comment(lib, "d3d9")
	#pragma comment(lib, "d3dx9")
	#pragma comment(lib, "dsound")
	#pragma comment(lib, "dxguid")

	// DirectX9.0 は既に標準サポートされていないため、
	// ビルドしようとすると vsnprintf でリンカエラーが起きる。
	// 互換性維持用のライブラリをリンクしておく
	#pragma comment(lib, "legacy_stdio_definitions")

	#if 1
		// DXGetErrorString9 (dxerr9.h) に必要なライブラリ
		// 古い SDK (2004 OCT) で使う。これは d3dx9_XX.dll が無くても実行できる
		#pragma comment(lib,"dxerr9")
	#else
		// DXGetErrorString (dxerr.h)に必要なライブラリ
		// 新しい SDK (2010 JUN) で使う。実行環境に d3dx9_XX.dll が必要になる
		#pragma comment(lib,"dxerr")
	#endif

#endif
