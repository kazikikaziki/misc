#include "build_sprite.h"
//
#include "GameRender.h"
#include "Engine.h"
#include "asset_path.h"
#include "build_callback.h"
#include "build_edge.h"
#include "build_imgpack.h"
#include "inspector.h"
#include "VideoBank.h"
#include "KConv.h"
#include "KEdgeFile.h"
#include "KFileLoader.h"
#include "KImage.h"
#include "KImagePack.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNumval.h"
#include "KPlatform.h"
#include "KStd.h"
#include "KXmlFile.h"
#include "KVideo.h"
#include <algorithm>


namespace mo {

class _SpriteBuilder {
public:
	IBuildCallback *cb_;
	KFileLoader *fs_;
	_SpriteBuilder() {
		cb_ = NULL;
	}
	FileInput getInputFile(const Path &name) {
		return fs_->getInputFile(name);
	}

	/// KImage からスプライトを作成する
	/// img_w, img_h 画像サイズ【スプライトサイズと同じ）
	/// sprite_name スプライト名
	/// texture_name テクスチャの名前
	/// original_image_name 作成元のファイル名。コールバックのために使う。コールバックで不要ならば空文字列で良い
	/// image_index 複数の画像を切り取る場合、そのインデックス
	/// blend ブレンド方法が既に分かっているなら、そのブレンド値。不明なら Blend_Invalid
	/// pack パックされたテクスチャを使っているなら true
	bool buildFromImage(Sprite *sf, int img_w, int img_h, const Path &sprite_name, const Path &texture_name, const Path &original_image_name, int image_index, Blend blend, bool packed) {
		Log_assert(sf);
		if (img_w <= 0) return false;
		if (img_h <= 0) return false;
		if (texture_name.empty()) return false;
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
		sf->texture_name_ = texture_name;
		sf->blend_hint_ = blend;
		if (cb_) {
			IBuildCallback::SPRITEPARAMS p;
			p.sprite = sf;
			p.sprite_path = sprite_name;
			p.original_path = original_image_name;
			p.image_path = texture_name;
			p.image_index = image_index;
			cb_->onBuildSprite(&p);
		}
		return true;
	}
	bool buildFromImage(const KImage &image, const Path &sprite_name, const Path &texture_name, Sprite *sf) {
		Log_assert(sf);
		// スプライト作成
		buildFromImage(sf, image.getWidth(), image.getHeight(), sprite_name, texture_name, texture_name, -1, Blend_Invalid, false);
		return true;
	}
	bool buildFromPngFileName(const Path &png_name, Path *name, Sprite *sf) {
		Log_assert(sf);
		// 画像サイズ
		int img_w = 0;
		int img_h = 0;
		{
			FileInput file = getInputFile(png_name);
			std::string bin24 = file.readBin(24);
			KImageUtils::readPngImageSizeFromFile(bin24.data(), &img_w, &img_h);
			if (img_w == 0 || img_h == 0) return false;
		}
		// スプライト作成
		Path sprite_name = png_name.changeExtension(".sprite");
		buildFromImage(sf, img_w, img_h, sprite_name, png_name, png_name, -1, Blend_Invalid, false);
		if (name) *name = sprite_name;
		return true;
	}
	void buildFromEdgeFileName(const Path &edg_name, std::unordered_map<Path, Sprite> *sprites) {
		Log_assert(sprites);
		FileInput file = getInputFile(edg_name);
		KEdgeDocument edge;
		if (!GameEdgeBuilder::loadFromFile(&edge, file, edg_name)) {
			Log_errorf("E_EDGE_FAIL: %s", edg_name.u8());
			return;
		}
		// .edg ファイルは Packer によって各レイヤーに分解され、1枚のpngとして保存される
		Path png_name = AssetPath::getEdgeAtlasPngPath(edg_name);

		int pack_index = 0; // edge の中身は１次元インデックスでリスト化されている or されるはず。
		for (int p = 0; p < edge.getPageCount(); p++) {
			const KEdgePage *page = edge.getPage(p);
			for (int l = 0; l < page->getLayerCount(); l++) {
				const KEdgeLayer *layer = page->getLayer(l);
				Path sprite_name = AssetPath::getSpriteAssetPath(edg_name, p, l);

				// ブレンド指定を見る
				Blend blend_hint = Blend_Invalid;
				{
					EdgeLayerLabel label;
					GameEdgeBuilder::parseLabel(layer->label_u8_, &label);
					for (int i = 0; i < label.numcmds; i++) {
						std::string key, val; // utf8
						K_strsplit_char(label.cmd[i].u8(), &key, '=', &val);
						if (key.compare("blend") == 0) {
							Blend tmp = mo::strToBlend(val.c_str(), Blend_Invalid);
							if (tmp == Blend_Invalid) {
								Log_errorf(u8"E_EDGE_BLEND: ブレンド指定方法が正しくありません: '%s'", label.cmd[i].u8());
							}
							else {
								blend_hint = tmp;
							}
							break;
						}
					}
				}
				Sprite sf;
				buildFromImage(&sf, page->width_, page->height_, sprite_name, png_name, edg_name, pack_index, blend_hint, true);

				(*sprites)[sprite_name] = sf;

				pack_index++;
			}
		}
	}
};

/// ユニークなインスタンス名を生成する。
/// 他と重複しない名前というだけならインスタンスIDを文字列化するだけでよいが、人間が識別しやすいように型名が入る
Path make_unique_name_p(const char *base) {
	static int s_uniq = 0;
	return Path::fromFormat("__%s%d", base, ++s_uniq);
}

#pragma region Sprite
Sprite::Sprite() {
	clear();
}
void Sprite::clear() {
	image_w_ = 0;
	image_h_ = 0;
	atlas_x_ = 0;
	atlas_y_ = 0;
	atlas_w_ = 0;
	atlas_h_ = 0;
	pivot_.x = 0.0f;
	pivot_.y = 0.0f;
	submesh_index_ = -1;
	name_ = make_unique_name_p("Sprite");
	num_palettes_ = 0;
	blend_hint_ = Blend_Invalid;
	pivot_in_pixels = false;
	using_packed_texture = false;
}
const KMesh * Sprite::getMesh(bool should_exist) const {
	return &mesh_;
}
Vec3 Sprite::offset() const {
	int bmp_x, bmp_y;

	// 左上を原点としたときのピボット座標（ビットマップ座標系）
	if (pivot_in_pixels) {
		bmp_x = (int)pivot_.x;
		bmp_y = (int)pivot_.y;
	} else {
		bmp_x = (int)(atlas_w_ * pivot_.x);
		bmp_y = (int)(atlas_h_ * pivot_.y);
	}

	// 左下を原点としたときのピボット座標（オブジェクト座標系）
	int obj_x = bmp_x;
	int obj_y = atlas_h_ - bmp_y;
		
	// 基準点を現在のスプライトオブジェクトの原点に合わせるためのオフセット量
	return Vec3(-obj_x, -obj_y, 0);
}
#pragma endregion // Sprite

#pragma region SpriteBank
SpriteBank::SpriteBank() {
}
SpriteBank::~SpriteBank() {
	clearAssets();
}
int SpriteBank::getCount() {
	return (int)items_.size();
}
void SpriteBank::delAsset(const Path &name) {
	mutex_.lock();
	{
		auto it = items_.find(name);
		if (it != items_.end()) {
			items_.erase(it);
		}
	}
	mutex_.unlock();
}
void SpriteBank::clearAssets() {
	mutex_.lock();
	{
		items_.clear();
	}
	mutex_.unlock();
}
bool SpriteBank::hasAsset(const Path &name) const {
	mutex_.lock();
	bool ret = items_.find(name) != items_.end();
	mutex_.unlock();
	return ret;
}
void SpriteBank::onInspectorGui(VideoBank *video_bank) {
#ifndef NO_IMGUI
	mutex_.lock();
	if (ImGui::TreeNode("Sprites", "Sprites (%d)", items_.size())) {
		if (ImGui::Button("Clear")) {
			clearAssets();
		}

		bool export_images = false;
		if (ImGui::Button("Export Listed Sprites")) {
			export_images = true;
		}

		// 表示フィルタ
		static char s_filter[256] = {0};
		ImGui::InputText("Filter", s_filter, sizeof(s_filter));

		// ソート済みスプライト名一覧
		PathList names;
		names.reserve(items_.size());
		if (s_filter[0]) {
			// フィルターあり
			for (auto it=items_.cbegin(); it!=items_.cend(); ++it) {
				if (it->first.toUtf8().find(s_filter, 0) != std::string::npos) {
					names.push_back(it->first);
				}
			}
		} else {
			// フィルターなし
			for (auto it=items_.cbegin(); it!=items_.cend(); ++it) {
				names.push_back(it->first);
			}
		}
		std::sort(names.begin(), names.end());

		// リスト表示
		for (auto it=names.cbegin(); it!=names.cend(); ++it) {
			const Path &name = *it;
			if (ImGui::TreeNode(name.u8())) {
				guiSpriteInfo(name, video_bank);
				ImGui::TreePop();
			}
		}

		if (export_images) {
			Path dir = "~ExportedSprites";
			if (Platform::makeDirectory(dir)) {
				for (auto it=names.cbegin(); it!=names.cend(); ++it) {
					const Path &name = *it;
					KImage img = g_engine->videobank()->exportSpriteImage(name);

					// nameはパス区切りなども含んでいるため、エスケープしつつフラットなパスにする。
					Path flatten = AssetPath::escapeFileName(name);
					Path filename = dir.join(flatten).changeExtension(".png");
					std::string png = img.saveToMemory();
					FileOutput(filename).write(png);
				}
			}
		}
		ImGui::TreePop();
	}
	mutex_.unlock();
#endif // !NO_IMGUI
}
void SpriteBank::guiSpriteInfo(const Path &name, VideoBank *video_bank) {
#ifndef NO_IMGUI
	Sprite *sprite = find(name, false);
	if (sprite == NULL) {
		ImGui::Text("(NO SPRITE)");
		return;
	}
	ImGui::Text("Source image Size: %d x %d", sprite->image_w_, sprite->image_h_);
	ImGui::Text("Atlas Pos : %d, %d", sprite->atlas_x_, sprite->atlas_y_);
	ImGui::Text("Atlas Size: %d, %d", sprite->atlas_w_, sprite->atlas_h_);
	if (sprite->submesh_index_ >= 0) {
		ImGui::Text("SubMesh index: %d", sprite->submesh_index_);
	} else {
		ImGui::Text("SubMesh index: ALL");
	}
	if (1) {
		ImGui::DragFloat2("Pivot", (float*)&sprite->pivot_);
		ImGui::Checkbox("Pivot in pixels", &sprite->pivot_in_pixels);
		ImGui::Text("Packed : %s", sprite->using_packed_texture ? "Yes" : "No");
	}
	if (1) {
		if (ImGui::TreeNode(ImGui_ID(1), "Tex: %s", sprite->texture_name_.u8())) {
			guiSpriteTextureInfo(name, video_bank);
			ImGui::TreePop();
		}
	}
	// 描画時のスプライト画像を再現
	if (sprite->using_packed_texture) {
		Path sprite_path = name;
		Path original_tex_path = AssetPath::getTextureAssetPath(sprite_path);
		Path escpath = AssetPath::escapeFileName(original_tex_path);
		Path relpath = Path::fromFormat("__restore__%s.png", escpath.u8());
		Path export_filename = relpath.getFullPath();
		if (video_bank && ImGui::Button("Export sprite image")) {
			KImage image = video_bank->exportSpriteImage(sprite_path);
			if (!image.empty()) {
				std::string png = image.saveToMemory();
				FileOutput(export_filename).write(png);
				Log_infof("Export texture image '%s' in '%s' ==> %s", original_tex_path.u8(), sprite->texture_name_.u8(), export_filename.u8());
			}
		}
		if (Platform::fileExists(export_filename)) {
			ImGui::SameLine();
			Path label = Path::fromFormat("Open: %s", export_filename.u8());
			if (ImGui::Button(label.u8())) {
				Platform::openFileInSystem(export_filename);
			}
		}
	}
#endif // !NO_IMGUI
}

Vec3 SpriteBank::bmpToLocal(const Path &name, const Vec3 &bmp) {
	return bmpToLocal(find(name, false), bmp);
}
Vec3 SpriteBank::bmpToLocal(const Sprite *sprite, const Vec3 &bmp) {
	if (sprite) {
		// ピクセル単位でのピボット座標
		Vec3 piv;
		if (sprite->pivot_in_pixels) {
			piv.x = sprite->pivot_.x;
			piv.y = sprite->pivot_.y;
		} else {
			piv.x = sprite->atlas_w_ * sprite->pivot_.x;
			piv.y = sprite->atlas_h_ * sprite->pivot_.y;
		}
		
		//
		Vec3 off = bmp - piv;

		// BMP座標はY軸下向きなので、これを上向きに直す
		off.y = -off.y;

		return Vec3(off.x, off.y, 0.0f);
	}
	return Vec3();
}

void SpriteBank::guiSpriteTextureInfo(const Path &name, VideoBank *video_bank) {
	Sprite *sprite = find(name, false);
	if (sprite) {
		TextureBank_guiTextureInfo(sprite->texture_name_, EDITOR_THUMBNAIL_SIZE);
	}
}
bool SpriteBank::guiSpriteSelector(const char *label, Path *path) {
	//int sel_index = -1;
	//if (ImGui_Combo(label, names, *path, &sel_index)) {
	//	if (sel_index > 0) {
	//		*path = names[sel_index];
	//	} else {
	//		*path = ""; // NULL が選択された
	//	}
	//	return true;
	//}
	return false;
}
static void _MakeMeshFromSprite(Sprite *sf) {
	// インデックスなし。ただの png ファイルを参照する
	const Path &tex_name = sf->texture_name_;
	TEXID tex = g_engine->videobank()->getTextureBank()->findTextureRaw(tex_name, true);
	int tex_w;
	int tex_h;
	Video::getTextureSize(tex, &tex_w, &tex_h);

	int x = 0;
	int y = 0;
//	int w = sf->image_w_;
//	int h = sf->image_h_;

	int tx0 = sf->atlas_x_;
	int ty0 = sf->atlas_y_;
	int tx1 = sf->atlas_x_ + sf->atlas_w_;
	int ty1 = sf->atlas_y_ + sf->atlas_h_;

	float u0 = (float)tx0 / tex_w;
	float v0 = (float)ty0 / tex_h;
	float u1 = (float)tx1 / tex_w;
	float v1 = (float)ty1 / tex_h;

	MeshShape::makeRect(&sf->mesh_, x, y, x+sf->atlas_w_, y+sf->atlas_h_, u0, v0, u1, v1, Color::WHITE);
}
static void _MakeRectMesh(KMesh *mesh, const RectF &rect, TEXID tex) {
	Log_assert(mesh);
	mesh->clear();
	Vertex *v = mesh->lock(0, 4);
	if (v) {
		float x0 = rect.xmin;
		float x1 = rect.xmax;
		float y0 = rect.ymin;
		float y1 = rect.ymax;

		v[0].pos = Vec3(x0, y1, 0.0f);
		v[1].pos = Vec3(x1, y1, 0.0f);
		v[2].pos = Vec3(x0, y0, 0.0f);
		v[3].pos = Vec3(x1, y0, 0.0f);

		v[0].tex = Vec2(0, 0);
		v[1].tex = Vec2(1, 0);
		v[2].tex = Vec2(0, 1);
		v[3].tex = Vec2(1, 1);

		for (int i=0; i<4; i++) {
			v[i].dif = Color::WHITE;
		}
		mesh->unlock();
	}
	KSubMesh sub;
	sub.start = 0;
	sub.count = 4;
	sub.primitive = Primitive_TriangleStrip;
	mesh->addSubMesh(sub);
}
Sprite * SpriteBank::find(const Path &name, bool should_exist) {
	Sprite *item = NULL;
	mutex_.lock();
	auto it = items_.find(name);
	if (it != items_.end()) {
		item = &it->second;
		Log_assert(item);
	} else if (should_exist) {
		// もしかして
		Path probably;
		for (auto it2=items_.begin(); it2!=items_.end(); it2++) {
			if (it2->first.endsWithPath(name)) {
				probably = it2->first;
				break;
			}
		}
		if (probably.empty()) {
			Log_errorf(u8"E_SPRITE: スプライト '%s' はロードされていません", name.u8());
		} else {
			Log_errorf(u8"E_SPRITE: スプライト '%s' はロードされていません。もしかして: '%s'", name.u8(), probably.u8());
		}
	}
	mutex_.unlock();
	return item;
}
bool SpriteBank::setSpriteTexture(const Path &name, const Path &texture) {
	Sprite *sp = find(name, false);
	if (sp) {
		sp->texture_name_ = texture;
		return true;
	}
	return false;
}
TEXID SpriteBank::getTextureAtlas(const Path &name, float *u0, float *v0, float *u1, float *v1) {
	const Sprite *sp = find(name, false);
	if (sp) {
		TextureBank *tbank = g_engine->videobank()->getTextureBank();
		if (tbank) {
			TEXID texid = tbank->findTextureRaw(sp->texture_name_, true);
			int texw, texh;
			if (texid && Video::getTextureSize(texid, &texw, &texh)) {
				*u0 = (float)sp->atlas_x_ / texw;
				*v0 = (float)sp->atlas_y_ / texh;
				*u1 = (float)(sp->atlas_x_ + sp->atlas_w_) / texw;
				*v1 = (float)(sp->atlas_y_ + sp->atlas_h_) / texh;
				return texid;
			}
		}
	}
	return NULL;
}

Sprite * SpriteBank::addEmpty(const Path &name) {
	mutex_.lock();
	items_[name] = Sprite();
	Sprite *ret = &items_[name];
	ret->name_ = name;
	mutex_.unlock();
	Log_verbosef("ADD_SPRITE: %s", name.u8());
	return ret;
}
bool SpriteBank::addFromSpriteDesc(const Path &name, const Sprite &sf) {
	if (name.empty()) return false;
	if (hasAsset(name)) return false;

	Sprite *loaded_item = find(name, false);
	if (loaded_item) return true;

	Sprite *item = addEmpty(name);
	*item = sf;
	if (sf.mesh_.getVertexCount() == 0) {
		_MakeMeshFromSprite(item);
	} else {
		item->mesh_ = sf.mesh_;
	}
	return true;
}
bool SpriteBank::addFromImage(const Path &name, const Path &tex_name, const KImage &img, int ox, int oy) {
	// .png 画像をテクスチャおよびスプライトとして登録する。
	// 画像ファイルから直接生成するため、atlas ファイルでの Pivot 指定などは利用できない。
	// その代わりに g_engine->build_callback_ でのコールバックを利用する
	if (img.empty()) return false;

	TextureBank *tbank = g_engine->videobank()->getTextureBank();
	tbank->addTextureFromImage(tex_name, img);

	bool ok = false;
	Sprite sf;
	_SpriteBuilder builder;
	builder.fs_ = g_engine->getFileSystem();
	builder.cb_ = g_engine->build_callback_;
	if (builder.buildFromImage(img, name, tex_name, &sf)) {
		sf.pivot_.x = (float)ox;
		sf.pivot_.y = (float)oy;
		sf.pivot_in_pixels = true;
		ok = addFromSpriteDesc(name, sf);
	}
	return ok;
}
bool SpriteBank::addFromPngFileName(const Path &png_name) {
	// .png 画像をテクスチャおよびスプライトとして登録する。
	// 画像ファイルから直接生成するため、atlas ファイルでの Pivot 指定などは利用できない。
	// その代わりに g_engine->build_callback_ でのコールバックを利用する
	
	KImage image;
	FileInput file = g_engine->getFileSystem()->getInputFile(png_name);
	if (file.isOpen()) {
		std::string png = file.readBin();
		image.loadFromMemory(png);
	}

	if (image.empty()) return false;

	TextureBank *tbank = g_engine->videobank()->getTextureBank();
	tbank->addTextureFromImage(png_name, image);

	bool ok = false;
	Sprite sf;
	Path sprite_name;
	_SpriteBuilder builder;
	builder.fs_ = g_engine->getFileSystem();
	builder.cb_ = g_engine->build_callback_;
	if (builder.buildFromPngFileName(png_name, &sprite_name, &sf)) {
		ok = addFromSpriteDesc(sprite_name, sf);
	}
	return ok;
}
bool SpriteBank::addFromTexture(const Path &name, const Path &texname) {
	if (name.empty()) return false;
	if (texname.empty()) return false;
	TEXID tex = g_engine->videobank()->getTexture(texname);

	int w, h;
	Video::getTextureSize(tex, &w, &h);
	
	Sprite *sprite = addEmpty(name);
	sprite->texture_name_ = texname;
	_MakeRectMesh(&sprite->mesh_, RectF(-w/2, -h/2, w/2, h/2), tex);
	sprite->submesh_index_ = 0;
	return sprite != NULL;
}

#pragma endregion // SpriteBank
} // namesapce