#pragma once
#include "GameRender.h"

namespace mo {

class Sprite;
class RenderSystem;

struct SpriteLayer {
	SpriteLayer() {
		visible = true;
	}
	Path sprite;
	Material material;
	Path label;
	Vec3 offset;
	bool visible;
};

struct RenderLayer {
	RenderLayer() {
		texid = NULL;
		index = 0;
	}
	int index;
	Vec3  offset;
	KMesh mesh;
	TEXID texid;
	Material material;
};


#define SS_TEST 0

class SpriteRendererCo
#if SS_TEST
	{
#else
	: public RendererCo {
#endif
public:
	SpriteRendererCo();
	void copyFrom(const SpriteRendererCo *co);
	void setLayerCount(int count);
	int  getLayerCount() const;
	bool getLayerAabb(int index, Vec3 *minpoint, Vec3 *maxpoint);
	void setLayerLabel(int index, const Path &label);
	const Path & getLayerLabel(int index) const;
	void setLayerOffset(int index, const Vec3 &value);
	Vec3 getLayerOffset(int index) const;
	const Path & getLayerSpritePath(int index) const;
	Sprite * getLayerSprite(int index=0);
	void setLayerSprite(int index, Sprite *sprite);
	void setLayerSprite(int index, const Path &asset_path);
	void setLayerVisible(int index, bool value);
	bool isLayerVisible(int index) const;
	void clearLayerFilters();
	void setLayerVisibleFilter(const Path &label, bool visible);
	void copyLayerVisibleFilters(const SpriteRendererCo *src);
	bool layerWillBeRendered(int index);
	Material * getLayerMaterial(int index=0, Material *def=NULL);
	const Material * getLayerMaterial(int index=0, const Material *def=NULL) const;
	Material * getLayerMaterialAnimatable(int index);
	void setLayerMaterial(int index, const Material &material);

	TEXID getBakedSpriteTexture(int layer);

	/// �w�肳�ꂽ�C���f�N�X�̃X�v���C�g�̌��摜�a�l�o�ł̍��W���A���[�J�����W�ɕϊ�����i�e�N�X�`�����W�ł͂Ȃ��j
	/// �Ⴆ�΃X�v���C�g�� player.png ����쐬����Ă���Ȃ�Aplayer.png �摜���̍��W�ɑΉ�����G���e�B�e�B���[�J�����W��Ԃ��B
	///
	/// png �Ȃǂ��璼�ڐ��������X�v���C�g�A�A�g���X�ɂ��؂������X�v���C�g�A�������u���b�N�����k�����X�v���C�g�ɑΉ�����B
	/// ����ȊO�̕��@�ō쐬���ꂽ�X�v���C�g�ł͐������v�Z�ł��Ȃ��\��������B
	///
	/// index �X�v���C�g�ԍ�
	/// bmp_x, bmp_y �X�v���C�g�̍쐬���r�b�g�}�b�v��̍��W�B���_�͉摜����łx���������B�s�N�Z���P�ʁB
	///
	Vec3 bitmapToLocalPoint(int index, const Vec3 &p);
	Vec3 localToBitmapPoint(int index, const Vec3 &p);

	void updateInspectorGui();

#if SS_TEST
	bool getAabb(Vec3 *min_point, Vec3 *max_point);
	void onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot);
	void draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist);
#else
	virtual bool getAabb(Vec3 *min_point, Vec3 *max_point) override;
	virtual void onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot) override;
	virtual void draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) override;
#endif
	void drawWithMeshRenderer();

	void drawInTexture(const RenderLayer *nodes, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const;
	void unpackInTexture(TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid);

	/// �X�v���C�g sprite ���A���� SpriteRendererCo �̐ݒ�ŕ`�悷��Ƃ��ɕK�v�ɂȂ�e�N�X�`���⃁�b�V������������B
	/// �V�����A�Z�b�g���K�v�Ȃ�\�z���A�A�Z�b�g�������[�h��ԂȂ�΃��[�h����B
	/// �e�N�X�`���A�g���X�A�u���b�N�����k�A�p���b�g�̗L���Ȃǂ��e������
	///
	/// tex: NULL �łȂ��l���w�肵���ꍇ�A�X�v���C�g��`�悷�邽�߂Ɏ��ۂɎg�p����e�N�X�`�����擾�ł���
	/// mesh: NULL �łȂ��l�����w�肵���ꍇ�A�X�v���C�g��`�悷�邽�߂Ɏ��ۂɎg�p���郁�b�V�����擾�ł���
	///
	bool preparateMeshAndTextureForSprite(const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh);

	/// �C���f�b�N�X index �̃��C���[�����݂��邱�Ƃ�ۏ؂���
	/// ���݂��Ȃ���� index+1 ���̃��C���[�ɂȂ�悤���C���[��ǉ�����B
	/// ���݂���ꍇ�͉������Ȃ�
	void guaranteeLayerIndex(int index);

	void setModifier(int index);
	int getModifier() const;

	/// �u���b�N���e�N�X�`���[���g���Ă���ꍇ�A���������e�N�X�`���[�ɑ΂��ă}�e���A����K�p���邩�ǂ���
	/// false �̏ꍇ�A�摜�����ƃ}�e���A���K�p�𓯎��ɍs�����߁A�������`�悳��Ȃ��ꍇ������B
	/// true �̏ꍇ�A�������񌳉摜�𕜌����Ă���}�e���A����K�p����B
	/// ���ɁA�����ΏۊO�̃s�N�Z�����Q�Ƃ���悤�ȃG�t�F�N�g���g���ꍇ�́A�������ꂽ�e�N�X�`���[�ɑ΂��ēK�p���Ȃ��Ƃ����Ȃ�
	void setUseUnpackedTexture(bool b);

	bool isVisibleLayerLabel(const Path &label) const;

	VideoBank * getVideoBank();
	RenderSystem * getRS();
	EID entity() const { return entity_; }

#if SS_TEST
	EID entity_;
#endif

private:
	std::vector<SpriteLayer> sprite_layers_;
	std::unordered_map<Path, bool> sprite_filter_layer_labels_;
	std::vector<RenderLayer> render_layers_;
	int layer_count_;
	int modifier_;
	int gui_max_layers_; // �C���X�y�N�^�[�̂��߂̕ϐ�
	bool use_unpacked_texture_;
};













class SpriteSystem: public System {
public:
	SpriteSystem();
	void init(Engine *engine);
	void attach(EID e);
	
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onDetach(EID e) override;
	virtual bool onIsAttached(EID e) override;
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual void onAppRenderUpdate() override;
	virtual void onFrameLateUpdate() override;
	virtual void onEntityInspectorGui(EID e) override;
	virtual bool setParameter(EID e, const char *key, const char *val) override;

	void copyFrom(EID e, const SpriteRendererCo *co);
	int  getLayerCount(EID e) const;
	bool getLayerAabb(EID e, int index, Vec3 *minpoint, Vec3 *maxpoint);
	const Path & getLayerLabel(EID e, int index) const;
	Vec3 getLayerOffset(EID e, int index) const;
	const Path & getLayerSpritePath(EID e, int index) const;
	Sprite * getLayerSprite(EID e, int index);

	void setLayerCount(EID e, int count);
	void setLayerLabel(EID e, int index, const Path &label);
	void setLayerOffset(EID e, int index, const Vec3 &value);
	void setLayerSprite(EID e, int index, Sprite *sprite);
	void setLayerSprite(EID e, int index, const Path &name);
	void setLayerAlpha(EID e, int layer, float alpha);
	void setLayerFilter(EID e, int layer, Filter f);
	void setLayerBlend(EID e, int layer, Blend blend);
	void setLayerColor(EID e, int layer, const Color &color);
	void setLayerSpecular(EID e, int layer, const Color &specular);
	void setLayerCallback(EID e, int layer, IMaterialCallback *cb, void *data);
	void setLayerMaterial(EID e, int index, const Material &material);
	void setLayerVisible(EID e, int index, bool value);

	void setColor(EID e, const Color &color);
	void setSpecular(EID e, const Color &specular);
	void setAlpha(EID e, float alpha);

	bool isLayerVisible(EID e, int index) const;
	void clearLayerFilters(EID e);
	void setLayerVisibleByLabel(EID e, const Path &label, bool visible);
	void copyLayerVisibleFilters(EID e, const SpriteRendererCo *src);
	bool layerWillBeRendered(EID e, int index);

	Material * getLayerMaterial(EID e, int index, Material *def=NULL);
	const Material * getLayerMaterial(EID e, int index, const Material *def=NULL) const;
	Material * getLayerMaterialAnimatable(EID e, int index);


	TEXID getBakedSpriteTexture(EID e, int layer);

	/// �w�肳�ꂽ�C���f�N�X�̃X�v���C�g�̌��摜�a�l�o�ł̍��W���A���[�J�����W�ɕϊ�����i�e�N�X�`�����W�ł͂Ȃ��j
	/// �Ⴆ�΃X�v���C�g�� player.png ����쐬����Ă���Ȃ�Aplayer.png �摜���̍��W�ɑΉ�����G���e�B�e�B���[�J�����W��Ԃ��B
	///
	/// png �Ȃǂ��璼�ڐ��������X�v���C�g�A�A�g���X�ɂ��؂������X�v���C�g�A�������u���b�N�����k�����X�v���C�g�ɑΉ�����B
	/// ����ȊO�̕��@�ō쐬���ꂽ�X�v���C�g�ł͐������v�Z�ł��Ȃ��\��������B
	///
	/// index �X�v���C�g�ԍ�
	/// bmp_x, bmp_y �X�v���C�g�̍쐬���r�b�g�}�b�v��̍��W�B���_�͉摜����łx���������B�s�N�Z���P�ʁB
	///
	Vec3 bitmapToLocalPoint(EID e, int index, const Vec3 &p);
	Vec3 localToBitmapPoint(EID e, int index, const Vec3 &p);

	bool RendererCo_getAabb(EID e, Vec3 *min_point, Vec3 *max_point);
	void RendererCo_onQueryGroupingImageSize(EID e, int *w, int *h, Vec3 *pivot);
	void RendererCo_draw(EID e, RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist);
	void drawInTexture(EID e, const RenderLayer *nodes, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const;
	void unpackInTexture(EID e, TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid);

	/// �X�v���C�g sprite ���A���� SpriteRendererCo �̐ݒ�ŕ`�悷��Ƃ��ɕK�v�ɂȂ�e�N�X�`���⃁�b�V������������B
	/// �V�����A�Z�b�g���K�v�Ȃ�\�z���A�A�Z�b�g�������[�h��ԂȂ�΃��[�h����B
	/// �e�N�X�`���A�g���X�A�u���b�N�����k�A�p���b�g�̗L���Ȃǂ��e������
	///
	/// tex: NULL �łȂ��l���w�肵���ꍇ�A�X�v���C�g��`�悷�邽�߂Ɏ��ۂɎg�p����e�N�X�`�����擾�ł���
	/// mesh: NULL �łȂ��l�����w�肵���ꍇ�A�X�v���C�g��`�悷�邽�߂Ɏ��ۂɎg�p���郁�b�V�����擾�ł���
	///
	bool preparateMeshAndTextureForSprite(EID e, const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh);

	/// �C���f�b�N�X index �̃��C���[�����݂��邱�Ƃ�ۏ؂���
	/// ���݂��Ȃ���� index+1 ���̃��C���[�ɂȂ�悤���C���[��ǉ�����B
	/// ���݂���ꍇ�͉������Ȃ�
	void guaranteeLayerIndex(EID e, int index);

	void setModifier(EID e, int index);
	int getModifier(EID e) const;

	/// �u���b�N���e�N�X�`���[���g���Ă���ꍇ�A���������e�N�X�`���[�ɑ΂��ă}�e���A����K�p���邩�ǂ���
	/// false �̏ꍇ�A�摜�����ƃ}�e���A���K�p�𓯎��ɍs�����߁A�������`�悳��Ȃ��ꍇ������B
	/// true �̏ꍇ�A�������񌳉摜�𕜌����Ă���}�e���A����K�p����B
	/// ���ɁA�����ΏۊO�̃s�N�Z�����Q�Ƃ���悤�ȃG�t�F�N�g���g���ꍇ�́A�������ꂽ�e�N�X�`���[�ɑ΂��ēK�p���Ȃ��Ƃ����Ȃ�
	void setUseUnpackedTexture(EID e, bool b);

	bool isVisibleLayerLabel(EID e, const Path &label) const;

private:
	const SpriteRendererCo * getNode(EID e) const;
	SpriteRendererCo * getNode(EID e);
	RenderSystem *rs_;
	std::unordered_set<EID> nodes_;
};

} // namespace
