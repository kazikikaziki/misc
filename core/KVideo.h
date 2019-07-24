#pragma once
#include "KImage.h"
#include "KPath.h"
#include "KVideoDef.h"
#include <inttypes.h>

#define MO_VIDEO_ENABLE_HALF_PIXEL  1

namespace mo {


class Graphics;
class KMesh;
struct Material;
struct RenderParams;
class RectF;
class Matrix4;
class Vec3;

struct BufDesc {
	int num_capacity;
	int num_used;
};

struct GraphicFlags {
	GraphicFlags() {
		max_texture_size_require = 2048;
		limit_texture_size = 0;
		use_square_texture_only = false;
		disable_pixel_shader = false;
	}
	/// 必要なテクスチャサイズ。
	/// この値は、ゲームエンジンの要求する最低スペック要件になる。
	/// このサイズ未満のテクスチャにしか対応しない環境で実行しようとした場合、正しい描画は保証されない
	int max_texture_size_require = 2048;

	/// テクスチャのサイズ制限
	/// 実際の環境とは無関係に適用する
	/// 0 を指定すると制限しない
	int limit_texture_size = 2048;

	/// 正方形のテクスチャしか作れないようにする。チープな環境をエミュレートするために使う
	bool use_square_texture_only = false;

	/// ピクセルシェーダーが使えないようにする。チープな環境をエミュレートするために使う
	bool disable_pixel_shader = false;
};

extern GraphicFlags g_graphic_flags;


/// ピクセルパーフェクトにするために座標を調整する
///
/// テクセルからピクセルへの直接的なマッピング (Direct3D 9)
/// "https://docs.microsoft.com/ja-jp/previous-versions/direct-x/ee417850(v=vs.85)"
static const float HALF_PIXEL = 0.5f;

void translateInPlace(Matrix4 &m, const Vec3 &delta);
void translateInPlace(Matrix4 &m, float x, float y, float z);
void translateHalfPixelInPlace(Matrix4 &m);

/// 変形行列の平行移動成分だけを整数化する
void translateFloorInPlace(Matrix4 &m);

#pragma region Graphics



class IVideoCallback {
public:
	virtual void on_video_device_lost() = 0;
	virtual void on_video_device_reset() = 0;
};

struct TexDesc {
	/// テクスチャのサイズ
	int w;
	int h;
	
	/// このテクスチャの元画像のサイズ
	/// 元画像のサイズが 2^n でなかった場合、テクスチャ化するときに右端と下端に余白が追加される場合がある。
	/// このメソッドは余白調整する前のビットマップ画像のサイズが必要な時に使う。
	int original_w;
	int original_h;

	/// このテクスチャ上で、元画像が占めている部分を uv 値で示したもの。
	/// w / original_w, h / original_h と同じ。
	float original_u;
	float original_v;

	/// テクスチャのピッチバイト数（ある行の先頭ピクセルから、その次の行の先頭ピクセルに移動するためのバイト数）
	int pitch;
	
	int size_in_bytes;
	
	/// このテクスチャがレンダーテクスチャかどうか
	bool is_render_target;

	/// RGBA そのままの順番なら true
	/// BGRA の順番なら false
	bool is_opengl_compatible_color_order;

	KImage::Format pixel_format;

	/// Direct3D を使用している場合、
	/// このテクスチャに関連付けられた IDirect3DTexture9 オブジェクト
	intptr_t d3dtex9;

	/// OpenGL を使用している場合、
	/// このテクスチャに関連付けられた Texture name (GLuint) オブジェクト
	intptr_t gltex;
};

struct BufferDesc {
	int count;
	int bytes;
};

struct ShaderDesc {
	int num_technique;
	char type[16];
	intptr_t d3dxeff;
};

/// @class
class Video {
public:
	enum ColorWriteMask_ {
		MASK_A = 1,
		MASK_R = 2,
		MASK_G = 4,
		MASK_B = 8,
		MASK_RGB  = MASK_R|MASK_G|MASK_B,
		MASK_RGBA = MASK_R|MASK_G|MASK_B|MASK_A,
	};
	typedef int ColorWriteMask;

	static bool init(void *hWnd, void *d3d9=NULL, void *d3ddev9=NULL);
	static void shutdown();
	static bool isInitialized();

	/// サイズを指定して、新しいテクスチャを作成する
	static TEXID createTexture2D(int w, int h);

	/// イメージを指定して、そのイメージを保持するテクスチャを作成する
	static TEXID createTexture2D(const KImage &image);

	/// レンダーテクスチャーを作成する
	static TEXID createRenderTexture(int w, int h);

	/// テクスチャーを削除する
	static void deleteTexture(TEXID tex);

	/// シェーダーを作成する
	static SHADERID createShaderFromHLSL(const char *code, const Path &name);
	static SHADERID createShaderFromGLSL(const char *vscode, const char *fscode, const Path &name);

	/// シェーダーを削除する
	static void deleteShader(SHADERID s);

	static void beginShader(SHADERID s, Material *material);
	static void endShader(SHADERID s, Material *material);
	static void setShaderInt(SHADERID s, const char *name, const int *values, int count);
	static void setShaderInt(SHADERID s, const char *name, int value);
	static void setShaderFloat(SHADERID s, const char *name, const float *values, int count);
	static void setShaderFloat(SHADERID s, const char *name, float value);
	static void setShaderTexture(SHADERID s, const char *name, TEXID tex);
	static bool getShaderDesc(SHADERID s, ShaderDesc *desc);

	/// 頂点配列
	static VERTEXBUFFERID createVertices();
	static void deleteVertices(VERTEXBUFFERID id);
	static void clearVertices(VERTEXBUFFERID id);
	static Vertex * lockVertices(VERTEXBUFFERID id, int offset, int count);
	static void unlockVertices(VERTEXBUFFERID id);
	static bool getVertexDesc(VERTEXBUFFERID id, BufDesc *desc);
	static bool getVertexAabb(VERTEXBUFFERID id, int offset, int count, Vec3 *mincoord, Vec3 *maxcoord);
	static void setVertices(VERTEXBUFFERID id);

	/// インデックス配列
	static INDEXBUFFERID createIndices();
	static void deleteIndices(INDEXBUFFERID id);
	static void clearIndices(INDEXBUFFERID id);
	static int * lockIndices(INDEXBUFFERID id, int offset, int count);
	static void unlockIndices(INDEXBUFFERID id);
	static bool getIndexDesc(INDEXBUFFERID id, BufDesc *desc);
	static void setIndices(INDEXBUFFERID id);


	/// グラフィックデバイスをリセットする
	///
	/// 解像度の変更、フルスクリーンモードの切り替え、デバイスロストからの復帰の必要があるときに使う
	/// w 新しいバックバッファ幅。今と同じでよい場合は 0
	/// h 新しいバックバッファの高さ。今と同じでよい場合は 0
	/// fullscreen 1=フルスクリーン 0=ウィンドウモード -1=今と同じ 
	/// リセット直前と復帰直後にコールバックを受け取りたい時は attachHandler() を設定する
	static bool resetDevice(int w=0, int h=0, int fullscreen=-1);

	/// 描画シーンを開始する
	/// 開始不可能な場合は false を返す（例えばデバイスがリセットを必要としているときなど）
	static bool beginScene();

	/// 描画シーンを終了し、フロントバッファに画像を転送する
	/// 転送に失敗した場合は false を返す
	static bool endScene();

	/// バックバッファを指定された色でクリアする
	static void clearColor(const Color &color);

	/// 深度バッファを指定された値でクリアする
	static void clearDepth(float z);

	/// ステンシルバッファを指定された値でクリアする
	static void clearStencil(int s);

	/// ビューポートを設定する
	/// x y にはウィンドウクライアント領域左上からの座標を指定する
	static void setViewport(int x, int y, int w, int h);

	/// 現在のビューポートを得る
	/// 不要なパラメータにはNULLを指定してもよい
	static void getViewport(int *x, int *y, int *w, int *h);

	/// 色の書き込みマスクを設定する
	static void setColorWriteMask(ColorWriteMask mask);

	/// テクスチャーに画像を転送する（現在のビューポートいっぱいに引き延ばすようにして描画する）
	/// @param target 転送先のテクスチャー。NULL だと画面に出力する
	/// @param src 入力テクスチャー
	/// @param mat 転送時に適用するマテリアル
	static void blit(TEXID dest, TEXID src, Material *mat);
	static void blitEx(TEXID dest, TEXID src, Material *mat, const RectF *src_rect, const RectF *dst_rect);

	/// 書き込み可能なテクスチャ target を color で塗りつぶす。
	/// mask には書き込む色成分のマスクを指定する。例えば MASK_R|MASK_G を指定した場合、R と G 成分だけ塗りつぶし、A と B 成分は変更しない
	/// mask に 0 が指定された場合は何も書き込まない
	static void fill(TEXID target, const Color &color, ColorWriteMask mask);

	/// 描画関連のイベントを受け取るレシーバーを登録する
	/// NULL が指定された場合は何もしない
	static void addCallback(IVideoCallback *h);

	/// 描画関連のイベントを受け取るレシーバーを削除する
	/// NULL が指定された場合は何もしない
	static void removeCallback(IVideoCallback *h);
	
	static void beginMaterial(const Material *material);

	static void endMaterial(const Material *material);

	static void draw(const Vertex *vertices, int count, Primitive primitive);
	static void drawIndex(const Vertex *vertices, int vertex_count, const int *indices, int index_count, Primitive primitive);

	static void pushRenderState();
	static void popRenderState();

	static void setProjection(const Matrix4 &m);

	static void pushTransform();
	static void setTransform(const Matrix4 &m);
	static void transform(const Matrix4 &m);
	static void popTransform();


	/// 指定されたメッシュを描画する
	/// mesh -- メッシュ
	/// submesh_index -- 描画するサブメッシュ番号
	/// flags -- 描画オプション
	static void drawMesh(const KMesh *mesh, int submesh_index);
	static void drawMesh(const KMesh *mesh, int start, int count, Primitive primitive);

	static void drawMeshId(int start, int count, Primitive primitive, int base_index_number);

	/// 新しいレンダーターゲットを設定し、内部スタックに積む
	/// 指定されたテクスチャがレンダーターゲットとして使えない場合でも必ずスタックに値を積むため、
	/// 成否に関係なく popRenderTarget() を呼ぶ必要がある
	static void pushRenderTarget(TEXID render_target);

	/// スタックに退避してあったレンダーターゲットを再設定し、内部スタックから取り除く
	static void popRenderTarget();

	/// 追加パラメータの取得
	enum Param {
		NONE,       ///< 何もしない
		HAS_SHADER, ///< プログラマブルシェーダーが利用可能か？ {int}
		HAS_HLSL,   ///< シェーダー言語としてHLSLが利用可能か？ {int}
		HAS_GLSL,   ///< シェーダー言語としてGLSLが利用可能か？ {int}
		IS_FULLSCREEN, ///< フルスクリーンかどうか？ {int} 1 or 0
		D3DDEV9,    ///< IDirect3DDevice9 オブジェクト {IDirect3DDevice9*}
		DEVICELOST, ///< デバイスロストしているか？ {int}
		VS_VER,     ///< 頂点シェーダーのバージョン番号 {int major, int minor}
		PS_VER,     ///< ピクセルシェーダーのバージョン番号　{int major, int minor}
		MAXTEXSIZE, ///< 作成可能な最大テクスチャサイズ {int w, int h}
		DRAWCALLS,  ///< 描画関数の累積呼び出し回数。この値を取得すると内部カウンタがリセットされる。{int 回数}
	};

	static void getParameter(Param name, void *data);

	/// 細かな設定
	static void command(const char *s);

	static bool getTextureSize(TEXID tex, int *w, int *h);
	static bool getTextureDesc(TEXID tex, TexDesc *desc);

	/// 全ピクセルの値を 0 にする
	static void fillTextureZero(TEXID tex);

	/// 書き込み可能テクスチャをロックし、書き込み用ポインタを返す
	static void * lockTexture(TEXID tex);

	/// 書き込み用ロックを解除する
	static void unlockTexture(TEXID tex);

	/// テクスチャの内容をコピーした新しい KImage を返す。
	/// 使い終わった KImage は drop すること
	static KImage exportTextureImage(TEXID tex, int channel=-1);

	/// 画像をテクスチャに書き込む（テクスチャのサイズは書き込み後も変わらない）
	/// UV 範囲に 0 が指定された場合は、コピー元、コピー先のサイズに関係なく、ピクセルが1対1で対応するように拡縮なしでコピーする
	/// UV 範囲に非0が指定された場合は、要求された UV 範囲に合うように拡縮して画像をコピーする
	static void writeImageToTexture(TEXID tex, const KImage &image, float u=0, float v=0);

	static Vec2 getTextureUVFromOriginalPoint(TEXID tex, const Vec2 &pixel);
	static Vec2 getTextureUVFromOriginalUV(TEXID tex, const Vec2 &orig_uv);
};

void mo__Texture_setPixel(TEXID tex, int x, int y, const Color &color);
Color mo__Texture_getPixel(TEXID tex, int x, int y);
void mo__FancyWriteImage(TEXID tex, const KImage &image);

/// mo__Texture_setPixel の高速版。すでにロック済みであると仮定する。
/// また、詳細なエラーチェックを行わない（デバッグモード時にはアサーションによるチェックだけ行うがリリースモードではノーチェックになる）
/// tex テクスチャ
/// locked_buf tex によってロック済みの書き込みバッファ
/// x, y 書き込み座標
/// color 書き込み色
void  mo__Texture_setPixelFast(TEXID tex, void *locked_buf, int x, int y, const Color &color);
Color mo__Texture_getPixelFast(TEXID tex, const void *locked_buf, int x, int y);



}

