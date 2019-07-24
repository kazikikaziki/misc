#include "KCursor.h"
#ifdef _WIN32
#include <Windows.h>
#endif

namespace mo {

#ifdef _WIN32
static HCURSOR win32_CreateCursorFromPixelsRGBA32(HWND hWnd, const uint8_t *pixels, int w, int h, int hotspot_x, int hotspot_y) {
	HDC hDC = GetDC(hWnd);
	HDC hMaskDC = CreateCompatibleDC(hDC); // 不透明部分を 0 とするマスク画像
	HDC hColorDC = CreateCompatibleDC(hDC); // RGB 画像
	HBITMAP hMaskBmp = CreateCompatibleBitmap(hDC, w, h);
	HBITMAP hColorBmp = CreateCompatibleBitmap(hDC, w, h);
	HBITMAP hOldMaskBmp = (HBITMAP)SelectObject(hMaskDC, hMaskBmp);
	HBITMAP hOldColorBmp = (HBITMAP)SelectObject(hColorDC, hColorBmp);
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			const uint8_t *p = pixels + (w * y + x) * 4;
			uint8_t r = p[0];
			uint8_t g = p[1];
			uint8_t b = p[2];
			uint8_t a = p[3];
			if (a > 0) {
				SetPixel(hMaskDC, x, y, RGB(0, 0, 0));
				SetPixel(hColorDC, x, y, RGB(r, g, b));
			} else {
				SetPixel(hMaskDC, x, y, RGB(255, 255, 255));
				SetPixel(hColorDC, x, y, RGB(0, 0, 0));
			}
		}
	}
	SelectObject(hMaskDC, hOldMaskBmp);
	SelectObject(hColorDC, hOldColorBmp);
	DeleteDC(hMaskDC);
	DeleteDC(hColorDC);
	ReleaseDC(hWnd, hDC);

	ICONINFO iconinfo;
	iconinfo.fIcon = false;	// type is cursor not icon
	iconinfo.xHotspot = hotspot_x;
	iconinfo.yHotspot = hotspot_y;
	iconinfo.hbmMask = hMaskBmp;
	iconinfo.hbmColor = hColorBmp;

	HCURSOR hCursor = CreateIconIndirect(&iconinfo);

	DeleteObject(hMaskBmp);
	DeleteObject(hColorBmp);

	return hCursor;
}

KCursor::KCursor() {
	hCur_ = LoadCursor(NULL, IDC_ARROW); // デフォルトカーソルをロードしておく
}
void KCursor::loadFromSystem(Type tp) {
	switch (tp) {
	case DEFAULT:   hCur_ = LoadCursor(NULL, IDC_ARROW); break;
	case CROSS:     hCur_ = LoadCursor(NULL, IDC_CROSS); break;
	case HAND:      hCur_ = LoadCursor(NULL, IDC_HAND); break;
	case HELP:      hCur_ = LoadCursor(NULL, IDC_HELP); break;
	case IBEAM:     hCur_ = LoadCursor(NULL, IDC_IBEAM); break;
	case NO:        hCur_ = LoadCursor(NULL, IDC_NO); break;
	case WAIT:      hCur_ = LoadCursor(NULL, IDC_WAIT); break;
	case SIZE_ALL:  hCur_ = LoadCursor(NULL, IDC_SIZEALL); break;
	case SIZE_NESW: hCur_ = LoadCursor(NULL, IDC_SIZENESW); break;
	case SIZE_NWSE: hCur_ = LoadCursor(NULL, IDC_SIZENWSE); break;
	case SIZE_NS:   hCur_ = LoadCursor(NULL, IDC_SIZENS); break;
	case SIZE_WE:   hCur_ = LoadCursor(NULL, IDC_SIZEWE); break;
	case UP:        hCur_ = LoadCursor(NULL, IDC_UPARROW); break;
	}
}
void KCursor::loadFromPixels(const uint8_t *pixels, int w, int h, int hotspot_x, int hotspot_y) {
	DestroyCursor((HCURSOR)hCur_);

	HWND hWnd = GetActiveWindow(); // カーソルの生成にはウィンドウハンドルが必要なので、適当なハンドルを得る
	hCur_ = win32_CreateCursorFromPixelsRGBA32(hWnd, pixels, w, h, hotspot_x, hotspot_y);
}
void * KCursor::getHandle() const {
	return hCur_;
}
#else // _WIN32
KCursor::KCursor() {}
void KCursor::loadFromSystem(Type tp) {}
void KCursor::loadFromPixels(const uint8_t *pixels, int w, int h, int hotspot_x, int hotspot_y) {}
void * KCursor::getHandle() const { return NULL; }
#endif // !_WIN32


}
