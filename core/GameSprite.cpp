#include "GameSprite.h"
//
#include "Engine.h"
#include "asset_path.h"
#include "inspector.h"
#include "VideoBank.h"
#include "KDrawList.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNum.h"
#include "KStd.h"
#include "KToken.h"
#include "KVideo.h"
#include "mo_cstr.h"


#define MAX_SPRITE_LAYER_COUNT 16

namespace mo {


#pragma region SpriteRendererCo
SpriteRendererCo::SpriteRendererCo() {
	modifier_ = 0;
	gui_max_layers_ = 0;
	layer_count_ = 0;
	use_unpacked_texture_ = false;
}
VideoBank * SpriteRendererCo::getVideoBank() {
	return g_engine->videobank();
}
RenderSystem * SpriteRendererCo::getRS() {
	return g_engine->findSystem<RenderSystem>();
}
void SpriteRendererCo::copyFrom(const SpriteRendererCo *co) {
	Log_assert(co);
	EID this_e = entity();
	int numLayers = co->getLayerCount();

	// �X�v���C�g��S���C���[�R�s�[����
	setLayerCount(numLayers);
	for (int i=0; i<numLayers; i++) {
		setLayerSprite (i, co->sprite_layers_[i].sprite);
		setLayerVisible(i, co->sprite_layers_[i].visible);
		setLayerLabel  (i, co->sprite_layers_[i].label);
		setLayerOffset (i, co->sprite_layers_[i].offset);
		const Material *mat_src = co->getLayerMaterial(i);
		if (mat_src) {
			Material *mat_dst = getLayerMaterialAnimatable(i);
			*mat_dst = *mat_src;
		}
	}
	setModifier(co->getModifier());
	copyLayerVisibleFilters(co);
	Matrix4 m = getRS()->getLocalTransform(co->entity());
	getRS()->setLocalTransform(this_e, m);
}
int SpriteRendererCo::getLayerCount() const {
	return layer_count_;
}
void SpriteRendererCo::setModifier(int modifier) {
	modifier_ = modifier;
}
int SpriteRendererCo::getModifier() const {
	return modifier_;
}
// �w�肳�ꂽ�C���f�b�N�X�̃��C���[�����݂��邱�Ƃ�ۏ؂���
void SpriteRendererCo::guaranteeLayerIndex(int index) {
	Log_assert(0 <= index && index < MAX_SPRITE_LAYER_COUNT);
	// ��x�m�ۂ������C���[�͌��炳�Ȃ��B
	// �i�A�j���ɂ���ă��C���[�������ω�����ꍇ�A���C���[�Ɋ��蓖�Ă�ꂽ�}�e���A����
	// ���񐶐��E�폜���Ȃ��Ă������悤�Ɉێ����Ă����߁j
	int n = (int)sprite_layers_.size();
	if (n < index+1) {
		sprite_layers_.resize(index+1);

		// �V�����������̈�����������Ă���
		for (int i=n; i<index+1; i++) {
			sprite_layers_[i] = SpriteLayer();
		}
	}
	if (layer_count_ < index+1) {
		layer_count_ = index+1;

		// ���������ꂽ�̈�����������Ă���
		for (int i=layer_count_; i<n; i++) {
			sprite_layers_[i] = SpriteLayer();
		}
	}
}
void SpriteRendererCo::setUseUnpackedTexture(bool b) {
	use_unpacked_texture_ = b;
}
void SpriteRendererCo::setLayerCount(int count) {
	if (count > 0) {
		Log_assert(count <= MAX_SPRITE_LAYER_COUNT);
		layer_count_ = count;
		guaranteeLayerIndex(count - 1);
	} else {
		layer_count_ = 0;
	}
}
bool SpriteRendererCo::getLayerAabb(int index, Vec3 *minpoint, Vec3 *maxpoint) {
	const Sprite *sp = getLayerSprite(index);
	if (sp == NULL) return false;
	const KMesh *mesh = sp->getMesh();
	if (mesh == NULL) return false;
	Vec3 p0, p1;
	if (! mesh->getAabb(&p0, &p1)) return false;
	Vec3 sp_offset = sp->offset();

	Vec3 offset = getRS()->getOffset(entity());
	*minpoint = sp_offset + offset+ p0;
	*maxpoint = sp_offset + offset+ p1;
	return true;
}
const Path & SpriteRendererCo::getLayerSpritePath(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].sprite;
	} else {
		static const Path s_def;
		return s_def;
	}
}
TEXID SpriteRendererCo::getBakedSpriteTexture(int index) {
	if (0 <= index && index < getLayerCount()) {
		return getVideoBank()->getBakedSpriteTexture(sprite_layers_[index].sprite, modifier_, false);
	}
	return NULL;
}
Sprite * SpriteRendererCo::getLayerSprite(int index) {
	const Path &path = getLayerSpritePath(index);
	if (! path.empty()) {
		SpriteBank *bank = getVideoBank()->getSpriteBank();
		bool should_be_found = true;
		return bank->find(path, should_be_found);
	} else {
		return NULL;
	}
}
void SpriteRendererCo::setLayerSprite(int index, Sprite *sprite) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].sprite = sprite ? sprite->name_ : "";
}

void SpriteRendererCo::setLayerSprite(int index, const Path &asset_path) {
	if (asset_path.empty()) {
		// ���F�����摜�Ƃ݂Ȃ�
	} else if (asset_path.extension().compare(".sprite") != 0) {
		Log_errorf("E_INVALID_FILEPATH_FOR_SPRITE: %s. (NO FILE EXT .sprite)", asset_path.u8());
	}
	guaranteeLayerIndex(index);
	sprite_layers_[index].sprite = asset_path;
}
void SpriteRendererCo::setLayerLabel(int index, const Path &label) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].label = label;
}
const Path & SpriteRendererCo::getLayerLabel(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].label;
	}
	return Path::Empty;
}
void SpriteRendererCo::setLayerOffset(int index, const Vec3 &value) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].offset = value;
}
Vec3 SpriteRendererCo::getLayerOffset(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].offset;
	}
	return Vec3();
}
void SpriteRendererCo::setLayerVisible(int index, bool value) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].visible = value;
}
bool SpriteRendererCo::isLayerVisible(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].visible;
	}
	return false;
}
void SpriteRendererCo::clearLayerFilters() {
	sprite_filter_layer_labels_.clear();
}
void SpriteRendererCo::copyLayerVisibleFilters(const SpriteRendererCo *src) {
	sprite_filter_layer_labels_ = src->sprite_filter_layer_labels_;
}
/// labelw ���x�����B�󕶎���̏ꍇ�́A�ǂ̃��x�����Ƃ��}�b�`���Ȃ��ꍇ�ɓK�p�����
void SpriteRendererCo::setLayerVisibleFilter(const Path &label, bool visible) {
	sprite_filter_layer_labels_[label] = visible;
}
bool SpriteRendererCo::layerWillBeRendered(int index) {
	if (index < 0) return false;
	if (getLayerCount() <= index) return false;
	const SpriteLayer &L = sprite_layers_[index];
	if (! L.visible) return false;
	if (L.sprite.empty()) return false;
	if (! sprite_filter_layer_labels_.empty()) {
		// �t�B���^�[�����݂���ꍇ�́A���̐ݒ�ɏ]��
		auto it = sprite_filter_layer_labels_.find(L.label);
		if (it != sprite_filter_layer_labels_.end()) {
			if (! it->second) return false; // �t�B���^�[�ɂ���Ĕ�\���ɐݒ肳��Ă���
		} else {
			// �t�B���^�[�ɓo�^����Ă��Ȃ����C���[�̏ꍇ�́A�󕶎��� "" ���L�[�Ƃ���ݒ��T���A������f�t�H���g�l�Ƃ���
			if (! sprite_filter_layer_labels_[""]) return false;
		}
	}
	return true;
}
/// �A�j���[�V�����\�ȃ}�e���A���𓾂�
Material * SpriteRendererCo::getLayerMaterialAnimatable(int index) {
	if (index < 0) {
		return NULL;
	}
	if (index < (int)sprite_layers_.size()) {
		return &sprite_layers_[index].material;
	}
	sprite_layers_.resize(index + 1);
	return &sprite_layers_[index].material;
}
Material * SpriteRendererCo::getLayerMaterial(int index, Material *def) {
	if (0 <= index && index < getLayerCount()) {
		return &sprite_layers_[index].material;
	}
	return def;
}
const Material * SpriteRendererCo::getLayerMaterial(int index, const Material *def) const {
	if (0 <= index && index < getLayerCount()) {
		return &sprite_layers_[index].material;
	}
	return def;
}
void SpriteRendererCo::setLayerMaterial(int index, const Material &material) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].material = material;
}
Vec3 SpriteRendererCo::bitmapToLocalPoint(int index, const Vec3 &p) {
	// �X�v���C�g�����擾
	Sprite *sprite = getLayerSprite(index);
	if (sprite == NULL) return Vec3();

	// �X�v���C�g��pivot
	Vec2 sprite_pivot = sprite->pivot_;

	// ���C���[�̃I�t�Z�b�g��
	Vec3 layer_offset = getLayerOffset(0);

	// ���摜�T�C�Y
	int bmp_h = sprite->image_h_;

	// �r�b�g�}�b�v������ł̍��W�i�x������j
	Vec3 bmp_local(p.x, bmp_h - p.y, p.z);

	// �G���e�B�e�B���[�J�����W
	Vec3 local(
		layer_offset.x + bmp_local.x - sprite_pivot.x,
		layer_offset.y + bmp_local.y - sprite_pivot.y,
		layer_offset.z + bmp_local.z);
	
	return local;
}
Vec3 SpriteRendererCo::localToBitmapPoint(int index, const Vec3 &p) {
	// �X�v���C�g�����擾
	Sprite *sprite = getLayerSprite(index);
	if (sprite == NULL) return Vec3();

	// �X�v���C�g��pivot
	Vec2 sprite_pivot = sprite->pivot_;

	// ���C���[�̃I�t�Z�b�g��
	Vec3 layer_offset = getLayerOffset(0);

	// ���摜�T�C�Y
	int bmp_h = sprite->image_h_;

	// �r�b�g�}�b�v������ł̍��W�i�x������j
	Vec3 bmp_local(
		p.x - layer_offset.x + sprite_pivot.x,
		p.y - layer_offset.y + sprite_pivot.y,
		p.z - layer_offset.z);
		
	// �r�b�g�}�b�v�����ł̍��W�i�x�������j
	Vec3 pos(
		bmp_local.x, 
		bmp_h - bmp_local.y,
		bmp_local.z);

	return pos;
}
static Vec3 vec3_min(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		Num_min(a.x, b.x),
		Num_min(a.y, b.y),
		Num_min(a.z, b.z));
}
static Vec3 vec3_max(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		Num_max(a.x, b.x),
		Num_max(a.y, b.y),
		Num_max(a.z, b.z));
}
bool SpriteRendererCo::getAabb(Vec3 *min_point, Vec3 *max_point) {
	bool found = false;
	Vec3 m0, m1;
	int n = getLayerCount();
	for (int i=0; i<n; i++) {
		Vec3 off = getLayerOffset(i);
		Sprite *sp = getLayerSprite(i);
		if (sp) {
			Vec3 roff = sp->offset();
			const KMesh *mesh = sp->getMesh(false);
			if (mesh) {
				Vec3 p0, p1;
				if (mesh->getAabb(&p0, &p1)) {
					p0 += roff + off;
					p1 += roff + off;
					m0 = vec3_min(p0, m0);
					m1 = vec3_max(p1, m1);
					found = true;
				}
			}
		}
	}
	if (found) {
		if (min_point) *min_point = m0;
		if (max_point) *max_point = m1;
		return true;
	}
	return false;
}
void SpriteRendererCo::onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot) {
	// �X�v���C�g�Ɏg����e���C���[�摜�����ׂē����傫���ł���Ɖ��肷��
	Sprite *sp = getLayerSprite(0);
	if (sp) {
		int img_w = sp->image_w_;
		int img_h = sp->image_h_;
		// �X�v���C�g�T�C�Y����łȂ��ꍇ�́A�����ɕ␳����B
		// �O���[�v���`�悷��ꍇ�A�����_�[�^�[�Q�b�g������ƃh�b�g�o�C�h�b�g�ł̕`�悪�c��
		if (img_w % 2 == 1) img_w = (int)ceilf(img_w / 2.0f) * 2; // img_w/2*2 �̓_���Bimg_w==1�Ŕj�]����
		if (img_h % 2 == 1) img_h = (int)ceilf(img_h / 2.0f) * 2; // img_h/2*2 �̓_���Bimg_h==1�Ŕj�]����
		if (w) *w = img_w;
		if (h) *h = img_h;
		if (pivot) {
			Vec2 piv2d = sp->pivot_;
			if (sp->pivot_in_pixels) {
				pivot->x = piv2d.x;
				pivot->y = piv2d.y;
				pivot->z = 0;
			} else {
				pivot->x = img_w * piv2d.x;
				pivot->y = img_h * piv2d.y;
				pivot->z = 0;
			}
		}
	}
}
// �t�B���^�[�̐ݒ�𓾂�
bool SpriteRendererCo::isVisibleLayerLabel(const Path &label) const {

	if (sprite_filter_layer_labels_.empty()) {
		// ���C���[���ɂ��t�B���^�[���w�肳��Ă��Ȃ��B
		// �������ŕ\���\�Ƃ���
		return true;
	}

	// �t�B���^�[�����݂���ꍇ�́A���̐ݒ�ɏ]��
	{
		auto it = sprite_filter_layer_labels_.find(label);
		if (it != sprite_filter_layer_labels_.end()) {
			return it->second;
		}
	}

	// �w�肳�ꂽ���C���[�����t�B���^�[�ɓo�^����Ă��Ȃ������B
	// �󕶎��� "" ���L�[�Ƃ���ݒ肪�f�t�H���g�l�Ȃ̂ŁA���̐ݒ�ɏ]��
	{
		auto it = sprite_filter_layer_labels_.find(Path::Empty);
		if (it != sprite_filter_layer_labels_.end()) {
			return it->second;
		}
	}

	// �f�t�H���g�l���w�肳��Ă��Ȃ��B
	// �������ŕ\���\�Ƃ���
	return true;
}


// �X�v���C�g���C���[�Ƃ��ĕ`�悳���摜���A�P���̃e�N�X�`���Ɏ��߂�
// target �`��惌���_�[�e�N�X�`��
// transform �`��p�ό`�s��B�P�ʍs��̏ꍇ�A�����_�[�e�N�X�`�����������_�Ƃ���A�s�N�Z���P�ʂ̍��W�n�ɂȂ�i�x��������j
void SpriteRendererCo::unpackInTexture(TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid) {
	RenderSystem *rs = g_engine->findSystem<RenderSystem>();
	EID this_e = entity();
	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	int texw=0, texh=0;
	Video::getTextureSize(target, &texw, &texh);

	// �����_�[�^�[�Q�b�g�̃T�C�Y����ɂȂ��Ă���ƁA�h�b�g�o�C�h�b�g�ł̕`�悪�c��
	Log_assert(texw>0 && texw%2==0);
	Log_assert(texh>0 && texh%2==0);

	RenderParams group_params;
	group_params.projection_matrix = Matrix4::fromOrtho((float)texw, (float)texh, (float)(-1000), (float)1000);
	group_params.transform_matrix = transform;
	
	Video::pushRenderTarget(target);
	Video::clearColor(Color::ZERO);
	Video::clearDepth(1.0f);
	Video::clearStencil(0);
	{
		RenderParams params = group_params; // Copy

		// �s�N�Z���p�[�t�F�N�g
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// �O��Ƃ��āA���s�N�Z�����炷�O�̍��W�͐����łȂ��Ƃ����Ȃ�
			if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
			if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
		}

		// �}�e���A��
		// �e�N�X�`���̓��e�����̂܂ܓ]�����邾��
		Material mat;
		mat.blend = Blend_One;
		mat.color = Color32::WHITE;
		mat.specular = Color32::ZERO;
		mat.texture = texid;

		// �`��
		{
			Video::pushRenderState();
			Video::setProjection(params.projection_matrix);
			Video::setTransform(params.transform_matrix);
			Video::beginMaterial(&mat);
			Video::drawMesh(&mesh, 0/*submesh_index*/);
			Video::endMaterial(&mat);
			Video::popRenderState();
		}
	}
	Video::popRenderTarget();
}

void SpriteRendererCo::drawInTexture(const RenderLayer *layers, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const {
	RenderSystem *rs = g_engine->findSystem<RenderSystem>();
	EID this_e = entity();

	// �����_�[�^�[�Q�b�g�̃T�C�Y����ɂȂ��Ă���ƁA�h�b�g�o�C�h�b�g�ł̕`�悪�c��
	Log_assert(w>0 && w%2==0);
	Log_assert(h>0 && h%2==0);

	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	RenderParams group_params;
	group_params.projection_matrix = Matrix4::fromOrtho((float)w, (float)h, (float)(-1000), (float)1000);
	group_params.transform_matrix = transform;
	
	Video::pushRenderTarget(target);
	Video::clearColor(Color::ZERO);
	Video::clearDepth(1.0f);
	Video::clearStencil(0);
	for (int i=0; i<num_nodes; i++) {
		const RenderLayer &layer = layers[i];
		RenderParams params = group_params; // Copy

		// �I�t�Z�b�g
		// �����ǃ����_�[�^�[�Q�b�g���o�R����̂ŁA�ǂ���̕`�掞�ɓK�p����I�t�Z�b�g�Ȃ̂����ӂ��邱��
		if (1) {
			params.transform_matrix = Matrix4::fromTranslation(layer.offset) * params.transform_matrix;
		}

		// �s�N�Z���p�[�t�F�N�g
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// �O��Ƃ��āA���s�N�Z�����炷�O�̍��W�͐����łȂ��Ƃ����Ȃ�
			if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
			if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
		}

		// �`��
		{
			Video::pushRenderState();
			Video::setProjection(params.projection_matrix);
			Video::setTransform(params.transform_matrix);
			Video::beginMaterial(&layer.material);
			Video::drawMesh(&layer.mesh, 0/*submesh_index*/);
			Video::endMaterial(&layer.material);
			Video::popRenderState();
		}
	}
	Video::popRenderTarget();

}
void SpriteRendererCo::drawWithMeshRenderer() {
	RenderSystem *rs = getRS();
	SpriteBank *sbank = getVideoBank()->getSpriteBank();
	TextureBank *tbank = getVideoBank()->getTextureBank();
	EID this_e = entity();

	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	// ���ۂɕ`�悷��ׂ����C���[���A�`�悷��ׂ����ԂŎ擾����
	render_layers_.clear();

	// ���C���[��ԍ��̑傫�Ȃق����珇�Ԃɕ`�悷��
	// �Ȃ��Asprite_layers �͎��ۂɕ`�悷�閇�������]���Ɋm�ۂ���Ă���ꍇ�����邽�߁A
	// �X�v���C�g���C���[�����̎擾�� sprite_layers_.size() ���g���Ă͂����Ȃ��B
	for (int i=layer_count_-1; i>=0; i--) {
		const SpriteLayer &spritelayer = sprite_layers_[i];

		// ��\�����C���[�����O
		if (!spritelayer.visible) {
			continue;
		}
		// �X�v���C�g���w��Ȃ珜�O
		if (spritelayer.sprite.empty()) {
			continue;
		}
		// ���C���[���ɂ��\���ݒ���m�F
		if (! isVisibleLayerLabel(spritelayer.label)) {
			continue;
		}
		// �X�v���C�g���擾
		Sprite *sprite = sbank->find(spritelayer.sprite, true);
		if (sprite == NULL) {
			continue;
		}

		// �`��Ɏg���e�N�X�`���ƃ��b�V�����擾
		RenderLayer renderlayer;
		preparateMeshAndTextureForSprite(sprite, spritelayer.sprite, modifier_, &renderlayer.texid, &renderlayer.mesh);

		if (renderlayer.mesh.getVertexCount() == 0) {
			continue; // ���b�V���Ȃ�
		}

		if (use_unpacked_texture_) {
			// ���C���[���ƂɃ����_�[�^�[�Q�b�g�ɓW�J����
			Path tex_path = AssetPath::makeUnpackedTexturePath(spritelayer.sprite);
			TEXID target_tex = tbank->findTextureRaw(tex_path, false);

			// �����_�[�e�N�X�`���[���擾
			int texW;
			int texH;
			rs->getGroupingImageSize(this_e, &texW, &texH, NULL);
			if (target_tex == NULL) {
				target_tex = tbank->addRenderTexture(tex_path, texW, texH, TextureBank::F_OVERWRITE_SIZE_NOT_MATCHED);
			}

			// �����_�[�e�N�X�`���[�Ƀ��C���[���e�������o��

			// �X�v���C�g�̃��b�V����Y��������ɍ��킹�Ă���B
			// �����_�[�e�N�X�`���̍��������_�ɂ���
			Vec3 pos;
			pos.x -= texW / 2;
			pos.y -= texH / 2;
			Matrix4 transform = Matrix4::fromTranslation(pos); // �`��ʒu
			unpackInTexture(target_tex, transform, renderlayer.mesh, renderlayer.texid);

			// �����_�[�e�N�X�`�����X�v���C�g�Ƃ��ĕ`�悷�邽�߂̃��b�V�����쐬����
			KMesh newmesh;
			int w = sprite->image_w_;
			int h = sprite->image_h_;
			float u0 = 0.0f;
			float u1 = (float)sprite->image_w_ / texW;
			float v0 = (float)(texH-sprite->image_h_) / texH;
			float v1 = 1.0f;
			MeshShape::makeRect(&newmesh, 0, 0, w, h, u0, v0, u1, v1, Color::WHITE);

			// ����ꂽ�����_�[�^�[�Q�b�g���X�v���C�g�Ƃ��ĕ`�悷��
			renderlayer.mesh = newmesh; //<--�V�������b�V���ɕύX
			renderlayer.texid = target_tex; //<-- ���C���[��`�悵�������_�[�e�N�X�`���ɕύX
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);

		} else {
			// �W�J�Ȃ��B�`��Ɠ����ɓW�J����
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);
		}
	}

	int total_vertices = 0;
	for (size_t i=0; i<render_layers_.size(); i++) {
		const RenderLayer &renderlayer = render_layers_[i];
		total_vertices += renderlayer.mesh.getVertexCount();
	}

	KMesh *mesh = rs->getMesh(this_e);
	mesh->clear();
	Vertex *v = mesh->lock(0, total_vertices);
	int offset = 0;
	int isub = 0;
	for (size_t i=0; i<render_layers_.size(); i++) {
		const RenderLayer &renderlayer = render_layers_[i];
		int count = renderlayer.mesh.getVertexCount();
		Log_assert(offset + count <= total_vertices);
		memcpy(v + offset, renderlayer.mesh.getVertices(), sizeof(Vertex)*count);
		KSubMesh laysub;
		renderlayer.mesh.getSubMesh(0, &laysub);

		KSubMesh sub;
		sub.start = offset + laysub.start;
		sub.count = laysub.count;
		sub.blend = laysub.blend;
		sub.primitive = laysub.primitive;
		mesh->addSubMesh(sub);

		Material *m = rs->getSubMeshMaterial(this_e, isub);
		*m = renderlayer.material;

		Matrix4 tr = Matrix4::fromTranslation(renderlayer.offset);
		rs->setSubMeshTransform(this_e, isub, tr);

		offset += count;
		isub++;
	}
	mesh->unlock();

	if (rs->isGrouping(this_e)) {
		int w=0, h=0;
		Vec3 pivot;
		onQueryGroupingImageSize(&w, &h, &pivot);
		rs->setGroupingImageSize(this_e, w, h, pivot);
	}
}

void SpriteRendererCo::draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) {
	SpriteBank *sbank = getVideoBank()->getSpriteBank();
	TextureBank *tbank = getVideoBank()->getTextureBank();
	EID this_e = entity();

	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	// ���ۂɕ`�悷��ׂ����C���[���A�`�悷��ׂ����ԂŎ擾����
	render_layers_.clear();

	// ���C���[��ԍ��̑傫�Ȃق����珇�Ԃɕ`�悷��
	// �Ȃ��Asprite_layers �͎��ۂɕ`�悷�閇�������]���Ɋm�ۂ���Ă���ꍇ�����邽�߁A
	// �X�v���C�g���C���[�����̎擾�� sprite_layers_.size() ���g���Ă͂����Ȃ��B
	for (int i=layer_count_-1; i>=0; i--) {
		const SpriteLayer &spritelayer = sprite_layers_[i];

		// ��\�����C���[�����O
		if (!spritelayer.visible) {
			continue;
		}
		// �X�v���C�g���w��Ȃ珜�O
		if (spritelayer.sprite.empty()) {
			continue;
		}
		// ���C���[���ɂ��\���ݒ���m�F
		if (! isVisibleLayerLabel(spritelayer.label)) {
			continue;
		}
		// �X�v���C�g���擾
		Sprite *sprite = sbank->find(spritelayer.sprite, true);
		if (sprite == NULL) {
			continue;
		}

		// �`��Ɏg���e�N�X�`���ƃ��b�V�����擾
		RenderLayer renderlayer;
		preparateMeshAndTextureForSprite(sprite, spritelayer.sprite, modifier_, &renderlayer.texid, &renderlayer.mesh);

		if (renderlayer.mesh.getVertexCount() == 0) {
			continue; // ���b�V���Ȃ�
		}

		if (use_unpacked_texture_) {
			// ���C���[���ƂɃ����_�[�^�[�Q�b�g�ɓW�J����
			Path tex_path = AssetPath::makeUnpackedTexturePath(spritelayer.sprite);
			TEXID target_tex = tbank->findTextureRaw(tex_path, false);

			// �����_�[�e�N�X�`���[���擾
			int texW;
			int texH;
			rs->getGroupingImageSize(this_e, &texW, &texH, NULL);
			if (target_tex == NULL) {
				target_tex = tbank->addRenderTexture(tex_path, texW, texH, TextureBank::F_OVERWRITE_SIZE_NOT_MATCHED);
			}

			// �����_�[�e�N�X�`���[�Ƀ��C���[���e�������o��

			// �X�v���C�g�̃��b�V����Y��������ɍ��킹�Ă���B
			// �����_�[�e�N�X�`���̍��������_�ɂ���
			Vec3 pos;
			pos.x -= texW / 2;
			pos.y -= texH / 2;
			Matrix4 transform = Matrix4::fromTranslation(pos); // �`��ʒu
			unpackInTexture(target_tex, transform, renderlayer.mesh, renderlayer.texid);

			// �����_�[�e�N�X�`�����X�v���C�g�Ƃ��ĕ`�悷�邽�߂̃��b�V�����쐬����
			KMesh newmesh;
			int w = sprite->image_w_;
			int h = sprite->image_h_;
			float u0 = 0.0f;
			float u1 = (float)sprite->image_w_ / texW;
			float v0 = (float)(texH-sprite->image_h_) / texH;
			float v1 = 1.0f;
			MeshShape::makeRect(&newmesh, 0, 0, w, h, u0, v0, u1, v1, Color::WHITE);

			// ����ꂽ�����_�[�^�[�Q�b�g���X�v���C�g�Ƃ��ĕ`�悷��
			renderlayer.mesh = newmesh; //<--�V�������b�V���ɕύX
			renderlayer.texid = target_tex; //<-- ���C���[��`�悵�������_�[�e�N�X�`���ɕύX
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);

		} else {
			// �W�J�Ȃ��B�`��Ɠ����ɓW�J����
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);
		}
	}

	// �}�X�^�[�J���[
	Color master_col = rs->getMasterColor(this_e);
	Color master_spe = rs->getMasterSpecular(this_e);

	// �e�̐ݒ���p������ꍇ�͍����}�X�^�[�J���[�𓾂�
	if (rs->getFlag(this_e, RenderSystem::INHERIT_COLOR)) {
		master_col = rs->getMasterColorInTree(this_e);
	}
	if (rs->getFlag(this_e, RenderSystem::INHERIT_SPECULAR)) {
		master_spe = rs->getMasterSpecularInTree(this_e);
	}

	if (rs->isGrouping(this_e)) {
		// �O���[�v������B
		// ��U�ʂ̃����_�[�e�N�X�`���ɏ����o���A������X�v���C�g�Ƃ��ĕ`�悷��
		int groupW;
		int groupH;
		Vec3 groupPiv;
		rs->getGroupingImageSize(this_e, &groupW, &groupH, &groupPiv);
		TEXID group_tex = rs->getGroupRenderTexture(this_e);
		Matrix4 local_transform = rs->getLocalTransform(this_e); // �摜�p�̕ό`

		// groupPiv�̓r�b�g�}�b�v���_�i�������_�[�^�[�Q�b�g�摜����j����ɂ��Ă��邱�Ƃɒ���
		Vec3 pos;
		pos.x -= groupW / 2; // �����_�[�e�N�X�`�������
		pos.y += groupH / 2;
		pos.x += groupPiv.x; // ����� groupPiv���炷
		pos.y -= groupPiv.y;
		Matrix4 group_transform = Matrix4::fromTranslation(pos) * local_transform; // �`��ʒu

		// �`��
		drawInTexture(render_layers_.data(), render_layers_.size(), group_tex, groupW, groupH, group_transform);

		// �O���[�v���`��I���B�����܂ł� render_target �e�N�X�`���ɂ̓X�v���C�g�̊G���`�悳��Ă���B
		// ��������߂ĕ`�悷��
		{
			RenderParams params = render_params; // Copy

			// �I�t�Z�b�g
			{
				Vec3 off;
				off += rs->getOffset(this_e); // �ꊇ�I�t�Z�b�g�B���ׂẴ��C���[���e�����󂯂�
			//	off += layer.offset; // ���C���[���ƂɎw�肳�ꂽ�I�t�Z�b�g
			//	off += sprite->offset(); // �X�v���C�g�Ɏw�肳�ꂽ Pivot ���W�ɂ��I�t�Z�b�g
				off.x += -groupPiv.x; // �����_�[�^�[�Q�b�g���̃s���H�b�g���A�ʏ�`�掞�̌��_�ƈ�v����悤�ɒ�������
				off.y +=  groupPiv.y - groupH;
				params.transform_matrix = Matrix4::fromTranslation(off) * params.transform_matrix;
			}

			// �s�N�Z���p�[�t�F�N�g
			if (MO_VIDEO_ENABLE_HALF_PIXEL) {
				// �O��Ƃ��āA���s�N�Z�����炷�O�̍��W�͐����łȂ��Ƃ����Ȃ�
				if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
				if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
			}

			// �}�e���A��
			Material *p_mat = rs->getGroupingMaterial(this_e);
			Log_assert(p_mat);
			Material mat = *p_mat; // COPY
			mat.texture = group_tex;

			// �}�X�^�[�J���[�ƍ�������
			mat.color    *= master_col; // MUL
			mat.specular += master_spe; // ADD

			// �`��
			KMesh *groupMesh = rs->getGroupRenderMesh(this_e);
			if (drawlist) {
				drawlist->setProjection(params.projection_matrix);
				drawlist->setTransform(params.transform_matrix);
				drawlist->setMaterial(mat);
				drawlist->addMesh(groupMesh, 0);
			} else {
				Video::pushRenderState();
				Video::setProjection(params.projection_matrix);
				Video::setTransform(params.transform_matrix);
				Video::beginMaterial(&mat);
				Video::drawMesh(groupMesh, 0);
				Video::endMaterial(&mat);
				Video::popRenderState();
			}
		}
		return;
	}
	
	// �O���[�v�����Ȃ��B
	// �ʏ�`�悷��
	for (size_t i=0; i<render_layers_.size(); i++) {
		const RenderLayer &renderlayer = render_layers_[i];

		RenderParams params = render_params; // Copy

		// �摜�p�̕ό`��K�p
		params.transform_matrix = rs->getLocalTransform(this_e) * params.transform_matrix;

		// �I�t�Z�b�g
		{
			Vec3 off;
			off += rs->getOffset(this_e); // �ꊇ�I�t�Z�b�g�B���ׂẴ��C���[���e�����󂯂�
			off += renderlayer.offset; // ���C���[���Ƃ̃I�t�Z�b�g
			params.transform_matrix = Matrix4::fromTranslation(off) * params.transform_matrix;
		}

		// �s�N�Z���p�[�t�F�N�g
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// �O��Ƃ��āA���s�N�Z�����炷�O�̍��W�͐����łȂ��Ƃ����Ȃ�
			if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
			if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
		}

		// �}�e���A��
		Material *p_mat = getLayerMaterialAnimatable(renderlayer.index);
		Log_assert(p_mat);
		Material mat = *p_mat; // COPY
		mat.texture = renderlayer.texid;

		// �}�X�^�[�J���[�ƍ�������
		mat.color    *= master_col; // MUL
		mat.specular += master_spe; // ADD

		// �`��
		if (drawlist) {
			drawlist->setProjection(params.projection_matrix);
			drawlist->setTransform(params.transform_matrix);
			drawlist->setMaterial(mat);
			drawlist->addMesh(&renderlayer.mesh, 0);
		} else {
			Video::pushRenderState();
			Video::setProjection(params.projection_matrix);
			Video::setTransform(params.transform_matrix);
			Video::beginMaterial(&mat);
			Video::drawMesh(&renderlayer.mesh, 0/*submesh_index*/);
			Video::endMaterial(&mat);
			Video::popRenderState();
		}
	}
}
bool SpriteRendererCo::preparateMeshAndTextureForSprite(const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh) {
	EID this_e = entity();

	// �e�N�X�`�����擾�B�Ȃ���΍쐬
	TEXID actual_tex = getVideoBank()->getTextureEx(sprite->texture_name_, modifier, true, this_e);

	if (tex) *tex = actual_tex;
	*mesh = sprite->mesh_;
	return true;
}

#ifndef NO_IMGUI
bool ImGui_TreeNodeColored(const void *ptr_id, const ImVec4& col, const char *fmt, ...) {
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	va_list args;
	va_start(args, fmt);
	bool is_open = ImGui::TreeNodeExV(ptr_id, 0, fmt, args);
	va_end(args);
	ImGui::PopStyleColor();
	return is_open;
}

#endif

void SpriteRendererCo::updateInspectorGui() {
#ifndef NO_IMGUI
	EID this_e = entity();

	SpriteBank *sprite_bank = getVideoBank()->getSpriteBank();

	// ����N���b�v���Ń��C���[�������ω�����ꍇ�A
	// �C���X�y�N�^�[�̕\�����C���[���t���[���P�ʂő������茸�����肵�Ĉ��肵�Ȃ��B
	// ���₷�����邽�߂ɁA����܂ł̍ő僌�C���[���̕\���̈���m�ۂ��Ă���
	gui_max_layers_ = Num_max(gui_max_layers_, getLayerCount());
	for (int i=0; i<gui_max_layers_; i++) {
		ImGui::PushID(0);
		if (i < getLayerCount()) {
			ImVec4 color = isLayerVisible(i) ? GuiColor_ActiveText : GuiColor_DeactiveText;
			if (ImGui_TreeNodeColored(ImGui_ID(i), color, "Layer[%d]: %s", i, sprite_layers_[i].sprite.u8())) {
				ImGui::Text("Label: %s", sprite_layers_[i].label.u8());
				sprite_bank->guiSpriteSelector("Sprite", &sprite_layers_[i].sprite);

				ImGui::Text("Will be rendered: %s", layerWillBeRendered(i) ? "Yes" : "No");
				bool b = isLayerVisible(i);
				if (ImGui::Checkbox("Visible", &b)) {
					setLayerVisible(i, b);
				}
				Vec3 p = getLayerOffset(i);
				if (ImGui::DragFloat3("Offset", (float*)&p, 1)) {
					setLayerOffset(i, p);
				}
				if (ImGui::TreeNode(ImGui_ID(0), "Sprite: %s", sprite_layers_[i].sprite.u8())) {
					sprite_bank->guiSpriteInfo(sprite_layers_[i].sprite, getVideoBank());
					ImGui::TreePop();
				}
				if (ImGui::TreeNode(ImGui_ID(1), "Material")) {
					Material *mat = getLayerMaterial(i);
					Data_guiMaterialInfo(mat);
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
		} else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
			if (ImGui::TreeNode(ImGui_ID(i), "Layer[%d]: (none)", i)) {
				ImGui::TreePop();
			}
			ImGui::PopStyleColor();
		}
		ImGui::PopID();
	}
	if (ImGui::TreeNode(ImGui_ID(-1), "Layer filters")) {
		for (auto it=sprite_filter_layer_labels_.begin(); it!=sprite_filter_layer_labels_.end(); ++it) {
			if (it->first.empty()) {
				ImGui::Checkbox("(Default)", &it->second);
			} else {
				ImGui::Checkbox(it->first.u8(), &it->second);
			}
		}
		ImGui::TreePop();
	}
	ImGui::Checkbox("Use unpacked texture", &use_unpacked_texture_);
/*
	ImGui::Checkbox(u8"Unpack/�A���p�b�N", &use_unpacked_texture_);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			u8"�X�v���C�g�Ƃ��ĕ\������e�N�X�`���[���u���b�N���k����Ă���ꍇ�A\n"
			u8"����̐ݒ�ł͕`��ƓW�J�𓯎��ɍs���܂����A���̍��ڂɃ`�F�b�N��t�����ꍇ�́A\n"
			u8"��x�����_�[�e�N�X�`���[�ɉ摜��W�J���Ă���`�悵�܂��B\n"
			u8"�V�F�[�_�[��UV�A�j�������������f����Ȃ��ꍇ�A�����Ƀ`�F�b�N������Ɖ��P�����ꍇ������܂�\n"
			u8"�u���b�N���k����Ă��Ȃ��e�N�X�`���[�𗘗p���Ă���ꍇ�A���̃I�v�V�����͖�������܂�"
		);
	}
*/
	if (ImGui::TreeNode(ImGui_ID(-2), "Test")) {
		ImGui::Text("SkewTest");
		if (ImGui::Button("-20")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(-20));
		}
		ImGui::SameLine();
		if (ImGui::Button("-10")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(-10));
		}
		ImGui::SameLine();
		if (ImGui::Button("0")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(0));
		}
		ImGui::SameLine();
		if (ImGui::Button("10")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(10));
		}
		ImGui::SameLine();
		if (ImGui::Button("20")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(20));
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
#pragma endregion // SpriteRendererCo














SpriteSystem::SpriteSystem() {
	rs_ = NULL;
}
void SpriteSystem::init(Engine *engine) {
	engine->addSystem(this);
}
void SpriteSystem::onStart() {
	rs_ = g_engine->findSystem<RenderSystem>();
}
void SpriteSystem::onEnd() {
	rs_ = NULL;
}
void SpriteSystem::attach(EID e) {
	Log_assert(rs_);

#if SS_TEST
	// �K�� MeshRenderer ���Z�b�g�ŃA�^�b�`����
	rs_->attachMesh(e);
	rs_->attachSprite(e);
	nodes_.insert(e);
#else
	rs_->attachSprite(e);
#endif
}
bool SpriteSystem::onAttach(EID e, const char *type) {
	if (strcmp(type, "SpriteRenderer") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void SpriteSystem::onDetach(EID e) {
	nodes_.erase(e);
}
bool SpriteSystem::onIsAttached(EID e) {
	return getNode(e) != NULL;

}
bool SpriteSystem::setParameter(EID e, const char *key, const char *val) {
	if (getNode(e) == NULL) {
		return false;
	}
	// ���C���[���Ƃ̃X�v���C�g�A�}�e���A���ݒ�
	// <SpriteRenderer
	//		layer-0.sprite   = "fg.sprite"
	//		layer-0.material = "fg.material"
	//		layer-0.visible  = 1
	//		layer-1.sprite   = "bg.sprite"
	//		layer-1.material = "bg.material"
	//		layer-1.visible  = 1
	//	/>
	// "layer-0.XXX"
	KToken tok(key, "-.");
	if (tok.compare(0, "layer") == 0) {
		int index = K_strtoi(tok.get(1, "-1"));
		if (index < 0) {
			Log_warningf("Invalid layer index '%s'", key);
			return false;
		}
		const char *subkey = tok.get(2, NULL);
		if (subkey == NULL) {
			Log_warningf("Invalid subkey '%s'", key);
			return false;
		}
		// "layer-0.sprite"
		if (strcmp(subkey, "sprite") == 0) {
			setLayerSprite(e, index, val);
			return true;
		}
		// "layer-0.visible"
		if (strcmp(subkey, "visible") == 0) {
			setLayerVisible(e, index, K_strtoi(val));
			return true;
		}
		// "layer-0.name"
		if (strcmp(subkey, "name") == 0) {
			setLayerLabel(e, index, val);
			return true;
		}
	}

	// �݊����BMeshRenderer.XXX �� SpriteRenderer.XXX �ł��A�N�Z�X�ł���悤��
	// �����͍폜�������B
	if (rs_->setParameter(e, key, val)) {
		return true;
	}

	return false;
}
void SpriteSystem::onAppRenderUpdate() {
#if SS_TEST
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = *it;
		SpriteRendererCo *co = getNode(e);
		if (co) {
			co->drawWithMeshRenderer();
		}
	}
#endif
}
void SpriteSystem::onFrameLateUpdate() {
}


const SpriteRendererCo * SpriteSystem::getNode(EID e) const {
	Log_assert(rs_);
	return rs_->getSpriteRenderer(e);
}
SpriteRendererCo * SpriteSystem::getNode(EID e) {
	Log_assert(rs_);
	return rs_->getSpriteRenderer(e);
}
void SpriteSystem::copyFrom(EID e, const SpriteRendererCo *orhter) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->copyFrom(orhter);
}
void SpriteSystem::setLayerCount(EID e, int count) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerCount(count);
}
int SpriteSystem::getLayerCount(EID e) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerCount() : 0;
}
bool SpriteSystem::getLayerAabb(EID e, int index, Vec3 *minpoint, Vec3 *maxpoint) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerAabb(index, minpoint, maxpoint) : false;
}
void SpriteSystem::setLayerLabel(EID e, int index, const Path &label) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerLabel(index, label);
}
const Path & SpriteSystem::getLayerLabel(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerLabel(index) : Path::Empty;
}
void SpriteSystem::setLayerOffset(EID e, int index, const Vec3 &value) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerOffset(index, value);
}
Vec3 SpriteSystem::getLayerOffset(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerOffset(index) : Vec3();
}
const Path & SpriteSystem::getLayerSpritePath(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerSpritePath(index) : Path::Empty;
}
Sprite * SpriteSystem::getLayerSprite(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerSprite(index) : NULL;
}
void SpriteSystem::setLayerSprite(EID e, int index, Sprite *sprite) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerSprite(index, sprite);
}
void SpriteSystem::setLayerSprite(EID e, int index, const Path &name) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerSprite(index, name);
}
void SpriteSystem::setLayerVisible(EID e, int index, bool value) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerVisible(index, value);
}
bool SpriteSystem::isLayerVisible(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->isLayerVisible(index) : false;
}
void SpriteSystem::setLayerAlpha(EID e, int layer, float alpha) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->color.a = alpha;
}
void SpriteSystem::setLayerFilter(EID e, int layer, Filter f) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->filter = f;
}
void SpriteSystem::setLayerBlend(EID e, int layer, Blend blend) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->blend = blend;
}
void SpriteSystem::setLayerColor(EID e, int layer, const Color &color) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->color = color;
}
void SpriteSystem::setLayerSpecular(EID e, int layer, const Color &specular) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->specular = specular;
}
void SpriteSystem::setLayerCallback(EID e, int layer, IMaterialCallback *cb, void *data) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) {
		mat->cb = cb;
		mat->cb_data = data;
	}
}
void SpriteSystem::clearLayerFilters(EID e) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->clearLayerFilters();
}
void SpriteSystem::setLayerVisibleByLabel(EID e, const Path &label, bool visible) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerVisibleFilter(label, visible);
}
void SpriteSystem::copyLayerVisibleFilters(EID e, const SpriteRendererCo *src) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->copyLayerVisibleFilters(src);
}
bool SpriteSystem::layerWillBeRendered(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->layerWillBeRendered(index) : false;
}
Material * SpriteSystem::getLayerMaterial(EID e, int index, Material *def) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerMaterial(index, def) : NULL;
}
const Material * SpriteSystem::getLayerMaterial(EID e, int index, const Material *def) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerMaterial(index, def) : NULL;
}
Material * SpriteSystem::getLayerMaterialAnimatable(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerMaterialAnimatable(index) : NULL;
}
void SpriteSystem::setLayerMaterial(EID e, int index, const Material &material) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerMaterial(index, material);
}
void SpriteSystem::setColor(EID e, const Color &color) {
	Log_assert(rs_);
	rs_->setMasterColor(e, color);
}
void SpriteSystem::setAlpha(EID e, float alpha) {
	Log_assert(rs_);
	rs_->setMasterAlpha(e, alpha);
}
void SpriteSystem::setSpecular(EID e, const Color &specular) {
	Log_assert(rs_);
	rs_->setMasterSpecular(e, specular);
}
TEXID SpriteSystem::getBakedSpriteTexture(EID e, int layer) {
	SpriteRendererCo *co = getNode(e);
	return co->getBakedSpriteTexture(layer);
}

/// �w�肳�ꂽ�C���f�N�X�̃X�v���C�g�̌��摜�a�l�o�ł̍��W���A���[�J�����W�ɕϊ�����i�e�N�X�`�����W�ł͂Ȃ��j
/// �Ⴆ�΃X�v���C�g�� player.png ����쐬����Ă���Ȃ�Aplayer.png �摜���̍��W�ɑΉ�����G���e�B�e�B���[�J�����W��Ԃ��B
///
/// png �Ȃǂ��璼�ڐ��������X�v���C�g�A�A�g���X�ɂ��؂������X�v���C�g�A�������u���b�N�����k�����X�v���C�g�ɑΉ�����B
/// ����ȊO�̕��@�ō쐬���ꂽ�X�v���C�g�ł͐������v�Z�ł��Ȃ��\��������B
///
/// index �X�v���C�g�ԍ�
/// bmp_x, bmp_y �X�v���C�g�̍쐬���r�b�g�}�b�v��̍��W�B���_�͉摜����łx���������B�s�N�Z���P�ʁB
///
Vec3 SpriteSystem::bitmapToLocalPoint(EID e, int index, const Vec3 &p) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->bitmapToLocalPoint(index, p) : Vec3();
}
Vec3 SpriteSystem::localToBitmapPoint(EID e, int index, const Vec3 &p) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->localToBitmapPoint(index, p) : Vec3();
}
bool SpriteSystem::RendererCo_getAabb(EID e, Vec3 *min_point, Vec3 *max_point) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getAabb(min_point, max_point) : false;
}
void SpriteSystem::RendererCo_onQueryGroupingImageSize(EID e, int *w, int *h, Vec3 *pivot) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->onQueryGroupingImageSize(w, h, pivot);
}
void SpriteSystem::RendererCo_draw(EID e, RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->draw(rs, render_params, drawlist);
}
void SpriteSystem::onEntityInspectorGui(EID e) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->updateInspectorGui();
}
void SpriteSystem::drawInTexture(EID e, const RenderLayer *nodes, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const {
	const SpriteRendererCo *co = getNode(e);
	if (co) co->drawInTexture(nodes, num_nodes, target, w, h, transform);
}
void SpriteSystem::unpackInTexture(EID e, TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->unpackInTexture(target, transform, mesh, texid);
}
bool SpriteSystem::preparateMeshAndTextureForSprite(EID e, const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->preparateMeshAndTextureForSprite(sprite, sprite_name, modifier, tex, mesh) : false;
}
void SpriteSystem::guaranteeLayerIndex(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->guaranteeLayerIndex(index);
}
void SpriteSystem::setModifier(EID e, int modifier) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setModifier(modifier);
}
int SpriteSystem::getModifier(EID e) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getModifier() : 0;
}
void SpriteSystem::setUseUnpackedTexture(EID e, bool b) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setUseUnpackedTexture(b);
}
bool SpriteSystem::isVisibleLayerLabel(EID e, const Path &label) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->isVisibleLayerLabel(label) : false;
}



} // namespace
