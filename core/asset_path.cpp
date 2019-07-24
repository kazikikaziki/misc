#include "asset_path.h"
#include "KLog.h"
#include "KStd.h"

namespace mo {

#define RESOURCE_NAME_CHAR_SET          " ._()[]{}<>#$%&=~^@+-"
#define PATH_VARIANT_SEPARATOR          '$' // パス名とバージョン名の区切り記号（バージョン違いのアセットが存在する場合に使う）
#define PATH_INDEX_SEPARATOR            '@' // パス名とインデックス番号の区切り文字
#define PATH_PALETTE_SEPALATOR_STR     "@@" // テクスチャ名と適用パレット名の区切り文字

/// ファイル名として安全な文字列にする
Path AssetPath::escapeFileName(const Path &name) {
	char tmp[Path::SIZE];
	memset(tmp, 0, sizeof(tmp));
	const char *s = name.u8(); // 内部のポインタを得る
	for (size_t i=0; s[i]; i++) {
		if (isSafeAssetChar(s[i])) {
			tmp[i] = s[i];
		} else if (isascii(s[i])) {
			tmp[i] = '_';
		} else {
			tmp[i  ] = '0' + (uint8_t)(s[i]) / 16;
			tmp[i+1] = '0' + (uint8_t)(s[i]) % 16;
			i++; // 2文字書き込んでいるので、iを1回余分にインクリメントする必要がある
		}
	} // 終端のヌル文字にはmemsetで初期化したときの0を使う
	return Path(tmp);
}
Path AssetPath::makeUnpackedTexturePath(const Path &sprite) {
	return Path::fromFormat("%s.[UNPACKED].tex", sprite.u8());
}
Path AssetPath::makePalettePath(const Path &texture_path, int palette_index, const Path &extra_palette_image_path) {
	// 負のパレット番号は「パレットなし」をあらわす
	if (palette_index < 0) 
		return Path::Empty;

	// パレットは、あくまでも元画像と対応して存在する。
	// 例えば hoge@2.tex というアセットであれば、元画像 hoge.edg や hoge.png から
	// 複数作成されたテクスチャのうちのインデックス 2 番目の画像、という意味であるが,
	// パレットは hoge@2 に対して存在するのではなく、元画像 hoge に対して存在する
	// パレット画像名は hoge@0.pal の形式になっている。0 はパレット番号
	Path simple_name;
	if (! extra_palette_image_path.empty()) {
		// パレット画像名とパレットインデックスを組み合わせてパレット用の名前を作成し、返す
		simple_name = extra_palette_image_path.changeExtension("");

	} else {
		// パレット名指定なし。スプライトに指定されたテクスチャの既定のパレット名を作成する
		simple_name = texture_path.changeExtension("");
	}

	char base[Path::SIZE];
	strcpy(base, simple_name.u8());
	char *sep = strrchr(base, PATH_INDEX_SEPARATOR);
	if (sep) {
		*sep = '\0';
	}
	return Path::fromFormat("%s%s%d.pal", base, PATH_PALETTE_SEPALATOR_STR, palette_index);
}

// makePalettePath で生成されたパスを分解する
bool AssetPath::parsePalettePath(const Path &path, int *palette_index, Path *palette_path) {
	// AssetPath::makePalettePath で生成された名前は {NAME}@@{INDEX}{EXT} の形式になっている。これを分解する
	// internal_path が {NAME}@@{INDEX}{EXT} の形式になっていた場合、
	// palette_index には {INDEX} が、
	// palette_path には {NAME}{EXT} が入る

	// 区切り記号 "@@" で分割
	std::string a, bc;
	if (! K_strsplit_rstr(path.u8(), &a, PATH_PALETTE_SEPALATOR_STR, &bc)) {
		return false;
	}

	// 区切り記号 "." で分割
	std::string b, c;
	K_strsplit_rchar(bc.c_str(), &b, '.', &c);

	// インデックスを整数化する
	int idx = -1;
	K_strtoi_try(b.c_str(), &idx);
	if (idx < 0) {
		return false;
	}

	// OK
	if (palette_index) *palette_index = idx;
	if (palette_path) *palette_path = Path(a).changeExtension(c);
	return true;
}
bool AssetPath::isMetaFileName(const Path &path) {
	return path.extension().compare(".meta") == 0;
}
Path AssetPath::makeMetaFileName(const Path &filename) {
	return filename.joinString(".meta");
}
bool AssetPath::isAtlasFileName(const Path &path) {
	return path.extension().compare(".atlas") == 0;
}
Path AssetPath::evalPath(const Path &name, const Path &base_filename, const Path &def_ext) {
	if (name.empty() || name.compare("auto")==0) {
		// 名前が省略されている。curr_path と同じファイル名で拡張子だけが異なるファイルが指定されているものとする
		return base_filename.changeExtension(def_ext);
	} else {
		// 名前が指定されている
		// base_filename と同じフォルダ内にあるファイルが指定されているものとする
		Path dir = base_filename.directory();
		return dir.join(name).changeExtension(def_ext);
	}
}
Path AssetPath::getClipAssetPath(const Path &base_path) {
	return base_path.changeExtension(".clip");
}
Path AssetPath::getClipAssetPath(const Path &base_path, int index) {
	Path base = base_path.changeExtension("");
	return Path::fromFormat("%s%c%d.clip", base.u8(), PATH_INDEX_SEPARATOR, index);
}
Path AssetPath::getSpriteAssetPath(const Path &base_path) {
	return base_path.changeExtension(".sprite");
}
Path AssetPath::getSpriteAssetPath(const Path &base_path, int index) {
	Path base = base_path.changeExtension("");
	return Path::fromFormat("%s%c%d.sprite", base.u8(), PATH_INDEX_SEPARATOR, index);
}
Path AssetPath::getSpriteAssetPath(const Path &base_path, int index1, int index2) {
	Path base = base_path.changeExtension("");
	return Path::fromFormat("%s%c%d.%d.sprite", base.u8(), PATH_INDEX_SEPARATOR, index1, index2);
}
Path AssetPath::getTextureAssetPath(const Path &base_path) {
	return base_path.changeExtension(".tex");
}
Path AssetPath::getTextureAssetPath(const Path &base_path, int index) {
	Path base = base_path.changeExtension("");
	return Path::fromFormat("%s%c%d.tex", base.u8(), PATH_INDEX_SEPARATOR, index);
}
Path AssetPath::getTextureAssetPath(const Path &base_path, int index1, int index2) {
	Path base = base_path.changeExtension("");
	return Path::fromFormat("%s%c%d.%d.tex", base.u8(), PATH_INDEX_SEPARATOR, index1, index2);
}
Path AssetPath::getMeshAssetPath(const Path &base_path) {
	return base_path.changeExtension(".mesh");
}
Path AssetPath::getMeshAssetPath(const Path &base_path, int index) {
	Path base = base_path.changeExtension("");
	return Path::fromFormat("%s%d.mesh", base.u8(), index);
}
Path AssetPath::getEdgeAtlasPngPath(const Path &edg_path) {
	return edg_path.joinString(".png");
}
Path AssetPath::makeVariantPath(const Path &asset_path, int variant) {
	if (variant > 0) {
		return Path::fromFormat("%s%c%d", asset_path.u8(), PATH_VARIANT_SEPARATOR, variant);
	} else {
		// バリアント番号 0 の場合は、番号表記を省略する
		Log_assert(variant == 0);
		return asset_path;
	}
}
bool AssetPath::isSafeAssetChar(char c) {
	if (isalnum(c)) {
		return true;
	}
	if (strchr(RESOURCE_NAME_CHAR_SET, c) != NULL) {
		return true;
	}
	return false;
}


} // namespace
