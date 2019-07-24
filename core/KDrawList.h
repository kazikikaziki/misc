#pragma once
#include "KVideoDef.h"
#include "KMesh.h"
#include "KMatrix4.h"

namespace mo {

class Matrix4;

struct DrawListItem {
	Material material;
	Matrix4 projection;
	Matrix4 transform;
	Primitive prim;
	int start, count, index_number_offset;
	bool with_index;

	// ユーザーが自分で描画する時に使う情報。
	// マテリアルのコールバックなしで描画するなら、個々の情報は使わない
	const KMesh *cb_mesh;
	int cb_submesh;

	DrawListItem() {
		index_number_offset = 0;
		start = count = 0;
		prim = (Primitive)Primitive_EnumMax; // 無効な値を入れておく
		with_index = false;
		cb_mesh = NULL;
		cb_submesh = 0;
	}
};

class DrawList {
public:
	DrawList();
	void clear();
	void setMaterial(const Material &m);
	void setTransform(const Matrix4 &m);
	void setProjection(const Matrix4 &m);
	void setPrimitive(Primitive prim);
	void addPrimitive(const Vertex *v, int count);
	void addPrimitive(const Vertex *v, int vcount, const int *i, int icount);
	void addMesh(const KMesh *mesh, int submesh);
	void endlist();
	void draw();
	size_t size() const;

private:
	bool ischanged();
	VERTEXBUFFERID uni_vb_;
	INDEXBUFFERID uni_ib_;
	KMesh unimesh_;
	DrawListItem next_;
	DrawListItem last_;
	bool changed_;
	std::vector<DrawListItem> items_;
};

} // namespace
