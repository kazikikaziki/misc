#include "KMesh.h"
#include "KStd.h"

namespace mo {

static Vec3 vec3_min(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		(a.x < b.x) ? a.x : b.x,
		(a.y < b.y) ? a.y : b.y,
		(a.z < b.z) ? a.z : b.z
	);
}

static Vec3 vec3_max(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		(a.x > b.x) ? a.x : b.x,
		(a.y > b.y) ? a.y : b.y,
		(a.z > b.z) ? a.z : b.z
	);
}

#pragma region KMesh
KMesh::KMesh() {
}
bool KMesh::getAabb(Vec3 *minpoint, Vec3 *maxpoint) const {
	return getAabb(0, getVertexCount(), minpoint, maxpoint);
}
bool KMesh::getAabb(int offset, int count, Vec3 *minpoint, Vec3 *maxpoint) const {
	Vec3 minp, maxp;
	if (offset < 0) return false;
	if (offset + count > (int)vertices_.size()) return false;
	if (count > 0) {
		minp = vertices_[offset].pos;
		maxp = vertices_[offset].pos;
		for (int i=offset+1; i<offset+count; i++) {
			const Vec3 &p = vertices_[i].pos;
			minp = vec3_min(minp, p);
			maxp = vec3_max(maxp, p);
		}
	}
	if (minpoint) *minpoint = minp;
	if (maxpoint) *maxpoint = maxp;
	return true;
}
void KMesh::clear() {
	indices_.clear();
	vertices_.clear();
	submeshes_.clear();
}
void KMesh::addSubMesh(const KSubMesh &sub) {
	K_assert(sub.start >= 0);
	K_assert(sub.count >= 0);
	submeshes_.push_back(sub);
}
int KMesh::getSubMeshCount() const {
	return (int)submeshes_.size();
}
bool KMesh::getSubMesh(int index, KSubMesh *sub) const {
	if (0 <= index && index < (int)submeshes_.size()) {
		if (sub) *sub = submeshes_[index];
		return true;
	}
	return false;
}
void KMesh::setSubMesh(int index, const KSubMesh &sub) {
	if (0 <= index && index < (int)submeshes_.size()) {
		submeshes_[index] = sub;
	}
}
const Vertex * KMesh::getVertices(int offset) const {
	K_assert(0 <= offset && offset < (int)vertices_.size());
	return vertices_.data() + offset;
}
const int * KMesh::getIndices(int offset) const {
	K_assert(0 <= offset && offset < (int)indices_.size());
	return indices_.data() + offset;
}
int KMesh::getVertexCount() const {
	return vertices_.size();
}
int KMesh::getIndexCount() const {
	return indices_.size();
}
Vertex * KMesh::lock(int offset, int count) {
	if (offset >= 0 && count > 0) {
		vertices_.resize(offset + count);
		return &vertices_[offset];
	}
	return NULL;
}
void KMesh::unlock() {
}
int * KMesh::lockIndices(int offset, int count) {
	if (offset >= 0 && count > 0) {
		indices_.resize(offset + count);
		return &indices_[offset];
	}
	return NULL;
}
void KMesh::unlockIndices() {
}
#pragma endregion // KMesh



#pragma region MeshShape
void MeshShape::makeRect(KMesh *mesh, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const Color &color) {
	mesh->clear();
	Vertex *v = mesh->lock(0, 4);
	if (v) {
		v[0].pos = Vec3(x0, y1, 0.0f);
		v[1].pos = Vec3(x1, y1, 0.0f);
		v[2].pos = Vec3(x0, y0, 0.0f);
		v[3].pos = Vec3(x1, y0, 0.0f);

		v[0].tex = Vec2(u0, v0);
		v[1].tex = Vec2(u1, v0);
		v[2].tex = Vec2(u0, v1);
		v[3].tex = Vec2(u1, v1);

		for (int i = 0; i < 4; i++) {
			v[i].dif = color;
		}
		mesh->unlock();
	}
	{
		KSubMesh sub;
		sub.start = 0;
		sub.count = 4;
		sub.primitive = Primitive_TriangleStrip;
		mesh->addSubMesh(sub);
	}
}
void MeshShape::makeRect(KMesh *mesh, int x0, int y0, int x1, int y1, float u0, float v0, float u1, float v1, const Color &color) {
	makeRect(mesh, (float)x0, (float)y0, (float)x1, (float)y1, u0, v0, u1, v1, color);
}


#pragma endregion // MeshShape

} // namespace
