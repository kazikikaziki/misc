#ifndef NO_IMGUI
#include "KImgui.h"
#include "KLog.h"
#include "KMatrix4.h"
#include "KNum.h"
#include "KPlatform.h"
#include "KQuat.h"
#include "KWindow.h"
#include "KVideo.h"

#if defined(_WIN32) && 1
#	include <d3d9.h>
#	include <d3dx9.h>
#	include <dsound.h>
#	define IMGUI_DX9 1
#else
#	define IMGUI_DX9 0
#endif


#if IMGUI_DX9
#	// ImGui::GetFont が win32api の GetFont マクロと競合するため、
#	// Windows.h よりも前に include する
#	include "imgui/imgui.h"
#	include "imgui/ImGui_impl_dx9.h"
#	include "imgui/ImGui_impl_win32.h"
#endif



#if IMGUI_DX9
// imgui/ImGui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif




namespace ImGui {
	extern void SetItemAllowOverlap(); // 非公開関数なので使えるようにしておく
}


namespace mo {

#pragma region imgui widgets
void ImGui_HSpace(int size) {
	ImGui::SameLine(0, (float)size);
}
void ImGui_VSpace(int size) {
	ImGui::Dummy(ImVec2(1.0f, (float)size));
}
bool ImGui_Combo(const char *label, int *current_item, const PathList &items, int height_in_items) {
	std::vector<const char *> items_u8_ptr(items.size());
	for (size_t i=0; i<items.size(); i++) {
		items_u8_ptr[i] = items[i].u8();
	}
	return ImGui::Combo(label, current_item, items_u8_ptr.data(), items_u8_ptr.size(), height_in_items);
}
bool ImGui_Combo(const char *label, const PathList &pathlist, const Path &selected, int *out_selected_index) {
	if (pathlist.empty()) return false;
	int sel = -1;
	for (size_t i=0; i<pathlist.size(); i++) {
		if (pathlist[i].compare(selected) == 0) {
			sel = (int)i;
		}
	}
	if (ImGui_Combo(label, &sel, pathlist)) {
		*out_selected_index = sel;
		return true;
	}
	*out_selected_index = sel;
	return false;
}
bool ImGui_BlendCombo(const char *label, Blend *blend) {
	static const char * s_table[Blend_EnumMax+1] = {
		"One",    // Blend_One
		"Add",    // Blend_Add
		"Alpha",  // Blend_Alpha
		"Sub",    // Blend_Sub
		"Mul",    // Blend_Mul
		"Screen", // Blend_Screen
		"Max",    // Blend_Max
		NULL,     // (sentinel)
	};
	Log_assert(blend);
	Log_assert(s_table[Blend_EnumMax] == NULL);
	int val = *blend;
	if (ImGui::Combo(label, &val, s_table, Blend_EnumMax)) {
		*blend = (Blend)val;
		return true;
	}
	return false;
}

bool ImGui_FilterCombo(const char *label, Filter *filter) {
	static const char * s_table[Filter_EnumMax+1] = {
		"None",        // Filter_None,
		"Linear",      // Filter_Linear
		"RichA",       // Filter_QuadA
		"RichB",       // Filter_QuadB
		"Anisotropic", // Filter_Anisotropic
		NULL,          // (sentinel)
	};
	Log_assert(filter);
	Log_assert(s_table[Filter_EnumMax] == NULL);
	int val = *filter;
	if (ImGui::Combo(label, &val, s_table, Filter_EnumMax)) {
		*filter = (Filter)val;
		return true;
	}
	return false;
}
bool ImGui_Matrix(const char *label, Matrix4 *m) {
	bool b = false;
	ImGui::Text("%s", label);
	ImGui::Indent();
	b |= ImGui::DragFloat4("##0", m->m +  0, 0.1f);
	b |= ImGui::DragFloat4("##1", m->m +  4, 0.1f);
	b |= ImGui::DragFloat4("##2", m->m +  8, 0.1f);
	b |= ImGui::DragFloat4("##3", m->m + 12, 0.1f);
	if (ImGui::Button("E")) {
		*m = Matrix4();
		b = true;
	}
	ImGui::Unindent();
	return b;
}




bool ImGui_DragQuaternion(const char *label, Quat *q) {
	bool mod = false;
	if (ImGui::DragFloat4(label, (float *)q, 0.1f)) { mod = true; }
	float deg = 5;
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("X+")) { *q *= Quat(Vec3(1, 0, 0),  deg); mod=true; } ImGui::SameLine();
	if (ImGui::Button("X-")) { *q *= Quat(Vec3(1, 0, 0), -deg); mod=true; } ImGui::SameLine();
	ImGui_HSpace(8); ImGui::SameLine();
	if (ImGui::Button("Y+")) { *q *= Quat(Vec3(0, 1, 0),  deg); mod=true; } ImGui::SameLine();
	if (ImGui::Button("Y-")) { *q *= Quat(Vec3(0, 1, 0), -deg); mod=true; } ImGui::SameLine();
	ImGui_HSpace(8); ImGui::SameLine();
	if (ImGui::Button("Z+")) { *q *= Quat(Vec3(0, 0, 1),  deg); mod=true; } ImGui::SameLine();
	if (ImGui::Button("Z-")) { *q *= Quat(Vec3(0, 0, 1), -deg); mod=true; } ImGui::SameLine();
	ImGui_HSpace(8); ImGui::SameLine();
	if (ImGui::Button("0") ) { *q = Quat(); mod=true; }
	ImGui::PopButtonRepeat();
	if (mod) {
		*q = q->getNormalized();
	}
	return mod;
}
bool ImGui_DrawSplitter(const char *id, int split_vertically, int thickness, int *size0, int *size1, int min_size0, int min_size1) {
	ImGui::PushID(id);
	// https://github.com/ocornut/imgui/issues/319
	ImVec2 backup_pos = ImGui::GetCursorPos();
	if (split_vertically) {
		ImGui::SetCursorPosY(backup_pos.y + *size0);
	} else {
		ImGui::SetCursorPosX(backup_pos.x + *size0);
	}
	ImVec2 splitter_size;
	if (split_vertically) {
		// 上下分割
		splitter_size.x = -1.0f;
		splitter_size.y = thickness * 1.0f;
	} else {
		// 左右分割
		splitter_size.x = thickness * 1.0f;
		splitter_size.y = -1.0f;
	}
	ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0, 0, 0, 0)); // We don't draw while active/pressed because as we move the panes the splitter button will be 1 frame late
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.1f));
	ImGui::Button("##splitter", splitter_size);
	ImGui::PopStyleColor(3);
	ImGui::SetItemAllowOverlap(); // This is to allow having other buttons OVER our splitter. 

	bool changed = false;
	if (ImGui::IsItemActive()) {
		ImVec2 md = ImGui::GetIO().MouseDelta;
		int mouse_delta = (int)(split_vertically ? md.y : md.x);
		// Minimum pane size
		if (mouse_delta < min_size0 - *size0) mouse_delta = min_size0 - *size0;
		if (mouse_delta > *size1 - min_size1) mouse_delta = *size1 - min_size1;
		// Apply resize
		*size0 += mouse_delta;
		*size1 -= mouse_delta;
		changed = true;
	}
	ImGui::SetCursorPosX(backup_pos.x);
	ImGui::SetCursorPosY(backup_pos.y);
	ImGui::PopID();
	return changed;
}
bool ImGui_ButtonRepeat(const char *label, ImVec2 size) {
	ImGui::PushButtonRepeat(true);
	bool ret = ImGui::Button(label, size);
	ImGui::PopButtonRepeat();
	return ret;
}
void ImGui_Image(TEXID tex, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col) {
	TexDesc desc;
	Video::getTextureDesc(tex, &desc);
	ImTextureID texid = tex ? (ImTextureID)desc.d3dtex9 : 0;
	ImGui::Image(texid, size, uv0, uv1, tint_col, border_col);
}
void ImGui_Image(TEXID tex, ImGui_SImageParams *_p) {
	if (tex == NULL) return;
	TexDesc desc;
	Video::getTextureDesc(tex, &desc);
	ImGui_SImageParams p;
	if (_p) {
		p = *_p;
	}
	int w = (p.w > 0) ? p.w : desc.w;
	int h = (p.h > 0) ? p.h : desc.h;
	if (p.keep_aspect) {
		if (desc.w >= desc.h) {
			h = (int)desc.h * w / desc.w;
		} else {
			w = (int)desc.w * h / desc.h;
		}
	}
	ImGui::Image(
		(ImTextureID)desc.d3dtex9,
		ImVec2((float)w, (float)h),
		ImVec2(p.u0, p.v0),
		ImVec2(p.u1, p.v1),
		ImVec4(p.color.r, p.color.g, p.color.b, p.color.a),
		ImVec4(p.border_color.r, p.border_color.g, p.border_color.b, p.border_color.a)
	);
}
#pragma endregion // imgui widgets


#pragma region imgui font helpers
void ImGui_PushFont(int index) {
	ImGuiIO &io = ImGui::GetIO();
	Log_assert(0 <= index && index < io.Fonts->Fonts.size());
	ImFont *font = io.Fonts->Fonts[index];
	if (font) {
		ImGui::PushFont(font);
	} else {
		ImGui::PushFont(io.Fonts->Fonts[0]); // default font
	}
}
void ImGui_PopFont() {
	ImGui::PopFont();
}
int ImGui_FontCount() {
	ImGuiIO &io = ImGui::GetIO();
	return io.Fonts->Fonts.size();
}
const ImWchar * ImGui_GetKanjiTable() {
	static std::vector<ImWchar> s_list;
	if (!s_list.empty()) {
		return &s_list[0];
	}

	// http://itpro.nikkeibp.co.jp/article/COLUMN/20091209/341831/?rt=nocnt
	static const wchar_t *EXTRA_CHARS = 
		// 2010年に常用漢字に追加された196文字。
		// 一部SJIS非対応の漢字があるので注意。SJISで保存しようとすると文字化けする
		L"挨曖宛嵐畏萎椅彙茨咽淫唄鬱怨媛艶旺岡臆俺苛牙瓦楷潰諧崖蓋"
		L"骸柿顎葛釜鎌韓玩伎亀毀畿臼嗅巾僅錦惧串窟熊詣憬稽隙桁拳鍵"
		L"舷股虎錮勾梗喉乞傲駒頃痕沙挫采塞埼柵刹拶斬恣摯餌鹿　嫉腫" // 「叱」の旧字体は SJIS 範囲外なので除外した
		L"呪袖羞蹴憧拭尻芯腎須裾凄醒脊戚煎羨腺詮箋膳狙遡曽爽痩踪捉"
		L"遜汰唾堆戴誰旦綻緻酎貼嘲捗椎爪鶴諦溺　妬賭藤瞳栃頓貪丼那" // 「填」の旧字体は SJIS 範囲外なので除外した
		L"奈梨謎鍋匂虹捻罵　箸氾汎阪斑眉膝肘訃阜蔽餅璧蔑哺蜂貌　睦" // 「剥」「頬」の旧字体は SJIS 範囲外なので除外した
		L"勃昧枕蜜冥麺冶弥闇喩湧妖瘍沃拉辣藍璃慄侶瞭瑠呂賂弄籠麓脇"
		// それ以外の文字
		L"※牢礫贄杖碧蒼橙鞘←→↑↓";
	
	s_list.clear();
	// ImGui 側で自動設定された日本語文字リストを追加する
	const ImWchar *jp_chars = ImGui::GetIO().Fonts->GetGlyphRangesJapanese();
	for (int i=0; jp_chars[i]; i++) {
		ImWchar imwc = (ImWchar)(jp_chars[i] & 0xFFFF);
		s_list.push_back(imwc);
	}

	// ImGui のリストに無い文字を追加する（2010年に常用漢字に追加された196文字）
	for (int i=0; EXTRA_CHARS[i]; i++) {
		// 2回追加することに注意
		ImWchar imwc = (ImWchar)(EXTRA_CHARS[i] & 0xFFFF);
		s_list.push_back(imwc);
		s_list.push_back(imwc);
	}
	s_list.push_back(0); // sentinel
	return &s_list[0];
}
void ImGui_AddDefaultFont() {
	ImGui::GetIO().Fonts->AddFontDefault();
}
ImFont * ImGui_AddFontFromMemoryTTF(const void *data, size_t size, float size_pixels) {
	Log_assert(data);
	Log_assert(size > 0);
	Log_assert(size_pixels > 0);

	// フォントデータを ImGUI 側で確保したバッファにコピーする。
	// フォントデータの解放は ImGUI 側に任せるため、ローカルなポインタを渡してはいけない
	void *buf = ImGui::MemAlloc(size);
	memcpy(buf, data, size);

	// フォント追加
	const ImWchar *kanji = ImGui_GetKanjiTable();
	ImFont *font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buf, size, size_pixels, NULL, kanji);
//	if (! font->IsLoaded()) return NULL;
	return font;
}
ImFont * ImGui_AddFontFromFileTTF(const Path &filename, int ttc_index, float size_pixels) {
	Log_assert(ttc_index >= 0);
	Log_assert(size_pixels > 0);

	std::string u8 = filename.toUtf8();

	ImFontConfig conf;
	conf.FontNo = ttc_index;

	// この設定にすると日本語フォントがキレイに描画される
	conf.PixelSnapH = true;
	conf.OversampleH = 1;
	conf.OversampleV = 1;

	// フォント追加
	const ImWchar *kanji = ImGui_GetKanjiTable();
	ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(u8.c_str(), size_pixels, &conf, kanji);
//	if (! font->IsLoaded()) return NULL;
	return font;
}
#pragma endregion // imgui font helpers




#pragma region CImgui
class CImgui: public IWindowCallback, public IVideoCallback {
	/// ImGui に関連付けられたウィンドウ
	Window *window_;
	int font_index_;
#if IMGUI_DX9
	IDirect3DDevice9 *d3d9_dev_;
	IDirect3DStateBlock9 *d3d9_block_;
#endif
public:
	CImgui() {
		window_ = NULL;
		font_index_ = 0;
	#if IMGUI_DX9
		d3d9_dev_ = NULL;
		d3d9_block_ = NULL;
	#endif
	}
	virtual ~CImgui() {
		shutdown();
	}
	bool init(Window *wnd) {
		Log_trace;
		// ImGui の本体はグローバルなオブジェクトであることに注意。
		// CImgui をいったん削除して再生成した場合など、初期化と終了処理の整合性に気を付ける事。
		Log_assert(ImGui::GetCurrentContext() == NULL && "CImgui::shutdown が呼ばれないまま init しようとしています。CImgui の裏にある ImGui はグローバルな");
		Log_assert(wnd);
		Log_assert(window_ == NULL);
		Log_assert(Video::isInitialized() && "Video::init() が呼ばれていません");

		window_ = wnd;

		// ImGui の初期化
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().IniFilename = ""; // ImGUI 独自の ini ファイルを抑制
		ImGui::GetIO().LogFilename = ""; // ImGUI 独自の log ファイルを抑制
		//ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

		// ImGui とビデオを関連付ける
		#if IMGUI_DX9
		if (1) {

			// IDirect3DDevice9 を取得
			Video::getParameter(Video::D3DDEV9, &d3d9_dev_);
			Log_assert(d3d9_dev_);
			d3d9_dev_->AddRef();
			d3d9_dev_->CreateStateBlock(D3DSBT_ALL, &d3d9_block_);

			// ImGui と DirectX を関連付ける
			ImGui_ImplWin32_Init(wnd->getHandle());
			ImGui_ImplDX9_Init(d3d9_dev_);
		}
		#endif

		// Guiスタイルの設定
		ImGui::StyleColorsDark(); //ImGui::StyleColorsClassic();
		loadDefaultStyle();

		// ImGui はフォントのリロードを推奨していない。
		// 一番最初に一度だけ作成しなければならない。
		// デバッグモードでシーンを切り替えたり、リロードを実行したりしても、
		// フォントデータだけは最初にロードしたものをずっと使い続ける。
		// フォントのリロードを実装する時は ImGui_ImplDX9_Shutdown を呼んですべての ImGui 描画設定をリセットし、
		// アプリの再起動を疑似的に実行する必要がある
		ImGui_AddDefaultFont();
		font_index_ = 0;

		// デバイスロストなどのイベントを取るために、コールバックを登録する
		Log_assert(Video::isInitialized());
		Video::addCallback(this);

		// ウィンドウメッセージを取るためにコールバックを登録する
		window_->addCallback(this);

		return true;
	}
	void shutdown() {
		// コールバックを解除
		if (window_) {
			window_->removeCallback(this); // IWindowCallback
		}
		Video::removeCallback(this); // IVideoCallback

	#if IMGUI_DX9
		if (ImGui::GetCurrentContext()) {
			ImGui::GetIO().Fonts->Clear();
			ImGui_ImplDX9_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}
		if (d3d9_block_) {
			d3d9_block_->Release();
			d3d9_block_ = NULL;
		}
		if (d3d9_dev_) {
			d3d9_dev_->Release();
			d3d9_dev_ = NULL;
		}
	#endif
		window_ = NULL;
	}

	/// IVideoCallback
	virtual void on_video_device_lost() override {
	#if IMGUI_DX9
		if (d3d9_block_) {
			d3d9_block_->Release();
			d3d9_block_ = NULL;
		}
		ImGui_ImplDX9_InvalidateDeviceObjects();
	#endif
	}

	/// IVideoCallback
	virtual void on_video_device_reset() override {
	#if IMGUI_DX9
		if (d3d9_dev_) {
			d3d9_dev_->CreateStateBlock(D3DSBT_ALL, &d3d9_block_);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
	#endif
	}

	/// IWindowCallback
	virtual void onWindowEvent(WindowEvent *e) override {
	#if IMGUI_DX9
		switch (e->type) {
		case WindowEvent::WIN32MSG:
			if (d3d9_dev_) {
				// ImGui 側のウィンドウプロシージャを呼ぶ。
				// ここで ImGui_ImplWin32_WndProcHandler が 1 を返した場合、そのメッセージは処理済みであるため
				// 次に伝搬してはならない。
				// ※マウスカーソルの形状設定などに影響する。
				// これを正しく処理しないと、ImGui のテキストエディタにカーソルを重ねてもカーソル形状が IBeam にならない。
				long lresult = ImGui_ImplWin32_WndProcHandler(
					(HWND)e->win32msg_ev.hWnd,
					(UINT)e->win32msg_ev.msg,
					(WPARAM)e->win32msg_ev.wp,
					(LPARAM)e->win32msg_ev.lp
				);
				if (lresult) {
					e->win32msg_ev.lresult = lresult;
				}
			}
			break;
		}
	#endif
	}

	void beginRender() {
		Log_assert(ImGui::GetCurrentContext());
	#if IMGUI_DX9
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
	#endif
		ImGui::NewFrame();
		ImGui_PushFont(font_index_);
	}
	void endRender() {
		Log_assert(ImGui::GetCurrentContext());
		ImGui::PopFont();
		ImGui::EndFrame();
	#if IMGUI_DX9
		d3d9_block_->Capture();
		d3d9_dev_->SetRenderState(D3DRS_ZENABLE, FALSE);
		d3d9_dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		d3d9_dev_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		d3d9_block_->Apply();
	#endif
	}
	void loadDefaultStyle() {
		Log_assert(ImGui::GetCurrentContext());
		ImGuiStyle &style = ImGui::GetStyle();
		style.Alpha = 1.0f; // このアルファ値は文字も含んだウィンドウ全体のアルファなので注意
		style.WindowPadding = ImVec2(2, 2);
		style.FramePadding = ImVec2(4, 0);
		style.ItemSpacing = ImVec2(2, 1);
		style.Colors[ImGuiCol_WindowBg]             = GuiColor_Window;
		style.Colors[ImGuiCol_ScrollbarBg]          = ImColor(0.3f, 0.3f, 0.3f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrab]        = ImColor(0.5f, 0.5f, 0.5f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0.8f, 0.8f, 0.8f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImColor(0.9f, 0.9f, 0.9f, 1.0f);
		style.WindowRounding = 2;
		style.IndentSpacing = 24;
		ImGuiIO &io = ImGui::GetIO();
		io.FontAllowUserScaling = true; // Ctrl+Wheel scaling
	}
	int addFont(FontFace ff, int size_px) {
		Log_assert(ImGui::GetCurrentContext());
		/*
		MSGOTHIC.TTC と MEIRYO.TTC のフォント情報を以下に示す
		（ちなみにこれらは KFont のテスト関数 

		msgothic.ttc[0]
			Family   : ＭＳ ゴシック
			CopyRight : © 2017 data : 株式会社リコー typeface : リョービイマジクス(株)
			Version : Version 5.31
			Trademark : ＭＳ ゴシックは米国 Microsoft Corporation の米国及びその他の国の登録商標または商標です。
		msgothic.ttc[1]
			Family : MS UI Gothic
			CopyRight : © 2017 data : 株式会社リコー typeface : リョービイマジクス(株)
			Version : Version 5.31
			Trademark : MS UI Gothicは米国 Microsoft Corporation の米国及びその他の国の登録商標または商標です。
		msgothic.ttc[2]
			Family : ＭＳ Ｐゴシック
			CopyRight : © 2017 data : 株式会社リコー typeface : リョービイマジクス(株)
			Version : Version 5.31
			Trademark : ＭＳ Ｐゴシックは米国 Microsoft Corporation の米国及びその他の国の登録商標または商標です。

		----
		meiryo.ttc [0]
			Family   : メイリオ
			CopyRight: © 2016 Microsoft Corporation. All Rights Reserved.
			Version  : Version 6.30
			Trademark: メイリオは、米国 Microsoft Corporation の米国およびその他の国における登録商標または商標です。
		meiryo.ttc [1]
			Family   : メイリオ
			CopyRight: © 2016 Microsoft Corporation. All Rights Reserved.
			Version  : Version 6.30
			Trademark: メイリオは、米国 Microsoft Corporation の米国およびその他の国における登録商標または商標です。
		meiryo.ttc [2]
			Family   : Meiryo UI
			CopyRight: © 2016 Microsoft Corporation. All Rights Reserved.
			Version  : Version 6.30
			Trademark: メイリオは、米国 Microsoft Corporation の米国およびその他の国における登録商標または商標です。
		meiryo.ttc [3]
			Family   : Meiryo UI
			CopyRight: © 2016 Microsoft Corporation. All Rights Reserved.
			Version  : Version 6.30
			Trademark: メイリオは、米国 Microsoft Corporation の米国およびその他の国における登録商標または商標です。
		*/

		switch (ff) {
		case FF_MSGOTHIC: // ＭＳ ゴシック
			return addFont("MSGOTHIC.TTC", 0, size_px);
		case FF_MSGOTHIC_UI: // MS UI Gothic
			return addFont("MSGOTHIC.TTC", 1, size_px);
		case FF_MSGOCHIC_P: // ＭＳ Ｐゴシック
			return addFont("MSGOTHIC.TTC", 2, size_px);
		case FF_MEIRYO: // メイリオ
			return addFont("MEIRYO.TTC", 0, size_px);
		case FF_MEIRYO_UI: // Meiryo UI
			return addFont("MEIRYO.TTC", 2, size_px);
		}
		return -1;
	}
	int addFont(const Path &filename, int ttc, int size_px) {
		Log_assert(ImGui::GetCurrentContext());
		Path path;

		// 指定されたままのファイルで探す
		if (Platform::fileExists(filename)) {
			path = filename;
		}

		// システムフォントディレクトリから探す
		if (path.empty()) {
			Path path_in_fonts = Platform::getSystemFontFileName(filename);
			if (Platform::fileExists(path_in_fonts)) {
				path = path_in_fonts;
			}
		}
		if (path.empty()) {
			Log_errorf("CImgui::addFont: no font named '%s'", filename.u8());
			return -1;
		}
		ImFont *font = ImGui_AddFontFromFileTTF(path, ttc, (float)size_px);
		if (font == NULL) {
			Log_errorf("CImgui::addFont: failed to load font '%s'", filename.u8());
			return -1;
		}
	
		// フォントは配列の末尾に追加されたものとする
		const auto &fonts = ImGui::GetIO().Fonts->Fonts;
		int idx = fonts.size() - 1;
		Log_assert(fonts[idx] == font); // インデックスが正しいことを確認
		return idx;
	}
	void setDefaultFont(int font_index) {
		font_index_ = font_index;
	}
}; // CImgui
static CImgui g_im;

bool ImGuiVideo_Init(Window *wnd) {
	return g_im.init(wnd);
}
bool ImGuiVideo_IsInitialized() {
	return ImGui::GetCurrentContext() != NULL;
}
void ImGuiVideo_Shutdown() {
	g_im.shutdown();
}
void ImGuiVideo_BeginRender() {
	g_im.beginRender();
}
void ImGuiVideo_EndRender() {
	g_im.endRender();
}
void ImGuiVideo_LoadDefaultStyle() {
	g_im.loadDefaultStyle();
}
int ImGuiVideo_AddFont(FontFace face, int size_px) {
	return g_im.addFont(face, size_px);
}
int ImGuiVideo_AddFont(const Path &filename, int ttc, int size_px) {
	return g_im.addFont(filename, ttc, size_px);
}
void ImGuiVideo_SetDefaultFont(int font_index) {
	g_im.setDefaultFont(font_index);
}

#pragma endregion // CImgui


} // namesapce

#endif // !NO_IMGUI
