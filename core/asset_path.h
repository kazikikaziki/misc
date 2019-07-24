#pragma once
#include "KPath.h"

namespace mo {

static const char * CLIP_EXT = ".clip";

class AssetPath {
public:
	static bool isMetaFileName(const Path &path);
	static Path makeMetaFileName(const Path &filename);
	static bool isAtlasFileName(const Path &path);
	static Path makePalettePath(const Path &texture_path, int palette_index, const Path &extra_palette_image_path);
	static bool parsePalettePath(const Path &path, int *palette_index, Path *extra_palette_path);
	static Path makeUnpackedTexturePath(const Path &sprite);

	/// ファイル名として安全な文字列にする
	static Path escapeFileName(const Path &name);

	/// アセット名として含めることができる文字かどうか
	static bool isSafeAssetChar(char c);

	/// 仮想ファイル名から、実際のファイル名を得る。仮想ファイル名とは、ファイル名の省略や、特定のファイルを表すための特別な識別子を許容するファイル名のこと
	/// name が省略または "AUTO" が指定されていれば、現在扱っている XML ファイル名から自動的に EDGE ファイル名を導き、それを返す
	/// それ以外の場合は現在のディレクトリを考慮したファイル名を返す
	static Path evalPath(const Path &expr, const Path &curr_path, const Path &def_ext);

	static Path getEdgeAtlasPngPath(const Path &edg_path);
	static Path getMeshAssetPath(const Path &base_path);
	static Path getMeshAssetPath(const Path &base_path, int index);
	static Path getTextureAssetPath(const Path &base_path);
	static Path getTextureAssetPath(const Path &base_path, int index);
	static Path getTextureAssetPath(const Path &base_path, int index1, int index2);
	static Path getSpriteAssetPath(const Path &base_path);
	static Path getSpriteAssetPath(const Path &base_path, int index);
	static Path getSpriteAssetPath(const Path &base_path, int index1, int index2);
	static Path getClipAssetPath(const Path &base_path);
	static Path getClipAssetPath(const Path &base_path, int index);

	/// asset_path にバリアント情報を付加したパスを作成する
	static Path makeVariantPath(const Path &asset_path, int variant);
};

} // namespace
