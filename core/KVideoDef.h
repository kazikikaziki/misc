#pragma once
#include "KColor.h"
#include "KVec2.h"
#include "KVec3.h"

namespace mo {


struct _TEXID { int dummy; };
typedef _TEXID * TEXID;


struct _SHADERID { int dummy; };
typedef _SHADERID * SHADERID;


struct _VERTEXBUFFERID { int dummy; };
typedef _VERTEXBUFFERID * VERTEXBUFFERID;


struct _INDEXBUFFERID { int dummy; };
typedef _INDEXBUFFERID * INDEXBUFFERID;
	

enum Primitive {
	Primitive_PointList,
	Primitive_LineList,
	Primitive_LineStrip,
	Primitive_TriangleList,
	Primitive_TriangleStrip,
	Primitive_TriangleFan,
	Primitive_EnumMax
};


enum Blend {
	Blend_Invalid = -1,
	Blend_One,
	Blend_Add,
	Blend_Alpha,
	Blend_Sub,
	Blend_Mul,
	Blend_Screen,
	Blend_Max,
	Blend_EnumMax
};


enum Filter {
	Filter_None,
	Filter_Linear,
	Filter_QuadA,
	Filter_QuadB,
	Filter_Anisotropic,
	Filter_EnumMax
};


/// 頂点構造体
///
/// この構造体のメモリ配置は Direct3D9 の頂点フォーマット
/// D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 と完全な互換性がある
struct Vertex {
	/// 座標。
	/// Direct3D9 の頂点フォーマット D3DFVF_XYZ に対応する
	Vec3 pos;

	/// ディフューズ色。
	/// Color32 は unsigned char が b, g, r, a の順で並んでおり、
	/// これは 32 ビット符号なし整数 0xAARRGGBB のリトルエンディアン表現と同じになっている。
	/// Direct3D9 の頂点フォーマット D3DFVF_DIFFUSE に対応する
	Color32 dif;

	/// テクスチャ座標。
	/// Direct3D9 の頂点フォーマット D3DFVF_TEX1 に対応する
	Vec2 tex;
};


struct Material;
struct DrawListItem;

class IMaterialCallback {
public:
	virtual void onMaterial_SetShaderVariable(SHADERID shader, const Material *material) {}
	virtual void onMaterial_Begin(const Material *material) {}
	virtual void onMaterial_End(const Material *material) {}
	virtual void onMaterial_Draw(const DrawListItem *item, bool *done) {
	//	Video::pushRenderState();
	//	Video::setProjection(item->projection);
	//	Video::setTransform(item->transform);
	//	Video::beginMaterial(&item->material);
	//	Video::drawMesh(item->cb_mesh, item->cb_submesh);
	//	Video::endMaterial(&mat);
	//	Video::popRenderState();
	}
};


/// @class
struct Material {
	Material() {
		color = Color::WHITE;
		specular = Color::ZERO;
		texture = NULL;
		shader = NULL;
		filter = Filter_None;
		blend = Blend_Alpha;
		cb = NULL;
		cb_data = NULL;
	}

	Color color;
	Color specular;
	Filter filter;
	TEXID texture;
	SHADERID shader;
	Blend blend;
	IMaterialCallback *cb;
	void *cb_data;
};


} // namespace


