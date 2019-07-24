#include "KPlatformFonts.h"
#include "KDirScan.h"
#include "KStd.h"
#include <algorithm> // std::sort
#include <Shlobj.h> // SHGetFolderPath

namespace mo {

// フォントをファミリー名でソートする
struct Pred {
	bool operator()(const KPlatformFonts::INFO &a, const KPlatformFonts::INFO &b) const {
		return K_stricmp(a.family.u8(), b.family.u8()) < 0;
	}
};
Path KPlatformFonts::getFontDirectory() {
	wchar_t dir[Path::SIZE];
	SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, dir);
	return Path(dir);
}
int KPlatformFonts::size() const {
	return list_.size();
}
const KPlatformFonts::INFO & KPlatformFonts::operator [](int index) const {
	return list_[index];
}
void KPlatformFonts::scan() {
	list_.clear();
	// フォントフォルダ内のファイルを列挙
	Path dir = getFontDirectory();
	KDirScan scan;
	scan.scanFlat(dir);
	for (int j=0; j<scan.size(); j++) {
		Path filename = dir.join(scan.items(j).name);
		FontFile ff;
		if (ff.loadFromFileName(filename)) {
			int numfonts = ff.getCount();
			for (int i=0; i<numfonts; i++) {
				KFont fo = ff.getFont(i);
				wchar_t tmp[256];
				// フォントファミリー
				// 日本語を優先、ダメだったら英語名を得る
				INFO info;
				if (fo.getNameInJapanese(KFont::NID_FAMILY, tmp)) {
					info.family = tmp;
				} else if (fo.getNameInEnglish(KFont::NID_FAMILY, tmp)) {
					info.family = tmp;
				}
				/*
				// サブファミリー "Regular" "Bold" など
				if (fo.getNameInEnglish(KFont::NID_SUBFAMILY, tmp)) {
					info.subfamily = tmp;
				}
				*/
				// サブファミリー名ではなく、フラグから直接スタイルを取得する
				info.style = fo.getFlags();

				info.filename = scan.items(j).name; // filename
			//	info.filename = filename; // fullpath
				info.ttc_index = i;
				list_.push_back(info);
			}
		}
	}
	// フォント名順でソート
	std::sort(list_.begin(), list_.end(), Pred());
}


void Test_fontscan() {
	KPlatformFonts fonts;
	fonts.scan();

	for (int i=0; i<fonts.size(); i++) {
		auto it = fonts[i];
		if (it.style == 0) {
			char s[1024];
			sprintf(s, "%s : %s[%d]\n", it.family.toAnsiString('\\', "").c_str(), it.filename.toAnsiString('\\', "").c_str(), it.ttc_index);
			OutputDebugStringA(s);
		}
	}
}



}
