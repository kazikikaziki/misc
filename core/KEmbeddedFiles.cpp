#include "KEmbeddedFiles.h"
#include "KPath.h"

#ifdef _WIN32
#include <Windows.h>
#include "Win32.h"
#endif

namespace mo {

#ifdef _WIN32

static CWin32ResourceFiles s_files;
static bool s_files_dirty = true;

static int Res_GetIndex(const Path &name) {
	if (s_files_dirty) {
		s_files.update();
		s_files_dirty = false;
	}
	for (int i=0; i<s_files.getCount(); i++) {
		const CWin32ResourceFiles::ITEM *item = s_files.getItem(i);
		if (Path(item->name).compare(name, true, true) == 0) {
			return i;
		}
	}
	return -1;
}

bool KEmbeddedFiles::hasFile(const Path &name) {
	return Res_GetIndex(name) >= 0;
}
FileInput KEmbeddedFiles::getFile(const Path &name) {
	FileInput file;
	int index = Res_GetIndex(name);
	if (index >= 0) {
		int size = 0;
		const void *data = s_files.getData(index, &size);
		if (data) {
			file.open(data, size);
		}
	}
	return file;
};
#endif

#ifdef __APPLE__
// 注意:
// 関数 macos_XXX の実体は mo_mac.mm にある。Objective-C++ のソースコードなので注意。
// mo_mac.h
// mo_mac.mm
bool KEmbeddedFiles::hasFile(const Path &name) {
	char dummy[Path::SIZE];
	return macos_GetBundleFileName(name.c_str(), dummy);
}
FileInput KEmbeddedFiles::getFile(const Path &name) override {
	char path[Path::SIZE];
	if (macos_GetBundleFileName(name.c_str(), path)) {
		return FileInput(path);
	}
	return NULL;
}
#endif


} // namespace
