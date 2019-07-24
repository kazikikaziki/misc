#pragma once
#include "KMesh.h"
#include "build_imgpack.h"
#include <unordered_map>
#include <mutex>

namespace mo {

class Path;
class VideoBank;

class Sprite {
public:
	Sprite();
	void clear();
	const KMesh * getMesh(bool should_exist=true) const;
	Vec3 offset() const;

	Path name_;
	KMesh mesh_;
	Path texture_name_;
	int submesh_index_;
	int image_w_; // 作成元画像の横幅。テクスチャの幅ではなく、元画像のサイズであることに注意。サイズが 2^n とは限らない
	int image_h_;
	int num_palettes_;

	// 元画像からの切り取り範囲
	// ※テクスチャからの切り取り範囲ではない！
	// あくまでも、元のBITMAP画像内での切り取り範囲であることに注意。
	// テクスチャ化するときに再配置したり余白の切り落としなどがあったとしても atlas 範囲は変化しない
	int atlas_x_;
	int atlas_y_;
	int atlas_w_;
	int atlas_h_;

	/// ピボット座標。
	/// アトラス範囲の左上を原点としたときのピボット座標を指定する。
	/// 座標単位は pivot_in_pixels によって決まる
	/// pivot_in_pixels が true ならばピクセル単位で指定する。
	/// pivot_in_pixels が false なら左上を(0, 0)右下を(1, 1)とする正規化ビットマップ座標で指定する。
	/// @note 「正規化ビットマップ座標」における右下座標(1, 1)は、テクスチャや元画像の右下ではなく、アトラス範囲での右下になることに注意。
	/// 元画像が 2^n サイズでない場合、自動的に余白が追加されたテクスチャになるため、元画像の右下とテクスチャの右下は一致しなくなる。

	/// 元画像内での pivot 座標は次のようにして計算する
	/// if (pivot_in_pixels) {
	///   point = Vec2(atlas_x_, atlas_y_) + spritepivot_;
	/// } else {
	///   point = Vec2(atlas_x_, atlas_y_) + Vec2(atlas_w_ * pivot_.x, atlas_h_ * pivot_.y);
	/// }
	Vec2 pivot_;

	Blend blend_hint_;
	bool pivot_in_pixels;
	bool using_packed_texture;
};

class SpriteBank {
public:
	SpriteBank();
	~SpriteBank();
	Sprite * addEmpty(const Path &name);
	Sprite * find(const Path &name, bool should_exist);
	bool setSpriteTexture(const Path &name, const Path &texture);
	TEXID getTextureAtlas(const Path &name, float *u0, float *v0, float *u1, float *v1);

	bool addFromSpriteDesc(const Path &name, const Sprite &sf);
	bool addFromTexture(const Path &name, const Path &tex_name);
	bool addFromPngFileName(const Path &png_name);
	bool addFromImage(const Path &name, const Path &tex_name, const KImage &img, int ox, int oy);
	int getCount();
	void delAsset(const Path &name);
	void clearAssets();
	bool hasAsset(const Path &name) const;
	void onInspectorGui(VideoBank *video_bank);
	bool guiSpriteSelector(const char *label, Path *path);
	void guiSpriteInfo(const Path &name, VideoBank *video_bank);
	void guiSpriteTextureInfo(const Path &name, VideoBank *video_bank);

	// スプライトの生成元画像乗での座標（ビットマップ座標系。左上原点Y軸下向き）を指定して、
	// スプライトのローカル座標（Pivot原点、Y軸上向き）を得る
	// 指定されたスプライトが存在しない場合はゼロを返す
	Vec3 bmpToLocal(const Path &name, const Vec3 &bmp);
	Vec3 bmpToLocal(const Sprite *sprite, const Vec3 &bmp);

private:
	std::unordered_map<Path, Sprite> items_;
	mutable std::recursive_mutex mutex_;
};

} // namespace
