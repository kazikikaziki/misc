#include "KIcon.h"
#include <vector>
#include <string>

#ifdef _WIN32
#	include <Windows.h>
#endif

namespace mo {

#ifdef _WIN32
class CWin32Icons {
public:
	typedef std::vector<HICON> std_vector_HICON;
	
	static BOOL CALLBACK callback(HINSTANCE instance, LPCTSTR type, LPTSTR name, void *std_vector_hicon) {
		std_vector_HICON *icons = reinterpret_cast<std_vector_HICON *>(std_vector_hicon);
		HICON icon = LoadIcon(instance, name);
		if (icon) {
			icons->push_back(icon);
		}
		return TRUE;
	}

	std::vector<HICON> icons_;

	CWin32Icons() {
		// この実行ファイルに含まれているアイコンを取得する
		HINSTANCE hInst = (HINSTANCE)GetModuleHandleW(NULL);
		EnumResourceNames(hInst, RT_GROUP_ICON, (ENUMRESNAMEPROC)callback, (LONG_PTR)&icons_);
	}
	int size() const {
		return (int)icons_.size();
	}
	HICON get(int index) const {
		if (0 <= index && index < (int)icons_.size()) {
			return icons_[index];
		} else {
			return NULL;
		}
	}
};
static CWin32Icons g_Icons;
static HICON win32_CreateIconFromPixelsRGBA32(const uint8_t *pixels, int w, int h) {
	// CreateIcon には ARGB32 リトルエディアンで渡す必要があるため、
	// 入力されたピクセル RGBA を BGRA に並び替えてセットする
	std::string mem(w * h * 4, '\0');
	uint8_t *buf = (uint8_t *)mem.data();
	for (int i=0; i<w * h; i++) {
		uint8_t r = pixels[i * 4 + 0];
		uint8_t g = pixels[i * 4 + 1];
		uint8_t b = pixels[i * 4 + 2];
		uint8_t a = pixels[i * 4 + 3];
		buf[i * 4 + 0] = b;
		buf[i * 4 + 1] = g;
		buf[i * 4 + 2] = r;
		buf[i * 4 + 3] = a;
	}
	HICON hIcon = CreateIcon(GetModuleHandleW(NULL), w, h, 1, 32, NULL, buf);
	return hIcon;
}
#endif // _WIN32



KIcon::KIcon() {
	hIcon_ = NULL;
}
bool KIcon::loadFromResource(int index) {
#ifdef _WIN32
	hIcon_ = g_Icons.get(index);
	return hIcon_ != NULL;
#else
	return false;
#endif
}
bool KIcon::loadFromPixels(const uint8_t *pixels, int w, int h) {
#ifdef _WIN32
	hIcon_ = win32_CreateIconFromPixelsRGBA32(pixels, w, h);
	return hIcon_ != NULL;
#else
	return false;
#endif
}
void * KIcon::getHandle() const {
	return hIcon_;
}

}
