#include "VideoBank.h"
//
#include "inspector.h"
#include "asset_path.h"
#include "KFile.h"
#include "KImgui.h"
#include "KLog.h"
#include "KStd.h"
#include "KVideo.h"

#define M_DELETE(a)  {if (a) delete a, (a)=NULL;}

namespace mo {





#pragma region VideoBank
VideoBank::~VideoBank() {
	shutdown();
}
void VideoBank::init() {
	Log_trace;
	shader_bank_.init();
}
void VideoBank::shutdown() {
	tex_bank_.clearAssets();
	sprite_bank_.clearAssets();
	shader_bank_.clearAssets();
}
void VideoBank::removeData(const Path &name) {
	tex_bank_.delAsset(name);
	sprite_bank_.delAsset(name);
	shader_bank_.delAsset(name);
}
bool VideoBank::isRenderTarget(const Path &tex_path) {
	TextureBank *tex_bank = getTextureBank();
	TEXID tex = tex_bank->findTextureRaw(tex_path, false);
	TexDesc desc;
	Video::getTextureDesc(tex, &desc);
	return tex && desc.is_render_target;
}
Path VideoBank::getTextureName(const TEXID tex) {
	TextureBank *bank = getTextureBank();
	return bank->getName(tex);
}
TEXID VideoBank::getTextureEx(const Path &tex_path, int modifier, bool should_exist, EID data) {
	TextureBank *tbank = getTextureBank();
	return tbank->findTexture(tex_path, modifier, false, data);
}
TEXID VideoBank::getTexture(const Path &tex_path) {
	TextureBank *tbank = getTextureBank();
	return tbank->findTextureRaw(tex_path, false);
}
KImage VideoBank::exportSpriteImage(const Path &sprite_path) {
	// 準備
	const Sprite *sp = getSpriteBank()->find(sprite_path, false);
	if (sp == NULL) {
		Log_errorf("Failed to exportSpriteImage: No sprite named '%s'", sprite_path.u8());
		return KImage();
	}
	const KMesh *mesh = &sp->mesh_;
	if (mesh == NULL) {
		Log_errorf("Failed to exportSpriteImage: No mesh in '%s'", sprite_path.u8());
		return KImage();
	}
	if (mesh->getVertexCount() == 0) {
		Log_errorf("Failed to exportSpriteImage: No vertices in '%s", sprite_path.u8());
		return KImage();
	}
	if (mesh->getSubMeshCount() == 0) {
		Log_errorf("Failed to exportSpriteImage: No submeshes in '%s", sprite_path.u8());
		return KImage();
	}
	TEXID tex = getTexture(sp->texture_name_);
	if (tex == NULL) {
		Log_errorf("Failed to exportSpriteImage: No texture named '%s'", sp->texture_name_.u8());
		return KImage();
	}
	// 実際のテクスチャ画像を取得。ブロック化されている場合はこの画像がパズル状態になっている
	KImage raw_img = Video::exportTextureImage(tex);
	if (raw_img.empty()) {
		Log_errorf("Failed to exportSpriteImage: Failed to Texture::exportImage() for texture '%s'", sp->texture_name_.u8());
		return KImage();
	}
	// 完成画像の保存先
	KImage image(sp->atlas_w_, sp->atlas_h_, KImage::RGBA32);
	// 元の画像を復元する
	_exportSpriteImageEx(image, raw_img, sp, mesh);
	// 終了
	return image;
}
TEXID VideoBank::getBakedSpriteTexture(const Path &sprite_path, int modifier, bool should_exist) {
	// 正しいテクスチャ名とパレット名の組み合わせを得る
	Path tex_path;
	SpriteBank *bank = getSpriteBank();
	const Sprite *sp = bank->find(sprite_path, should_exist);
	if (sp == NULL) return NULL;
	if (sp->using_packed_texture) {
		// スプライトには、ブロック化テクスチャが指定されている
		const Path &bctex_path = sp->texture_name_;

		// このスプライトの本来の画像を復元したものを保持するためのテクスチャ名
		tex_path = AssetPath::getTextureAssetPath(sprite_path);

	} else {
		// スプライトには通常テクスチャが割り当てられている
		tex_path = sp->texture_name_;
	}
	// 得られた組み合わせで作成されたテクスチャを得る
	return getTextureEx(tex_path, modifier, should_exist);
}
void VideoBank::_exportSpriteImageEx(KImage &dest_image, const KImage &src_image, const Sprite *sprite, const KMesh *mesh) {
	Log_assert(sprite);
	Log_assert(mesh);
	Log_assert(mesh->getSubMeshCount() > 0);
	Log_assert(mesh->getVertexCount() > 0);
	if (mesh->getIndexCount() > 0) {
		// インデックス配列を使って定義されている

		// 画像を復元する。この部分は CImgPackR::getMeshPositionArray によるメッシュ作成手順に依存していることに注意！
		KSubMesh sub;
		mesh->getSubMesh(0, &sub);
		Log_assert(sub.primitive == Primitive_TriangleList);
		Log_assert(mesh->getIndexCount() % 6 == 0); // 頂点数が6の倍数であること
		const int index_count = mesh->getIndexCount();
		const int *indices = mesh->getIndices();
		const Vertex *vertex = mesh->getVertices();
		const int src_w = src_image.getWidth();
		const int src_h = src_image.getHeight();
		for (int i=0; i<index_count; i+=6) {
			Log_assert(indices[i] == i);
			const Vec3 p0 = vertex[i + 0].pos; // 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になっている
			const Vec3 p1 = vertex[i + 1].pos; // |      |
			const Vec3 p2 = vertex[i + 2].pos; // |      |
			const Vec3 p3 = vertex[i + 3].pos; // 3------2
			// p0, p1, p2, p3 は座標軸に平行な矩形になっている
			Log_assert(p0.x == p3.x);
			Log_assert(p1.x == p2.x);
			Log_assert(p0.y == p1.y);
			Log_assert(p2.y == p3.y);
			int left   = (int)p0.x;
			int right  = (int)p2.x;
			int top    = (int)p0.y;
			int bottom = (int)p2.y;
			if (bottom < top) {
				// 左下を原点とした座標を使っている
				top    = sprite->image_h_ - top;
				bottom = sprite->image_h_ - bottom;
			}
			// 必ず一つ以上のピクセルを含んでいる。
			// 元画像が不透明ピクセルを一切含んでいない場合は、そもそもメッシュが生成されないはず
			Log_assert(left < right);
			Log_assert(top < bottom);
			if (! sprite->using_packed_texture) {
				// 元画像が中途半端なサイズだった場合、ブロックが元画像範囲からはみ出ている場合がある事に注意
				if (sprite->image_w_ < right ) right  = sprite->image_w_;
				if (sprite->image_h_ < bottom) bottom = sprite->image_h_;
			}
			const Vec2 t0 = vertex[i + 0].tex;
			const Vec2 t1 = vertex[i + 1].tex;
			const Vec2 t2 = vertex[i + 2].tex;
			const Vec2 t3 = vertex[i + 3].tex;
			const int cpy_w = right - left;
			const int cpy_h = bottom - top;
			const int src_x = (int)(src_w * t0.x);
			const int src_y = (int)(src_h * t0.y);
			dest_image.copyRect(left, top, src_image, src_x, src_y, cpy_w, cpy_h);
		}

	} else {
		// インデックス配列なしで定義されている

		// 画像を復元する。この部分は CImgPackR::getMeshPositionArray によるメッシュ作成手順に依存していることに注意！
		KSubMesh sub;
		mesh->getSubMesh(0, &sub);
		const int vertex_count = mesh->getVertexCount();
		if (sub.primitive == Primitive_TriangleList) {
			Log_assert(vertex_count % 6 == 0); // 頂点数が6の倍数であること
			const Vertex *vertex = mesh->getVertices();
			const int src_w = src_image.getWidth();
			const int src_h = src_image.getHeight();
			for (int i=0; i<vertex_count; i+=6) {
				const Vec3 p0 = vertex[i + 0].pos; // 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になっている
				const Vec3 p1 = vertex[i + 1].pos; // |      |
				const Vec3 p2 = vertex[i + 2].pos; // |      |
				const Vec3 p3 = vertex[i + 3].pos; // 3------2
				// p0, p1, p2, p3 は座標軸に平行な矩形になっている
				Log_assert(p0.x == p3.x);
				Log_assert(p1.x == p2.x);
				Log_assert(p0.y == p1.y);
				Log_assert(p2.y == p3.y);
				int left   = (int)p0.x;
				int right  = (int)p2.x;
				int top    = (int)p0.y;
				int bottom = (int)p2.y;
				if (bottom < top) {
					// 左下を原点とした座標を使っている
					top    = sprite->image_h_ - top;
					bottom = sprite->image_h_ - bottom;
				}
				// 必ず一つ以上のピクセルを含んでいる。
				// 元画像が不透明ピクセルを一切含んでいない場合は、そもそもメッシュが生成されないはず
				Log_assert(left < right);
				Log_assert(top < bottom);
				if (! sprite->using_packed_texture) {
					// 元画像が中途半端なサイズだった場合、ブロックが元画像範囲からはみ出ている場合がある事に注意
					if (sprite->image_w_ < right ) right  = sprite->image_w_;
					if (sprite->image_h_ < bottom) bottom = sprite->image_h_;
				}
				const Vec2 t0 = vertex[i + 0].tex;
				const Vec2 t1 = vertex[i + 1].tex;
				const Vec2 t2 = vertex[i + 2].tex;
				const Vec2 t3 = vertex[i + 3].tex;
				const int cpy_w = right - left;
				const int cpy_h = bottom - top;
				const int src_x = (int)(src_w * t0.x);
				const int src_y = (int)(src_h * t0.y);
				dest_image.copyRect(left, top, src_image, src_x, src_y, cpy_w, cpy_h);
			}
		} else if (sub.primitive == Primitive_TriangleStrip) {
			Log_assert(vertex_count == 4);
			const Vertex *vertex = mesh->getVertices();
			const int src_w = src_image.getWidth();
			const int src_h = src_image.getHeight();
			for (int i=0; i<vertex_count; i+=4) {
				const Vec3 p0 = vertex[i + 0].pos; // 0 ---- 1 左上からZ字型に 0, 1, 2, 3 になっている
				const Vec3 p1 = vertex[i + 1].pos; // |      |
				const Vec3 p2 = vertex[i + 2].pos; // |      |
				const Vec3 p3 = vertex[i + 3].pos; // 2------3
				// p0, p1, p2, p3 は座標軸に平行な矩形になっている
				Log_assert(p0.x == p2.x);
				Log_assert(p1.x == p3.x);
				Log_assert(p0.y == p1.y);
				Log_assert(p2.y == p3.y);
				int left   = (int)p0.x;
				int right  = (int)p3.x;
				int top    = (int)p0.y;
				int bottom = (int)p3.y;
				if (bottom < top) {
					// 左下を原点とした座標を使っている
					top    = sprite->image_h_ - top;
					bottom = sprite->image_h_ - bottom;
				}
				// 必ず一つ以上のピクセルを含んでいる。
				// 元画像が不透明ピクセルを一切含んでいない場合は、そもそもメッシュが生成されないはず
				Log_assert(left < right);
				Log_assert(top < bottom);
				if (! sprite->using_packed_texture) {
					// 元画像が中途半端なサイズだった場合、ブロックが元画像範囲からはみ出ている場合がある事に注意
					if (sprite->image_w_ < right ) right  = sprite->image_w_;
					if (sprite->image_h_ < bottom) bottom = sprite->image_h_;
				}
				const Vec2 t0 = vertex[i + 0].tex;
				const Vec2 t1 = vertex[i + 1].tex;
				const Vec2 t2 = vertex[i + 2].tex;
				const Vec2 t3 = vertex[i + 3].tex;
				const int cpy_w = right - left;
				const int cpy_h = bottom - top;
				const int src_x = (int)(src_w * t0.x);
				const int src_y = (int)(src_h * t0.y);
				dest_image.copyRect(left, top, src_image, src_x, src_y, cpy_w, cpy_h);
			}
		} else {
			Log_assert(0 && "Invalid primitive type");
		}
	}
}
SHADERID VideoBank::findShader(const Path &asset_path, bool should_exist) {
	return shader_bank_.findShader(asset_path, should_exist);
}
void VideoBank::updateInspector() {
#ifndef NO_IMGUI
	ImGui::PushID(ImGui_ID(0));
	TextureBank_onInspectorGui(this);
	ImGui::PopID();

	ImGui::PushID(ImGui_ID(1));
	sprite_bank_.onInspectorGui(this);
	ImGui::PopID();

	ImGui::PushID(ImGui_ID(2));
	ShaderBank_onInspectorGui();
	ImGui::PopID();
#endif // !NO_IMGUI
}

#pragma endregion // VideoBank


#pragma region MeshFile
void meshSaveToFile(const KMesh *mesh, FileOutput &file) {
	meshSaveToXml(mesh, file);
}
void meshSaveToFileName(const KMesh *mesh, const Path &filename) {
	FileOutput file;
	if (file.open(filename)) {
		mo::meshSaveToFile(mesh, file);
	} else {
		Log_error("meshSaveToFileName: file can not be opened");
	}
}
void meshSaveToXml(const KMesh *mesh, FileOutput &file) {
	std::string s; // utf8
	s += "<Mesh>\n";
	int num_vertices = mesh->getVertexCount();
	const Vertex *vertex = mesh->getVertices();
	// Position
	if (num_vertices > 0) {
		s += "\t<Pos3D>\n";
		for (int i=0; i<num_vertices; i++) {
			s += K_sprintf("\t\t%f %f %f\n", vertex[i].pos.x, vertex[i].pos.y, vertex[i].pos.z);
		}
		s += "\t</Pos3D>\n";
	}
	// UV
	if (num_vertices > 0) {
		s += "\t<UV>\n";
		for (int i=0; i<num_vertices; i++) {
			s += K_sprintf("\t\t%f %f\n", vertex[i].tex.x, vertex[i].tex.y);
		}
		s += "\t</UV>\n";
	}
	// Color
	if (num_vertices > 0) {
		s += "\t<Colors><!-- 0xAARRGGBB --> \n";
		for (int i=0; i<num_vertices; i++) {
			s += K_sprintf("\t\t0x%08X\n", vertex[i].dif.toUInt32());
		}
		s += "\t</Colors>\n";
	}
	// Submeshes
	if (num_vertices > 0) {
		Log_assert(mesh->getSubMeshCount() > 0);
		const int *indices = mesh->getIndices();
		for (int sidx=0; sidx<mesh->getSubMeshCount(); ++sidx) {
			KSubMesh sub;
			mesh->getSubMesh(sidx, &sub);
			int start = sub.start;
			int count = sub.count;
			switch (sub.primitive) {
			case Primitive_PointList:
				s += "\t<PointList>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</PointList>\n";
				break;
			case Primitive_LineList:
				s += "\t<LineList>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</LineList>\n";
				break;
			case Primitive_LineStrip:
				s += "\t<LineStrip>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</LineStrip>\n";
				break;
			case Primitive_TriangleList:
				s += "\t<TriangleList>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K_sprintf("%d", idx);
					if ((i+1) % 3 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</TriangleList>\n";
				break;
			case Primitive_TriangleStrip:
				s += "\t<TriangleStrip>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</TriangleStrip>\n";
				break;
			case Primitive_TriangleFan:
				s += "\t<TriangleFan>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</TriangleFan>\n";
				break;
			default:
				Log_error("NOT SUPPORTED");
				break;
			}
		}
	}
	s += "</Mesh>\n";
	
	// Total
	if (1) {
		s += "<!--\n";
		for (int i=0; i<num_vertices; i++) {
			const Vec3 &pos = vertex[i].pos;
			const Vec2 &tex = vertex[i].tex;
			s += K_sprintf("[%4d] xyz(%f %f %f) uv(%f %f)\n", i, pos.x, pos.y, pos.z, tex.x, tex.y);
		}
		s += "-->\n";
	}
	file.write(s);
}
#pragma endregion // MeshFile


} // namespace

