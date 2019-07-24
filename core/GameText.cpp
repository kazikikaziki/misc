#include "GameText.h"
//
#include "GameAction.h"
#include "GameRender.h"
#include "Engine.h"
#include "VideoBank.h"
#include "KConv.h"
#include "KFileLoader.h"
#include "KFont.h"
#include "KImgui.h"
#include "KLog.h"
#include "KPlatform.h"
#include "KText.h"
#include "KVideo.h"

namespace mo {


class MyGlyphPack: public UserFontTextureCallback {
	int numtex_;
public:
	MyGlyphPack() {
		numtex_ = 0;
	}
	virtual void on_update_texture(UserFontTexture ftex, const KImage &image) override {
		TEXID tex = (TEXID )ftex;
		void *ptr = Video::lockTexture(tex);
		if (ptr) {
			memcpy(ptr, image.getData(), image.getDataSize());
			Video::unlockTexture(tex);
		}
	}
	virtual UserFontTexture on_new_texture(int w, int h) override {
		TextureBank *tex_bank = g_engine->videobank()->getTextureBank();
		Path texname = Path::fromFormat("__font%04d", numtex_);
		numtex_++;
		return tex_bank->addTextureFromSize(texname, w, h);
	}
};
static MyGlyphPack g_FontCB;



#pragma region TextSystem
TextSystem::TextSystem() {
	engine_ = NULL;
	as_ = NULL;
	rs_ = NULL;
}
TextSystem::~TextSystem() {
	onEnd();
}
void TextSystem::init(Engine *engine) {
	Log_trace;
	Log_assert(engine);
	engine->addSystem(this);
	engine_ = engine;
	KFont::setUserTextureCallback(&g_FontCB);
}
void TextSystem::onEnd() {
	fonts_.clear();
	engine_ = NULL;
	as_ = NULL;
	rs_ = NULL;
}
void TextSystem::onStart() {
	as_ = g_engine->findSystem<ActionSystem>();
	rs_ = g_engine->findSystem<RenderSystem>();
	Log_assert(as_);
}

class CTextAct: public Action {
public:
	TextSystem *fs_;
	RenderSystem *rs_;

	CTextAct(TextSystem *fs, RenderSystem *rs) {
		fs_ = fs;
		rs_ = rs;
		Log_assert(fs_);
		Log_assert(rs_);
	}
	virtual void onInspector() override {
		EID e = getEntity();
		{
			const char *lbl[] = {"Left", "Center", "Right"};
			int al = horz_align_ - HORZALIGN_LEFT; // make zero based index
			if (ImGui::Combo("Horz align", &al, lbl, 3)) {
				horz_align_ = (EHorzAlign)(HORZALIGN_LEFT + al);
				updateTextMesh();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Horizontal textbox alignment");
			}
		}
		{
			const char *lbl[] = {"Top", "Center", "Bottom"};
			int al = vert_align_ - VERTALIGN_TOP; // make zero based index
			if (ImGui::Combo("Vert align", &al, lbl, 3)) {
				vert_align_ = (EVertAlign)(VERTALIGN_TOP + al);
				updateTextMesh();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Vertical textbox alignment");
			}
		}
		{
			TextSystem *fontsys = g_engine->findSystem<TextSystem>();
			PathList list = fontsys->getNames();
			if (list.size() > 0) {
				int sel = -1;
				for (size_t i=0; i<list.size(); i++) {
					if (fontsys->getFont(list[i]) == tb_font_) {
						sel = i;
					}
				}
				if (ImGui_Combo("Font", &sel, list)) {
					tb_font_ = fontsys->getFont(list[sel]);
					updateTextMesh();
				}
			}
		}
		if (ImGui::DragFloat("Font height", &tb_fontsize_, 0.5f, 1, 120)) {
			updateTextMesh();
		}
		if (ImGui::DragFloat("Pitch", &tb_pitch_, 0.1f, -20, 20)) {
			updateTextMesh();
		}
		char s[1024];
		strcpy(s, KConv::wideToUtf8(text_).c_str());

		if (ImGui::InputText("Text", s, sizeof(s))) {
			text_ = KConv::utf8ToWide(s);
			updateTextMesh();
		}

		ImGui::Separator();
		if (1) {
			Color color = rs_->getMasterColor(e);
			if (ImGui::ColorEdit4("Color (MeshRenderer)", (float*)&color)) {
				// マスターカラーはメッシュから独立しているため、メッシュの再生成は不要
				rs_->setMasterColor(e, color);
			}
		}
		if (1) {
			if (ImGui::Checkbox("Linear Fiter (MeshRenderer)", &linear_filter_)) {
				updateTextMesh();
			}
		}
	}
	virtual void onEnterAction() override {
		EID e = getEntity();
		should_update_mesh_ = false;
		horz_align_ = HORZALIGN_LEFT;
		vert_align_ = VERTALIGN_TOP;
		tb_font_ = fs_->getDefaultFont();
		tb_fontstyle_ = KFont::NORMAL;
		tb_fontsize_ = 20;
		tb_pitch_ = 0.0f;
		tb_color0 = Color32::WHITE;
		tb_color1 = Color32::BLACK;
		linear_filter_ = true;
		rs_->attachMesh(e);
		rs_->setLocalTransform(e, Matrix4::fromScale(1, -1, 1)); // Y軸下向きにしておく
	}
	virtual bool onStepAction() override {
		if (should_update_mesh_) {
			updateTextMesh();
		}
		return true;
	}
	void updateTextMesh() {
		EID e = getEntity();
		should_update_mesh_ = false;

		// 現在設定されている文字属性を使って、テキストを再登録する
		textbox_.clear();
		textbox_.setKerning(false);
		textbox_.setFont(tb_font_);
		textbox_.setFontStyle(tb_fontstyle_);
		textbox_.setFontSize(tb_fontsize_);
		textbox_.setLineHeight(tb_fontsize_);
		textbox_.setFontColor(Color32::WHITE); // フォント色はメッシュの master_color によって変化させるため、ここでは常に WHITE を指定しておく
		textbox_.setFontPitch(tb_pitch_);
		textbox_.addString(text_.c_str());

		// メッシュを更新する
		KMesh *mesh = rs_->getMesh(e);
		Log_assert(mesh);

		mesh->clear();

		// テキストメッシュ用の頂点を追加する
		int num_points = textbox_.getMeshVertexCount();
		if (num_points > 0) {
			Vertex *vertex = mesh->lock(0, num_points);
			textbox_.getMeshVertexArray(vertex, num_points);
			mesh->unlock();
		}

		// 描画する文字の数だけサブメッシュを用意する
		int num_glyph = textbox_.getMeshCharCount();
		for (int i=0; i<num_glyph; i++) {
			KSubMesh sub;
			sub.start = i*6; // 1文字につき6頂点
			sub.count = 6;
			sub.primitive = Primitive_TriangleList;
			mesh->addSubMesh(sub);
		}

		// それぞれの文字について、テクスチャを設定する
		for (int i=0; i<num_glyph; i++) {
			Material *mat = rs_->getSubMeshMaterial(e, i);
			mat->texture = (TEXID)textbox_.getMeshTexture(i);
			mat->blend = Blend_Alpha;
			mat->filter = linear_filter_ ? Filter_Linear : Filter_None;
		}

		// メッシュのAABBを得る
		Vec3 minp, maxp;
		mesh->getAabb(&minp, &maxp);

		// アラインメントにしたがって位置を調整する。
		// 座標系に注意。左上を原点としてY軸下向きなので、
		// minp は top-left を表し、maxp は bottom-right を表す
		Vec3 offset;
		switch (horz_align_) {
		case HORZALIGN_LEFT:   offset.x = -minp.x; break;
		case HORZALIGN_CENTER: offset.x = -(maxp.x + minp.x)/2; break;
		case HORZALIGN_RIGHT:  offset.x = -maxp.x; break;
		}
		int numchar = textbox_.getCharCount();
		switch (vert_align_) {
		case VERTALIGN_TOP:
#if 0
			offset.y = -minp.y; これもやめておいたほうがよさそう。1行目の文字のアセント具合によって、ベースラインがそろわなくなる
#else
			// 1行目のエムハイトを基準にする
			if (numchar > 0) {
				const KTextBox::Char &chr = textbox_.getCharByIndex(0);
				FontMetrics fm = chr.attr.font.getMetrics(chr.attr.size);
				offset.y = chr.pos.y + fm.em_height; // 1行目のベースラインにエムハイト(大文字の 'M' の高さ）を足す
			}
#endif
			break;
		case VERTALIGN_CENTER: offset.y = -(maxp.y + minp.y)/2; break;
		case VERTALIGN_BOTTOM:
#if 0
			// AABB下側を基準にする
			offset.y = -maxp.y; これはダメ。最終行の文字のデセントの値によってベースラインがそろわない
#else
			// 最終行のベースラインを基準にする
			if (numchar > 0) {
				const KTextBox::Char &chr = textbox_.getCharByIndex(numchar - 1);
				offset.y = -chr.pos.y;
			}
#endif
			break;
		}
		rs_->setOffset(e, offset);

		aabb_min_ = minp + offset;
		aabb_max_ = maxp + offset;
	}
	std::wstring text_;
	KTextBox textbox_;
	EHorzAlign horz_align_;
	EVertAlign vert_align_;
	Vec3 aabb_min_;
	Vec3 aabb_max_;
	bool linear_filter_;
	bool should_update_mesh_;

	// テキスト属性。
	// KTextBox に設定するテキスト属性は clear のたびに失われる。
	// また、KTextBox に設定するテキストは、それ以降に追加される文字にのみ適用される。
	// そのため、KTextBox に設定するフォント属性を保持しておく
	KFont tb_font_;
	KFont::Style tb_fontstyle_;
	Color32 tb_color0;
	Color32 tb_color1;
	float tb_fontsize_;
	float tb_pitch_;
};

bool TextSystem::onAttach(EID e, const char *type) {
	if (type==NULL || strcmp(type, "Text") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void TextSystem::onAppRenderUpdate() {

}
void TextSystem::attach(EID e) {
	as_->attach(e, new CTextAct(this, rs_));
}
void TextSystem::setText(EID e, const char *text_u8) {
	std::wstring ws = KConv::utf8ToWide(text_u8);
	setText(e, ws.c_str());
}
void TextSystem::setText(EID e, const wchar_t *text) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->text_ = text;
		act->should_update_mesh_ = true;
	}
}
void TextSystem::setFont(EID e, KFont &font) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->tb_font_ = font;
		act->should_update_mesh_ = true;
	}
}
void TextSystem::setFont(EID e, const Path &alias) {
	KFont font = getFont(alias, false);
	if (font.empty()) {
		Log_errorf("E_FONT: NO FONT ALIASED '%s", alias.u8());
	}
	setFont(e, font);
}
void TextSystem::setFontSize(EID e, float value) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->tb_fontsize_ = value;
		act->should_update_mesh_ = true;
	}
}
void TextSystem::setFontStyle(EID e, KFont::Style value, const Color32 &primary_color, const Color32 &secondary_color) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->tb_fontstyle_ = value;
		act->tb_color0 = primary_color;
		act->tb_color1 = secondary_color;
		act->should_update_mesh_ = true;
	}
}
void TextSystem::setFontPitch(EID e, float value) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->tb_pitch_ = value;
		act->should_update_mesh_ = true;
	}
}
void TextSystem::setHorzAlign(EID e, EHorzAlign align) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->horz_align_ = align;
		act->should_update_mesh_ = true;
	}
}
void TextSystem::setVertAlign(EID e, EVertAlign align) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->vert_align_ = align;
		act->should_update_mesh_ = true;
	}
}
void TextSystem::updateMesh(EID e) {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		act->updateTextMesh();
	}
}
void TextSystem::setColor(EID e, const Color &color) {
	rs_->setMasterColor(e, color);
}
void TextSystem::setAlpha(EID e, float alpha) {
	rs_->setMasterAlpha(e, alpha);
}
Color TextSystem::getColor(EID e) const {
	return rs_->getMasterColor(e);
}
float TextSystem::getAlpha(EID e) const {
	return getColor(e).a;
}
void TextSystem::getAabb(EID e, Vec3 *minpoint, Vec3 *maxpoint) const {
	CTextAct *act = as_->getCurrentActionT<CTextAct*>(e);
	if (act) {
		*minpoint = act->aabb_min_;
		*maxpoint = act->aabb_max_;
	}
}
Vec3 TextSystem::getAabbSize(EID e) const {
	Vec3 minp, maxp;
	getAabb(e, &minp, &maxp);
	return maxp - minp;
}


bool TextSystem::addFont(const Path &name, KFont &font) {
	if (name.empty()) return false;
	if (font.empty()) return false;
	deleteFont(name);
	fonts_[name] = font;
	names_.push_back(name);
	return true;
}
KFont TextSystem::addFont(const Path &name, const Path &_filename, int ttc_index) {
	Log_assert(engine_);

	if (name.empty()) return KFont();
	Path filename = _filename;
	if (filename.empty()) {
		filename = name;
	}
	// 可能ならばロード済みフォントを返す
	{
		KFont font = getFont(name, false);
		if (!font.empty()) return font;
	}
	// アセットからのロードを試みる
	if (1) {
		FileInput file = engine_->getFileSystem()->getInputFile(filename);
		KFont font;
		if (font.loadFromFile(file)) {
			if (addFont(name, font)) {
				return font;
			}
		}
	}
	// 標準フォントフォルダからのロードを試みる
	if (1) {
		FontFile fontfile;
		if (fontfile.loadFromFileNameInSystemFontDirectory(filename)) {
			KFont font = fontfile.getFont(ttc_index);
			if (addFont(name, font)) {
				return font;
			}
		}
	}
	Log_errorf(u8"E_FONT: フォント '%s' はアセットフォルダとシステムフォントフォルダのどちらにも存在しません", _filename.u8());
	return KFont();
}
void TextSystem::deleteFont(const Path &name) {
	{
		auto it = fonts_.find(name);
		if (it != fonts_.end()) {
			fonts_.erase(it);
		}
	}
	{
		auto it = std::find(names_.begin(), names_.end(), name);
		if (it != names_.end()) {
			names_.erase(it);
		}
	}
}
const PathList & TextSystem::getNames() const {
	return names_;
}
KFont TextSystem::getDefaultFont() const {
	// fallback_ で指定されたフォントを探す
	if (1) {
		auto it = fonts_.find(fallback_);
		if (it!=fonts_.end()) return it->second;
	}

	// フォントテーブルの最初のフォントを得る
	if (1) {
		auto it = fonts_.begin();
		if (it!=fonts_.end()) return it->second;
	}

	// フォントが全くロードされていない。
	// システムのデフォルトフォントを取得しておく
	if (system_default_.empty()) {
		const Path name = "arial.ttf";
		if (!system_default_.loadFromFileNameInSystemFontDirectory(name)) {
			// システムフォントのロードにすら失敗した
			Log_errorf("E_FONT: Failed to load default font: %s", name.u8());
		}
	}
	return system_default_;
}
KFont TextSystem::getFont(const Path &name, bool fallback) const {
	// name で指定されたフォントを探す
	{
		auto it = fonts_.find(name);
		if (it!=fonts_.end()) return it->second;
	}

	// fallback が許可されているならデフォルトフォントを返す
	if (fallback) {
		return getDefaultFont();
	}

	return KFont();
}
void TextSystem::setDefaultFont(const Path &name) {
	fallback_ = name;
}
#pragma endregion // TextSystem


} // namespace
