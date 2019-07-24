#pragma once
#include "KVideoDef.h"
#include "KImage.h"
#include "KImagePack.h"

namespace mo {

class Path;
class KEdgeDocument;

class ImgPackFile {
public:
	struct Dir {
		Path edge_dir;
		Path edge_name;
		Path edge_path() const { return edge_dir.join(edge_name); }

		Path cache_dir;
		Path cache_png_name;
		Path cache_meta_name;
		Path cache_png_path() const { return cache_dir.join(cache_png_name); }
		Path cache_meta_path() const { return cache_dir.join(cache_meta_name); }
	};
	enum PaletteFlag {
		MAKE_INDEXED_TRUE  = 0x01, // パレットがあるとき、最初のパレットを適用してRGBA画像として作成する
		MAKE_INDEXED_FALSE = 0x02, // パレットが1つならばパレットを適用してRGBA画像として作成し、複数ある場合はインデックス画像として作成する
		MAKE_INDEXED_AUTO  = 0x04, // 必ずインデックス画像として作成する
		SAVE_PALETTE_TRUE  = 0x10, // パレットがあれば必ずパレット情報も保存する
		SAVE_PALETTE_FALSE = 0x20, // 複数のパレットがあるときだけパレット情報を保存する
		SAVE_PALETTE_AUTO  = 0x40, // パレットを保存しない
		DEFAULT = MAKE_INDEXED_AUTO | SAVE_PALETTE_AUTO,
	};
	typedef int PaletteFlags;

	struct ExInfo {
		ExInfo() {
			blend = Blend_Alpha;
			pivot_x = 0;
			pivot_y = 0;
			pivot_x_in_percent = false;
			pivot_y_in_percent = false;
		}
		Blend blend;
		float pivot_x;
		float pivot_y;
		bool pivot_x_in_percent;
		bool pivot_y_in_percent;
	};
	std::vector<KImage> images_;
	std::vector<KImage> pals_;
	CImgPackW packW_;
	CImgPackR packR_;
	~ImgPackFile();
	void addSpriteFromPack(const Path &edge_name, int tex_w, int tex_h, ExInfo *info);
	bool tryLoadFromCache(const Dir &dir, int cellsize, PaletteFlags flags, ExInfo *info);
	bool loadAuto(const Dir &dir, int cellsize, PaletteFlags flags, ExInfo *info);
	void loadFromEdge(const KEdgeDocument *edge, int cellsize, PaletteFlags flags=0);
	bool loadFromEdgeFileName(const Path &edge_name, int cellsize, PaletteFlags flags=0);
	void save(const Path &out_dir, const Path &edgename);
	void clear();
};


}
