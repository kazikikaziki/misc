#include "build_imgpack.h"
#include "build_edge.h"
#include "build_callback.h"
#include "KEdgeFile.h"
#include "KLog.h"
#include "KPath.h"
#include "KPlatform.h"
#include "asset_path.h"
#include "Engine.h"
#include "KFileLoader.h"
#include "KStd.h"
#include "GameRender.h"
#include "VideoBank.h"

namespace mo {
ImgPackFile::~ImgPackFile() {
	clear();
}
void ImgPackFile::save(const Path &out_dir, const Path &edg_name) {
	Path out_palette_base_name = edg_name;
	Path out_png_name = AssetPath::getEdgeAtlasPngPath(edg_name); // ※拡張子の置き換えではなく、文字列追加である
	Path out_meta_name = edg_name.joinString(".meta"); // ※拡張子の置き換えではなく、文字列追加である
	// 画像リストをすべてまとめたものを保存する
	for (size_t i=0; i<pals_.size(); i++) {
		Path palname = AssetPath::makePalettePath(out_palette_base_name, i, "");
		Path palfilename = out_dir.join(palname);

		std::string png = pals_[i].saveToMemory(0/*無圧縮*/);
		FileOutput(palfilename).write(png);
		Log_infof("パレットを保存 :%s", palname.u8());
	}

	Path out_imagefile = out_dir.join(out_png_name);
	Path out_metafile = out_dir.join(out_meta_name);
	packW_.saveToFileNames(out_imagefile, out_metafile);
}
void ImgPackFile::clear() {
	images_.clear();
	pals_.clear();
}

static Blend _GetBlendHint(const KEdgeLayer *layer) {
	// ブレンド指定を見る
	EdgeLayerLabel label;
	GameEdgeBuilder::parseLabel(layer->label_u8_, &label);
	for (int i=0; i<label.numcmds; i++) {
		std::string key, val; // utf8
		K_strsplit_char(label.cmd[i].u8(), &key, '=', &val);
		if (key.compare("blend") == 0) {
			Blend tmp = mo::strToBlend(val.c_str(), Blend_Invalid);
			if (tmp == Blend_Invalid) {
				Log_errorf(u8"E_EDGE_BLEND: ブレンド指定方法が正しくありません: '%s'", label.cmd[i].u8());
				break;
			}
			return tmp;
		}
	}
	return Blend_Invalid;
}
static bool _SpriteFromImage(Sprite *sf, int img_w, int img_h, const Path &sprite_name, const Path &image_name, const Path &original_image_name, int image_index, Blend blend, bool packed, const Vec2 *pivot) {
	Log_assert(sf);
	if (img_w <= 0) return false;
	if (img_h <= 0) return false;
	if (image_name.empty()) return false;
	if (original_image_name.empty()) return false;
	if (sprite_name.empty()) return false;

	// 切り取り範囲
	int atlas_x = 0;
	int atlas_y = 0;
	int atlas_w = img_w;
	int atlas_h = img_h;

	// 基準点
	float pivot_x = floorf(atlas_w * 0.5f);
	float pivot_y = floorf(atlas_h * 0.5f);
	if (pivot) {
		pivot_x = pivot->x;
		pivot_y = pivot->y;
	}

	// あまり変な値が入ってたら警告
	Log_assert(0 <= pivot_x && pivot_x <= atlas_w);
	Log_assert(0 <= pivot_y && pivot_y <= atlas_h);

	// スプライト構築
	sf->clear();
	sf->image_w_ = img_w;
	sf->image_h_ = img_h;
	sf->atlas_x_ = atlas_x;
	sf->atlas_y_ = atlas_y;
	sf->atlas_w_ = atlas_w;
	sf->atlas_h_ = atlas_h;
	sf->pivot_.x = pivot_x;
	sf->pivot_.y = pivot_y;
	sf->pivot_in_pixels = true;
	sf->using_packed_texture = packed;
	sf->num_palettes_ = 0; // PALETTE_IMAGE_SIZE
	sf->mesh_ = KMesh();
	sf->submesh_index_ = image_index; // <-- 何番目の画像を取り出すか？
	sf->texture_name_ = image_name;
	sf->blend_hint_ = blend;
	Log_assert(g_engine);
	if (g_engine->build_callback_) {
		IBuildCallback::SPRITEPARAMS p;
		p.sprite = sf;
		p.sprite_path = sprite_name;
		p.original_path = original_image_name;
		p.image_path = image_name;
		p.image_index = image_index;
		g_engine->build_callback_->onBuildSprite(&p);
	}
	return true;
}

void ImgPackFile::addSpriteFromPack(const Path &edge_name, int tex_w, int tex_h, ExInfo *info) {
	// パック画像に含まれているサブ画像をスプライトとして保存
	Log_assert(g_engine && g_engine->videobank());
	SpriteBank *sbank = g_engine->videobank()->getSpriteBank();

	Path tex_name = AssetPath::getEdgeAtlasPngPath(edge_name);

	int num_images = packR_.getImageCount();
	for (int ii=0; ii<num_images; ii++) {
		SImgExtraData extra;
		int img_w, img_h;
		packR_.getImageSize(ii, &img_w, &img_h);
		packR_.getImageExtra(ii, &extra);

		Path sprite_name = AssetPath::getSpriteAssetPath(edge_name, extra.page, extra.layer);

		// ブレンド指定を見る
		Blend blend_hint = (Blend)extra.blend;
		if (info && info->blend != Blend_Invalid) {
			blend_hint = info->blend;
		}

		// スプライト作成
		Sprite sf;
		if (info) {
			Vec2 pivot;
			pivot.x = info->pivot_x_in_percent ? (img_w * info->pivot_x / 100) : info->pivot_x;
			pivot.y = info->pivot_y_in_percent ? (img_h * info->pivot_y / 100) : info->pivot_y;
			_SpriteFromImage(&sf, img_w, img_h, sprite_name, tex_name, edge_name, ii, blend_hint, true, &pivot);
		} else {
			_SpriteFromImage(&sf, img_w, img_h, sprite_name, tex_name, edge_name, ii, blend_hint, true, NULL);
		}
		// メッシュを作成
		KMesh *mesh = &sf.mesh_;
		{
			int num_vertices = packR_.getVertexCount(sf.submesh_index_);
			if (num_vertices > 0) {
				Vertex *v = mesh->lock(0, num_vertices);
				if (v) {
					packR_.getVertexArray(tex_w, tex_h, sf.submesh_index_, v, num_vertices);
					mesh->unlock();
				}
			}
			KSubMesh sub;
			sub.start = 0;
			sub.count = num_vertices;
			sub.primitive = Primitive_TriangleList;
			mesh->addSubMesh(sub);
		}

		// スプライトを登録
		Sprite *item = sbank->addEmpty(sprite_name);
		*item = sf;
	}
}

bool ImgPackFile::tryLoadFromCache(const Dir &dir, int cellsize, PaletteFlags flags, ExInfo *info) {
	// キャッシュが存在する？
//	Path real_edge = dir.edge_path();
	Path real_cache_png = dir.cache_png_path();
	Path real_cache_meta = dir.cache_meta_path();
	if (!Platform::fileExists(real_cache_png)) return false;
	if (!Platform::fileExists(real_cache_meta)) return false;

	// キャッシュからメタデータをロード
	FileInput file;
	file.open(real_cache_meta);
	std::string meta = file.readBin();
	if (meta.empty()) return false;
	packR_.loadFromMetaString(meta.c_str());

	// データ構築時の .edg ファイルのタイムスタンプを得る
	const char *extra = packR_.getExtraString();
	if (extra == NULL) return false;
	time_t lastmod_original = Platform::getLastModifiedFileTime(dir.edge_path());
	if (lastmod_original == 0) return false;

	// 実際の .edg ファイルのタイムスタンプを得る
	time_t lastmod_current = Platform::getLastModifiedFileTime(dir.edge_path());
	if (lastmod_current == 0) return false;

	// メターデータに記録されたタイムスタンプと、実際のタイムスタンプを比較する
	// 一致すればキャッシュからあロードしてよい。不一致ならダメ
	if (lastmod_original != lastmod_current) return false;

	// パック画像をロード
	std::string bin = FileInput(real_cache_png).readBin();
	KImage tex_image;
	if (!tex_image.loadFromMemory(bin)) {
		return false;
	}

	Log_verbosef("Load packed image from cache: %s", dir.edge_path().u8());

	// パック画像をテクスチャとして登録
	Path tex_name = AssetPath::getEdgeAtlasPngPath(dir.edge_name);
	{
		Log_assert(g_engine && g_engine->videobank());
		TextureBank *tbank = g_engine->videobank()->getTextureBank();
		tbank->addTextureFromImage(tex_name, tex_image);
	}

	// パック画像に含まれているサブ画像をスプライトとして保存
	addSpriteFromPack(dir.edge_name, tex_image.getWidth(), tex_image.getHeight(), info);
	return true;
}

bool ImgPackFile::loadAuto(const Dir &dir, int cellsize, PaletteFlags flags, ExInfo *info) {
	clear();

	// キャッシュからのロードを試みる
	if (tryLoadFromCache(dir, cellsize, flags, info)) {
		return true; // OK
	}
	Path real_edge = dir.edge_path();
	Path real_cache_png = dir.cache_png_path();
	Path real_cache_meta = dir.cache_meta_path();

	// Edge を開く
	KEdgeDocument edge;
	{
		Log_assert(g_engine && g_engine->getFileSystem());
		FileInput file = g_engine->getFileSystem()->getInputFile(dir.edge_name);
		if (file.empty()) {
			Log_errorf(u8"E_FAIL_OPEN: ファイルを開けません: %s", dir.edge_name.u8());
			return false;
		}
		if (!GameEdgeBuilder::loadFromFile(&edge, file, dir.edge_name)) {
			Log_errorf(u8"E_FAIL_EDGE: 正しい EDGE ファイルではありません: %s", dir.edge_name.u8());
			return false;
		}
	}
	// ロードする
	loadFromEdge(&edge, cellsize, flags);

	// 作成元になった edg ファイルのタイムスタンプを extra フィールドに記録しておく
	time_t edg_time = Platform::getLastModifiedFileTime(real_edge);
	packW_.setExtraString(K_sprintf("%ll", edg_time).c_str());

	// キャッシュを保存する
	packW_.saveToFileNames(real_cache_png, real_cache_meta);

	// 復元用パック
	std::string meta;
	packW_.getMetaString(&meta);
	packR_.loadFromMetaString(meta.c_str());

	// パック画像を作成
	KImage teximg = packW_.getPackedImage();

	// パック画像をテクスチャとして登録
	Path tex_name = AssetPath::getEdgeAtlasPngPath(dir.edge_name);
	{
		Log_assert(g_engine && g_engine->videobank());
		TextureBank *tbank = g_engine->videobank()->getTextureBank();
		tbank->addTextureFromImage(tex_name, teximg);
	}

	// パック画像に含まれているサブ画像をスプライトとして保存
	addSpriteFromPack(dir.edge_name, teximg.getWidth(), teximg.getHeight(), info);

	return true;
}
bool ImgPackFile::loadFromEdgeFileName(const Path &edge_name, int cellsize, PaletteFlags flags) {
	Log_assert(g_engine && g_engine->getFileSystem());

	// 入力ファイル
	FileInput file = g_engine->getFileSystem()->getInputFile(edge_name);
	if (!file.isOpen()) {
		return false;
	}

	// Edge ファイルを開く
	KEdgeDocument edge;
	if (!GameEdgeBuilder::loadFromFile(&edge, file, edge_name)) {
		return false;
	}

	loadFromEdge(&edge, cellsize, flags);
	return true;
}
void ImgPackFile::loadFromEdge(const KEdgeDocument *edge, int cellsize, PaletteFlags flags) {
	Log_assert(edge);
	int num_palettes = edge->palettes_.size();

	bool is_indexed = false;
	if (flags & MAKE_INDEXED_TRUE) {
		is_indexed = true;
	} else if (flags & MAKE_INDEXED_FALSE) {
		is_indexed = false;
	} else if (flags & MAKE_INDEXED_AUTO) {
		is_indexed = num_palettes >= 2; // 複数のパレットがある場合のみインデックス画像
	}

	bool save_palettes = false;
	if (flags & SAVE_PALETTE_TRUE) {
		save_palettes = true;
	} else if (flags & SAVE_PALETTE_FALSE) {
		save_palettes = false;
	} else if (flags & SAVE_PALETTE_AUTO) {
		save_palettes = num_palettes >= 2; // 複数のパレットがあれば保存
	}

	// EDGE 内にあるすべてのページ、レイヤーの画像を得る。
	// パレットの有無によって得られる画像の種類が異なるので注意すること

	packW_.init(cellsize);
	if (is_indexed) {
		// データビルド時ではなくゲーム実行時にパレットを適用できるように、インデックス画像を保存する。
		// ただし、1ピクセル8ビットで保存するのではなく、
		// インデックス値をそのまま輝度としてグレースケール化し、RGBA 画像として保存する。
		// パレットの透過色の設定とは関係なく、アルファ値は常に 255 になる
		for (int p=0; p<edge->getPageCount(); p++) {
			const KEdgePage *page = edge->getPage(p);
			for (int l=0; l<page->getLayerCount(); l++) {
				const KEdgeLayer *layer = page->getLayer(l);
				KImage img = edge->exportSurfaceRaw(p, l, edge->bg_palette_.color_index);
				images_.push_back(img);

				SImgExtraData extra;
				extra.page = p;
				extra.layer = l;
				extra.blend = _GetBlendHint(layer);
				packW_.addImage(img, &extra);
			}
		}

	} else {
		// パレット適用済みの画像を作成し、RGBA カラーで保存する
		for (int p=0; p<edge->getPageCount(); p++) {
			const KEdgePage *page = edge->getPage(p);
			for (int l=0; l<page->getLayerCount(); l++) {
				const KEdgeLayer *layer = page->getLayer(l);
				int mask_index = -1; // no used
				int palette_index = 0; // always 0
				KImage img = edge->exportSurfaceWithAlpha(p, l, mask_index, palette_index);
				images_.push_back(img);

				SImgExtraData extra;
				extra.page = p;
				extra.layer = l;
				extra.blend = _GetBlendHint(layer);
				packW_.addImage(img, &extra);
			}
		}
	}

	// パレットを画像化したものを保存する
	if (save_palettes) {
		KImage palette = edge->exportPaletteImage();
		for (int i=0; i<num_palettes; i++) {
			KImage img(16, 16);
			img.copyRect(0, 0, palette, 0, 16*i, 16, 16);
			pals_.push_back(img);
		}
	}
}


} // namespace
