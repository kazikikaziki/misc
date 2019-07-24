#include "GameGizmo.h"
//
#include "KCollisionMath.h"
#include "KLog.h"
#include "KNum.h"
#include "KQuat.h"
#include "KVideo.h"

namespace mo {

#pragma region Gizmo
Gizmo::Gizmo() {
	engine_ = NULL;
	material_max_index_ = -1;
}
void Gizmo::_addAabb2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, float z, const Color &color) {
	addLine(matrix, Vec3(x0, y0, z), Vec3(x1, y0, z), color);
	addLine(matrix, Vec3(x1, y0, z), Vec3(x1, y1, z), color);
	addLine(matrix, Vec3(x1, y1, z), Vec3(x0, y1, z), color);
	addLine(matrix, Vec3(x0, y1, z), Vec3(x0, y0, z), color);
}
void Gizmo::addLineDash(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color) {
	const int span = 8; // 線分同士の距離（線分の先頭から、次の線分の先頭まで）
	const int seg  = 4; // 線分の長さ
	float len = (a - b).getLength();
	if (len < seg) return;
	int num = (int)len / span;
	for (int i=0; i<num; i++) {
		Vec3 p0 = a + (b - a) * (float)(span*i    ) / len;
		Vec3 p1 = a + (b - a) * (float)(span*i+seg) / len;
		addLine(matrix, p0, p1, color);
	}
	{
		Vec3 p0 = a + (b - a) * (float)(span * num) / len;
		Vec3 p1 = b;
		addLine(matrix, p0, p1, color);
	}
}
void Gizmo::addArrow(const Matrix4 &matrix, const Vec3 &from, const Vec3 &to, float head, const Color &color) {
	Vec3 p1(head,  head, 0.0f);
	Vec3 p2(head, -head, 0.0f);
	//
	Vec3 arrow_axis = to - from;
	Vec3 a = arrow_axis.getNormalized();
	Vec3 b(1, 0, 0);
	Vec3 o(0, 0, 0);
	Vec3 rot_axis;
	if (KCollisionMath::getTriangleNormal(&rot_axis, a, b, o)) {
		float angle = acosf(a.dot(b));
		Quat q(rot_axis, Num_degrees(angle));
		addLine(matrix, from, to, color, color);
		addLine(matrix, to, to + q.rot(p1), color, color);
		addLine(matrix, to, to + q.rot(p2), color, color);
	}
}
void Gizmo::addLine(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color) {
	const Vec3 p[] = {a, b};
	drawer_.pushMatrix(matrix);
	drawer_.pushColor(color);
	drawer_.addShape(Primitive_LineList, p, 2);
	drawer_.popColor();
	drawer_.popMatrix();
}
void Gizmo::addLine(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color, const Color &color2) {
	const Vec3 p[] = {a, b};
	drawer_.pushMatrix(matrix);
	drawer_.pushColor(color);
	drawer_.addShape(Primitive_LineList, p, 2);
	drawer_.popColor();
	drawer_.popMatrix();
}
void Gizmo::addAabbFill2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color) {
	const Vec3 p[] = {
		Vec3(x0, y0, 0.0f),
		Vec3(x0, y1, 0.0f),
		Vec3(x1, y1, 0.0f),
		Vec3(x1, y0, 0.0f),
		Vec3(x0, y0, 0.0f),
		Vec3(x1, y1, 0.0f),
	};
	drawer_.pushMatrix(matrix);
	drawer_.pushColor(color);
	drawer_.addShape(Primitive_TriangleList, p, 6);
	drawer_.popColor();
	drawer_.popMatrix();
}
void Gizmo::addLine2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color) {
	addLine(matrix, Vec3(x0, y0, 0.0f), Vec3(x1, y1, 0.0f), color);
}
void Gizmo::addCircle(const Matrix4 &matrix, const Vec3 &p, float r, int axis_index, const Color &color) {
	int n = 16;
	Vec3 a, b;
	switch (axis_index) {
	case 0:
		// X axis
		break;
	case 1:
		// Y axis
		a = p + Vec3(r, 0.0f, 0.0f);
		for (int i=1; i<=n; i++) {
			float t = Num_PI * 2 * i / n;
			b = p + Vec3(r * cosf(t), 0.0f, r * sinf(t));
			addLine(matrix, a, b, color, color);
			a = b;
		}
		break;
	case 2:
		// Z axis
		a = p + Vec3(r, 0.0f, 0.0f);
		for (int i=1; i<=n; i++) {
			float t = Num_PI * 2 * i / n;
			b = p + Vec3(r * cosf(t), r * sinf(t), 0.0f);
			addLine(matrix, a, b, color, color);
			a = b;
		}
		break;
	}
}
void Gizmo::addAabbEllipse2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color) {
	const int N = 32;
	const float rx = (x1 - x0) / 2;
	const float ry = (y1 - y0) / 2;
	const float ox = (x1 + x0) / 2;
	const float oy = (y1 + y0) / 2;
	Vec3 p0(x1, oy, 0.0f);
	for (int i=1; i<=N; i++) {
		float t = Num_PI * 2 * i / N;
		Vec3 p1(ox + rx * cosf(t), oy + ry * sinf(t), 0.0f);
		addLine(matrix, p0, p1, color, color);
		p0 = p1;
	}
}
void Gizmo::addAabbFrame2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color) {
	_addAabb2D(matrix, x0, y0, x1, y1, 0.0f, color);
}
void Gizmo::addAabb(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color) {
	Color color2(color.r*0.7f, color.g*0.7f, color.b*0.7f, color.a);
	if (1) {
		// 手前面
		addLine(matrix, Vec3(a.x, a.y, a.z), Vec3(b.x, a.y, a.z), color, color2);
		addLine(matrix, Vec3(b.x, a.y, a.z), Vec3(b.x, b.y, a.z), color, color2);
		addLine(matrix, Vec3(b.x, b.y, a.z), Vec3(a.x, b.y, a.z), color, color2);
		addLine(matrix, Vec3(a.x, b.y, a.z), Vec3(a.x, a.y, a.z), color, color2);

		// 奥面
		addLineDash(matrix, Vec3(a.x, a.y, b.z), Vec3(b.x, a.y, b.z), color);
		addLine(matrix, Vec3(b.x, a.y, b.z), Vec3(b.x, b.y, b.z), color, color2);
		addLine(matrix, Vec3(b.x, b.y, b.z), Vec3(a.x, b.y, b.z), color, color2);
		addLineDash(matrix, Vec3(a.x, b.y, b.z), Vec3(a.x, a.y, b.z), color);

		// Z方向ライン
		addLine(matrix, Vec3(a.x, a.y, a.z), Vec3(a.x, a.y, b.z), color, color2);
		addLine(matrix, Vec3(b.x, a.y, a.z), Vec3(b.x, a.y, b.z), color, color2);
		addLine(matrix, Vec3(b.x, b.y, a.z), Vec3(b.x, b.y, b.z), color, color2);
		addLineDash(matrix, Vec3(a.x, b.y, a.z), Vec3(a.x, b.y, b.z), color);
	} else {
		// XY平面 AABB2D
		_addAabb2D(matrix, a.x, a.y, b.x, b.y, a.z, color);
		_addAabb2D(matrix, a.x, a.y, b.x, b.y, b.z, color2);
		// Z方向ライン
		addLine(matrix, Vec3(a.x, a.y, a.z), Vec3(a.x, a.y, b.z), color, color2);
		addLine(matrix, Vec3(b.x, a.y, a.z), Vec3(b.x, a.y, b.z), color, color2);
		addLine(matrix, Vec3(b.x, b.y, a.z), Vec3(b.x, b.y, b.z), color, color2);
		addLine(matrix, Vec3(a.x, b.y, a.z), Vec3(a.x, b.y, b.z), color, color2);
	}
}
void Gizmo::addAxis(const Matrix4 &matrix, const Vec3 &pos, float size) {
	if (1) {
		const Vec3 xarm(size, 0.0f, 0.0f);
		const Vec3 yarm(0.0f, size, 0.0f);
		const Vec3 zarm(0.0f, 0.0f, size);
		addLine(matrix, pos - xarm, pos + xarm, Color(1, 0, 0, 1), Color(1, 0, 0, 1)*1.0f);
		addLine(matrix, pos - yarm, pos + yarm, Color(0, 1, 0, 1), Color(0, 1, 0, 1)*1.0f);
		addLine(matrix, pos - zarm, pos + zarm, Color(0, 0, 1, 1), Color(0, 0, 1, 1)*0.5f);
	} else {
		float head = 8;
		addArrow(matrix, Vec3(-size, 0.0f, 0.0f), Vec3(size, 0.0f, 0.0f), head, Color(1, 0, 0, 1));
		addArrow(matrix, Vec3(0.0f, -size, 0.0f), Vec3(0.0f, size, 0.0f), head, Color(0, 1, 0, 1));
		addArrow(matrix, Vec3(0.0f, 0.0f, -size), Vec3(0.0f, 0.0f, size), head, Color(0, 0, 1, 1));
	}
}
void Gizmo::addMesh(const Matrix4 &matrix, KMesh *mesh, const Color &color) {
	if (mesh == NULL) return;
	MeshCmd meshcmd = {matrix, mesh, color, 1};
	mesh_cmds_.push_back(meshcmd);
}
void Gizmo::newFrame() {
	drawer_.clear();
}
void Gizmo::initResources(Engine *engine) {
	engine_ = engine;
}
void Gizmo::shutdownResources() {
}
void Gizmo::render(const Matrix4 &projection) {
	drawer_.render(projection);
}
#pragma endregion // Gizmo


#pragma region Drawer
void Drawer::clear() {
	matrix_stack_.clear();
	matrix_stack_.push_back(Matrix4());
	color_stack_.clear();
	color_stack_.push_back(Color::WHITE);
	points_.clear();
	colors_.clear();
	elms_.clear();
}
void Drawer::pushMatrix(const Matrix4 &m) {
	matrix_stack_.push_back(m);
}
void Drawer::popMatrix() {
	matrix_stack_.pop_back();
}
void Drawer::pushColor(const Color &c) {
	color_stack_.push_back(c);
}
void Drawer::popColor() {
	color_stack_.pop_back();
}
static int _CompareMatrix(const Matrix4 &m1, const Matrix4 &m2) {
	return memcmp(&m1, &m2, sizeof(Matrix4));
}
void Drawer::addShape(Primitive prim, const Vec3 *points, int count) {
	Log_assert(colors_.size() == points_.size());
	Log_assert(!color_stack_.empty());
	int start = points_.size();
	int newsize = start + count;
	points_.resize(newsize);
	colors_.resize(newsize);
	{
		Vec3 *dest = &points_[start];
		memcpy(dest, points, sizeof(Vec3) * count);
	}
	{
		const Color &color = color_stack_.back();
		Color *dest = &colors_[start];
		for (int i=0; i<count; i++) {
			dest[i] = color;
		}
	}
	Elm elm;
	elm.transform = matrix_stack_.back();
	elm.submesh.primitive = prim;
	elm.submesh.start = start;
	elm.submesh.count = count;

	bool append_new = true;
	if (!elms_.empty()) {
		Elm &last_elm = elms_.back();
		if (elm.submesh.primitive == last_elm.submesh.primitive) {
			if (_CompareMatrix(elm.transform, last_elm.transform) == 0) {
				// 直前の要素を拡張する
				last_elm.submesh.count += elm.submesh.count;
				append_new = false;
			}
		}
	}

	if (append_new) {
		elms_.push_back(elm);
	}
}
void Drawer::render(const Matrix4 &projection) {
	if (points_.empty()) return;
	mesh_.clear();
	Vertex *v = mesh_.lock(0, points_.size());
	if (v) {
		for (size_t i=0; i<points_.size(); i++) {
			v[i].pos = points_[i];
			v[i].tex = Vec2();
			v[i].dif = colors_[i];
		}
		mesh_.unlock();
	}
	for (size_t i=0; i<elms_.size(); i++) {
		const Elm &elm = elms_[i];
		mesh_.addSubMesh(elm.submesh);
	}
	for (size_t i=0; i<elms_.size(); i++) {
		const Elm &elm = elms_[i];
		Video::setProjection(projection);
		Video::setTransform(elm.transform);
		Video::beginMaterial(&material_);
		Video::drawMesh(&mesh_, i);
		Video::endMaterial(&material_);
	}
}
#pragma endregion // Drawer

} // namespace
