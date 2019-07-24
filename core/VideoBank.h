#pragma once
#include "build_sprite.h" // SpriteBank

#include "GameId.h"
#include "KTextureBank.h"
#include "KShaderBank.h"
#include "KImage.h"
#include "KPath.h"


namespace mo {

class FileOutput;
class Sprite;
class KMesh;

void TextureBank_onInspectorGui(VideoBank *video_bank);


class VideoBank {
public:
	VideoBank() {}
	~VideoBank();
	void init();
	void shutdown();

	TextureBank * getTextureBank() { return &tex_bank_; }
	SpriteBank * getSpriteBank() { return &sprite_bank_; }
	ShaderBank * getShaderBank() { return &shader_bank_; }

	void updateInspector(); 

	/// 実際にスプライトとして表示されるときのスプライト画像を作成する
	KImage exportSpriteImage(const Path &sprite_path);

	/// アセットの追加と取得
	TEXID getTexture(const Path &tex_path);
	TEXID getTextureEx(const Path &tex_path, int modifier, bool should_exist, EID data=NULL);
	Path getTextureName(const TEXID tex);

	bool isRenderTarget(const Path &tex_path);

	/// 実際に画面に描画する時のスプライト画像を反映させたテクスチャを得る
	TEXID getBakedSpriteTexture(const Path &sprite_path, int modifier, bool should_exist=true);

	SHADERID findShader(const Path &asset_path, bool should_exist=true);
	void removeData(const Path &name);

private:
	void _exportSpriteImageEx(KImage &dest_image, const KImage &src_image, const Sprite *sprite, const KMesh *mesh);
	TextureBank tex_bank_;
	SpriteBank sprite_bank_;
	ShaderBank shader_bank_;
};


void meshSaveToXml(const KMesh *mesh, FileOutput &file);
void meshSaveToFile(const KMesh *mesh, FileOutput &file);
void meshSaveToFileName(const KMesh *mesh, const Path &filename);

} // namespace

