#pragma once
#include "KVideoDef.h"
#include <vector>

namespace mo {

struct KSubMesh {
	int start;           ///< 開始頂点インデックス
	int count;           ///< 頂点数
	Primitive primitive; ///< 頂点結合タイプ
	TEXID texture;
	Blend blend;

	KSubMesh() {
		start = 0;
		count = 0;
		primitive = Primitive_TriangleStrip;
		blend = Blend_One;
		texture = NULL;
	}
};

class KMesh {
public:
	KMesh();

	/// 頂点をすべて削除する
	void clear();

	/// サブメッシュを追加する
	void addSubMesh(const KSubMesh &sub);

	/// サブメッシュ数を返す
	int getSubMeshCount() const;

	/// サブメッシュを取得する
	/// 範囲外のインデックスを指定した場合は初期状態の KSubMesh を返す
	bool getSubMesh(int index, KSubMesh *sub) const;
	void setSubMesh(int index, const KSubMesh &sub);

	/// 現在の頂点配列によって定義される AABB を取得する。
	/// 取得に成功した場合は結果を min_point, max_point にセットして true を返す
	/// 頂点配列が存在しないかサイズが 0 の場合は false を返す
	bool getAabb(int offset, int count, Vec3 *minpoint, Vec3 *maxpoint) const;
	bool getAabb(Vec3 *minpoint, Vec3 *maxpoint) const;

	/// 頂点個数を返す
	int getVertexCount() const;

	/// インデックス個数を返す
	int getIndexCount() const;

	const Vertex * getVertices(int offset=0) const;
	const int * getIndices(int offset=0) const;

	/// 頂点配列をロックして、書き込み可能なポインタを返す
	Vertex * lock(int offset, int count);
	
	/// 頂点配列のロックを解除する
	void unlock();

	/// インデックス配列をロックして、書き込み可能なポインタを返す
	int * lockIndices(int offset, int count);
	
	/// インデックス配列のロックを解除する
	void unlockIndices();

protected:
	std::vector<int> indices_;
	std::vector<Vertex> vertices_;
	std::vector<KSubMesh> submeshes_;
};

namespace MeshShape {
	void makeRect(KMesh *mesh, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const Color &color);
	void makeRect(KMesh *mesh, int x0, int y0, int x1, int y1, float u0, float v0, float u1, float v1, const Color &color);
}

} // namespace
