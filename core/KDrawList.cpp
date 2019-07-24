#include "KDrawList.h"
#include "KVideo.h"

// 同一ステートをまとめて描画する
#define UNIDRAW_TEST  1

// プリミティブを結合しない（テスト用）
#define NO_UNITE 0

namespace mo {

DrawList::DrawList() {
	uni_vb_ = NULL;
	uni_ib_ = NULL;
	clear();
}
bool DrawList::ischanged() {
#if NO_UNITE
	return true;
#else
	// ユーザー定義描画がある場合は、常に変更済みとみなす
	if (next_.material.cb) return true;

	// プリミティブが List 系の場合のみ結合描画できる
	bool pr = (next_.prim == Primitive_LineList || next_.prim == Primitive_TriangleList);
	if (!pr) return true;

	// マテリアル
	const float col_tolerance = 4.0f / 255;
	if (last_.material.color != next_.material.color) {
		if (! last_.material.color.equals(next_.material.color, col_tolerance)) {
			return true;
		}
	}
	if (last_.material.texture != next_.material.texture) return true;
	if (last_.material.blend   != next_.material.blend  ) return true;
	if (last_.material.filter  != next_.material.filter ) return true;

	// プリミティブ
	if (last_.prim != next_.prim) return true;

	const float mat_tolerance = 0.01f;

	// 行列
	if (last_.transform != next_.transform) {
		if (!last_.transform.equals(next_.transform, mat_tolerance)) {
			return true;
		}
	}
	if (last_.projection != next_.projection) {
		if (!last_.projection.equals(next_.projection, mat_tolerance)) {
			return true;
		}
	}
	// 変更なし
	return false;
#endif
}
void DrawList::clear() {
	unimesh_.clear();
	next_ = DrawListItem();
	last_ = DrawListItem();
	items_.clear();
	changed_ = false;
}
void DrawList::setMaterial(const Material &m) {
	next_.material = m;
}
void DrawList::setTransform(const Matrix4 &m) {
	next_.transform = m;
}
void DrawList::setProjection(const Matrix4 &m) {
	next_.projection = m;
}
void DrawList::setPrimitive(Primitive prim) {
	next_.prim = prim;
}
void DrawList::addMesh(const KMesh *mesh, int submesh) {
	KSubMesh sub;
	if (mesh && mesh->getSubMesh(submesh, &sub)) {
		// setPrimitive() を呼んだ時点でリストが登録されてしまうため、
		// メッシュパラメータは setPrimitive() よりも先に設定しておく
		next_.cb_mesh = mesh;
		next_.cb_submesh = submesh;

		setPrimitive(sub.primitive);
		if (mesh->getIndexCount() > 0) {
			addPrimitive(mesh->getVertices(), mesh->getVertexCount(), mesh->getIndices()+sub.start, sub.count);
		} else {
			addPrimitive(mesh->getVertices()+sub.start, sub.count);
		}
	}
}
void DrawList::addPrimitive(const Vertex *v, int count) {
#if UNIDRAW_TEST
	if (v && count > 0) {
		int numv = unimesh_.getVertexCount();

		if (last_.with_index==true || ischanged()) {
			// 前回 addPrimitive を呼んだ時とは描画設定が変わっている。
			// 前回 addPrimitive したときの描画アイテムを確定させ、新しい描画単位として登録する
			if (last_.count > 0) {
				items_.push_back(last_);
			}
			next_.index_number_offset = 0; // 使わないが念のため初期化
			next_.start = numv;
			next_.count = 0;
			next_.with_index = false;
		}
		{
			Vertex *buf = unimesh_.lock(numv, count);
			memcpy(buf, v, sizeof(Vertex) * count);
			unimesh_.unlock();
		}
		next_.count += count;
		last_ = next_; // 今回の addPrimitive で更新された描画アイテム
	}
#else
	Video::pushRenderState();
	Video::setProjection(curr_.projection);
	Video::setTransform(curr_.transform);
	Video::beginMaterial(&curr_.material);
	Video::draw(v, count, curr_.prim);
	Video::endMaterial(&curr_.material);
	Video::popRenderState();
#endif
}
void DrawList::addPrimitive(const Vertex *v, int vcount, const int *i, int icount) {
#if UNIDRAW_TEST
	if (v && vcount>0 && i && icount>0) {
		int numv = unimesh_.getVertexCount();
		int numi = unimesh_.getIndexCount();
		
		// インデックス付きの描画は結合できない（インデックスをずらさないといけないので）
		// 常に独立して描画する
		if (1/*last_.with_index==false || ischanged()*/) {
			// 前回 addPrimitive を呼んだ時とは描画設定が変わっている。
			// 既存の描画をコミットし、新しい描画単位として登録する
			if (last_.count > 0) {
				items_.push_back(last_);
			}
			next_.index_number_offset = numv;
			next_.start = numi;
			next_.count = 0;
			next_.with_index = true;
		}
		{
			Vertex *vbuf = unimesh_.lock(numv, vcount);
			memcpy(vbuf, v, sizeof(Vertex) * vcount);
			unimesh_.unlock();
		}
		{
			int *ibuf = unimesh_.lockIndices(numi, icount);
			memcpy(ibuf, i, sizeof(int) * icount);
			unimesh_.unlockIndices();
		}
		next_.count += icount;
		last_ = next_; // 今回の addPrimitive で更新された描画アイテム
	}
#else
	Video::pushRenderState();
	Video::setProjection(curr_.projection);
	Video::setTransform(curr_.transform);
	Video::beginMaterial(&curr_.material);
	Video::drawIndex(v, vcount, i, icount, curr_.prim);
	Video::endMaterial(&curr_.material);
	Video::popRenderState();
#endif
}
void DrawList::endlist() {
#if UNIDRAW_TEST
	if (last_.count > 0) {
		items_.push_back(last_);
	}
	next_ = DrawListItem();
	last_ = DrawListItem();
#endif
}
size_t DrawList::size() const {
	return items_.size();
}
void DrawList::draw() {
#if UNIDRAW_TEST
	if (uni_vb_ == NULL) {
		uni_vb_ = Video::createVertices();
	}
	if (uni_ib_ == NULL) {
		uni_ib_ = Video::createIndices();
	}
	// メッシュの内容を転送
	Video::clearVertices(uni_vb_);
	Video::clearIndices(uni_ib_);
	int vnum = unimesh_.getVertexCount();
	int inum = unimesh_.getIndexCount();
	if (vnum > 0) {
		const Vertex *vsrc = unimesh_.getVertices();
		Vertex *vdst = Video::lockVertices(uni_vb_, 0, vnum);
		memcpy(vdst, vsrc, sizeof(Vertex) * vnum);
		Video::unlockVertices(uni_vb_);
	}
	if (inum > 0) {
		const int *isrc = unimesh_.getIndices();
		int *idst = Video::lockIndices(uni_ib_, 0, inum);
		memcpy(idst, isrc, sizeof(int) * inum);
		Video::unlockIndices(uni_ib_);
	}
	Video::pushRenderState();
	Video::setVertices(uni_vb_);
	for (size_t i=0; i<items_.size(); i++) {
		const DrawListItem &item = items_[i];
		Video::setIndices(item.with_index ? uni_ib_ : NULL);
		Video::setVertices(uni_vb_); // 再設定する
		bool done = false;
		if (item.material.cb) {
			item.material.cb->onMaterial_Draw(&item, &done);
		}
		if (!done) { // ユーザーによって描画が完了していないなら、続ける
			Video::setProjection(item.projection);
			Video::setTransform(item.transform);
			Video::beginMaterial(&item.material);
			Video::drawMeshId(item.start, item.count, item.prim, item.index_number_offset);
			Video::endMaterial(&item.material);
		}
	}
	Video::popRenderState();
#endif
}


} // namespace
