#include "KDirScan.h"
#include "KStd.h"

#ifdef _WIN32
#	include <Windows.h>
#endif

namespace mo {


KDirScan::KDirScan() {
	depth_ = 0;
}
void KDirScan::clear() {
	items_.clear();
	depth_ = 0;
}
void KDirScan::scanFlat(const Path &_dir) {
	scanFlat2(_dir, Path::Empty);
}
void KDirScan::scanFlat2(const Path &_dir, const Path &prefix) {
#ifdef _WIN32
	std::wstring pattern = _dir.getFullPath().join("*").toWideString(Path::NATIVE_DELIM);
	// ここで pattern は
	// "d:\\system\\*"
	// のような文字列になっている。ワイルドカードに注意！！
	WIN32_FIND_DATAW fdata;
	HANDLE hFind = FindFirstFileW(pattern.c_str(), &fdata);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (fdata.cFileName[0] != L'.') {
				Item item;
				item.name = prefix.join(fdata.cFileName);
				item.is_directory = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
				items_.push_back(item);
			}
		} while (FindNextFileW(hFind, &fdata));
		FindClose(hFind);
	}
#else
	K_assert(0 && "NOT IMPLEMENTED");
#endif
}
void KDirScan::scanRecursive(const Path &dir) {
	scanRecursive2(dir, Path::Empty);
}
void KDirScan::scanRecursive2(const Path &dir, const Path &prefix) {
	if (depth_ >= 256) {
		K_assert(0 && "DIRECTORY_TOO_DEEP!!\n");
	}
	size_t start = items_.size();
	scanFlat2(dir, prefix);
	size_t end = items_.size();
	for (size_t i=start; i<end; i++) {
		const Item &item = items_[i];
		if (item.is_directory) {
			depth_++;
			scanRecursive2(dir.join(item.name), prefix.join(item.name));
			depth_--;
		}
	}
}
int KDirScan::size() const {
	return (int)items_.size();
}
const KDirScan::Item & KDirScan::items(int index) const {
	return items_[index];
}

} // namespace
