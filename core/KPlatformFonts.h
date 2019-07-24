#pragma once
#include "KPath.h"
#include "KFont.h"

namespace mo {

/// インストール済みのフォント情報を得る
class KPlatformFonts {
public:
	struct INFO {
		Path family;       ///< フォントファミリー名（"ＭＳ ゴシック" "Arial"）
		Path filename;     ///< フォントファイル名（"arial.ttf")
		KFont::Flags style;///< スタイルフラグ
		int ttc_index;     ///< フォントファイルが複数のフォントを含んでいる場合、そのインデックス
	};
	static Path getFontDirectory();
	void scan();
	int size() const;
	const INFO & operator[](int index) const;

private:
	std::vector<INFO> list_;
};

void Test_fontscan();

}
