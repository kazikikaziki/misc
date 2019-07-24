#pragma once
#include "KMesh.h"
#include "KMatrix4.h"
#include "KVideoDef.h"
#include <vector>

namespace mo {

static const Color Gizmo_DefaultLineColor(0, 1, 0, 1);

class Engine;

class Drawer {
public:
	void clear();
	void pushMatrix(const Matrix4 &m);
	void popMatrix();
	void pushColor(const Color &c);
	void popColor();
	void addShape(Primitive prim, const Vec3 *points, int count);
	void render(const Matrix4 &projection);
//private:
	struct Elm {
		Matrix4 transform;
		KSubMesh submesh;
	};
	Material material_;
	KMesh mesh_;
	std::vector<Vec3> points_;
	std::vector<Color> colors_;
	std::vector<Elm> elms_;
	std::vector<Matrix4> matrix_stack_;
	std::vector<Color> color_stack_;
};

class Gizmo {
public:
	Gizmo();
	void initResources(Engine *engine);
	void shutdownResources();
	void addLine(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color=Gizmo_DefaultLineColor);
	void addLine(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color, const Color &color2);
	void addLineDash(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color=Gizmo_DefaultLineColor);
	void addLine2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color=Gizmo_DefaultLineColor);
	void addAabb(const Matrix4 &matrix, const Vec3 &a, const Vec3 &b, const Color &color=Gizmo_DefaultLineColor);
	void addAabbFrame2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color=Gizmo_DefaultLineColor);
	void addAabbFill2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color=Gizmo_DefaultLineColor);
	void addAabbEllipse2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, const Color &color=Gizmo_DefaultLineColor);
	void addAxis(const Matrix4 &matrix, const Vec3 &pos, float size);
	void addMesh(const Matrix4 &matrix, KMesh *mesh, const Color &color=Gizmo_DefaultLineColor);
	void addCircle(const Matrix4 &matrix, const Vec3 &p, float r, int axis, const Color &color=Gizmo_DefaultLineColor);
	void addArrow(const Matrix4 &matrix, const Vec3 &from, const Vec3 &to, float head, const Color &color=Gizmo_DefaultLineColor);
	void newFrame();
	void render(const Matrix4 &projection);
private:
	struct _Vert {
		Vec3 pos;
		uint32_t col;
		Vec2 tex;
	};
	struct LineCmd {
		Matrix4 matrix;
		Vec3 a, b;
		Color color;
		Color color2;
		int timer;
	};
	struct RectCmd {
		Matrix4 matrix;
		Vec3 a, b;
		Color color;
		int timer;
	};
	struct MeshCmd {
		Matrix4 matrix;
		KMesh *mesh;
		Color color;
		int timer;
	};
	void _addAabb2D(const Matrix4 &matrix, float x0, float y0, float x1, float y1, float z, const Color &color);
	std::vector<_Vert> vertices_;
	std::vector<LineCmd> line_cmds_;
	std::vector<RectCmd> rect_cmds_;
	std::vector<MeshCmd> mesh_cmds_;
	std::vector<Vec3> tmp_vec3_;
	std::vector<Vec2> tmp_vec2_;
	std::vector<Color> tmp_colors;
	std::vector<int> tmp_ints_;
	Engine *engine_;
	int material_max_index_;

	Drawer drawer_;
};


} // namespace
