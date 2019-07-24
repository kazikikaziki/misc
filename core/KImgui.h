#pragma once
#include "KVideoDef.h"
#include "KPath.h"

#ifndef NO_IMGUI

// Imgui
// https://github.com/ocornut/imgui
// imgui 内部の define が Windows マクロと競合するため、Windows.h よりも先にインクルードする
#include "imgui/imgui.h"



#define ImGui_ID(x)  ((void *)(intptr_t)(x))


namespace mo {

class Matrix4;
class Vec2;
class Vec3;
class Quat;
class RectF;
class Window;

inline ImVec2 ImVec2i(int x, int y) { return ImVec2((float)x, (float)y); }

struct ImGui_SImageParams {
	ImGui_SImageParams() {
		w = 0;
		h = 0;
		u0 = 0.0f;
		v0 = 0.0f;
		u1 = 1.0f;
		v1 = 1.0f;
		color = Color(1, 1, 1, 1);
		border_color = Color(0, 0, 0, 0);
		keep_aspect = true;
	}

	int w;
	int h;
	float u0;
	float v0;
	float u1;
	float v1;
	Color color;
	Color border_color;
	bool keep_aspect;
};
bool ImGui_ButtonRepeat(const char *label, ImVec2 size=ImVec2(0, 0));
bool ImGui_Combo(const char *label, int *current_item, const PathList &items, int height_in_items=-1);
bool ImGui_Combo(const char *label, const PathList &pathlist, const Path &selected, int *out_selected_index);
bool ImGui_DrawSplitter(const char *id, int split_vertically, int thickness, int *size0, int *size1, int min_size0, int min_size1);
bool ImGui_DragQuaternion(const char *label, Quat *q);
int  ImGui_FontCount();
void ImGui_Image(TEXID tex, ImGui_SImageParams *p);
void ImGui_Image(TEXID tex, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,0), const ImVec2& uv1 = ImVec2(1,1), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
void ImGui_HSpace(int size);
void ImGui_PushFont(int index);
void ImGui_PopFont();
void ImGui_VSpace(int size=8);
bool ImGui_Matrix(const char *label, Matrix4 *m);

/// ImGui で日本語を表示するために必要な漢字テーブルを得る。
///
/// ImGui のデフォルトの状態だと2010以前の常用漢字しか正しく表示できない。
/// 2010年に追加された常用漢字およびそれ以外の頻出漢字を ImGui で表示できるようにするため、
/// それらの漢字を含めたテーブルを作成し、それを ImGuiIO::Fonts の AddFontFromMemoryTTF で指定する
///
/// const ImWchar *jpChars = ImGui_JapaneseCharTable();
/// ImFont *font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buf, size, size_pixels, NULL, jpChars);
const ImWchar * ImGui_GetKanjiTable();

/// ImGui のデフォルトフォント (ProggyClean.ttf) を追加する。
/// ImGui::GetIO().Fonts->AddFontDefault() と完全に同じ
void ImGui_AddDefaultFont();

/// ImGui にフォントを追加する。
/// 基本的には ImGui::GetIO().Fonts->AddFontFromMemoryTTF と同じだが、漢字テーブルを自動的に追加する
/// @param data フォントデータ
/// size フォントデータバイト数
/// size_pixels ピクセル単位でのフォントサイズ
ImFont * ImGui_AddFontFromMemoryTTF(const void *data, size_t size, float size_pixels);

/// ImGui にフォントを追加する。
/// 基本的には ImGui::GetIO().Fonts->AddFontFromFileTTF と同じだが、漢字テーブルを自動的に追加する
/// filename フォントファイル名
/// ttc_index TTCフォントからロードする場合、フォント番号
/// size_pixels ピクセル単位でのフォントサイズ
ImFont * ImGui_AddFontFromFileTTF(const Path &filename, int ttc_index, float size_pixels);


bool ImGui_BlendCombo(const char *label, Blend *blend);
bool ImGui_FilterCombo(const char *label, Filter *filter);




enum FontFace {
	FF_MSGOTHIC,    ///< ＭＳ ゴシック (Windows 95 以降で利用可能）
	FF_MSGOTHIC_UI, ///< MS UI Gothic (Windows 98 以降で利用可能）
	FF_MSGOCHIC_P,  ///< ＭＳ Ｐゴシック (Windows 95 以降で利用可能）
	FF_MEIRYO,      ///< メイリオ (Windows Vista 以降で利用可能）
	FF_MEIRYO_UI,   ///< Meiryo UI (Windows 7 以降で利用可能）
};
static const ImVec4 GuiColor_Window         = ImVec4(0.1f, 0.1f, 0.1f, 0.9f);
static const ImVec4 GuiColor_ErrorText      = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
static const ImVec4 GuiColor_WarningText    = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
static const ImVec4 GuiColor_OrangeText     = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
static const ImVec4 GuiColor_OrangeTextDark = ImVec4(1.0f, 0.8f, 0.2f, 0.2f);
static const ImVec4 GuiColor_ActiveText     = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
static const ImVec4 GuiColor_DeactiveText   = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);




/// DirectX を使って ImGui を初期化する。
/// 内部で ImGui::CreateContext を呼び出すため、
/// この関数を呼んだ後でないと他の大部分の ImGui 関数は
/// 使う事はできない（コンテキスト未設定エラーになる）
/// @see ImGuiVideo_Shutdown
bool ImGuiVideo_Init(Window *wnd);

/// ImGuiVideo_Init が呼ばれているかどうか調べる。
/// true を返した場合、ImGui は初期化済みであり、関連関数を正しく使う事ができる
bool ImGuiVideo_IsInitialized();

/// ImGuiVideo_Init と対になる終了関数。
/// 内部で ImGui::DestroyContext を呼び出すため、
/// この関数を呼んだ後に ImGui 関数を使おうとするとコンテキスト未設定エラーになる
void ImGuiVideo_Shutdown();

/// 新しい描画フレームを開始する。
/// ImGui::Textなどの関数は、ImGuiVideo_BeginRender() と ImGuiVideo_EndRender() の間で呼ぶこと。
void ImGuiVideo_BeginRender();

/// ImGui::の描画を終了し、画面を更新する。
void ImGuiVideo_EndRender();

/// 標準的なUI設定を適用する
void ImGuiVideo_LoadDefaultStyle();

/// フォントを追加する。
/// @param filename
///    フォントのファイル名。フォント名ではないので注意すること。
///    指定された名前のファイルがそのまま見つかった場合は、それをロードして使う。
///    そのままの名前では見つからなかった場合、システムフォントのフォルダから検索する。
/// @param ttc_index
///    フォントファイルに複数のフォントが入っている場合、インデックスを指定する。
///    例えば MSGOTHIC.TTC には MS ゴシック体が入っているが、
///    「ＭＳ ゴシック」はインデックス 0、「MS UI Gothic」はインデックス 1、
///    「ＭＳ Ｐゴシック」はインデックス 2 になる。
/// @param size_px
///    フォントサイズ（高さ）をピクセル単位で指定する
/// @return
///    追加したフォントのインデックス番号を返す。
///    フォントの実体は ImGui::GetIO().Fonts->Fonts[index] にある。
///    失敗した場合は -1 を返す
/// @note
///    ImGui はフォントのリロードを推奨していない。
///    一番最初に一度だけ作成しなければならない。
///    デバッグモードでシーンを切り替えたり、リロードを実行したりしても、
///    フォントデータだけは最初にロードしたものをずっと使い続ける。
///
int ImGuiVideo_AddFont(const Path &filename, int ttc_index, int size_px);

/// フォントファイル名ではなく、フォントフェイスIDを指定してフォントを追加する。
int ImGuiVideo_AddFont(FontFace face, int size_px);

/// デフォルトのフォントを指定する。
/// font_index には ImGui のフォント配列のインデックスを指定する。
/// @see ImGuiVideo_AddFont
/// ここで指定したフォントは ImGuiVideo_BeginRender() を呼んだ時に有効になる。
/// UIパーツごとにその場でフォントを変更したい場合は、ImGui_PushFont/ImGui_PopFont を使う
void ImGuiVideo_SetDefaultFont(int font_index);

} // namespace

#endif // !NO_IMGUI
