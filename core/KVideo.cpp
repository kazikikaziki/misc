#include "KVideo.h"
//
#include "__Config.h" // MO_STRICT_CHECK
#include "KClock.h"
#include "KDxGetErrorString.h"
#include "KFile.h"
#include "KLog.h"
#include "KMatrix4.h"
#include "KMesh.h"
#include "KNum.h"
#include "KPlatform.h"
#include "KRect.h"
#include "KStd.h"
#include "mo_cstr.h"
#include <unordered_map>
#include <mutex>

// MainTexture, Diffuse, Specular をデフォルトマテリアルパラメータとして扱う
#define DEF_MATPARAM_TEST 1

#define BLENDOP_TEST 1

#ifdef _WIN32
#include <string>
#include <Shlwapi.h>
#include <Windows.h>

#ifdef _WIN32
#	include <d3d9.h>
#	include <d3dx9.h>
#endif


#define VERBOSE 0

#define DX9_REL(x) if (x) { (x)->Release(); (x) = NULL; }

#endif // _WIN32


/// Vertex に対応するFVF
/// @see struct Vertex
#define DX9_FVF_VERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)


/// 定義済みシェーダーパラメータ名
/// この名前のパラメータは、パラメータ名とその型、役割があらかじめ決まっている。
/// （アンダースコアで始まる or 終わる名前は GLSL でコンパイルエラーになることに注意）
#define SHADERPARAM_MAIN_TEXTURE          "mo__Texture"
#define SHADERPARAM_MAIN_TEXTURE_SIZE     "mo__TextureSize"
#define SHADERPARAM_MAIN_TEXTURE_SIZE_INV "mo__TextureSizeInv"
#define SHADERPARAM_DIFFUSE               "mo__Color"
#define SHADERPARAM_SPECULAR              "mo__Specular"
#define SHADERPARAM_MATRIX_VIEW           "mo__MatrixView"
#define SHADERPARAM_MATRIX_PROJ           "mo__MatrixProj"
#define SHADERPARAM_TIME_SEC              "mo__TimeSec"
#define SHADERPARAM_PIXEL_UV              "u_PixelUV" // mo__TextureSizeInv の旧名


namespace mo {

#define LOCK_GUARD(mtx)  std::lock_guard<std::recursive_mutex> __lock__(mtx)


// 特殊フラグ。GraphicsDX, GraphicsGL などから使う
GraphicFlags g_graphic_flags;



void translateInPlace(Matrix4 &m, float x, float y, float z) {
	// (* * * *) 
	// (* * * *)
	// (* * * *)
	// (x y z 1) <--- ここをずらす
	m._41 += x;
	m._42 += y;
	m._43 += z;
}
void translateInPlace(Matrix4 &m, const Vec3 &delta) {
	translateInPlace(m, delta.x, delta.y, delta.z);
}
void translateHalfPixelInPlace(Matrix4 &m) {
	translateInPlace(m, -HALF_PIXEL, HALF_PIXEL, 0.0f);
}
/// 変形行列の平行移動成分だけを整数化する
void translateFloorInPlace(Matrix4 &m) {
	// (* * * *)
	// (* * * *)
	// (* * * *)
	// (x y z 1) <--- ここの x, y, z を整数化する
	m._41 = floorf(m._41);
	m._42 = floorf(m._42);
	m._43 = floorf(m._43); // キャビネット図法など、Z値によってX,Yの座標が変化する場合はZの整数化もしないとダメ
}



void mo__Texture_setPixelFast(TEXID tex, void *locked_buf, int x, int y, const Color &color) {
	Log_assert(tex);
	Log_assert(locked_buf);
	TexDesc desc;
	if (!Video::getTextureDesc(tex, &desc)) {
		Log_assert(0);
	}
	Log_assert(0 <= x && x < desc.w);
	Log_assert(0 <= y && y < desc.h);
	uint8_t *buf = reinterpret_cast<uint8_t *>(locked_buf);
	const int pitch = desc.pitch;
	Log_assert(pitch > 0);
	if (desc.is_opengl_compatible_color_order) {
		// RGBA順(OpenGL系)
		buf[pitch * y + 4 * x + 0] = (uint8_t)(color.r * 255);
		buf[pitch * y + 4 * x + 1] = (uint8_t)(color.g * 255);
		buf[pitch * y + 4 * x + 2] = (uint8_t)(color.b * 255);
		buf[pitch * y + 4 * x + 3] = (uint8_t)(color.a * 255);
	} else {
		// BGRA順(Direct3D系)
		buf[pitch * y + 4 * x + 0] = (uint8_t)(color.b * 255);
		buf[pitch * y + 4 * x + 1] = (uint8_t)(color.g * 255);
		buf[pitch * y + 4 * x + 2] = (uint8_t)(color.r * 255);
		buf[pitch * y + 4 * x + 3] = (uint8_t)(color.a * 255);
	}
}
Color mo__Texture_getPixelFast(TEXID tex, const void *locked_buf, int x, int y) {
	Log_assert(tex);
	Log_assert(locked_buf);
	TexDesc desc;
	memset(&desc, 0, sizeof(desc));
	Video::getTextureDesc(tex, &desc);
	Log_assert(0 <= x && x < desc.w);
	Log_assert(0 <= y && y < desc.h);
	const uint8_t *buf = reinterpret_cast<const uint8_t *>(locked_buf);
	const int pitch = desc.pitch;
	Log_assert(pitch > 0);
	Color color;
	if (desc.is_opengl_compatible_color_order) {
		// RGBA順(OpenGL系)
		color.r = buf[pitch * y + 4 * x + 0] / 255.0f;
		color.g = buf[pitch * y + 4 * x + 1] / 255.0f;
		color.b = buf[pitch * y + 4 * x + 2] / 255.0f;
		color.a = buf[pitch * y + 4 * x + 3] / 255.0f;
	} else {
		// BGRA順(Direct3D系)
		color.b = buf[pitch * y + 4 * x + 0] / 255.0f;
		color.g = buf[pitch * y + 4 * x + 1] / 255.0f;
		color.r = buf[pitch * y + 4 * x + 2] / 255.0f;
		color.a = buf[pitch * y + 4 * x + 3] / 255.0f;
	}
	return color;
}
void mo__Texture_setPixel(TEXID tex, int x, int y, const Color &color) {
	TexDesc desc;
	if (!Video::getTextureDesc(tex, &desc)) return;
	if (desc.pixel_format != KImage::RGBA32) return;
	if (desc.is_render_target) return;
	if (x < 0 || desc.w <= x) return;
	if (y < 0 || desc.h <= y) return;
	const int pitch = desc.pitch;
	Log_assert(pitch > 0);
	uint8_t *data = (uint8_t *)Video::lockTexture(tex);
	if (data) {
		mo__Texture_setPixelFast(tex, data, x, y, color);
		Video::unlockTexture(tex);
	}
}
Color mo__Texture_getPixel(TEXID tex, int x, int y) {
	Color color;
	TexDesc desc;
	if (!Video::getTextureDesc(tex, &desc)) return color;
	if (desc.pixel_format != KImage::RGBA32) return color;
	if (desc.is_render_target) return color;
	if (x < 0 || desc.w <= x) return color;
	if (y < 0 || desc.h <= y) return color;
	const int pitch = desc.pitch;
	Log_assert(pitch > 0);
	uint8_t *data = (uint8_t *)Video::lockTexture(tex);
	if (data) {
		color = mo__Texture_getPixelFast(tex, data, x, y);
		Video::unlockTexture(tex);
	}
	return color;
}
void mo__FancyWriteImage(TEXID tex, const KImage &image) {
	Log_assert(tex);

	const int imgW = image.getWidth();
	const int imgH = image.getHeight();

	// 想定されたであろうテクスチャサイズ
	int assumedW = imgW;
	int assumedH = imgH;

	// 想定されたであろうテクスチャサイズに対する、画像UV範囲
	float assumedU = (float)imgW / assumedW;
	float assumedV = (float)imgH / assumedH;

	// 実際に作成されたテクスチャサイズ
	int texW;
	int texH;
	Video::getTextureSize(tex, &texW, &texH);
	// テクスチャに画像を描画する
	//
	// ※テクスチャサイズが画像サイズと異なっている場合でも、
	// UV 範囲が想定値 (assumedU, assumedV) と変わらないようにする
	// 
	// たとえば画像サイズが 768 x 768 の場合、想定されるテクスチャサイズは 1024 x 1024 なので、
	// この画像を表示しようとしたら UV 範囲を 0.75 x 0.75 に設定するはずである。
	// そして、別に作成されているであろうメッシュファイルには UV 範囲 0.75 x 0.75 を使う前提で UV 座標が設定されているはずである。
	// ところがもしも、実際に作成されたテクスチャサイズが 1024 x 1024 ではなく 512 x 512 だった場合（テクスチャ最大サイズを超えた場合など）
	// 画像を 512 x 512 の範囲に縮小して描画すればよいのだが、
	// そのとき、512 x 512 いっぱいに引き延ばして描画してしまうと UV 範囲が 1.0 x 1.0 になり、
	// あらかじめ想定していた UV 範囲 0.75 x 0.75 と異なってしまう。
	// そのため、実際のテクスチャがどんなサイズであろうと、想定 UV 範囲に画像を描画するように調整する
	Video::writeImageToTexture(tex, image, assumedU, assumedV);
}







/// 必要な頂点ストリーム数。
/// この値は、ゲームエンジンの要求する最低スペック要件になる。
/// この数未満の頂点ストリームにしか対応していない環境で実行しようとした場合、正しい描画は保証されない
static const int SYSTEMREQ_MAX_VERTEX_ELMS = 8;

#pragma region D3D9 Functions
/// HRESULT 表示用のマクロ
/// S_OK ならば通常ログを出力し、エラーコードであればエラーログを出力する
static void DX9_log(const char *msg_u8, HRESULT hr) {
	std::string text; // UTF8
	text = msg_u8;
	text += ": ";
	text += DX_GetErrorString(hr);
	if (SUCCEEDED(hr)) {
		Log_info(text.c_str());
	} else {
		Log_error(text.c_str());
	}
}
static void DX9_printPresentParameter(const char *label, const D3DPRESENT_PARAMETERS &pp) {
	Log_textf("--");
	Log_textf("%s", label);
	Log_textf("- BackBufferWidth           : %d", pp.BackBufferWidth);
	Log_textf("- BackBufferHeight          : %d", pp.BackBufferHeight);
	Log_textf("- BackBufferFormat          : (D3DFORMAT)%d", pp.BackBufferFormat);
	Log_textf("- BackBufferCount           : %d", pp.BackBufferCount);
	Log_textf("- MultiSampleType           : (D3DMULTISAMPLE_TYPE)%d", pp.MultiSampleType);
	Log_textf("- MultiSampleQuality        : %d", pp.MultiSampleQuality);
	Log_textf("- SwapEffect                : (D3DSWAPEFFECT)%d", pp.SwapEffect);
	Log_textf("- hDeviceWindow             : 0x%08x", pp.hDeviceWindow);
	Log_textf("- Windowed                  : %d", pp.Windowed);
	Log_textf("- EnableAutoDepthStencil    : %d", pp.EnableAutoDepthStencil);
	Log_textf("- AutoDepthStencilFmt       : (D3DFORMAT)%d", pp.AutoDepthStencilFormat);
	Log_textf("- Flags                     : %d (0x%08x)", pp.Flags, pp.Flags);
	Log_textf("- FullScreen_RefreshRateInHz: %d", pp.FullScreen_RefreshRateInHz);
	Log_textf("- PresentationInterval      : 0x%08x", pp.PresentationInterval);
	Log_textf("");
}
static void DX9_printAdapter(IDirect3D9 *d3d9) {
	Log_assert(d3d9);
	D3DADAPTER_IDENTIFIER9 adap;
	ZeroMemory(&adap, sizeof(adap));
	d3d9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adap);
	Log_textf("--");
	Log_textf("Adapter");
	Log_textf("- Driver     : %s", adap.Driver);
	Log_textf("- Description: %s", adap.Description);
	Log_textf("- DeviceName : %s", adap.DeviceName);
	Log_textf("");
}
static void DX9_printFullscreenResolutions(IDirect3D9 *d3d9) {
	Log_assert(d3d9);
	Log_textf("Supported Fullscreen Resolutions:");
	int num = d3d9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
	for (int i=0; i<num; i++) {
		D3DDISPLAYMODE mode;
		if (d3d9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode) == D3D_OK) {
			Log_textf("- [%2d] %dx%d D3DFORMAT(%d)", i, mode.Width, mode.Height, mode.Format);
		}
	}
	Log_textf("");
}
static void DX9_makeDefaultPresentParameters(IDirect3D9 *d3d9, HWND hWnd, D3DPRESENT_PARAMETERS *pp) {
	Log_assert(d3d9);
	Log_assert(pp);
	D3DDISPLAYMODE disp;
	ZeroMemory(&disp, sizeof(disp));
	d3d9->GetAdapterDisplayMode(0, &disp);
	// D3DPRESENT_PARAMETERS で EnableAutoDepthStencil を TRUE にしたら、
	// たとえ D3DRS_ZENABLE を FALSE にしていたとしても Z バッファーの生成を省略してはいけない。
	// 特にレンダーターゲット設定時にはZバッファーもレンダーターゲットに応じた大きさのものに差し替えておかないと、
	// 古いほうのZバッファサイズでクリッピングされてしまう！（大丈夫な環境もある）
	D3DPRESENT_PARAMETERS d3dpp = {
		0,                             // BackBufferWidth
		0,                             // BackBufferHeight
		disp.Format,                   // BackBufferFormat
		1,                             // BackBufferCount
		D3DMULTISAMPLE_NONE,           // MultiSampleType アンチエイリアス。使用の際には D3DRS_MULTISAMPLEANTIALIAS を忘れずに
		0,                             // MultiSampleQuality
		D3DSWAPEFFECT_DISCARD,         // SwapEffect
		hWnd,                          // hDeviceWindow
		TRUE,                          // Windowed
		TRUE,                          // EnableAutoDepthStencil
		D3DFMT_D24S8,                  // AutoDepthStencilFormat
		0,                             // Flags
		D3DPRESENT_RATE_DEFAULT,       // FullScreen_RefreshRateInHz
		D3DPRESENT_INTERVAL_IMMEDIATE, // PresentationInterval
	};
	if (1) {
		// http://maverickproj.web.fc2.com/pg11.html
		// この時点で d3dpp_ はマルチサンプリングを使わない設定なっている。
		// 最高品質から順番に調べていき、サポートしているならその設定を使う
		// ※アンチエイリアスが有効なのはバックバッファに描画する時であって、レンダーターゲットに描画する時には無効という噂あり。
		// 当然だがポリゴンエッジに対してAAがかかるのであって、テクスチャ画像に対してではないので、あまり意味はない。
		D3DMULTISAMPLE_TYPE mst = D3DMULTISAMPLE_16_SAMPLES;
		while (mst > 0) {
			DWORD back_msq;
			DWORD depth_msq;
			HRESULT hr1 = d3d9->CheckDeviceMultiSampleType(
				D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL, 
				d3dpp.BackBufferFormat,
				d3dpp.Windowed,
				mst,
				&back_msq);
			HRESULT hr2 = d3d9->CheckDeviceMultiSampleType(
				D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL, 
				d3dpp.AutoDepthStencilFormat,
				d3dpp.Windowed,
				mst,
				&depth_msq);
			if (SUCCEEDED(hr1) && SUCCEEDED(hr2)) {
				d3dpp.MultiSampleType = mst;
				d3dpp.MultiSampleQuality = Num_min(back_msq-1, depth_msq-1);
				break;
			}
			mst = (D3DMULTISAMPLE_TYPE)(mst - 1);
		}
		Log_textf("--");
		Log_textf("Multi sampling check:");
		Log_textf("- Max multi sampling type: (D3DMULTISAMPLE_TYPE)%d", mst);
		Log_textf("");
	}
	*pp = d3dpp;
}
/// D3DCAPS9 が D3DPTEXTURECAPS_SQUAREONLY フラグを持っているかどうか
///
/// この関数はテスト用フラグの影響を受ける。テスト中でない場合は D3DPTEXTURECAPS_SQUAREONLY フラグの有無をそのまま返すが、
/// 正方形テクスチャのテスト中だった場合は caps の値とは無関係に必ず false を返す。
static bool DX9_hasD3DPTEXTURECAPS_SQUAREONLY(const D3DCAPS9 &caps) {
	if (g_graphic_flags.use_square_texture_only) {
		return false;
	}
	return (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0;
}

/// D3DCAPS9 に設定されている MaxTextureWidth の値を返す
///
/// この関数はテスト用フラグの影響を受ける。テスト中でない場合は D3DCAPS9::MaxTextureWidth の値をそのまま返すが、
/// サイズ制限が有効な場合は g_graphic_flags.limit_texture_size と比較して小さいほうの値を返す。
static int DX9_getMaxTextureWidth(const D3DCAPS9 &caps) {
	int lim = g_graphic_flags.limit_texture_size;
	if (lim > 0) {
		if (lim < (int)caps.MaxTextureWidth) {
			return lim;
		}
	}
	return (int)caps.MaxTextureWidth;
}

/// D3DCAPS9 に設定されている MaxTextureHeight の値を返す
///
/// この関数はテスト用フラグの影響を受ける。テスト中でない場合は D3DCAPS9::MaxTextureHeight の値をそのまま返すが、
/// サイズ制限が有効な場合は g_graphic_flags.limit_texture_size と比較して小さいほうの値を返す。
static int DX9_getMaxTextureHeight(const D3DCAPS9 &caps) {
	int lim = g_graphic_flags.limit_texture_size;
	if (lim > 0) {
		if (lim < (int)caps.MaxTextureHeight) {
			return lim;
		}
	}
	return (int)caps.MaxTextureHeight;
}
static void DX9_checkRenderIOTexture(IDirect3DDevice9 *d3ddev, IDirect3DTexture9 *tex) {
	Log_assert(d3ddev);
	static bool err_raised = false; // 最初のエラーだけを調べる
	if (MO_STRICT_CHECK) {
		if (!err_raised && tex) {
			IDirect3DSurface9 *surf_target;
			d3ddev->GetRenderTarget(0, &surf_target);
			IDirect3DSurface9 *surf_tex;
			tex->GetSurfaceLevel(0, &surf_tex);
			if (surf_target == surf_tex) {
				Log_error(u8"E_D3D_SOURCE_AND_DEST_TEXTURE_ARE_SAME: 描画するテクスチャと描画先レンダーテクスチャが同じになっています。動作が未確定になります");
				err_raised = true;
			}
			DX9_REL(surf_tex);
			DX9_REL(surf_target);
		}
	}
}
static void DX9_printDeviceCaps(IDirect3DDevice9 *d3ddev9) {
	Log_assert(d3ddev9);
	D3DCAPS9 caps;
	d3ddev9->GetDeviceCaps(&caps);

	Log_trace;
	Log_textf("--");
	Log_textf("Check Caps");
	Log_textf("- VertexShaderVersion                : %d.%d", D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion), D3DSHADER_VERSION_MINOR(caps.VertexShaderVersion));
	Log_textf("- PixelShaderVersion                 : %d.%d", D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion),  D3DSHADER_VERSION_MINOR(caps.PixelShaderVersion));
	Log_textf("- MaxTextureWidth                    : %d", DX9_getMaxTextureWidth(caps));
	Log_textf("- MaxTextureHeight                   : %d", DX9_getMaxTextureHeight(caps));
	Log_textf("- D3DPTEXTURECAPS_ALPHA              : %s", (caps.TextureCaps & D3DPTEXTURECAPS_ALPHA) ? "Yes" : "No");
	Log_textf("- D3DPTEXTURECAPS_SQUAREONLY         : %s", DX9_hasD3DPTEXTURECAPS_SQUAREONLY(caps) ? "Yes" : "No");
	Log_textf("- D3DPTEXTURECAPS_POW2               : %s", (caps.TextureCaps & D3DPTEXTURECAPS_POW2) ? "Yes" : "No");
	Log_textf("- D3DPTEXTURECAPS_NONPOW2CONDITIONAL : %s", (caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) ? "Yes" : "No");
	Log_textf("- D3DTEXOPCAPS_MULTIPLYADD           : %s", (caps.TextureOpCaps & D3DTEXOPCAPS_MULTIPLYADD) ? "Yes" : "No"); // D3DTOP_MULTIPLYADD
	Log_textf("- D3DPMISCCAPS_PERSTAGECONSTANT      : %s", (caps.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) ? "Yes" : "No"); // D3DTSS_CONSTANT
	Log_textf("- D3DPMISCCAPS_COLORWRITEENABLE      : %s", (caps.PrimitiveMiscCaps & D3DPMISCCAPS_COLORWRITEENABLE) ? "Yes" : "No");
	Log_textf("- D3DPMISCCAPS_SEPARATEALPHABLEND    : %s", (caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) ? "Yes" : "No");
	Log_textf("- [Src] D3DPBLENDCAPS_BLENDFACTOR    : %s", (caps.SrcBlendCaps  & D3DPBLENDCAPS_BLENDFACTOR) ? "Yes" : "No");
	Log_textf("- [Dst] D3DPBLENDCAPS_BLENDFACTOR    : %s", (caps.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR) ? "Yes" : "No");
	Log_textf("- NumSimultaneousRTs                 : %d", caps.NumSimultaneousRTs); // Multi path rendering
	Log_textf("- MaxStreams                         : %d", caps.MaxStreams); // 頂点ストリーム数
	Log_textf("- MaxTextureBlendStages              : %d", caps.MaxTextureBlendStages);
	Log_textf("- AvailableTextureMem                : %d[MB]", d3ddev9->GetAvailableTextureMem()/1024/1024);
	Log_textf("");
}
/// フルスクリーン解像度のチェック
/// 指定したパラメータでフルスクリーンできるなら true を返す
static bool DX9_checkFullScreenParams(IDirect3D9 *d3d9, int w, int h) {
	Log_assert(d3d9);
	Log_assert(w > 0);
	Log_assert(h > 0);
	int num = d3d9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
	for (int i=0; i<num; i++) {
		D3DDISPLAYMODE mode;
		if (d3d9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode) == D3D_OK) {
			if ((int)mode.Width == w && (int)mode.Height == h) {
				return true;
			}
		}
	}
	return false;
}
static void DX9_setBlend(IDirect3DDevice9 *d3ddev, Blend blend) {
	// RGB と Alpha の双方に同一のブレンド式を設定する。
	// これを別々にしたい場合は D3DRS_SEPARATEALPHABLENDENABLE を参照せよ
	Log_assert(d3ddev);
	switch (blend) {
	case Blend_One:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		break;
	case Blend_Add: // add + alpha
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	case Blend_Sub: // sub + alpha
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_REVSUBTRACT);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
		break;
	case Blend_Mul:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ZERO);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
		break;
	case Blend_Alpha:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		break;
	case Blend_Screen:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_INVDESTCOLOR);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	case Blend_Max:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_MAX);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	}
}
static void DX9_setFilter(IDirect3DDevice9 *d3ddev, Filter filter) {
	switch (filter) {
	case Filter_None:
		d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		break;

	case Filter_Linear:
		d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		break;

	case Filter_QuadA:
		d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_PYRAMIDALQUAD);
		d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_PYRAMIDALQUAD);
		break;

	case Filter_QuadB:
		d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_GAUSSIANQUAD);
		d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_GAUSSIANQUAD);
		break;

	case Filter_Anisotropic:
		d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
		d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
		break;
	}
}
static IDirect3D9 * DX9_init() {
	IDirect3D9 *d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	DX9_log("Direct3DCreate9", d3d9 ? S_OK : E_FAIL);
	if (d3d9) {
		// アダプター情報
		if (1) DX9_printAdapter(d3d9);

		// サポートしているフルスクリーン解像度
		if (0) DX9_printFullscreenResolutions(d3d9);

		return d3d9;
	}
	Log_error(
		u8"E_FAIL: Direct3DCreate9\n"
		u8"グラフィックの初期化に失敗しました。\n"
		u8"システム要件を満たし、最新版の Direct3D がインストールされていることを確認してください\n"
		u8"・念のため、他のソフトを全て終了させるか、パソコンを再起動してからもう一度ゲームを実行してみてください\n\n"
	);
	return NULL;
}
static IDirect3DDevice9 * DX9_initDevice(IDirect3D9 *d3d9, HWND hWnd, D3DPRESENT_PARAMETERS *d3dpp) {
	IDirect3DDevice9 *d3ddev9 = NULL;
	DX9_makeDefaultPresentParameters(d3d9, hWnd, d3dpp);
	DX9_printPresentParameter("Present Parameters (Input)", *d3dpp);
	if (d3ddev9 == NULL) {
		HRESULT hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED, d3dpp, &d3ddev9);
		DX9_log("IDirect3DDevice9::CreateDevice (HAL/HARD)", hr);
	}
	if (d3ddev9 == NULL) {
		HRESULT hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED, d3dpp, &d3ddev9);
		DX9_log("IDirect3DDevice9::CreateDevice (HAL/SOFT)", hr);
	}
	if (d3ddev9 == NULL) {
		HRESULT hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED, d3dpp, &d3ddev9);
		DX9_log("IDirect3DDevice9::CreateDevice (REF/SOFT)", hr);
	}
	if (d3ddev9 == NULL) {
		// デバイスを初期化できなかった。
		// [[デバッグ方法]]
		// このエラーを意図的に出すには F5 で実行した直後に Ctrl+Alt+Delete でタスクマネージャのフルスクリーン画面を出し、
		// その間に裏でプログラムが起動するのを待てばよい。起動した頃を見計らってフルスクリーンを解除すれば、このダイアログが出ているはず。
		Log_errorf(
			u8"E_FAIL: IDirect3DDevice9::CreateDevice\n"
			u8"Direct3D デバイスの初期化に失敗しました。\n"
			u8"Direct3D はインストールされていますが、利用することができません\n"
			u8"--\n"
			u8"・他のアプリケーションによって画面がフルスクリーンになっている最中にゲームが起動した場合、このエラーが出ます\n"
			u8"・念のため、他のソフトを全て終了させるか、パソコンを再起動してからもう一度ゲームを実行してみてください\n\n"
		);
	}
	return d3ddev9;
}
#pragma endregion // D3D9 Functions





#pragma region DXTex
struct DXTex {
	D3DSURFACE_DESC desc;
	D3DCAPS9 caps;
	IDirect3DDevice9  *d3ddev;
	IDirect3DTexture9 *d3dtex;
	IDirect3DSurface9 *color_surf;
	IDirect3DSurface9 *depth_surf;
	IDirect3DSurface9 *target_surf;
	UINT pitch;
	UINT original_w;
	UINT original_h;
	UINT required_w;
	UINT required_h;

	DXTex() {
		d3ddev = NULL;
		d3dtex = NULL;
		color_surf = NULL;
		depth_surf = NULL;
		target_surf = NULL;
		pitch = 0;
		original_w = 0;
		original_h = 0;
		required_w = 0;
		required_h = 0;
		ZeroMemory(&desc, sizeof(desc));
		ZeroMemory(&caps, sizeof(caps));
	}
};
static bool DXTex_createD3DObjects(DXTex &dxtex, const D3DSURFACE_DESC *_desc) {
	if (dxtex.d3ddev == NULL) {
		return false;
	}
	Log_assert(dxtex.d3ddev);
	Log_assert(dxtex.color_surf == NULL);
	Log_assert(dxtex.depth_surf == NULL);
	Log_assert(dxtex.d3dtex == NULL);
	Log_assert(dxtex.pitch == 0);
	D3DSURFACE_DESC param = _desc ? *_desc : dxtex.desc;

	dxtex.required_w = param.Width;
	dxtex.required_h = param.Height;

	// 作成しようとしているサイズがサポートサイズを超えているなら、作成可能なサイズまで小さくしておく。
	// テクスチャは正規化されたUV座標で描画するため、テクスチャ自体のサイズは関係ない。
	// ただし、メッシュなどでテクスチャ範囲がピクセル単位で指定されている場合は、テクスチャの実際のサイズを考慮しないといけないので注意。
	// --> mo__Texture_writeImageStretch()
	UINT maxW = DX9_getMaxTextureWidth(dxtex.caps);
	UINT maxH = DX9_getMaxTextureHeight(dxtex.caps);
	if (param.Width > maxW) {
		param.Width = maxW;
	}
	if (param.Height > maxH) {
		param.Height = maxH;
	}

	// テクスチャを作成
	HRESULT hr = dxtex.d3ddev->CreateTexture(param.Width, param.Height, 1, param.Usage, param.Format, param.Pool, &dxtex.d3dtex, NULL);
	if (FAILED(hr)) {
		Log_errorf("Failed to create texture: Size=(%d, %d), Format=D3DFMT_A8R8G8B8, HRESULT=%s",
			param.Width, param.Height, DX_GetErrorString(hr).c_str());
		return false;
	}
	// テクスチャ情報を取得
	dxtex.d3dtex->GetLevelDesc(0, &dxtex.desc);

	if (dxtex.desc.Usage == D3DUSAGE_RENDERTARGET) {
		// レンダーテクスチャを生成した
		// ターゲットサーフェスと深度バッファを取得しておく
		dxtex.d3dtex->GetSurfaceLevel(0, &dxtex.color_surf);
		hr = dxtex.d3ddev->CreateDepthStencilSurface(dxtex.desc.Width, dxtex.desc.Height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &dxtex.depth_surf, NULL);
		if (FAILED(hr)) {
			Log_errorf("Failed to create depth stencil surface for render target: Size=(%d, %d), Format=D3DFMT_D24S8, HRESULT=%s",
				(int)dxtex.desc.Width, (int)dxtex.desc.Height, DX_GetErrorString(hr).c_str());
			return false;
		}
	}
	switch (dxtex.desc.Format) {
	case D3DFMT_A8R8G8B8:
	case D3DFMT_A8B8G8R8:
		dxtex.pitch = dxtex.desc.Width * 4;
		break;
	default:
		Log_assert(0);
		dxtex.pitch = 0;
		break;
	}
	return true;
}
static void DXTex_destroyD3DObjects(DXTex &dxtex) {
	DX9_REL(dxtex.color_surf);
	DX9_REL(dxtex.depth_surf);
	DX9_REL(dxtex.d3dtex);
	DX9_REL(dxtex.target_surf);
	dxtex.pitch = 0;
}
static void DXTex_onDeviceLost(DXTex &tex) {
	if (tex.desc.Pool == D3DPOOL_DEFAULT) {
		DXTex_destroyD3DObjects(tex);
	}
}
static void DXTex_onDeviceReset(DXTex &tex) {
	if (tex.desc.Pool == D3DPOOL_DEFAULT) {
		DXTex_createD3DObjects(tex, &tex.desc);
	}
}
static bool DXTex_isRenderTarget(const DXTex &dxtex) {
	return (dxtex.desc.Usage & D3DUSAGE_RENDERTARGET) != 0;
}
static void * DXTex_lock(DXTex &dxtex) {
	if (dxtex.d3dtex == NULL) {
		Log_error("E_LOCK: null device");
		return NULL;
	}
	if (dxtex.desc.Width == 0 || dxtex.desc.Height == 0) {
		Log_error("E_LOCK: invalid texture params");
		return NULL;
	}
	if (dxtex.pitch == 0) {
		Log_error("E_LOCK: invalid pitch");
		return NULL;
	}
	if (DXTex_isRenderTarget(dxtex)) {
		HRESULT hr;
		if (dxtex.target_surf == NULL) {
			hr = dxtex.d3ddev->CreateOffscreenPlainSurface(
				dxtex.desc.Width, dxtex.desc.Height, dxtex.desc.Format,
				D3DPOOL_SYSTEMMEM, &dxtex.target_surf, NULL);  // コピー用サーフェスを作成
			if (FAILED(hr)) {
				DX9_log("E_LOCK: CreateOffscreenPlainSurface failed", hr);
				return NULL;
			}
		}
		IDirect3DSurface9 *tmp_surf = NULL;
		hr = dxtex.d3dtex->GetSurfaceLevel(0, &tmp_surf);
		if (FAILED(hr)) {
			DX9_log("E_LOCK: GetSurfaceLevel failed", hr);
			return NULL;
		}
		hr = dxtex.d3ddev->GetRenderTargetData(tmp_surf, dxtex.target_surf);
		DX9_REL(tmp_surf);
		if (FAILED(hr)) {
			DX9_log("E_LOCK: Sruface GetRenderTargetData failed", hr);
			return NULL;
		}
		D3DLOCKED_RECT lrect;
		hr = dxtex.target_surf->LockRect(&lrect, NULL, D3DLOCK_NOSYSLOCK);
		if (FAILED(hr)) {
			DX9_log("E_LOCK: RenderTexture LockRect failed", hr);
			return NULL;
		}
		return lrect.pBits;

	} else {
		D3DLOCKED_RECT lrect;
		HRESULT hr = dxtex.d3dtex->LockRect(0, &lrect, NULL, 0);
		if (FAILED(hr)) {
			DX9_log("E_LOCK: Texture2D LockRect failed", hr);
			return NULL;
		}
		return lrect.pBits;
	}
}
static void DXTex_unlock(DXTex &dxtex) {
	if (DXTex_isRenderTarget(dxtex)) {
		if (dxtex.target_surf) {
			dxtex.target_surf->UnlockRect();
		}
	} else {
		if (dxtex.d3dtex) {
			dxtex.d3dtex->UnlockRect(0);
		}
	}
}
static KImage::Format DXTex_getPixelFormat(const DXTex &dxtex) {
	switch (dxtex.desc.Format) {
	case D3DFMT_A8R8G8B8: return KImage::RGBA32;
	case D3DFMT_R8G8B8:   return KImage::RGB24;
	}
	return KImage::NOFMT;
}
static void DXTex_getDesc(DXTex &dxtex, TexDesc *desc) {
	Log_assert(desc);
	memset(desc, 0, sizeof(*desc));
	desc->w = dxtex.desc.Width;
	desc->h = dxtex.desc.Height;
	desc->original_w = dxtex.original_w;
	desc->original_h = dxtex.original_h;
	desc->original_u = (float)dxtex.original_w / dxtex.desc.Width;
	desc->original_v = (float)dxtex.original_h / dxtex.desc.Height;
	desc->pitch = dxtex.pitch;
	desc->size_in_bytes = dxtex.pitch * dxtex.desc.Height;
	desc->pixel_format = DXTex_getPixelFormat(dxtex);
	desc->is_render_target = DXTex_isRenderTarget(dxtex);
	desc->d3dtex9 = (intptr_t)dxtex.d3dtex;
	desc->is_opengl_compatible_color_order = false;
}
static void DXTex_writeImage(DXTex &dxtex, const KImage &image) {
	if (image.empty()) return;

	TexDesc texdesc;
	DXTex_getDesc(dxtex, &texdesc);
	int w = Num_min(texdesc.w, image.getWidth());
	int h = Num_min(texdesc.h, image.getHeight());
	int srcPitch = image.getWidth() * 4;
	int dstPitch = texdesc.pitch;
	const uint8_t *srcBuf = image.getData();
	uint8_t *dstBuf = (uint8_t *)DXTex_lock(dxtex);
	if (srcBuf && dstBuf) {
		for (int y=0; y<h; y++) {
			auto src = srcBuf + srcPitch * y;
			auto dst = dstBuf + dstPitch * y;
			for (int x=0; x<w; x++) {
				const int srcIdx = x * 4;
				const int dstIdx = x * 4;
				const uint8_t r = src[srcIdx + 0];
				const uint8_t g = src[srcIdx + 1];
				const uint8_t b = src[srcIdx + 2];
				const uint8_t a = src[srcIdx + 3];
				dst[dstIdx + 0] = b;
				dst[dstIdx + 1] = g;
				dst[dstIdx + 2] = r;
				dst[dstIdx + 3] = a;
			}
		}
		DXTex_unlock(dxtex);
	}
}
static void DXTex_writeImageStretch(DXTex &dxtex, const KImage &image, float u, float v) {
	if (image.empty()) return;

	TexDesc dst_desc;
	DXTex_getDesc(dxtex, &dst_desc);

	int srcW = image.getWidth();
	int srcH = image.getHeight();
	int srcPitch = image.getWidth() * 4;
	int dstPitch = dst_desc.pitch;
	const uint8_t *srcBuf = image.getData();
	uint8_t *dstBuf = (uint8_t *)DXTex_lock(dxtex);
	if (srcBuf == NULL) return;
	if (dstBuf == NULL) return;

	if (dst_desc.is_opengl_compatible_color_order) {
		// 書き込み先の配列が RGBA 順になっている(OpenGL系)
		for (int y=0; y<dst_desc.h; y++) {
			auto src = srcBuf + srcPitch * (int)(srcH / dst_desc.h * u * y);
			auto dst = dstBuf + dstPitch * y;
			for (int x=0; x<dst_desc.w; x++) {
				const int srcIdx = 4 * (int)(srcW / dst_desc.w * v * x);
				const int dstIdx = 4 * x;
				const uint8_t r = src[srcIdx + 0];
				const uint8_t g = src[srcIdx + 1];
				const uint8_t b = src[srcIdx + 2];
				const uint8_t a = src[srcIdx + 3];
				dst[dstIdx + 0] = r;
				dst[dstIdx + 1] = g;
				dst[dstIdx + 2] = b;
				dst[dstIdx + 3] = a;
			}
		}
	} else {
		// 書き込み先の配列が BGRA 順になっている(Direct3D系)
		for (int y=0; y<dst_desc.h; y++) {
			auto src = srcBuf + srcPitch * (int)(srcH / dst_desc.h * u * y);
			auto dst = dstBuf + dstPitch * y;
			for (int x=0; x<dst_desc.w; x++) {
				const int srcIdx = 4 * (int)(srcW / dst_desc.w * v * x);
				const int dstIdx = 4 * x;
				const uint8_t r = src[srcIdx + 0];
				const uint8_t g = src[srcIdx + 1];
				const uint8_t b = src[srcIdx + 2];
				const uint8_t a = src[srcIdx + 3];
				dst[dstIdx + 0] = b;
				dst[dstIdx + 1] = g;
				dst[dstIdx + 2] = r;
				dst[dstIdx + 3] = a;
			}
		}
		
	}
	DXTex_unlock(dxtex);
}
static void DXTex_writeImage(DXTex &dxtex, const KImage &image, float u, float v) {
	dxtex.original_w = image.getWidth();
	dxtex.original_h = image.getHeight();

	if (Num_is_zero(u) || Num_is_zero(v)) {
		// コピー元、コピー先のサイズに関係なく、ピクセルが1対1で対応するように拡縮なしでコピーする
		DXTex_writeImage(dxtex, image);
		return;
	}

	if (dxtex.desc.Width  == dxtex.required_w &&
		dxtex.desc.Height == dxtex.required_h &&
		Num_equals(u, 1.0f) && Num_equals(v, 1.0f)
	) {
		// コピー元とコピー先のサイズが一致していて、ＵＶ範囲が 1.0 x 1.0 になっている場合は
		// ピクセルが1対1で対応するように拡縮なしでコピーできる
		DXTex_writeImage(dxtex, image);
		return;
	}

	// 要求されたサイズと、実際に作成されたテクスチャサイズが異なるか、
	// コピー UV 範囲が異なる。拡縮ありでコピーする
	DXTex_writeImageStretch(dxtex, image, u, v);
}
static KImage DXTex_getImage(DXTex &dxtex, int channel) {
	int pitch = dxtex.pitch;
	std::string buf;
	const uint8_t *locked_p = (const uint8_t *)DXTex_lock(dxtex);
	if (locked_p) {
		buf.resize(pitch * dxtex.desc.Height);
		memcpy(&buf[0], locked_p, buf.size());
		DXTex_unlock(dxtex);
	} else {
		Log_error("Failed to lock texture");
	}
	if (buf.empty()) {
		return KImage();
	}
	uint8_t *p = (uint8_t *)&buf[0];
	switch (channel) {
	case -1: // RGBA
	case 0: // R (NOT SUPPORTED)
	case 1: // G (NOT SUPPORTED)
	case 2: // B (NOT SUPPORTED)
		if (0) {
			// テクスチャには RGBA 順で格納されている(OpenGL系)
			// 入れ替え不要

		} else {
			// テクスチャには BGRA 順で格納されている(Direct3D系)
			// KImage に転送するにはバッファ内での BGRA 順を RGBA 順に並び替える必要がある
			for (UINT y=0; y<dxtex.desc.Height; y++) {
				for (UINT x=0; x<dxtex.desc.Width; x++) {
					const uint8_t b = p[pitch*y + 4*x + 0];
					const uint8_t g = p[pitch*y + 4*x + 1];
					const uint8_t r = p[pitch*y + 4*x + 2];
					const uint8_t a = p[pitch*y + 4*x + 3];
					p[pitch*y + 4*x + 0] = r;
					p[pitch*y + 4*x + 1] = g;
					p[pitch*y + 4*x + 2] = b;
					p[pitch*y + 4*x + 3] = a;
				}
			}
		}
		break;
	case 3: // A
		// アルファチャンネルを可視化する
		for (UINT y=0; y<dxtex.desc.Height; y++) {
			for (UINT x=0; x<dxtex.desc.Width; x++) {
			//	const uint8_t b = p[pitch*y + 4*x + 0];
			//	const uint8_t g = p[pitch*y + 4*x + 1];
			//	const uint8_t r = p[pitch*y + 4*x + 2];
				const uint8_t a = p[pitch*y + 4*x + 3];
				p[pitch*y + 4*x + 0] = a;
				p[pitch*y + 4*x + 1] = a;
				p[pitch*y + 4*x + 2] = a;
				p[pitch*y + 4*x + 3] = 255;
			}
		}
		break;
	}
	return KImage(dxtex.desc.Width, dxtex.desc.Height, KImage::RGBA32, buf.data());
}
#pragma endregion // DXTex


#pragma region DXVB
struct DXVB {
	IDirect3DDevice9 *d3ddev;
	IDirect3DVertexBuffer9 *vb;
	int num_capacity; ///< 確保している頂点数（バイト数ではない！）
	int num_used;     ///< 実際に使用している頂点数。必ず numvert_used <= numvert_capacity になる
};
static void DXVB_create(DXVB &obj, IDirect3DDevice9 *d3ddev) {
	obj.d3ddev = d3ddev;
	obj.vb = NULL;
	obj.num_capacity  = 0;
	obj.num_used      = 0;
}
static void DXVB_getDesc(DXVB &obj, BufDesc *desc) {
	Log_assert(desc);
	desc->num_capacity = obj.num_capacity;
	desc->num_used = obj.num_used;
}
static void DXVB_destroy(DXVB &obj) {
	if (obj.vb) {
		obj.vb->Release();
		obj.vb = NULL;
	}
	obj.num_capacity = 0;
	obj.num_used = 0;
}
static void DXVB_onDeviceLost(DXVB &obj) {
	DXVB_destroy(obj); // ただ破棄するだけでよい
}
static void DXVB_onDeviceReset(DXVB &obj) {
	// なにもしなくてよい。バッファが NULL なら自動的に作成される
}
static void DXVB_clear(DXVB &obj) {
	// 使用量を 0 にするだけで、バッファは破棄しない
	obj.num_used = 0;
}
static Vertex * DXVB_lock(DXVB &obj, int offset, int count) {
	if (offset < 0 || count <= 0) {
		return NULL;
	}
	// バッファ容量を超えた場合は、再生成するためにいったん廃棄する
	if (obj.vb) {
		if (obj.num_capacity < offset + count) {
			obj.vb->Release();
			obj.vb = NULL;
		}
	}
	// バッファ生成
	if (obj.vb == NULL) {
		obj.num_capacity = offset + count + 1024; // 大きめにとっておく
		HRESULT hr = obj.d3ddev->CreateVertexBuffer(obj.num_capacity * sizeof(Vertex), D3DUSAGE_DYNAMIC/*|D3DUSAGE_WRITEONLY*/, DX9_FVF_VERTEX, D3DPOOL_DEFAULT, &obj.vb, NULL);
		if (SUCCEEDED(hr)) {
			Log_infof("Vertex buffer resized: %d vertices (%d bytes)", obj.num_capacity, obj.num_capacity * sizeof(Vertex));
		} else {
			Log_errorf("E_FAIL: CreateVertexBuffer: %s", DX_GetErrorString(hr).c_str());
			obj.num_capacity = 0;
			obj.num_used = 0;
			return NULL;
		}
	}
	// 使用サイズを更新
	obj.num_used = Num_max(obj.num_used, offset + count);
	// ロックする
	Vertex *ptr = NULL;
	obj.vb->Lock(offset * sizeof(Vertex), count * sizeof(Vertex), (void**)&ptr, D3DLOCK_DISCARD);
	return ptr;
}
static void DXVB_unlock(DXVB &obj) {
	if (obj.vb) {
		obj.vb->Unlock();
	}
}
static bool DXVB_aabb(DXVB &obj, int offset, int count, Vec3 *mincoord, Vec3 *maxcoord) {
	if (offset < 0) {
		Log_assert(0);
		return false;
	}
	if (obj.num_used < offset + count) {
		Log_assert(0);
		return false;
	}
	if (obj.vb) {
		Vertex *vertex = NULL;
		obj.vb->Lock(offset * sizeof(Vertex), count * sizeof(Vertex), (void**)&vertex, D3DLOCK_READONLY);
		if (vertex) {
			if (mincoord) {
				Vec3 result = vertex[0].pos;
				for (int i=1; i<count; i++) {
					const Vec3 &p = vertex[i].pos;
					result.x = (p.x < result.x) ? p.x : result.x;
					result.y = (p.y < result.y) ? p.y : result.y;
					result.z = (p.z < result.z) ? p.z : result.z;
				}
				*mincoord = result;
			}
			if (maxcoord) {
				Vec3 result = vertex[0].pos;
				for (int i=1; i<count; i++) {
					const Vec3 &p = vertex[i].pos;
					result.x = (p.x > result.x) ? p.x : result.x;
					result.y = (p.y > result.y) ? p.y : result.y;
					result.z = (p.z > result.z) ? p.z : result.z;
				}
				*maxcoord = result;
			}
			obj.vb->Unlock();
			return true;
		}
	}
	return false;
}

#pragma endregion // DXVB

#pragma region DXIB
struct DXIB {
	IDirect3DDevice9 *d3ddev;
	IDirect3DIndexBuffer9 *ib;
	int num_capacity; ///< 確保している頂点数（バイト数ではない！）
	int num_used;     ///< 実際に使用している頂点数。必ず numvert_used <= numvert_capacity になる
};
static void DXIB_create(DXIB &obj, IDirect3DDevice9 *d3ddev) {
	obj.d3ddev = d3ddev;
	obj.ib = NULL;
	obj.num_capacity  = 0;
	obj.num_used      = 0;
}

static void DXIB_getDesc(DXIB &obj, BufDesc *desc) {
	Log_assert(desc);
	desc->num_capacity = obj.num_capacity;
	desc->num_used = obj.num_used;
}
static void DXIB_destroy(DXIB &obj) {
	if (obj.ib) {
		obj.ib->Release();
		obj.ib = NULL;
	}
	obj.num_capacity = 0;
	obj.num_used = 0;
}
static void DXIB_onDeviceLost(DXIB &obj) {
	DXIB_destroy(obj); // ただ破棄するだけでよい
}
static void DXIB_onDeviceReset(DXIB &obj) {
	// なにもしなくてよい。バッファが NULL なら自動的に作成される
}
static void DXIB_clear(DXIB &obj) {
	// 使用量を 0 にするだけで、バッファは破棄しない
	obj.num_used = 0;
}
static int * DXIB_lock(DXIB &obj, int offset, int count) {
	if (offset < 0 || count <= 0) {
		return NULL;
	}
	// バッファ容量を超えた場合は、再生成するためにいったん廃棄する
	if (obj.ib) {
		if (obj.num_capacity < offset + count) {
			obj.ib->Release();
			obj.ib = NULL;
		}
	}
	// バッファ生成
	if (obj.ib == NULL) {
		obj.num_capacity = offset + count + 1024; // 大きめにとっておく
		HRESULT hr = obj.d3ddev->CreateIndexBuffer(obj.num_capacity * sizeof(int), D3DUSAGE_DYNAMIC/*|D3DUSAGE_WRITEONLY*/, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &obj.ib, NULL);
		if (SUCCEEDED(hr)) {
			Log_infof("Index buffer resized: %d indices (%d bytes)", obj.num_capacity, obj.num_capacity * sizeof(int));
		} else {
			Log_errorf("E_FAIL: CreateIndexBuffer: %s", DX_GetErrorString(hr).c_str());
			obj.num_capacity = 0;
			obj.num_used = 0;
			return NULL;
		}
	}
	// 使用サイズを更新
	obj.num_used = Num_max(obj.num_used, offset + count);
	// ロックする
	int *ptr = NULL;
	obj.ib->Lock(offset * sizeof(int), count * sizeof(int), (void**)&ptr, D3DLOCK_DISCARD);
	return ptr;
}
static void DXIB_unlock(DXIB &obj) {
	if (obj.ib) {
		obj.ib->Unlock();
	}
}
#pragma endregion // DXIB


#pragma region DXShader
enum PARAMHANDLE {
	PH_MATRIX_PROJ,
	PH_MATRIX_VIEW,
	PH_DIFFUSE,
	PH_SPECULAR,
	PH_TEXTURE,
	PH_TEX_SIZE,
	PH_TEX_SIZE_INV,
	PH_PIXEL_UV,
	PH_TIME_SEC,
	PH_ENUM_MAX
};
struct DXShader {
	IDirect3DDevice9 *d3ddev;
	ID3DXEffect *d3deff;
	D3DXEFFECT_DESC desc;
	D3DXHANDLE paramId[PH_ENUM_MAX];
	DXShader() {
		d3ddev = NULL;
		d3deff = NULL;
		ZeroMemory(paramId, sizeof(paramId));
		ZeroMemory(&desc, sizeof(desc));
	}
};
static void DXShader_destroy(DXShader &sh) {
	DX9_REL(sh.d3deff);
}
static bool DXShader_load(DXShader &sh, IDirect3DDevice9 *d3ddev, const char *hlsl_u8, const char *name_debug) {
	Log_assert(sh.d3ddev == NULL);
	Log_assert(sh.d3deff == NULL);
	Log_assert(d3ddev);
	Log_assert(hlsl_u8);
	Log_assert(name_debug);
	DWORD flags = 0;// = D3DXSHADER_PARTIALPRECISION; <-- もしかして、時間小数計算の精度が悪かったのでこれが原因じゃね？？
#ifdef _DEBUG
	flags |= D3DXSHADER_DEBUG; // デバッグ有効
#endif
	ID3DXBuffer *dxerr = NULL;
	ID3DXEffect *dxeff = NULL;

	SetLastError(0);
	HRESULT hr = D3DXCreateEffect(d3ddev, hlsl_u8, strlen(hlsl_u8), NULL, NULL, flags, NULL, &dxeff, &dxerr);

	if (FAILED(hr)) {
		void *msg_p = dxerr ? dxerr->GetBufferPointer() : NULL;
		const char *msg = msg_p ? (const char *)msg_p : "";
	//	DX9_log("D3DXCreateEffect Fail", hr);
		Log_errorf("D3DXCreateEffect: an error occurred in '%s': %s", name_debug, msg);
		DX9_REL(dxerr);
		return false;
	}
	dxeff->GetDesc(&sh.desc);
	sh.d3ddev = d3ddev;
	sh.d3deff = dxeff;

	// よく使うシェーダー変数のIDを取得
	sh.paramId[PH_MATRIX_PROJ]  = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_MATRIX_PROJ);
	sh.paramId[PH_MATRIX_VIEW]  = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_MATRIX_VIEW);
	sh.paramId[PH_TIME_SEC]     = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_TIME_SEC);
	sh.paramId[PH_TEX_SIZE]     = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_MAIN_TEXTURE_SIZE);
	sh.paramId[PH_TEX_SIZE_INV] = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_MAIN_TEXTURE_SIZE_INV);
	sh.paramId[PH_PIXEL_UV]     = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_PIXEL_UV);
	sh.paramId[PH_TEXTURE]      = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_MAIN_TEXTURE);
	sh.paramId[PH_DIFFUSE]      = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_DIFFUSE);
	sh.paramId[PH_SPECULAR]     = sh.d3deff->GetParameterByName(NULL, SHADERPARAM_SPECULAR);
	return true;
}
static void DXShader_applyMaterial(DXShader &sh, SHADERID shader, const Material *material) {
	Log_assert(material);
	Log_assert(sh.d3deff);
	DX9_setBlend(sh.d3ddev, material->blend);
	if (sh.paramId[PH_TIME_SEC]) {
		// time_sec の値が数万程度になるとHLSLでの浮動小数計算の情報落ちが顕著になるので注意
		// HLSLに渡す値があまり大きくならないよう、一定の周期で区切っておく
		// int msec = KClock::getSystemTimeMsec() % (60 * 1000);
		// float time_sec = msec / 1000.0f;
		float time_sec = KClock::getSystemTimeSecF();
		sh.d3deff->SetFloat(sh.paramId[PH_TIME_SEC], time_sec);
	}
	if (sh.paramId[PH_TEX_SIZE]) {
		if (material->texture) {
			int w, h;
			Video::getTextureSize(material->texture, &w, &h);
			float val[] = {
				(float)w,
				(float)h
			};
			sh.d3deff->SetFloatArray(sh.paramId[PH_TEX_SIZE], val, 2);
		}
	}
	if (sh.paramId[PH_TEX_SIZE_INV]) {
		if (material->texture) {
			int w, h;
			Video::getTextureSize(material->texture, &w, &h);
			float val[] = {
				1.0f / (float)w,
				1.0f / (float)h
			};
			sh.d3deff->SetFloatArray(sh.paramId[PH_TEX_SIZE_INV], val, 2);
		}
	}
	if (sh.paramId[PH_PIXEL_UV]) {
		// 互換性のためのもの。内容は setBuiltinConstant_TextureSizeInversed と同じ
		if (material->texture) {
			int w, h;
			Video::getTextureSize(material->texture, &w, &h);
			float val[] = {
				1.0f / (float)w,
				1.0f / (float)h,
			};
			sh.d3deff->SetFloatArray(sh.paramId[PH_PIXEL_UV], val, 2);
		}
	}
	if (sh.paramId[PH_TEXTURE]) {
		if (material->texture) {
			TexDesc desc;
			memset(&desc, 0, sizeof(desc));
			Video::getTextureDesc(material->texture, &desc);
			IDirect3DTexture9 *d3dtex = (IDirect3DTexture9 *)desc.d3dtex9;
			sh.d3deff->SetTexture(sh.paramId[PH_TEXTURE], d3dtex); // d3dtex は NULL の場合もある
			DX9_checkRenderIOTexture(sh.d3ddev, d3dtex);
		}
	}
	if (sh.paramId[PH_DIFFUSE]) {
		sh.d3deff->SetFloatArray(sh.paramId[PH_DIFFUSE], reinterpret_cast<const float*>(&material->color), 4);
	}
	if (sh.paramId[PH_SPECULAR]) {
		sh.d3deff->SetFloatArray(sh.paramId[PH_SPECULAR], reinterpret_cast<const float*>(&material->specular), 4);
	}
	if (material->cb) {
		material->cb->onMaterial_SetShaderVariable(shader, material);
	}
}
static void DXShader_begin(DXShader &sh, SHADERID shader, const Material *m, const Matrix4 &proj, const Matrix4 &transform) {
	if (sh.d3deff) {

		// テクニック選択
		D3DXHANDLE technique = sh.d3deff->GetTechnique(0);
		DXShader_applyMaterial(sh, shader, m);

		// マテリアルと関係ないシェーダー変数をセット
		if (sh.paramId[PH_MATRIX_PROJ]) {
			sh.d3deff->SetMatrix(sh.paramId[PH_MATRIX_PROJ], (D3DXMATRIX *)&proj);
		}
		if (sh.paramId[PH_MATRIX_VIEW]) {
			sh.d3deff->SetMatrix(sh.paramId[PH_MATRIX_VIEW], (D3DXMATRIX *)&transform);
		}
		// 描画開始
		{
			HRESULT hr;
			hr = sh.d3deff->SetTechnique(technique);
			if (FAILED(hr)) {
				Log_errorf("E_HLSL_SHADER: Error at SetTechnique: Err=%s", DX_GetErrorString(hr).c_str());
			}
			sh.d3deff->Begin(NULL, 0);
			hr = sh.d3deff->BeginPass(0);
			Log_assert(SUCCEEDED(hr));
		}
	}
}
static void DXShader_end(DXShader &sh) {
	if (sh.d3deff) {
		sh.d3deff->EndPass();
		sh.d3deff->End();
	}
}
static void DXShader_setInt(DXShader &sh, const char *name, const int *values, int count) {
	Log_assert(sh.d3deff);
	D3DXHANDLE handle = sh.d3deff->GetParameterByName(NULL, name);
	sh.d3deff->SetIntArray(handle, values, count);
}
static void DXShader_setFloat(DXShader &sh, const char *name, const float *values, int count) {
	Log_assert(sh.d3deff);
	D3DXHANDLE handle = sh.d3deff->GetParameterByName(NULL, name);
	sh.d3deff->SetFloatArray(handle, values, count);
}
static void DXShader_setTexture(DXShader &sh, const char *name, IDirect3DTexture9 *d3dtex) {
	Log_assert(sh.d3deff);
	D3DXHANDLE handle = sh.d3deff->GetParameterByName(NULL, name);
	sh.d3deff->SetTexture(handle, d3dtex);
	DX9_checkRenderIOTexture(sh.d3ddev, d3dtex);
}
static void DXShader_onDeviceLost(DXShader &sh) {
	if (sh.d3deff) {
		sh.d3deff->OnLostDevice();
	}
}
static void DXShader_onDeviceReset(DXShader &sh) {
	if (sh.d3deff) {
		sh.d3deff->OnResetDevice();
	}
}
#pragma endregion // DXShader


#pragma region D3D9Graphics
class D3D9Graphics {
	struct _DXSurfItem {
		_DXSurfItem() {
			color_surf = NULL;
			depth_surf = NULL;
		}
		IDirect3DSurface9 *color_surf;
		IDirect3DSurface9 *depth_surf;
	};
	Matrix4 projection_matrix_;
	Matrix4 transform_matrix_;
	std::vector<Matrix4> transform_stack_;
	std::vector<_DXSurfItem> rendertarget_stack_;
	std::vector<IVideoCallback *> cblist_;
	std::vector<IDirect3DStateBlock9 *> d3dblocks_;
	std::unordered_map<TEXID , DXTex> texlist_;
	std::unordered_map<SHADERID , DXShader> shaderlist_;
	std::unordered_map<VERTEXBUFFERID, DXVB> vblist_;
	std::unordered_map<INDEXBUFFERID, DXIB> iblist_;
	D3DCAPS9 devcaps_;
	HWND hWnd_;
	D3DPRESENT_PARAMETERS d3dpp_;
	IDirect3D9 *d3d9_;
	IDirect3DDevice9 *d3ddev_;
	std::recursive_mutex mutex_;
	int drawcalls_;
	int newid_;
	int drawbuf_maxvert_;
	bool drawbuf_using_index_;
	bool shader_available_;
public:
	D3D9Graphics() {
		initInit(false);
	}
	bool isInitialized() const {
		return d3d9_ != NULL;
	}
	void initInit(bool check_flag) {
		hWnd_ = NULL;
		ZeroMemory(&d3dpp_, sizeof(d3dpp_));
		ZeroMemory(&devcaps_, sizeof(devcaps_));
		d3d9_ = NULL;
		d3ddev_ = NULL;
		newid_ = 0;
		drawcalls_ = 0;
		drawbuf_maxvert_ = 0;
		drawbuf_using_index_ = false;
		shader_available_ = false;
		if (check_flag) {
			if (g_graphic_flags.use_square_texture_only) {
				Log_warningf(u8"use_square_texture_only スイッチにより、テクスチャサイズは正方形のみに制限されました");
			}
			if (g_graphic_flags.limit_texture_size > 0) {
				Log_warningf(u8"limit_texture_size スイッチにより、テクスチャサイズが %d 以下に制限されました", g_graphic_flags.limit_texture_size);
			}
			if (g_graphic_flags.disable_pixel_shader) {
				Log_warningf(u8"disable_pixel_shader によりプログラマブルシェーダーが無効になりました");
			}
		}
	}
	bool initD3D9(IDirect3D9 *d3d9) {
		Log_trace;
		Log_assert(d3d9_ == NULL);
		if (d3d9) {
			// 与えられた IDirect3D9 を使う
			Log_trace;
			d3d9_ = d3d9;
			d3d9_->AddRef();
			return true;
		}
		d3d9_ = DX9_init();
		return d3d9_ != NULL;
	}
	bool initD3DDevice9(IDirect3DDevice9 *d3ddev) {
		if (d3d9_ == NULL) return false;
		Log_assert(d3ddev_ == NULL);
		shader_available_ = false;

		if (d3ddev) {
			// 与えられた IDirect3DDevice9 を使う
			d3ddev_ = d3ddev;
			d3ddev_->AddRef();
		
		} else {
			// 新しい IDirect3DDevice9 を作成する
			d3ddev_ = DX9_initDevice(d3d9_, hWnd_, &d3dpp_);
		}
		if (d3ddev_) {
			// デバイス能力を取得
			d3ddev_->GetDeviceCaps(&devcaps_);

			// 実際に適用されたプレゼントパラメータを表示
			DX9_printPresentParameter("Present Parameters (Applied)", d3dpp_);

			// デバイス能力の取得を表示
			DX9_printDeviceCaps(d3ddev_);
		}
		return d3ddev_ != NULL;
	}
	void setupDeviceStates() {
		if (d3ddev_ == NULL) return;
		d3ddev_->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		d3ddev_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		d3ddev_->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		d3ddev_->SetRenderState(D3DRS_LIGHTING, FALSE);
		d3ddev_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		d3ddev_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		d3ddev_->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		d3ddev_->SetRenderState(D3DRS_ALPHAREF, 8); 
		d3ddev_->SetRenderState(D3DRS_BLENDOP,  D3DBLENDOP_ADD);
		d3ddev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		d3ddev_->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

		// スペキュラーは頂点カラーから外したので、デフォルトではOFFにしておく。
		// マテリアルによるスペキュラ指定はSetTextureStageStateでの指定を参照せよ
		d3ddev_->SetRenderState(D3DRS_SPECULARENABLE, FALSE); 

		d3ddev_->SetTexture(0, NULL);
		d3ddev_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		d3ddev_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		d3ddev_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		d3ddev_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		d3ddev_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		d3ddev_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		d3ddev_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		D3DMATERIAL9 material;
		memset(&material, 0, sizeof(D3DMATERIAL9));
		d3ddev_->SetMaterial(&material);
		D3DXMATRIX matrix;
		D3DXMatrixIdentity(&matrix);
		d3ddev_->SetTransform(D3DTS_WORLD, &matrix);
		d3ddev_->SetTransform(D3DTS_VIEW, &matrix);
		d3ddev_->SetTransform(D3DTS_PROJECTION, &matrix);
	}
	bool checkCaps(std::string *message_u8) {
		Log_assert(d3ddev_);
		
		std::string text_u8;
		bool ok = true;

		// テクスチャ
		Log_trace;
		{
			int reqsize = g_graphic_flags.max_texture_size_require;
			if (reqsize > 0) {
				if (DX9_getMaxTextureWidth(devcaps_) < reqsize || DX9_getMaxTextureHeight(devcaps_) < reqsize) {
					const char *msg_u8 = u8"利用可能なテクスチャサイズが小さすぎます";
					Log_errorf("E_LOW_SPEC: TEXTURE SIZE %dx%d NOT SUPPORTED -- %s", reqsize, reqsize, msg_u8);
					text_u8 += K_sprintf("- %s\n", msg_u8);
					ok = false;
				}
			}
			if (DX9_hasD3DPTEXTURECAPS_SQUAREONLY(devcaps_)) {
				const char *msg_u8 = u8"非正方形のテクスチャをサポートしていません";
				Log_errorf("E_LOW_SPEC: SQUARE TEXTURE ONLY -- %s", msg_u8);
				text_u8 += K_sprintf("- %s\n", msg_u8);
				ok = false;
			}
			if ((devcaps_.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) == 0) {
				const char *msg_u8 = u8"テクスチャステージごとの定数色指定をサポートしていません";
				Log_errorf("E_LOW_SPEC: PERSTAGECONSTANT NOT SUPPORTED -- %s", msg_u8);
				text_u8 += K_sprintf("- %s\n", msg_u8);
				ok = false;
			}
		}

		// シェーダー
		Log_trace;
		{
			shader_available_ = true;

			int vs_major_ver = D3DSHADER_VERSION_MAJOR(devcaps_.VertexShaderVersion);
			if (vs_major_ver < 2) {
				const char *msg_u8 = u8"バーテックスシェーダーをサポートしていないか、バージョンが古すぎます";
				Log_warningf("E_NO_VERTEX_SHADER: %s", msg_u8);
				text_u8 += K_sprintf("- %s\n", msg_u8);
				shader_available_ = false;
			}

			int ps_major_ver = D3DSHADER_VERSION_MAJOR(devcaps_.PixelShaderVersion);
			if (ps_major_ver < 2) {
				const char *msg_u8 = u8"ピクセルシェーダーをサポートしていないか、バージョンが古すぎます";
				Log_warningf("E_NO_VERTEX_SHADER: %s", msg_u8);
				text_u8 += K_sprintf("- %s\n", msg_u8);
				shader_available_ = false;
			}

			if (g_graphic_flags.disable_pixel_shader) {
				const char *msg_u8 = u8"disable_pixel_shader スイッチにより、プログラマブルシェーダーが無効になりました";
				Log_warningf("W_SHADER_DISABLED: %s", msg_u8);
				text_u8 += K_sprintf("- %s\n", msg_u8);
				shader_available_ = false;
			}
		}

		// 描画性能
		Log_trace;
		{
			if ((devcaps_.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) == 0) {
				const char *msg_u8 = u8"セパレートアルファブレンドに対応していません";
				Log_errorf("E_LOW_SPEC: NO SEPARATE ALPHA BLEND -- %s", msg_u8);
				text_u8 += K_sprintf("- %s\n", msg_u8);
				ok = false;
			}
			if ((int)devcaps_.MaxStreams < SYSTEMREQ_MAX_VERTEX_ELMS) {
				const char *msg_u8 = u8"頂点ストリーム数が足りません";
				Log_errorf("E_LOW_SPEC: NUMBER OF VERTEX STREAMS %d NOT SUPPORTED -- %s", SYSTEMREQ_MAX_VERTEX_ELMS, msg_u8);
				text_u8 += K_sprintf("- %s\n", msg_u8);
				ok = false;
			}
		}

		if (message_u8) {
			*message_u8 = text_u8;
		}
		return ok;
	}
	bool setup(HWND window_handle, IDirect3D9 *d3d9, IDirect3DDevice9 *d3ddev9) {
		Log_trace;
		// 初期化
		ZeroMemory(&d3dpp_, sizeof(d3dpp_));
		d3d9_ = NULL;
		d3ddev_ = NULL;
		projection_matrix_ = Matrix4();
		transform_matrix_ = Matrix4();
		transform_stack_.clear();
		// ウィンドウハンドル。NULLが指定された場合はこのスレッドに関連付けられているアクティブウィンドウを使う
		if (window_handle) {
			hWnd_ = window_handle;
		} else {
			hWnd_ = ::GetActiveWindow();
		}
		// Direct3D9 オブジェクト。NULL が指定された場合は新規作成する
		if (! initD3D9(d3d9)) {
			return false;
		}
		// Direct3DDevice9 オブジェクト。NULL が指定された場合は新規作成する
		if (! initD3DDevice9(d3ddev9)) {
			return false;
		}
		// レンダーステートを初期化
		setupDeviceStates();
		// デバイスが必要条件を満たしているかどうかを返す
		// （デバイスの作成に失敗したわけではないので、一応成功したとみなす）
		std::string msg_u8;
		if (! checkCaps(&msg_u8)) {
			std::string text;
			text  = u8"【注意】\n";
			text += u8"お使いのパソコンは、必要なグラフィック性能を満たしていません。\n";
			text += u8"このまま起動すると、一部あるいは全ての画像が正しく表示されない可能性があります\n";
			text += u8"\n";
			text += msg_u8;
			Platform::dialog(text.c_str());
		}
		return true;
	}
	void shutdown() {
		LOCK_GUARD(mutex_);
		for (auto it=texlist_.begin(); it!=texlist_.end(); ++it) {
			DXTex &tex = it->second;
			DXTex_destroyD3DObjects(tex);
		}
		transform_stack_.clear();
		texlist_.clear();
		Log_assert(d3dblocks_.empty()); // pushRenderState と popRenderState が等しく呼ばれていること
		d3dblocks_.clear();
		DX9_REL(d3ddev_);
		DX9_REL(d3d9_);
		initInit(false);
	}
	~D3D9Graphics() {
		shutdown();
	}
	void addCallback(IVideoCallback *h) {
		LOCK_GUARD(mutex_);
		cblist_.push_back(h);
	}
	void removeCallback(IVideoCallback *h) {
		LOCK_GUARD(mutex_);
		auto it = std::find(cblist_.begin(), cblist_.end(), h);
		if (it != cblist_.end()) {
			cblist_.erase(it);
		}
	}

	#pragma region texture
	TEXID createTexture2D(int w, int h) {
		Log_assert(d3ddev_);
		D3DSURFACE_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width  = w;
		desc.Height = h;
		desc.Format = D3DFMT_A8R8G8B8;
		desc.Usage  = 0;
		desc.Pool   = D3DPOOL_MANAGED;
		//
		DXTex tex;
		tex.d3ddev = d3ddev_;
		tex.d3ddev->GetDeviceCaps(&tex.caps);
		DXTex_createD3DObjects(tex, &desc);
		//
		TEXID id = (TEXID)(++newid_);
		{
			LOCK_GUARD(mutex_);
			texlist_[id] = tex;
		}
		return id;
	}
	TEXID createTexture2D(const KImage &image) {
		if (image.empty()) return NULL;
		const int imgW = image.getWidth();
		const int imgH = image.getHeight();

		// テクスチャを作成
		TEXID tex = createTexture2D(imgW, imgH);
		if (tex) {
			mo__FancyWriteImage(tex, image);
		}
		return tex;
	}
	TEXID createRenderTexture(int w, int h) {
		Log_assert(d3ddev_);
		D3DSURFACE_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width  = w;
		desc.Height = h;
		desc.Format = D3DFMT_A8R8G8B8;
		desc.Usage  = D3DUSAGE_RENDERTARGET;
		desc.Pool   = D3DPOOL_DEFAULT;
		//
		DXTex tex;
		tex.d3ddev = d3ddev_;
		tex.d3ddev->GetDeviceCaps(&tex.caps);
		DXTex_createD3DObjects(tex, &desc);
		//
		TEXID id = (TEXID)(++newid_);
		{
			LOCK_GUARD(mutex_);
			texlist_[id] = tex;
		}
		return id;
	}
	void deleteTexture(TEXID tex) {
		LOCK_GUARD(mutex_);
		auto it = texlist_.find(tex);
		if (it != texlist_.end()) {
			DXTex_destroyD3DObjects(it->second);
			texlist_.erase(it);
		}
	}
	bool getTextureDesc(TEXID tex, TexDesc *desc) {
		LOCK_GUARD(mutex_);
		auto it = texlist_.find(tex);
		if (it != texlist_.end()) {
			if (desc) {
				DXTex_getDesc(it->second, desc);
			}
			return true;
		}
		return false;
	}
	void * lockTexture(TEXID tex) {
		LOCK_GUARD(mutex_);
		auto it = texlist_.find(tex);
		if (it != texlist_.end()) {
			return DXTex_lock(it->second);
		}
		return NULL;
	}
	void unlockTexture(TEXID tex) {
		LOCK_GUARD(mutex_);
		auto it = texlist_.find(tex);
		if (it != texlist_.end()) {
			return DXTex_unlock(it->second);
		}
	}
	KImage exportTextureImage(TEXID tex, int channel) {
		LOCK_GUARD(mutex_);
		auto it = texlist_.find(tex);
		if (it != texlist_.end()) {
			return DXTex_getImage(it->second, channel);
		}
		return KImage();
	}
	void writeImageToTexture(TEXID tex, const KImage &image, float u, float v) {
		LOCK_GUARD(mutex_);
		auto it = texlist_.find(tex);
		if (it != texlist_.end()) {
			DXTex_writeImage(it->second, image, u, v);
		}
	}
	void blit(TEXID dest, TEXID src, Material *mat) {
		blitEx(dest, src, mat, NULL, NULL);
	}
	void blitEx(TEXID dest, TEXID src, Material *mat, const RectF *src_rect, const RectF *dst_rect) {
		Log_assert(src);
		Log_assert(dest != src);
		Log_assert(d3ddev_);
		if (dest) {
			pushRenderTarget(dest);
		}
		// ブレンドモード
		if (mat) {
			mat->texture = src;
			setProjection(Matrix4());
			setTransform(Matrix4());
			beginShader(mat->shader, mat);
		} else {
			// 変換済み頂点 XYZEHW で描画するため行列設定は不要
			// d3ddev_->SetTransform(D3DTS_PROJECTION, ...);
			// d3ddev_->SetTransform(D3DTS_VIEW, ...);
			Material defmat;
			defmat.texture = src;
			defmat.blend = Blend_One;
			DX9_setFilter(d3ddev_, defmat.filter);
			setProjection(Matrix4());
			setTransform(Matrix4());
			applyMaterialToFixedShader(&defmat);

		}
		// ポリゴン
		struct _Vert {
			Vec4 p; // XYZEHW
			Vec2 t; // UV
		};
		_Vert vertex[4];

		float x0, y0, x1, y1;
		if (dst_rect) {
			// 出力 rect が指定されている
			x0 = dst_rect->xmin;
			y0 = dst_rect->ymin;
			x1 = dst_rect->xmax;
			y1 = dst_rect->ymax;
		} else {
			// 出力 rect が省略されている。ビューポート全体（≒レンダーターゲット全体）に描画する
			int x, y, w, h;
			getViewport(&x, &y, &w, &h);
			x0 = (float)x;
			y0 = (float)y;
			x1 = (float)x + w;
			y1 = (float)y + h;
		}
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			vertex[0].p = Vec4(x0-HALF_PIXEL, y0-HALF_PIXEL, 0.0f, 1.0f); // ピクセル単位で指定することに注意
			vertex[1].p = Vec4(x1-HALF_PIXEL, y0-HALF_PIXEL, 0.0f, 1.0f);
			vertex[2].p = Vec4(x0-HALF_PIXEL, y1-HALF_PIXEL, 0.0f, 1.0f);
			vertex[3].p = Vec4(x1-HALF_PIXEL, y1-HALF_PIXEL, 0.0f, 1.0f);
		} else {
			vertex[0].p = Vec4(x0, y0, 0.0f, 1.0f); // ピクセル単位で指定することに注意
			vertex[1].p = Vec4(x1, y0, 0.0f, 1.0f);
			vertex[2].p = Vec4(x0, y1, 0.0f, 1.0f);
			vertex[3].p = Vec4(x1, y1, 0.0f, 1.0f);
		}

		float u0, v0, u1, v1;
		if (src_rect) {
			// 入力 rect が指定されている。ピクセル単位で指定されているものとして、UV範囲に直す
			TexDesc desc;
			memset(&desc, 0, sizeof(desc));
			getTextureDesc(src, &desc);
			float src_w = (float)desc.w;
			float src_h = (float)desc.h;
			u0 = src_rect->xmin / src_w;
			v0 = src_rect->ymin / src_h;
			u1 = src_rect->xmax / src_w;
			v1 = src_rect->ymax / src_h;
		} else {
			// 入力 rect が省略されている。入力テクスチャ全体を指定する
			u0 = 0.0f;
			v0 = 0.0f;
			u1 = 1.0f;
			v1 = 1.0f;
		}
		vertex[0].t = Vec2(u0, v0);
		vertex[1].t = Vec2(u1, v0);
		vertex[2].t = Vec2(u0, v1);
		vertex[3].t = Vec2(u1, v1);

		// 描画
		d3ddev_->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
		d3ddev_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &vertex[0], sizeof(_Vert));
		// 後始末
		if (mat) {
			endShader(mat->shader, mat);
		}
		if (dest) {
			popRenderTarget();
		}
	}
	void fill(TEXID target, const Color &color, Video::ColorWriteMask mask) {
		Log_assert(d3ddev_);
		if (mask == Video::MASK_RGBA) {
			TexDesc desc;
			memset(&desc, 0, sizeof(desc));
			getTextureDesc(target, &desc);
			IDirect3DTexture9 *d3dtex = (IDirect3DTexture9 *)desc.d3dtex9;
			IDirect3DSurface9 *d3dsurf;
			if (d3dtex && d3dtex->GetSurfaceLevel(0, &d3dsurf) == S_OK) {
				d3ddev_->ColorFill(d3dsurf, NULL, Color32(color).toUInt32());
				DX9_REL(d3dsurf);
			}
			return;
		}
		// ターゲット指定
		pushRenderTarget(target);
		// マスク設定
		DWORD oldmask;
		d3ddev_->GetRenderState(D3DRS_COLORWRITEENABLE, &oldmask);
		setColorWriteMask(mask);
		// ポリゴン
		struct _Vert {
			Vec4  p; // XYZEHW
			DWORD c; // Color
		};
		_Vert vertex[4];
		{
			const DWORD diffuse = Color32(color).toUInt32();
			int x, y, w, h;
			getViewport(&x, &y, &w, &h);
			float x0 = (float)x;
			float y0 = (float)y;
			float x1 = (float)x + w;
			float y1 = (float)y + h;
			if (MO_VIDEO_ENABLE_HALF_PIXEL) {
				vertex[0].p = Vec4(x0-HALF_PIXEL, y0-HALF_PIXEL, 0.0f, 1.0f); // ピクセル単位で指定することに注意
				vertex[1].p = Vec4(x1-HALF_PIXEL, y0-HALF_PIXEL, 0.0f, 1.0f);
				vertex[2].p = Vec4(x0-HALF_PIXEL, y1-HALF_PIXEL, 0.0f, 1.0f);
				vertex[3].p = Vec4(x1-HALF_PIXEL, y1-HALF_PIXEL, 0.0f, 1.0f);
			} else {
				vertex[0].p = Vec4(x0, y0, 0.0f, 1.0f); // ピクセル単位で指定することに注意
				vertex[1].p = Vec4(x1, y0, 0.0f, 1.0f);
				vertex[2].p = Vec4(x0, y1, 0.0f, 1.0f);
				vertex[3].p = Vec4(x1, y1, 0.0f, 1.0f);
			}
			vertex[0].c = diffuse;
			vertex[1].c = diffuse;
			vertex[2].c = diffuse;
			vertex[3].c = diffuse;
		}
		// 描画
		d3ddev_->SetTexture(0, NULL);
		d3ddev_->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
		d3ddev_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &vertex[0], sizeof(_Vert));
		// マスク戻す
		d3ddev_->SetRenderState(D3DRS_COLORWRITEENABLE, oldmask);
		// ターゲット戻す
		popRenderTarget();
	}
	#pragma endregion // texture

	#pragma region vertex buffer
	VERTEXBUFFERID createVertices() {
		Log_assert(d3ddev_);
		DXVB obj;
		DXVB_create(obj, d3ddev_);
		VERTEXBUFFERID id = (VERTEXBUFFERID)(++newid_);
		{
			LOCK_GUARD(mutex_);
			vblist_[id] = obj;
		}
		return id;
	}
	void deleteVertices(VERTEXBUFFERID id) {
		LOCK_GUARD(mutex_);
		auto it = vblist_.find(id);
		if (it != vblist_.end()) {
			DXVB_destroy(it->second);
			vblist_.erase(it);
		}
	}
	void clearVertices(VERTEXBUFFERID id) {
		LOCK_GUARD(mutex_);
		auto it = vblist_.find(id);
		if (it != vblist_.end()) {
			DXVB_clear(it->second);
		}
	}
	Vertex * lockVertices(VERTEXBUFFERID id, int offset, int count) {
		LOCK_GUARD(mutex_);
		auto it = vblist_.find(id);
		if (it != vblist_.end()) {
			return DXVB_lock(it->second, offset, count);
		}
		return NULL;
	}
	void unlockVertices(VERTEXBUFFERID id) {
		LOCK_GUARD(mutex_);
		auto it = vblist_.find(id);
		if (it != vblist_.end()) {
			DXVB_unlock(it->second);
		}
	}
	void setVertices(VERTEXBUFFERID id) {
		LOCK_GUARD(mutex_);
		Log_assert(d3ddev_);
		auto it = vblist_.find(id);
		if (it != vblist_.end()) {
			DXVB &obj = it->second;
			d3ddev_->SetStreamSource(0, obj.vb, 0, sizeof(Vertex));
			d3ddev_->SetFVF(DX9_FVF_VERTEX);
			drawbuf_maxvert_ = obj.num_used;
		}
	}
	bool getVertexDesc(VERTEXBUFFERID id, BufDesc *desc) {
		LOCK_GUARD(mutex_);
		auto it = vblist_.find(id);
		if (it != vblist_.end()) {
			if (desc) {
				DXVB_getDesc(it->second, desc);
			}
			return true;
		}
		return false;
	}
	bool getVertexAabb(VERTEXBUFFERID id, int offset, int count, Vec3 *mincoord, Vec3 *maxcoord) {
		LOCK_GUARD(mutex_);
		auto it = vblist_.find(id);
		if (it != vblist_.end()) {
			if (DXVB_aabb(it->second, offset, count, mincoord, maxcoord)) {
				return true;
			}
		}
		return false;
	}
	#pragma endregion // vertex buffer

	#pragma region index buffer
	INDEXBUFFERID createIndices() {
		Log_assert(d3ddev_);
		DXIB obj;
		DXIB_create(obj, d3ddev_);
		INDEXBUFFERID id = (INDEXBUFFERID)(++newid_);
		{
			LOCK_GUARD(mutex_);
			iblist_[id] = obj;
		}
		return id;
	}
	void deleteIndices(INDEXBUFFERID id) {
		LOCK_GUARD(mutex_);
		auto it = iblist_.find(id);
		if (it != iblist_.end()) {
			DXIB_destroy(it->second);
			iblist_.erase(it);
		}
	}
	void clearIndices(INDEXBUFFERID id) {
		LOCK_GUARD(mutex_);
		auto it = iblist_.find(id);
		if (it != iblist_.end()) {
			DXIB_clear(it->second);
		}
	}
	int * lockIndices(INDEXBUFFERID id, int offset, int count) {
		LOCK_GUARD(mutex_);
		auto it = iblist_.find(id);
		if (it != iblist_.end()) {
			return DXIB_lock(it->second, offset, count);
		}
		return NULL;
	}
	void unlockIndices(INDEXBUFFERID id) {
		LOCK_GUARD(mutex_);
		auto it = iblist_.find(id);
		if (it != iblist_.end()) {
			DXIB_unlock(it->second);
		}
	}
	void setIndices(INDEXBUFFERID id) {
		Log_assert(d3ddev_);
		if (id) {
			LOCK_GUARD(mutex_);
			auto it = iblist_.find(id);
			if (it != iblist_.end()) {
				DXIB &obj = it->second;
				if (obj.ib) {
					d3ddev_->SetIndices(obj.ib);
					drawbuf_using_index_ = true;
					return;
				}
			}
		}
		d3ddev_->SetIndices(NULL);
		drawbuf_using_index_ = false;
	}
	bool getIndexDesc(INDEXBUFFERID id, BufDesc *desc) {
		LOCK_GUARD(mutex_);
		auto it = iblist_.find(id);
		if (it != iblist_.end()) {
			if (desc) {
				DXIB_getDesc(it->second, desc);
			}
			return true;
		}
		return false;
	}
	#pragma endregion // index buffer

	#pragma region shader
	SHADERID createShaderFromHLSL(const char *code, const Path &name) {
		Log_assert(d3ddev_);
		if (! shader_available_) {
			Log_warningf(u8"プログラマブルシェーダーは利用できません。'%s' はロードされませんでした", name.u8());
			return NULL;
		}

		DXShader sh;
		if (! DXShader_load(sh, d3ddev_, code, name.u8())) {
			return NULL;
		}
		SHADERID id = (SHADERID)(++newid_);
		{
			LOCK_GUARD(mutex_);
			shaderlist_[id] = sh;
		}
		return id;
	}
	void deleteShader(SHADERID s) {
		LOCK_GUARD(mutex_);
		auto it = shaderlist_.find(s);
		if (it != shaderlist_.end()) {
			DXShader_destroy(it->second);
			shaderlist_.erase(it);
		}
	}

	/// シェーダーを適用する。
	/// s シェーダーID. NULL を指定すると既定のシェーダーを使う
	/// material: 適用するマテリアル。NULLを指定すると既定のマテリアルを適用する
	void beginShader(SHADERID s, const Material *material) {
		Log_assert(d3ddev_);
		DXShader shader;
		if (s) {
			LOCK_GUARD(mutex_);
			// 指定されたシェーダーを探す
			auto it = shaderlist_.find(s);
			if (it != shaderlist_.end()) {
				shader = it->second;
			}
		}
		if (shader.d3deff) {
			// シェーダーが見つかった。それを使う
			Log_assert(material);
			DXShader_begin(shader, s, material, projection_matrix_, transform_matrix_);
		} else {
			// NULL が指定されたか、シェーダーが存在しなかった。
			// 固定機能シェーダーを使う
			if (material) {
				DX9_setFilter(d3ddev_, material->filter);
				applyMaterialToFixedShader(material);
			} else {
				DX9_setFilter(d3ddev_, Filter_None);
				applyMaterialToFixedShader(NULL);
			}
			d3ddev_->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX *>(&projection_matrix_));
			d3ddev_->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX *>(&transform_matrix_));
		}
		if (material && material->cb) {
			material->cb->onMaterial_Begin(material);
		}
	}
	void endShader(SHADERID s, const Material *material) {
		if (material && material->cb) {
			material->cb->onMaterial_End(material);
		}
		{
			LOCK_GUARD(mutex_);
			auto it = shaderlist_.find(s);
			if (it != shaderlist_.end()) {
				DXShader_end(it->second);
			}
		}
	}
	void setShaderInt(SHADERID s, const char *name, const int *values, int count) {
		LOCK_GUARD(mutex_);
		auto it = shaderlist_.find(s);
		if (it != shaderlist_.end()) {
			DXShader_setInt(it->second, name, values, count);
		}
	}
	void setShaderFloat(SHADERID s, const char *name, const float *values, int count) {
		LOCK_GUARD(mutex_);
		auto it = shaderlist_.find(s);
		if (it != shaderlist_.end()) {
			DXShader_setFloat(it->second, name, values, count);
		}
	}
	void setShaderTexture(SHADERID s, const char *name, TEXID tex) {
		LOCK_GUARD(mutex_);
		auto it = shaderlist_.find(s);
		if (it != shaderlist_.end()) {
			TexDesc desc;
			memset(&desc, 0, sizeof(desc));
			getTextureDesc(tex, &desc);
			IDirect3DTexture9 *d3dtex = tex ? (IDirect3DTexture9 *)desc.d3dtex9 : NULL;
			DXShader_setTexture(it->second, name, d3dtex);
		}
	}
	bool getShaderDesc(SHADERID s, ShaderDesc *desc) {
		LOCK_GUARD(mutex_);
		auto it = shaderlist_.find(s);
		if (it != shaderlist_.end()) {
			if (desc) {
				DXShader &sh = it->second;
				memset(desc, 0, sizeof(*desc));
				desc->num_technique = sh.desc.Techniques;
				desc->d3dxeff = (intptr_t)sh.d3deff;
				strcpy(desc->type, "HLSL");
			}
			return true;
		}
		return false;
	}
	#pragma endregion // shader

	#pragma region material

	// 固定シェーダーの描画設定を行う。
	// material に NULL を指定した場合はデフォルト設定を適用する
	void applyMaterialToFixedShader(const Material *material) {
		Log_assert(d3ddev_);
		// ブレンド
		if (material) {
			DX9_setBlend(d3ddev_, material->blend);
		} else {
			DX9_setBlend(d3ddev_, Blend_Alpha);
		}

		// 色とテクスチャ
		{
			// ブレンド
			// D3DTA_TFACTOR を使う上での注意
			//
			// https://msdn.microsoft.com/ja-jp/library/cc324251.aspx
			// > ハードウェアによっては、D3DTA_TFACTOR と D3DTA_DIFFUSE の同時使用をサポートしていない場合もある
			//
			Color color = Color::WHITE;
			Color specular = Color::ZERO;
			if (material) {
				color = material->color;
				specular = material->specular;
			}

			// D3DTA_TFACTOR で使われる色を設定
			d3ddev_->SetRenderState(D3DRS_TEXTUREFACTOR, Color32(color).toUInt32());

			if (material && material->texture) {
				// テクスチャが設定されている

				// ステージ0
				// テクスチャ色と頂点カラーを乗算
				if (1) {
					TEXID texture = material->texture;
					TexDesc desc;
					memset(&desc, 0, sizeof(desc));
					getTextureDesc(texture, &desc);
					IDirect3DTexture9 *d3dtex = texture ? (IDirect3DTexture9 *)desc.d3dtex9 : NULL;
					d3ddev_->SetTexture(0, d3dtex);
					d3ddev_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
					d3ddev_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
					d3ddev_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
					d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
					d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
					d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
				}
				// ステージ1
				// 前ステージの結果に定数カラーを乗算
				if (1) {
					d3ddev_->SetTexture(1, NULL);
					d3ddev_->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
					d3ddev_->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
					d3ddev_->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
					d3ddev_->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
					d3ddev_->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
					d3ddev_->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
				}
				// ステージ2
				// 前ステージの結果に定数カラーを加算（スペキュラ）
				if (devcaps_.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) { // D3DTSS_CONSTANT のサポートを確認
					d3ddev_->SetTexture(2, NULL);
					d3ddev_->SetTextureStageState(2, D3DTSS_CONSTANT, Color32(specular).toUInt32()); // D3DTA_CONSTANT で参照される色
					d3ddev_->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_ADD);
					d3ddev_->SetTextureStageState(2, D3DTSS_COLORARG1, D3DTA_CURRENT);
					d3ddev_->SetTextureStageState(2, D3DTSS_COLORARG2, D3DTA_CONSTANT); // <-- D3DTA_CONSTANT が使えないカードが意外と多いので注意
					d3ddev_->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // アルファは変更しない
					d3ddev_->SetTextureStageState(2, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
					d3ddev_->SetTextureStageState(2, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
				}

			} else {
				// テクスチャが設定されていない。
				// 頂点カラー D3DTA_DIFFUSE と、マテリアルによって指定された色 D3DTA_TFACTOR を直接合成する

				// ステージ0
				// 頂点カラーと定数カラーを乗算
				if (1) {
					d3ddev_->SetTexture(0, NULL);
					d3ddev_->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
					d3ddev_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
					d3ddev_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
					d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
					d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
					d3ddev_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
				}
				// ステージ1
				// 前ステージの結果に定数カラーを加算（スペキュラ）
				if (1) {
					d3ddev_->SetTexture(1, NULL);
					d3ddev_->SetTextureStageState(1, D3DTSS_CONSTANT, Color32(specular).toUInt32()); // D3DTA_CONSTANT で参照される色
					d3ddev_->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADD);
					d3ddev_->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
					d3ddev_->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CONSTANT); // <-- D3DTA_CONSTANT が使えないカードが意外と多いので注意
					d3ddev_->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // アルファは変更しない
					d3ddev_->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
					d3ddev_->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
				}
				// ステージ2
				// 使わない
				if (1) {
					d3ddev_->SetTexture(2, NULL);
					d3ddev_->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
					d3ddev_->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				}
			}
		}
	}
	#pragma endregion // material

	#pragma region render state
	void setProjection(const Matrix4 &m) {
		projection_matrix_ = m;
	}
	void pushTransform() {
		LOCK_GUARD(mutex_);
		transform_stack_.push_back(transform_matrix_);
	}
	void setTransform(const Matrix4 &m) {
		transform_matrix_ = m;
	}
	void transform(const Matrix4 &m) {
		transform_matrix_ = m * transform_matrix_;
	}
	void popTransform() {
		LOCK_GUARD(mutex_);
		if (!transform_stack_.empty()) {
			transform_matrix_ = transform_stack_.back();
			transform_stack_.pop_back();
		}
	}
	void pushRenderState() {
		Log_assert(d3ddev_);
		IDirect3DStateBlock9 *block = NULL;
		d3ddev_->CreateStateBlock(D3DSBT_ALL, &block);
		d3dblocks_.push_back(block);
	}
	void popRenderState() {
		if (d3dblocks_.size() > 0) {
			IDirect3DStateBlock9 *block = d3dblocks_.back();
			if (block) {
				block->Apply();
				block->Release();
			}
			d3dblocks_.pop_back();
		}
	}
	void pushRenderTarget(TEXID render_target) {
		LOCK_GUARD(mutex_);
		Log_assert(d3ddev_);
		// 現在のターゲットを退避
		_DXSurfItem item;
		d3ddev_->GetRenderTarget(0, &item.color_surf);
		d3ddev_->GetDepthStencilSurface(&item.depth_surf);
		rendertarget_stack_.push_back(item);

		// ターゲットとして利用可能か調べて、DXTex 情報を得る。
		DXTex *dxtex = NULL;
		if (render_target) {
			LOCK_GUARD(mutex_);
			auto it = texlist_.find(render_target);
			if (it!=texlist_.end() && DXTex_isRenderTarget(it->second)) {
				dxtex = &it->second;
				if (MO_STRICT_CHECK) {
					// カラーバッファと深度バッファのサイズが同じであることを確認する
					D3DSURFACE_DESC cdesc;
					D3DSURFACE_DESC zdesc;
					Log_assert(dxtex->color_surf);
					Log_assert(dxtex->depth_surf);
					dxtex->color_surf->GetDesc(&cdesc);
					dxtex->depth_surf->GetDesc(&zdesc);
					Log_assert(cdesc.Width == zdesc.Width);
					Log_assert(cdesc.Height == zdesc.Height);
				}
			} else {
				// render_target が非NULLなのに、存在しない or レンダーテクスチャではない
				Log_error("E_VIDEO_INVALID_RENDER_TEXTURE");
			}
		}
		
		// ターゲットをスタックに入れる。
		// 整合性を保つため、無効なターゲットが指定された場合は NULL を入れる
		if (dxtex) {
			d3ddev_->SetRenderTarget(0, dxtex->color_surf);
			d3ddev_->SetDepthStencilSurface(dxtex->depth_surf);
		} else {
			d3ddev_->SetRenderTarget(0, NULL);
			d3ddev_->SetDepthStencilSurface(NULL);
		}
	}
	void popRenderTarget() {
		LOCK_GUARD(mutex_);
		Log_assert(d3ddev_);
		// 退避しておいたターゲットを元に戻す
		if (! rendertarget_stack_.empty()) {
			_DXSurfItem item = rendertarget_stack_.back();
			d3ddev_->SetRenderTarget(0, item.color_surf);
			d3ddev_->SetDepthStencilSurface(item.depth_surf);
			DX9_REL(item.color_surf);
			DX9_REL(item.depth_surf);
			rendertarget_stack_.pop_back();
		} else {
			Log_error("E_EMPTY_REDNER_STACK");
		}
	}
	void setViewport(int x, int y, int w, int h) {
		Log_assert(d3ddev_);
		D3DVIEWPORT9 vp;
		vp.X = (DWORD)Num_max(x, 0);
		vp.Y = (DWORD)Num_max(y, 0);
		vp.Width  = (DWORD)Num_max(w, 0);
		vp.Height = (DWORD)Num_max(h, 0);
		vp.MinZ = 0.0f; // MinZ, MaxZ はカメラ空間ではなくスクリーン空間の値であることに注意
		vp.MaxZ = 1.0f;
		d3ddev_->SetViewport(&vp);
	}
	void getViewport(int *x, int *y, int *w, int *h) {
		Log_assert(d3ddev_);
		D3DVIEWPORT9 vp;
		d3ddev_->GetViewport(&vp);
		if (x) *x = (int)vp.X;
		if (y) *y = (int)vp.Y;
		if (w) *w = (int)vp.Width;
		if (h) *h = (int)vp.Height;
	}
	void setColorWriteMask(Video::ColorWriteMask mask) {
		Log_assert(d3ddev_);
		DWORD d3dmask = 0;
		d3dmask |= ((mask & Video::MASK_R) ? D3DCOLORWRITEENABLE_RED   : 0);
		d3dmask |= ((mask & Video::MASK_G) ? D3DCOLORWRITEENABLE_GREEN : 0);
		d3dmask |= ((mask & Video::MASK_B) ? D3DCOLORWRITEENABLE_BLUE  : 0);
		d3dmask |= ((mask & Video::MASK_A) ? D3DCOLORWRITEENABLE_ALPHA : 0);
		d3ddev_->SetRenderState(D3DRS_COLORWRITEENABLE, d3dmask);
	}
	#pragma endregion // render state

	#pragma region device
	bool resetDevice(int w, int h, int fullscreen) {
		LOCK_GUARD(mutex_);
		Log_assert(d3ddev_);
		D3DPRESENT_PARAMETERS new_pp = d3dpp_;
		{
			for (size_t i=0; i<cblist_.size(); i++) {
				IVideoCallback *cb = cblist_[i];
				cb->on_video_device_lost();
			}
			for (auto it=texlist_.begin(); it!=texlist_.end(); ++it) {
				DXTex &obj = it->second;
				DXTex_onDeviceLost(obj);
			}
			for (auto it=shaderlist_.begin(); it!=shaderlist_.end(); ++it) {
				DXShader &obj = it->second;
				DXShader_onDeviceLost(obj);
			}
			for (auto it=vblist_.begin(); it!=vblist_.end(); ++it) {
				DXVB &obj = it->second;
				DXVB_onDeviceLost(obj);
			}
			for (auto it=iblist_.begin(); it!=iblist_.end(); ++it) {
				DXIB &obj = it->second;
				DXIB_onDeviceLost(obj);
			}
		}
		if (w > 0) new_pp.BackBufferWidth = w;
		if (h > 0) new_pp.BackBufferHeight = h;
		if (fullscreen >= 0) new_pp.Windowed = fullscreen ? FALSE : TRUE;
		DX9_printPresentParameter("ResetDevice", new_pp);
		if (! new_pp.Windowed) {
			if (! DX9_checkFullScreenParams(d3d9_, new_pp.BackBufferWidth, new_pp.BackBufferHeight)) {
				Log_errorf("Invalid resolution for fullscreen: Size=%dx%d",
					new_pp.BackBufferWidth, new_pp.BackBufferHeight);
				::InvalidateRect(hWnd_, NULL, TRUE);
				//return false;
				// 回復を試みる
				w = 640;
				h = 480;
				new_pp.BackBufferWidth = w;
				new_pp.BackBufferHeight = h;
				Log_infof("Default resolution setted: Size=%dx%d", w, h);
			}
		}
		HRESULT hr = d3ddev_->Reset(&new_pp);
		if (FAILED(hr)) {
			// このエラーは割とありがちなので、エラーではなく情報レベルでのログにしておく
			Log_infof("E_FAIL_D3D_DEVICE_RESET: %s", DX_GetErrorString(hr).c_str());
			::InvalidateRect(hWnd_, NULL, TRUE);
			return false;
		}
		d3dpp_ = new_pp;
		{
			for (auto it=iblist_.begin(); it!=iblist_.end(); ++it) {
				DXIB &obj = it->second;
				DXIB_onDeviceReset(obj);
			}
			for (auto it=vblist_.begin(); it!=vblist_.end(); ++it) {
				DXVB &obj = it->second;
				DXVB_onDeviceReset(obj);
			}
			for (auto it=shaderlist_.begin(); it!=shaderlist_.end(); ++it) {
				DXShader &obj = it->second;
				DXShader_onDeviceReset(obj);
			}
			for (auto it=texlist_.begin(); it!=texlist_.end(); ++it) {
				DXTex &obj = it->second;
				DXTex_onDeviceReset(obj);
			}
			for (size_t i=0; i<cblist_.size(); i++) {
				IVideoCallback *cb = cblist_[i];
				cb->on_video_device_reset();
			}
		}
		return true;
	}
	bool beginScene() {
		Log_assert(d3ddev_);
		if (d3ddev_->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
			resetDevice(0, 0, -1);
			return false; // リセットの可否に関係なく、このフレームでは描画しないでおく
		}
		d3ddev_->BeginScene();
		setupDeviceStates();
		return true;
	}
	bool endScene() {
		Log_assert(d3ddev_);
		d3ddev_->EndScene();
		HRESULT hr = d3ddev_->Present(NULL, NULL, NULL, NULL);
		if (FAILED(hr)) {
			// このエラーは割とありがちなので、エラーではなく情報レベルでのログにしておく
			Log_infof("E_FAIL_D3D_DEVICE_PRESENT: %s", DX_GetErrorString(hr).c_str());
			return false;
		}
		return true;
	}
	void clearColor(const Color &color) {
		Log_assert(d3ddev_);
		d3ddev_->Clear(0, NULL, D3DCLEAR_TARGET, Color32(color).toUInt32(), 0, 0);
	}
	void clearDepth(float z) {
		Log_assert(d3ddev_);
		d3ddev_->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, z, 0);
	}
	void clearStencil(int s) {
		Log_assert(d3ddev_);
		d3ddev_->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 0, s);
	}
	#pragma endregion // device

	#pragma region draw
	void drawBuf(int offset, int count, Primitive primitive, int base_index_number) {
		Log_assert(d3ddev_);
		Log_assert(offset >= 0);
		Log_assert(count >= 0);
		drawcalls_++;

		// プリミティブ数を計算
		D3DPRIMITIVETYPE d3dpt = D3DPT_TRIANGLELIST;
		int numface = 0;
		switch (primitive) {
		case Primitive_PointList:     d3dpt=D3DPT_POINTLIST;     numface=count; break;
		case Primitive_LineList:      d3dpt=D3DPT_LINELIST;      numface=count / 2; break;
		case Primitive_LineStrip:     d3dpt=D3DPT_LINESTRIP;     numface=count - 1; break;
		case Primitive_TriangleList:  d3dpt=D3DPT_TRIANGLELIST;  numface=count / 3; break;
		case Primitive_TriangleStrip: d3dpt=D3DPT_TRIANGLESTRIP; numface=count - 2; break;
		case Primitive_TriangleFan:   d3dpt=D3DPT_TRIANGLEFAN;   numface=count - 2; break;
		}

		// 描画
		if (numface > 0) {
			if (drawbuf_using_index_) {
				d3ddev_->DrawIndexedPrimitive(d3dpt, base_index_number, 0, drawbuf_maxvert_, offset, numface);
			} else {
				d3ddev_->DrawPrimitive(d3dpt, offset, numface);
			}
		}
	}
	void draw(const Vertex *vertices, int count, Primitive primitive) {
		Log_assert(d3ddev_);
		Log_assert(vertices);
		Log_assert(count >= 0);
		drawcalls_++;

		// プリミティブ数を計算
		D3DPRIMITIVETYPE d3dpt = D3DPT_TRIANGLELIST;
		int numface = 0;
		switch (primitive) {
		case Primitive_PointList:     d3dpt=D3DPT_POINTLIST;     numface=count; break;
		case Primitive_LineList:      d3dpt=D3DPT_LINELIST;      numface=count / 2; break;
		case Primitive_LineStrip:     d3dpt=D3DPT_LINESTRIP;     numface=count - 1; break;
		case Primitive_TriangleList:  d3dpt=D3DPT_TRIANGLELIST;  numface=count / 3; break;
		case Primitive_TriangleStrip: d3dpt=D3DPT_TRIANGLESTRIP; numface=count - 2; break;
		case Primitive_TriangleFan:   d3dpt=D3DPT_TRIANGLEFAN;   numface=count - 2; break;
		}

		// 描画
		if (numface > 0) {
			d3ddev_->SetFVF(DX9_FVF_VERTEX);
			d3ddev_->DrawPrimitiveUP(d3dpt, numface, vertices, sizeof(Vertex));
		}
	}
	void drawIndex(const Vertex *vertices, int vertex_count, const int *indices, int index_count, Primitive primitive) {
		Log_assert(d3ddev_);
		Log_assert(vertices);
		Log_assert(vertex_count >= 0);
		drawcalls_++;

		// プリミティブ数を計算
		D3DPRIMITIVETYPE d3dpt = D3DPT_TRIANGLELIST;
		int numface = 0;
		switch (primitive) {
		case Primitive_PointList:     d3dpt=D3DPT_POINTLIST;     numface=index_count; break;
		case Primitive_LineList:      d3dpt=D3DPT_LINELIST;      numface=index_count / 2; break;
		case Primitive_LineStrip:     d3dpt=D3DPT_LINESTRIP;     numface=index_count - 1; break;
		case Primitive_TriangleList:  d3dpt=D3DPT_TRIANGLELIST;  numface=index_count / 3; break;
		case Primitive_TriangleStrip: d3dpt=D3DPT_TRIANGLESTRIP; numface=index_count - 2; break;
		case Primitive_TriangleFan:   d3dpt=D3DPT_TRIANGLEFAN;   numface=index_count - 2; break;
		}

		// 描画
		if (numface > 0) {
			// https://msdn.microsoft.com/ja-jp/library/ee422176(v=vs.85).aspx
			d3ddev_->SetFVF(DX9_FVF_VERTEX);
			d3ddev_->DrawIndexedPrimitiveUP(d3dpt, 0, vertex_count, numface, indices, D3DFMT_INDEX32, vertices, sizeof(Vertex));
		}
	}
	#pragma endregion // draw

	void command(const char *cmd) {
		Log_assert(d3ddev_);
		if (strcmp(cmd, "wireframe on") == 0) {
			d3ddev_->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
			d3ddev_->SetTexture(0, NULL);
		}
		if (strcmp(cmd, "wireframe off") == 0) {
			d3ddev_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		}
		if (strcmp(cmd, "cull cw") == 0) {
			d3ddev_->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
		}
		if (strcmp(cmd, "cull ccw") == 0) {
			d3ddev_->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}
		if (strcmp(cmd, "cull none") == 0) {
			d3ddev_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		}
	}

	void getParameter(Video::Param param, void *data) {
		switch (param) {
		case Video::HAS_SHADER:
			if (data) {
				int *out = (int *)data;
				*out = shader_available_;
			}
			return;

		case Video::HAS_HLSL:
			if (data) {
				int *out = (int *)data;
				*out = 1; // OK
			}
			return;

		case Video::HAS_GLSL:
			if (data) {
				int *out = (int *)data;
				*out = 0; // not supported
			}
			return;

		case Video::IS_FULLSCREEN:
			if (data) {
				int *out = (int *)data;
				*out = d3dpp_.Windowed ? 0 : 1;
			}
			return;

		case Video::D3DDEV9:
			if (data) {
				IDirect3DDevice9 **out = (IDirect3DDevice9 **)data;
				*out = d3ddev_;
			}
			return;

		case Video::DEVICELOST:
			if (data) {
				int *out = (int *)data;
				if (d3ddev_) {
					HRESULT hr = d3ddev_->TestCooperativeLevel();
					*out = (hr == D3DERR_DEVICENOTRESET) ? 1 : 0;
				} else {
					*out = 0;
				}
			}
			return;

		case Video::VS_VER:
			if (data) {
				int *out = (int *)data;
				if (d3ddev_) {
					out[0] = D3DSHADER_VERSION_MAJOR(devcaps_.VertexShaderVersion);
					out[1] = D3DSHADER_VERSION_MINOR(devcaps_.VertexShaderVersion);
				} else {
					out[0] = 0;
					out[1] = 0;
				}
			}
			return;

		case Video::PS_VER:
			if (data) {
				int *out = (int *)data;
				if (d3ddev_) {
					out[0] = D3DSHADER_VERSION_MAJOR(devcaps_.PixelShaderVersion);
					out[1] = D3DSHADER_VERSION_MINOR(devcaps_.PixelShaderVersion);
				} else {
					out[0] = 0;
					out[1] = 0;
				}
			}
			return;

		case Video::MAXTEXSIZE:
			if (data) {
				int *out = (int *)data;
				if (d3ddev_) {
					out[0] = DX9_getMaxTextureWidth(devcaps_);
					out[1] = DX9_getMaxTextureHeight(devcaps_);
				} else {
					out[0] = 0;
					out[1] = 0;
				}
			}
			return;
	
		case Video::DRAWCALLS:
			if (data) {
				int *out = (int *)data;
				*out = drawcalls_;
				drawcalls_ = 0;
			}
			return;
		}
	}
};
#pragma endregion // D3D9Graphics




#pragma region Video

static D3D9Graphics s_gra;

bool Video::isInitialized() {
	return s_gra.isInitialized();
}
bool Video::init(void *hWnd, void *d3d9, void *d3ddev9) {
	Log_trace;
	Log_assert(IsWindow((HWND)hWnd)); // 有効な HWND であることを確認
	Log_assert(!s_gra.isInitialized());
	s_gra.initInit(true);
	if (s_gra.setup((HWND)hWnd, (IDirect3D9 *)d3d9, (IDirect3DDevice9 *)d3ddev9)) {
		return true;
	}
	s_gra.shutdown();
	return false;
}
void Video::shutdown() {
	s_gra.shutdown();
}
TEXID Video::createTexture2D(int w, int h) {
	return s_gra.createTexture2D(w, h);
}
TEXID Video::createTexture2D(const KImage &image) {
	return s_gra.createTexture2D(image);
}
TEXID Video::createRenderTexture(int w, int h) {
	return s_gra.createRenderTexture(w, h);
}
void Video::deleteTexture(TEXID tex) {
	s_gra.deleteTexture(tex);
}
VERTEXBUFFERID Video::createVertices() {
	return s_gra.createVertices();
}
void Video::deleteVertices(VERTEXBUFFERID id) {
	s_gra.deleteVertices(id);
}
void Video::clearVertices(VERTEXBUFFERID id) {
	s_gra.clearVertices(id);
}
Vertex * Video::lockVertices(VERTEXBUFFERID id, int offset, int count) {
	return s_gra.lockVertices(id, offset, count);
}
void Video::unlockVertices(VERTEXBUFFERID id) {
	s_gra.unlockVertices(id);
}
bool Video::getVertexDesc(VERTEXBUFFERID id, BufDesc *desc) {
	return s_gra.getVertexDesc(id, desc);
}
bool Video::getVertexAabb(VERTEXBUFFERID id, int offset, int count, Vec3 *mincoord, Vec3 *maxcoord) {
	return s_gra.getVertexAabb(id, offset, count, mincoord, maxcoord);
}
void Video::setVertices(VERTEXBUFFERID id) {
	s_gra.setVertices(id);
}

INDEXBUFFERID Video::createIndices() {
	return s_gra.createIndices();
}
void Video::deleteIndices(INDEXBUFFERID id) {
	s_gra.deleteIndices(id);
}
void Video::clearIndices(INDEXBUFFERID id) {
	s_gra.clearIndices(id);
}
int * Video::lockIndices(INDEXBUFFERID id, int offset, int count) {
	return s_gra.lockIndices(id, offset, count);
}
void Video::unlockIndices(INDEXBUFFERID id) {
	s_gra.unlockIndices(id);
}
bool Video::getIndexDesc(INDEXBUFFERID id, BufDesc *desc) {
	return s_gra.getIndexDesc(id, desc);
}
void Video::setIndices(INDEXBUFFERID id) {
	s_gra.setIndices(id);
}


SHADERID Video::createShaderFromHLSL(const char *code, const Path &name) {
	return s_gra.createShaderFromHLSL(code, name);
}
SHADERID Video::createShaderFromGLSL(const char *vscode, const char *fscode, const Path &name) {
	return NULL;
}
void Video::deleteShader(SHADERID s) {
	s_gra.deleteShader(s);
}
void Video::beginShader(SHADERID s, Material *material) {
	s_gra.beginShader(s, material);
}
void Video::endShader(SHADERID s, Material *material) {
	s_gra.endShader(s, material);
}
void Video::beginMaterial(const Material *material) {
	if (material) {
		s_gra.beginShader(material->shader, material);
	} else {
		s_gra.beginShader(NULL, NULL);
	}
}
void Video::endMaterial(const Material *material) {
	if (material) {
		s_gra.endShader(material->shader, material);
	} else {
		s_gra.endShader(NULL, NULL);
	}
}
void Video::setShaderInt(SHADERID s, const char *name, const int *values, int count) {
	s_gra.setShaderInt(s, name, values, count);
}
void Video::setShaderInt(SHADERID s, const char *name, int value) {
	s_gra.setShaderInt(s, name, &value, 1);
}
void Video::setShaderFloat(SHADERID s, const char *name, const float *values, int count) {
	s_gra.setShaderFloat(s, name, values, count);
}
void Video::setShaderFloat(SHADERID s, const char *name, float value) {
	s_gra.setShaderFloat(s, name, &value, 1);
}
void Video::setShaderTexture(SHADERID s, const char *name, TEXID tex) {
	s_gra.setShaderTexture(s, name, tex);
}
bool Video::getShaderDesc(SHADERID s, ShaderDesc *desc) {
	return s_gra.getShaderDesc(s, desc);
}
bool Video::resetDevice(int w, int h, int fullscreen) {
	return s_gra.resetDevice(w, h, fullscreen);
}
bool Video::beginScene() {
	return s_gra.beginScene();
}
bool Video::endScene() {
	return s_gra.endScene();
}
void Video::clearColor(const Color &color) {
	s_gra.clearColor(color);
}
void Video::clearDepth(float z) {
	s_gra.clearDepth(z);
}
void Video::clearStencil(int s) {
	s_gra.clearStencil(s);
}
void Video::setViewport(int x, int y, int w, int h) {
	s_gra.setViewport(x, y, w, h);
}
void Video::getViewport(int *x, int *y, int *w, int *h) {
	s_gra.getViewport(x, y, w, h);
}
void Video::setColorWriteMask(ColorWriteMask mask) {
	s_gra.setColorWriteMask(mask);
}
void Video::blit(TEXID dest, TEXID src, Material *mat) {
	s_gra.blit(dest, src, mat);
}
void Video::blitEx(TEXID dest, TEXID src, Material *mat, const RectF *src_rect, const RectF *dst_rect) {
	s_gra.blitEx(dest, src, mat, src_rect, dst_rect);
}
void Video::fill(TEXID target, const Color &color, ColorWriteMask mask) {
	s_gra.fill(target, color, mask);
}
void Video::addCallback(IVideoCallback *h) {
	s_gra.addCallback(h);
}
void Video::removeCallback(IVideoCallback *h) {
	s_gra.removeCallback(h);
}
void Video::draw(const Vertex *vertices, int count, Primitive primitive) {
	if (vertices) {
		s_gra.draw(vertices, count, primitive);
	}
}
void Video::drawIndex(const Vertex *vertices, int vertex_count, const int *indices, int index_count, Primitive primitive) {
	if (vertices) {
		s_gra.drawIndex(vertices, vertex_count, indices, index_count, primitive);
	}
}
void Video::drawMeshId(int start, int count, Primitive primitive, int base_index_number) {
	s_gra.drawBuf(start, count, primitive, base_index_number);
}
void Video::drawMesh(const KMesh *mesh, int start, int count, Primitive primitive) {
	if (mesh && mesh->getSubMeshCount() > 0) {
		int numi = mesh->getIndexCount();
		int numv = mesh->getVertexCount();
		if (numi > 0) {
			const Vertex *v = mesh->getVertices();
			const int *i = mesh->getIndices(start);
			drawIndex(v, numv, i, count, primitive);
		} else {
			const Vertex *v = mesh->getVertices(start);
			draw(v, count, primitive);
		}
	}
}
void Video::drawMesh(const KMesh *mesh, int submesh_index) {
	if (mesh && mesh->getSubMeshCount() > 0) {
		KSubMesh submesh;
		if (mesh->getSubMesh(submesh_index, &submesh)) {
			drawMesh(mesh, submesh.start, submesh.count, submesh.primitive);
		}
	}
}
void Video::pushRenderState() {
	s_gra.pushRenderState();
}
void Video::popRenderState() {
	s_gra.popRenderState();
}
void Video::setProjection(const Matrix4 &m) {
	s_gra.setProjection(m);
}
void Video::pushTransform() {
	s_gra.pushTransform();
}
void Video::setTransform(const Matrix4 &m) {
	s_gra.setTransform(m);
}
void Video::transform(const Matrix4 &m) {
	s_gra.transform(m);
}
void Video::popTransform() {
	s_gra.popTransform();
}
void Video::pushRenderTarget(TEXID render_target) {
	s_gra.pushRenderTarget(render_target);
}
void Video::popRenderTarget() {
	s_gra.popRenderTarget();
}
void Video::getParameter(Param name, void *data) {
	s_gra.getParameter(name, data);
}
void Video::command(const char *s) {
	s_gra.command(s);
}
bool Video::getTextureSize(TEXID tex, int *w, int *h) {
	TexDesc desc;
	if (getTextureDesc(tex, &desc)) {
		if (w) *w = desc.w;
		if (h) *h = desc.h;
		return true;
	}
	return false;
}
bool Video::getTextureDesc(TEXID tex, TexDesc *desc) {
	return s_gra.getTextureDesc(tex, desc);
}
void Video::fillTextureZero(TEXID tex) {
	TexDesc desc;
	if (getTextureDesc(tex, &desc)) {
		void *ptr = lockTexture(tex);
		if (ptr) {
			memset(ptr, 0, desc.size_in_bytes);
			unlockTexture(tex);
		}
	}
}
void * Video::lockTexture(TEXID tex) {
	return s_gra.lockTexture(tex);
}
void Video::unlockTexture(TEXID tex) {
	return s_gra.unlockTexture(tex);
}
KImage Video::exportTextureImage(TEXID tex, int channel) {
	return s_gra.exportTextureImage(tex, channel);
}
void Video::writeImageToTexture(TEXID tex, const KImage &image, float u, float v) {
	s_gra.writeImageToTexture(tex, image, u, v);
}
Vec2 Video::getTextureUVFromOriginalPoint(TEXID tex, const Vec2 &pixel) {
	TexDesc desc;
	getTextureDesc(tex, &desc);
	Vec2 texsize(desc.w, desc.h);
	return Vec2(
		pixel.x / texsize.x,
		pixel.y / texsize.y
	);
}
/// 元画像のUV単位での座標に対する。現在のテクスチャのUV座標を返す
/// orig_u 元画像の横幅を 1.0 としたときの座標。
/// orig_v 元画像の高さを 1.0 としたときの座標。
/// 戻り値は、現在のテクスチャのサイズを (1.0, 1.0) としたときの座標 (UV値)
Vec2 Video::getTextureUVFromOriginalUV(TEXID tex, const Vec2 &orig_uv) {
	TexDesc desc;
	getTextureDesc(tex, &desc);
	Vec2 texsize(desc.w, desc.h);
	Vec2 orisize(desc.original_w, desc.original_h);
	return Vec2(
		orig_uv.x * orisize.x / texsize.x,
		orig_uv.y * orisize.y / texsize.y
	);
}
#pragma endregion // Video


} // namespace



