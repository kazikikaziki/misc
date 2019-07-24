#include "KText.h"
#include "KPath.h"
#include "KStd.h"
#include "KNum.h"
#include "KFile.h"

namespace mo {

#define TEXT_USE_INT_POS 0
#define TEXT_USE_INT_CUR 1
#define PARENT_NONE (-1)
#define PAERNT_SELF (-2)
#define BLANK_ADVANCE_FACTOR 0.3f


#pragma region KTextBox
KTextBox::KTextBox() {
	clear();
}
void KTextBox::clear() {
	curstack_.clear();
	attrstack_.clear();
	sequence_.clear();
	curattr_ = Attr();
	curattr_.color = Color32::WHITE;
	curattr_.size = 24;
	lineheight_ = 24;
	curx_ = 0;
	cury_ = 0;
	auto_wrap_width_ = 0;
	curlinestart_ = 0;
	curparent_ = PARENT_NONE;
	rowcount_ = 1;
	progress_animation_ = false;
	kerning_ = true;
}
float KTextBox::getCharProgress(int index) const {
	return sequence_[index].anim.progress;
}
void KTextBox::setCharProgress(int index, float progress) {
	sequence_[index].anim.progress = Num_clampf(progress);
}
void KTextBox::setAutoWrapWidth(int w) {
	auto_wrap_width_ = (float)w;
}
void KTextBox::pushCursor() {
	curstack_.push_back(Vec2(curx_, cury_));
}
void KTextBox::popCursor() {
	curx_ = curstack_.back().x;
	cury_ = curstack_.back().y;
	curstack_.pop_back();
}
void KTextBox::setCursor(const Vec2 &pos) {
	curx_ = pos.x;
	cury_ = pos.y;
}
Vec2 KTextBox::getCursor() const {
	return Vec2(curx_, cury_);
}
void KTextBox::setFont(KFont &font) {
	curattr_.font = font;
}
void KTextBox::setFontSize(float size) {
	curattr_.size = size;
}
void KTextBox::setFontStyle(KFont::Style style) {
	curattr_.style = style;
}
void KTextBox::setFontPitch(float pitch) {
	curattr_.pitch = pitch;
}
void KTextBox::setFontColor(const Color32 &color) {
	curattr_.color = color;
}
void KTextBox::setTextureGlyphColor(const Color32 &primary, const Color32 &secondary) {
	curattr_.glyph_color1 = primary;
	curattr_.glyph_color2 = secondary;
}
void KTextBox::setUserData(int userdata) {
	curattr_.userdata = userdata;
}
void KTextBox::setLineHeight(float height) {
	lineheight_ = height;
}
void KTextBox::setKerning(bool value) {
	kerning_ = value;
}
float KTextBox::getLineHeight() const {
	return lineheight_;
}
void KTextBox::pushFontAttr() {
	attrstack_.push_back(curattr_);
}
void KTextBox::popFontAttr() {
	curattr_ = attrstack_.back();
	attrstack_.pop_back();
}
int KTextBox::beginGroup() {
	curparent_ = sequence_.size();

	// グループの親としてふるまう文字を入れておく
	Char chr;
	chr.pos.x = curx_;
	chr.pos.y = cury_;
	chr.parent = PAERNT_SELF; // 自分がグループの親になっている
	sequence_.push_back(chr);
	return curparent_;
}
void KTextBox::endGroup() {
	curparent_ = PARENT_NONE;
}
Vec2 KTextBox::getCharPos(int index) const {
	const Char &chr = sequence_[index];
	if (chr.parent >= 0) {
		// グループ文字になっている。基準文字からの相対座標が設定されている
		const Char &parent = sequence_[chr.parent];
		return parent.pos + chr.pos;
	} else {
		// グループ文字ではない。テキストボックス内座標が設定されている
		return chr.pos;
	}
}
void KTextBox::addString(const wchar_t *text) {
	for (const wchar_t *s=text; *s; s++) {
		if (*s == L'\n') {
			newLine();
		} else if (K_iswprint(*s)) {
			addChar(*s);
		}
	}
}
void KTextBox::addChar(wchar_t code) {
	if (curattr_.font.empty()) {
	//	Log_warning(u8"フォントが未設定です");
		K_assert(0 && "NO FONT LOADED"); // フォントがロードされていない。何も描画できない
	}

	// カーニング適用
	if (kerning_) {
		if (sequence_.size() > 0) {
			const Char &last = sequence_.back();
			// 自分のすぐ左かつベースライン、フォントサイズが同じならカーニングを適用する
			if (last.pos.x < curx_ && last.pos.y == cury_) {
				if (last.attr.size == curattr_.size) {
					if (last.attr.font.id() == curattr_.font.id()) {
						// カーニングを適用し、カーソルを後退させる
						float kern = last.attr.font.getKerningAdvanve(last.code, code, last.attr.size);

						// ユーザーがカーニングに介入したい場合はここで行う
						onKerning(last.code, code, last.attr.size, &kern);

						// カーニング適用
						if (kern != 0.0f) {
							curx_ += kern;
						}
					}
				}
			}
		}
	}

	// 文字情報を得る
	KGlyph glyph = curattr_.font.getGlyph(code, curattr_.size, curattr_.style, true, curattr_.glyph_color1, curattr_.glyph_color2);

	// 空白文字の文字送り量を調整する
	if (1) {
		if (code == ' ') {
			glyph.advance = curattr_.size * BLANK_ADVANCE_FACTOR;
		}
	}

	float bound_right  = curx_ + glyph.right;
//	float bound_bottom = cury_ + _min(glyph.bottom, 0); // bottom がベースラインよりも上側にある場合 (bottom < 0) は 0 扱いにする

	// 文字の位置を決定する
	// 文字の右側が自動改行位置を超えてしまう場合は、先に改行しておく
	if (auto_wrap_width_ > 0) {
		if (auto_wrap_width_ <= bound_right) {
			if (curparent_ >= 0) {
				// グループ文字を処理中なので、途中で改行できない。
				// グループ先頭までさかのぼって改行させる
				if (curlinestart_ == curparent_) {
					// すでにグループ先頭が行頭になっている。改行しても意味がない。何もしない
				
				} else {
					// グループ親
					Char *parent = &sequence_[curparent_];

					// 現在のカーソル位置をグループ親からの相対座標で記録
					float relx = curx_ - parent->pos.x;
					float rely = cury_ - parent->pos.y;

					// グループ親の位置までカーソルを戻す
					curx_ = parent->pos.x;
					cury_ = parent->pos.y;

					// 改行する。この呼び出しで curx_, cury_ が改行後の位置になる
					newLine();
					curlinestart_ = curparent_; // 現在の行の先頭文字インデックス

					// 改行後のカーソル位置にグループ親を置く
					parent->pos.x = curx_;
					parent->pos.y = cury_;

					// カーソル位置を復帰する。
					// カーソル位置とグループ親位置の相対座標が、改行前と変化しないようにする
					curx_ += relx;
					cury_ += rely;
				}

			} else {
				// 空白文字での改行を試みる。
				// 現在の行の最後にある空白文字を探す
				int space_index = -1;
				for (int i=(int)sequence_.size()-1; i>=curlinestart_; i--) {
					const Char *chr = &sequence_[i];
					if (chr->parent >= 0) {
						// グループ化されている場合は途中改行できない
					} else {
						if (iswblank(sequence_[i].code)) {
							space_index = i;
							break;
						}
					}
				}
				// 最後の空白文字が先頭でなかったなら、そこで改行する
				if (space_index > 0) {
					// 空白文字が見つかった。
					// 空白文字の「次の文字」を改行させる。
					// （空白文字で改行させると行の先頭が不自然に開いてしまう）
					int break_index = space_index + 1;
					Char *break_chr = &sequence_[break_index];

					// 改行位置文字から、現在の文字までを次の行に移動させる

					// 改行位置文字から、現在の文字までの相対距離
					float relx = curx_ - break_chr->pos.x;
					float rely = cury_ - break_chr->pos.y;

					// 現在の改行文字位置
					int oldx = break_chr->pos.x;
					int oldy = break_chr->pos.y;

					// 改行する。この呼び出しで curx_, cury_ が改行後の位置になる
					newLine();
					curlinestart_ = break_index; // 新しい行の先頭に来る文字のインデックス

					// 改行前と改行後で、改行文字位置文字がどのくらい移動したか
					int deltax = curx_ - oldx;
					int deltay = cury_ - oldy;

					// 現時点で、次の行に移動したのはカーソルだけであり、文字はまだ移動していない。
					// 改行しなければならない文字列を次の行へ移動する
					for (int i=break_index; i<(int)sequence_.size(); i++) {
						sequence_[i].pos.x += deltax;
						sequence_[i].pos.y += deltay;
					}

					// この時点で、カーソルはまだ新しい行の先頭にいる。
					// これが文字列末尾になるよう移動する
					curx_ += relx;
					cury_ += rely;
				
				} else {
					// 空白文字が存在しない。日本語準拠の処理を行う
					// 基本的にはそのまま改行してもよいが、禁則文字などを考慮して問い合わせを行う
					if (dontBeStartOfLine(code)) {
						// 新しい文字は行頭禁止文字である。改行してはいけない
				
					} else if (sequence_.size() > 0 && dontBeLastOfLine(sequence_.back().code)) {
						// 最後に追加した文字は行末禁止文字だった。改行してはいけない

					} else {
						newLine();
						curlinestart_ = (int)sequence_.size(); // 現在の行の先頭文字インデックス
					}
				}
			}
		}
	}

	/*
		改行文字を明示的に追加したい場合があるかもしれないので、ここでは改行文字を処理しない。
		改行文字で実際に改行させたい場合は、呼び出し側で処理すること。
		if (code == L'\n') {
			newLine();
			return;
		}
	*/

	Char chr;
	chr.code = code;
	chr.anim = Anim();
	chr.attr = curattr_;
	chr.glyph = glyph;
	chr.parent = curparent_; // グループ番号は、そのグループの開始インデックスを表している。同時に、グループのローカル座標基準も表す

	if (chr.parent >= 0) {
		// グループ内文字の場合、文字座標はグループ先頭の文字からの相対座標で指定する
		Vec2 parentpos = sequence_[chr.parent].pos; // グループ先頭の文字位置
		Vec2 relpos = Vec2(curx_, cury_) - parentpos;
		chr.pos.x = relpos.x;
		chr.pos.y = relpos.y;

	} else {
		// テキストボックス左上からの相対座標
		chr.pos.x = curx_;
		chr.pos.y = cury_;
	}
	if (TEXT_USE_INT_POS) {
		// 座標を整数化する（カーソルは実数のまま、描画位置だけ調整する）
		#if 0
		chr.pos.x = floorf(curx_);
		chr.pos.y = floorf(cury_);
		#else
		chr.pos.x = Num_round(curx_);
		chr.pos.y = Num_round(cury_);
		#endif
	}

	sequence_.push_back(chr);

	if (TEXT_USE_INT_CUR) {
		// カーソルの位置を常に整数に合わせる
		curx_ += floorf(chr.glyph.advance + curattr_.pitch);
	//	curx_ += Num_round(chr.glyph.advance + curattr_.pitch);
	} else {
		// advance, pitch をそのまま信用し、適用する
		curx_ += chr.glyph.advance + curattr_.pitch;
	}
}
void KTextBox::setRuby(int group_id, const wchar_t *text, int count) {
	pushCursor();

	// 対象文字列の矩形
	Vec2 topleft, bottomright;
	getGroupBoundRect(group_id, &topleft, &bottomright);

	// ルビ対象文字列の左上にカーソルを移動
	setCursor(topleft);

	// ルビ文字列を追加
	int ruby_start = getCharCount();
	for (int i=0; i<count; i++) {
		addChar(text[i]);
	}

	// ルビ対象文字列の中心位置を基準にして、ルビ文字列を中央ぞろえにする
	float center = (topleft.x + bottomright.x) / 2;
	horzCentering(ruby_start, count, center);
	
	popCursor();
}
void KTextBox::newLine() {
	curx_ = 0;
	cury_ += lineheight_;
	rowcount_++;
}
const KTextBox::Char & KTextBox::getCharByIndex(int index) const {
	static const Char s_char;
	if (0 <= index && index < (int)sequence_.size()) {
		return sequence_[index];
	}
	return s_char;
}
bool KTextBox::getBoundRect(int start, int count, Vec2 *out_topleft, Vec2 *out_bottomright) const {
	int s = Num_max(start, 0);
	int e = Num_min(start + count, (int)sequence_.size());
	if (s < e) {
		float box_l = FLT_MAX;
		float box_r = FLT_MIN;
		float box_t = FLT_MAX;
		float box_b = FLT_MIN;
		for (int i=s; i<e; i++) {
			const Char &elm = sequence_[i];

			Vec2 elmpos = getCharPos(i);

			// グリフの座標系はベースポイント基準のY軸下向き
			float char_l = elmpos.x + elm.glyph.left;
			float char_r = elmpos.x + elm.glyph.right;
			float char_t = elmpos.y + elm.glyph.top;
			float char_b = elmpos.y + elm.glyph.bottom;

			box_l = Num_min(box_l, char_l);
			box_r = Num_max(box_r, char_r);
			box_t = Num_min(box_t, char_t);
			box_b = Num_max(box_b, char_b);
		}
		if (out_topleft    ) *out_topleft     = Vec2(box_l, box_t);
		if (out_bottomright) *out_bottomright = Vec2(box_r, box_b);
		return true;
	}
	return false;
}
bool KTextBox::getGroupBoundRect(int parent, Vec2 *out_topleft, Vec2 *out_bottomright) const {
	if (parent < 0) return false;

	// グループ先頭インデックスと、グループ長さを得る。
	int start = parent + 1; // 親自身をカウントしないように注意
	int count = 0;
	for (int i=start; i<(int)sequence_.size(); i++) {
		if (sequence_[i].parent == parent) {
			count++;
		} else {
			break;
		}
	}
	// 矩形を取得する
	return getBoundRect(start, count, out_topleft, out_bottomright);
}
bool KTextBox::horzCentering(int start, int count, float x) {
	Vec2 topleft, bottomright;
	if (getBoundRect(start, count, &topleft, &bottomright)) {
		float width = bottomright.x - topleft.x; // 全体の幅
		float left = x - width / 2; // 中央ぞろえにしたときの左端座標
		float offset = left - topleft.x; // 左端が dest_x に一致するための移動量
		for (int i=start; i<start+count; i++) {
			sequence_[i].pos.x += offset;
		}
	}
	return false;
}
KImage KTextBox::exportImage(const ExportDesc *pDesc) {
	ExportDesc desc;
	if (pDesc) {
		desc = *pDesc;
	}

	// テキスト座標系からビットマップ座標系に変換
	#define TEXT2BMP_X(x) origin_x + (int)(x)
	#define TEXT2BMP_Y(y) origin_y + (int)(y)

	// 全テキストのバウンドボックスを求める
	// バウンドボックスの原点は、１行目１文字目のベースライン始点になる。
	// 1行目に文字が存在するなら、ほとんどの場合バウンドボックスの topleft.y は
	// 負の値（１行目のベースラインより上）になる
	int numchars = getCharCount();
	Vec2 topleft, bottomright;
	getBoundRect(0, numchars, &topleft, &bottomright);

	// ビットマップの大きさを決める
	if (desc.xsize <= 0) desc.xsize = (int)(bottomright.x - topleft.x);
	if (desc.ysize <= 0) desc.ysize = (int)(bottomright.y - topleft.y);
	int bmp_w = desc.margin_l + desc.xsize + desc.margin_r;
	int bmp_h = desc.margin_top + desc.ysize + desc.margin_btm;

	// ビットマップを作成
	KImage img(bmp_w, bmp_h, Color32::BLACK);

	// テキストボックスの原点がビットマップのどこにくるか？
	// テキストボックスの原点は、１行目１文字目のベースラインに当たる。
	// 従って、テキストボックスの原点をビットマップ左上に重ねてしまうと、1行目がビットマップの上側にはみ出てしまう。
	// 正しくビットマップ内に収めるためには、テキスト全体を1行分下に下げないといけない。

	// 今回の場合は、テキスト全体バウンドボックスの左上が
	// ビットマップ左上よりもマージン分内側になるように重ねる
	int origin_x = desc.margin_l - (int)topleft.x;
	int origin_y = desc.margin_top - (int)topleft.y;
//	int origin_y = desc.margin_top + lineheight_;
	// 行線を描画
	if (desc.show_rowlines) {
		int x0 = TEXT2BMP_X(0);
		int x1 = TEXT2BMP_X(desc.xsize);
		Color32 linecolor(255, 50, 50, 50);
		for (int i=0; i<rowcount_; i++) {
			int y = TEXT2BMP_Y(lineheight_ * i);
			KImageUtils::horzLine(img, x0, x1, y, linecolor);
		}
	}

	// マージンが設定されている場合、その範囲を描画
	if (desc.show_margin) {
		int x0 = desc.margin_l;   //TEXT2BMP_X(0);
		int y0 = desc.margin_top; //TEXT2BMP_Y(0);
		int x1 = bmp_w - desc.margin_r; //TEXT2BMP_X(desc.xsize);
		int y1 = bmp_h - desc.margin_btm; //TEXT2BMP_Y(desc.ysize);
		Color32 wrapcolor(255, 75, 0, 0);
		KImageUtils::horzLine(img, x0, x1, y0, wrapcolor);
		KImageUtils::horzLine(img, x0, x1, y1, wrapcolor);
		KImageUtils::vertLine(img, x0, y0, y1, wrapcolor);
		KImageUtils::vertLine(img, x1, y0, y1, wrapcolor);
	}

	// 自動折り返しが設定されているなら、その線を描画
	if (desc.show_wrap && auto_wrap_width_ > 0) {
		int x  = TEXT2BMP_X(auto_wrap_width_);
		int y0 = TEXT2BMP_Y(topleft.y);
		int y1 = TEXT2BMP_Y(bottomright.y);
		Color32 wrapcolor(255, 75, 0, 0);
		KImageUtils::vertLine(img, x, y0, y1, wrapcolor);
	}

	// バウンドボックスを描画
	if (desc.show_boundbox) {
		int x0 = TEXT2BMP_X(topleft.x);
		int y0 = TEXT2BMP_Y(topleft.y);
		int x1 = TEXT2BMP_X(bottomright.x);
		int y1 = TEXT2BMP_Y(bottomright.y);
		Color32 bordercolor(255, 120, 120, 120);
		KImageUtils::horzLine(img, x0, x1, y0, bordercolor);
		KImageUtils::horzLine(img, x0, x1, y1, bordercolor);
		KImageUtils::vertLine(img, x0, y0, y1, bordercolor);
		KImageUtils::vertLine(img, x1, y0, y1, bordercolor);
	}

	// 文字を描画
	for (size_t i=0; i<sequence_.size(); i++) {
		const Char &chr = sequence_[i];
		KImage glyph = chr.attr.font.getGlyphImage32(chr.code, chr.attr.size, chr.attr.style, true, chr.attr.color);
		Vec2 chrpos = getCharPos(i);
		int x = origin_x + (int)chrpos.x + chr.glyph.left;
		int y = origin_y + (int)chrpos.y + chr.glyph.top;
		KImageUtils::blendAlpha(img, x, y, glyph);
	}
	return img;

	#undef TEXT2BMP_X
	#undef TEXT2BMP_Y
}

/// アドレス p に2個の float を書き込む
static inline void _WriteFloat2(void *p, float x, float y) {
	float *pFloats = (float *)p;
	pFloats[0] = x;
	pFloats[1] = y;
}
static inline void _WriteFloat4(void *p, const Color &color) {
	float *pFloats = (float *)p;
	pFloats[0] = color.r;
	pFloats[1] = color.g;
	pFloats[2] = color.b;
	pFloats[3] = color.a;
}
int KTextBox::getCharCount() const {
	return (int)sequence_.size();
}
int KTextBox::getMeshCharCount() const {
	int num = 0;
	for (size_t i=0; i<sequence_.size(); i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) { // no_glyph な文字は数えない。getMeshPositionArray での頂点設定と合わせておくこと
			num++;
		}
	}
	return num;
}

#define NUM_VERTEX_PER_GLPYH 6 // 1グリフにつき6頂点消費する (TriangleList)

int KTextBox::getMeshVertexCount() const {
	return getMeshCharCount() * NUM_VERTEX_PER_GLPYH;
}
void KTextBox::getMeshVertexArray(Vertex *vertices, int max_count) const {
#if 0
	// 1グリフにつき6頂点消費する
	// 利便性のため、6頂点のうちの最初の4頂点は、左上を基準に時計回りに並ぶようにする
	//
	// 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になる
	// |      |
	// |      |
	// 3------2
	const size_t max_glyphs = max_count / NUM_VERTEX_PER_GLPYH; // 書き込み可能な文字数
	size_t glyph_count = 0;
	for (size_t i=0; i<sequence_.size() && glyph_count<max_glyphs; i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) {
			K_assert(i*6+5 < max_count);

			// POS
			{
				Vec2 chrpos = getCharPos(i);
				float left   = chrpos.x + (float)chr.glyph.left;
				float right  = chrpos.x + (float)chr.glyph.right;
				float top    = chrpos.y + (float)chr.glyph.top;
				float bottom = chrpos.y + (float)chr.glyph.bottom;
				if (progress_animation_) {
					onGlyphMesh(i, chr, &left, &top, &right, &bottom);
				}
				vertices[i*6+0].pos = Vec3(left,  top,    0.0f); // 0:左上
				vertices[i*6+1].pos = Vec3(right, top,    0.0f); // 1:右上
				vertices[i*6+2].pos = Vec3(right, bottom, 0.0f); // 2:右下
				vertices[i*6+3].pos = Vec3(left,  bottom, 0.0f); // 3:左下
				vertices[i*6+4].pos = vertices[i*6+0].pos; // 0:左上
				vertices[i*6+5].pos = vertices[i*6+2].pos; // 2:右下
			}

			// UV
			{
				float u0 = chr.glyph.u0;
				float v0 = chr.glyph.v0;
				float u1 = chr.glyph.u1;
				float v1 = chr.glyph.v1;
				vertices[i*6+0].tex = Vec2(u0, v0); // 0:左上
				vertices[i*6+1].tex = Vec2(u1, v0); // 1:右上
				vertices[i*6+2].tex = Vec2(u1, v1); // 2:右下
				vertices[i*6+3].tex = Vec2(u0, v1); // 3:左下
				vertices[i*6+4].tex = vertices[i*6+0].tex; // 0:左上
				vertices[i*6+5].tex = vertices[i*6+2].tex; // 2:右下
			}

			// Color
			{
				Color color(chr.attr.color); // 実数カラーに変換
				if (progress_animation_) {
					color.a *= chr.anim.progress;
				}
				Color32 color32(color);
				vertices[i*6+0].dif = color32; // 0:左上
				vertices[i*6+1].dif = color32; // 1:右上
				vertices[i*6+2].dif = color32; // 2:右下
				vertices[i*6+3].dif = color32; // 3:左下
				vertices[i*6+4].dif = vertices[i*6+0].dif; // 0:左上
				vertices[i*6+5].dif = vertices[i*6+2].dif; // 2:右下
			}
			
			glyph_count++;
		}
	}
#else
		{
			std::vector<Vec3> p(max_count);
			getMeshPositionArray(&p[0], max_count*sizeof(Vec3), sizeof(Vec3));
			for (int i=0; i<max_count; i++) {
				vertices[i].pos = p[i];
			}
		}
		{
			std::vector<Vec2> t(max_count);
			getMeshTexCoordArray(&t[0], max_count*sizeof(Vec2), sizeof(Vec2));
			for (int i=0; i<max_count; i++) {
				vertices[i].tex = t[i];
			}
		}
		{
			std::vector<Color> c(max_count);
			getMeshColorArray(&c[0], max_count*sizeof(Color), sizeof(Color));
			for (int i=0; i<max_count; i++) {
				vertices[i].dif = Color32(c[i]);
			}
		}
#endif
}
void KTextBox::getMeshPositionArray(void *buffer, int max_bytes, int stride_bytes) const {
	const size_t bytes_per_glyph = stride_bytes * 6; // 1文字書き込むために6頂点が必要
	const size_t max_glyphs = max_bytes / bytes_per_glyph; // 書き込み可能な文字数
	size_t glyph_count = 0;
	uint8_t *dst = (uint8_t*)buffer; // float* にしてはいけない。stride_bytes ずらすときの計算が狂う
	for (size_t i=0; i<sequence_.size() && glyph_count<max_glyphs; i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) {
			Vec2 chrpos = getCharPos(i);
			float left   = chrpos.x + (float)chr.glyph.left;
			float right  = chrpos.x + (float)chr.glyph.right;
			float top    = chrpos.y + (float)chr.glyph.top;
			float bottom = chrpos.y + (float)chr.glyph.bottom;
			// 1グリフにつき6頂点消費する
			// 利便性のため、6頂点のうちの最初の4頂点は、左上を基準に時計回りに並ぶようにする
			//
			// 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になる
			// |      |
			// |      |
			// 3------2
			if (progress_animation_) {
				onGlyphMesh(i, chr, &left, &top, &right, &bottom);
			}
			_WriteFloat2(dst, left,  top   ); dst += stride_bytes; // 0:左上
			_WriteFloat2(dst, right, top   ); dst += stride_bytes; // 1:右上
			_WriteFloat2(dst, right, bottom); dst += stride_bytes; // 2:右下
			_WriteFloat2(dst, left,  bottom); dst += stride_bytes; // 3:左下
			_WriteFloat2(dst, left,  top   ); dst += stride_bytes; // 0:左上
			_WriteFloat2(dst, right, bottom); dst += stride_bytes; // 2:右下
			glyph_count++;
		}
	}
}
void KTextBox::getMeshTexCoordArray(void *buffer, int max_bytes, int stride_bytes) const {
	const size_t bytes_per_glyph = stride_bytes * 6; // 1文字書き込むために6頂点が必要
	const size_t max_glyphs = max_bytes / bytes_per_glyph; // 書き込み可能な文字数
	size_t glyph_count = 0;
	uint8_t *dst = (uint8_t*)buffer; // float* にしてはいけない。stride_bytes ずらすときの計算が狂う
	for (size_t i=0; i<sequence_.size() && glyph_count<max_glyphs; i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) {
			float u0 = chr.glyph.u0;
			float v0 = chr.glyph.v0;
			float u1 = chr.glyph.u1;
			float v1 = chr.glyph.v1;
			// 1グリフにつき6頂点消費する
			// 頂点の順番については getMeshPositionArray を参照
			_WriteFloat2(dst, u0, v0); dst += stride_bytes; // 0:左上
			_WriteFloat2(dst, u1, v0); dst += stride_bytes; // 1:右上
			_WriteFloat2(dst, u1, v1); dst += stride_bytes; // 2:右下
			_WriteFloat2(dst, u0, v1); dst += stride_bytes; // 3:左下
			_WriteFloat2(dst, u0, v0); dst += stride_bytes; // 0:左上
			_WriteFloat2(dst, u1, v1); dst += stride_bytes; // 2:右下
			glyph_count++;
		}
	}
}
void KTextBox::getMeshColorArray(void *buffer, int max_bytes, int stride_bytes) const {
	const size_t bytes_per_glyph = stride_bytes * 6; // 1文字書き込むために6頂点が必要
	const size_t max_glyphs = max_bytes / bytes_per_glyph; // 書き込み可能な文字数
	size_t glyph_count = 0;
	uint8_t *dst = (uint8_t*)buffer; // float* にしてはいけない。stride_bytes ずらすときの計算が狂う
	for (size_t i=0; i<sequence_.size() && glyph_count<max_glyphs; i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) {
			// 1グリフにつき6頂点消費する
			// 頂点の順番については getMeshPositionArray を参照
			Color color = Color(chr.attr.color); // 実数カラーに変換
			if (progress_animation_) {
				color.a *= chr.anim.progress;
			}
			_WriteFloat4(dst, color); dst += stride_bytes; // 0:左上
			_WriteFloat4(dst, color); dst += stride_bytes; // 1:右上
			_WriteFloat4(dst, color); dst += stride_bytes; // 2:右下
			_WriteFloat4(dst, color); dst += stride_bytes; // 3:左下
			_WriteFloat4(dst, color); dst += stride_bytes; // 0:左上
			_WriteFloat4(dst, color); dst += stride_bytes; // 2:右下
			glyph_count++;
		}
	}
}
UserFontTexture KTextBox::getMeshTexture(int index) const {
	int idx = 0;
	for (size_t i=0; i<sequence_.size(); i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) { // no_glyph な文字は数えない。getMeshPositionArray での頂点設定と合わせておくこと
			if (idx == index) {
				return chr.glyph.texture;
			}
			idx++;
		}
	}
	return (UserFontTexture)0;
}
void KTextBox::setCharProgressEnable(bool value) {
	progress_animation_ = value;
}
bool KTextBox::dontBeStartOfLine(wchar_t code) {
	static const wchar_t *list = L"｝〕〉》）」』】。、！？ァィゥェォッャュョ!?.,";
	return wcschr(list, code) != NULL;
}
bool KTextBox::dontBeLastOfLine(wchar_t code) {
	static const wchar_t *list = L"｛〔〈《（「『【";
	return wcschr(list, code) != NULL;
}
#pragma endregion // KTextBox








#pragma region KTextParser
KTextParser::KTextParser() {
	escape_ = L'\\'; // エスケープ文字
}
void KTextParser::addStyle(int id, const wchar_t *start) {
	addStyle(id, start, L"\n");
}
void KTextParser::addStyle(int id, const wchar_t *start, const wchar_t *end) {
	K_assert(id > 0); // id == 0 はデフォルト書式を表す。開始、終端文字は登録できない
	K_assert(start && start[0]); // 空文字列は登録できない
	K_assert(end && end[0]); // 空文字列は登録できない
	K_assert(wcslen(start) < STYLE::MAXTOKENLEN);
	K_assert(wcslen(end) < STYLE::MAXTOKENLEN);

	STYLE fmt;
	fmt.id = id;
	wcscpy(fmt.start_token, start);
	wcscpy(fmt.end_token, end);
	styles_.push_back(fmt);
}
const KTextParser::STYLE * KTextParser::isStart(const wchar_t *text) const {
	K_assert(text);
	// 書式範囲の開始記号に一致するか調べる
	// 複数の記号が該当する場合は最も長いものを返す
	const STYLE *style = NULL;
	size_t maxlen = 0;
	for (auto it=styles_.begin(); it!=styles_.end(); it++) {
		size_t n = wcslen(it->start_token);
		if (wcsncmp(text, it->start_token, n) == 0) {
			if (maxlen < n) {
				maxlen = n;
				style = &(*it);
			}
		}
	}
	return style;
}
const KTextParser::STYLE * KTextParser::isEnd(const wchar_t *text) const {
	K_assert(text);

	// 現在の書式の終了記号に一致するか調べる
	if (stack_.empty()) {
		return NULL; // 適用されている書式なし
	}

	// 現在の書式（スタックトップにある書式）
	const STYLE *current_style = stack_.back().style;
	K_assert(current_style);
	
	// 現在の書式の終端記号にマッチすれば OK
	const wchar_t *end = current_style->end_token;
	if (end && end[0]) {
		int n = wcslen(end);
		if (wcsncmp(text, end, n) == 0) {
			return current_style;
		}
	}

	// 終端記号にマッチしなかったか、もしくは文末を終端記号とする特殊書式だった
	return NULL;
}
void KTextParser::startStyle(const STYLE *style, const wchar_t *pos) {
	K_assert(style);
	K_assert(pos);

	onStartStyle(style->id);

	// スタイルと、その適用開始位置をスタックに積む
	MARK mark = {style, pos};
	stack_.push_back(mark);
}
void KTextParser::endStyle(const STYLE *style, const wchar_t *pos) {
	K_assert(style);
	K_assert(pos);

	MARK mark = stack_.back();
	stack_.pop_back();

	if (mark.style == style) {
		// 最後に push したスタイルが pop されている。
		// スタイル開始と終端の対応関係が正しいということ
		const wchar_t *sep = NULL;
		for (const wchar_t *s=mark.start; s<pos; s++) {
			if (*s == L'|') {
				sep = s;
				break;
			}
		}
		std::wstring left, right;
		if (sep) {
			// 区切り文字が見つかった。これを境にして前半と後半に分ける
			left.assign(mark.start, sep - mark.start);
			right.assign(sep+1, pos - sep - 1);
		} else {
			// 区切り文字なし。区切り文字の前半だけが存在し、後半は空文字列とする
			left.assign(mark.start, pos - mark.start);
		}
		onEndStyle(style->id, left.c_str(), right.c_str());

	} else {
		// 最後に push したスタイルとは異なるスタイルを pop した。
		// スタイル開始と終端の対応関係が正しくない
		onEndStyle(style->id, NULL, 0);
	}
}
void KTextParser::parse(const wchar_t *text) {
	K_assert(text);

	// 一番最初は必ず書式番号 0 で始まる
	STYLE default_style = {0, L"", L""};
	startStyle(&default_style, text);

	for (const wchar_t *it=text; *it!=L'\0'; ) {
		const STYLE *style;

		// エスケープ文字がある場合、その次の文字は単なる文字として処理する
		if (*it == escape_) {
			it++;
			onChar(*it);
			it++;
			continue;
		}

		// 開始記号にマッチすれば書式開始イベントを呼ぶ
		style = isStart(it);
		if (style) {
			it += wcslen(style->start_token);
			startStyle(style, it);
			continue;
		}

		// 終了記号にマッチすれば書式終了イベントを呼ぶ
		style = isEnd(it);
		if (style) {
			endStyle(style, it);
			it += wcslen(style->end_token);
			continue;
		}
		
		// 通常文字
		onChar(*it);

		it++;
	}
	// 一番最後は必ず書式番号 0 で終わる
	endStyle(&default_style, text + wcslen(text) /*末尾のヌル文字のアドレス*/);
}
#pragma endregion // KTextParser



#pragma region Test
void Test_textbox1() {
	const wchar_t *text = 
		L"ウィキペディアは百科事典を作成するプロジェクトです。その記事に執筆者独自の意見や研究内容が含まれてはならず（Wikipedia:独自研究は載せないを参照）、"
		L"その記事の内容は、信頼できる文献を参照することによって検証可能でなければなりません（Wikipedia:検証可能性を参照）。"
		L"したがって、記事の執筆者は、複数の信頼できる検証可能な文献を参照し、その内容に即して記事を執筆することが要求されます。"
		L"一方で、参考文献に掲載されている文章をそのまま引き写すことは、剽窃（盗作）であり、場合によっては著作権の侵害という法律上の問題も生じることから、"
		L"各執筆者は、独自の表現で記事を執筆しなければなりません。";
	KTextBox box;
	KFont font;
//	font.loadFromFileName(L"C:\\windows\\fonts\\msgothic.ttc", 0); // ＭＳ ゴシック
//	font.loadFromFileName(L"C:\\windows\\fonts\\msgothic.ttc", 1); // MS UI Gothic
//	font.loadFromFileName(L"C:\\windows\\fonts\\msgothic.ttc", 2); // ＭＳ Ｐ ゴシック
	font.loadFromFileName(L"C:\\windows\\fonts\\meiryo.ttc"); // メイリオ
	box.setFont(font);
	box.setFontSize(22);
	box.setAutoWrapWidth(400);
	for (const wchar_t *it=text; *it!=L'\0'; it++) {
		box.addChar(*it);
	}
	KImage img = box.exportImage(NULL);
	std::string png = img.saveToMemory();
	FileOutput("~text1.png").write(png);
}


class SampleParser: public KTextParser {
public:
	enum STYLE {
		S_NONE,
		S_YELLOW,
		S_RED,
		S_BOLD,
		S_BIG,
		S_WEAK,
		S_RUBY,
		S_COMMENT,
	};
	KTextBox *box_;
	int comment_depth_;
	bool is_ruby_; // ルビ書式を処理中

	SampleParser(KTextBox *box) {
		box_ = box;
		comment_depth_ = 0;
		is_ruby_ = false;
		// [] を色変え系として定義
		addStyle(S_YELLOW, L"[", L"]");
		addStyle(S_RED,    L"[[", L"]]");
		// <--> を太字強調系として定義
		addStyle(S_BOLD,   L"<", L">");
		addStyle(S_BIG,    L"<<", L">>");
		// {} を弱める系として定義
		addStyle(S_WEAK,   L"{", L"}");
		// () を引数付として定義
		addStyle(S_RUBY,   L"(", L")");
		// コメントを定義
		addStyle(S_COMMENT, L"#"); // # から行末まで（方法１）
		addStyle(S_COMMENT, L"//", L"\n"); // // から行末まで（方法２）
		addStyle(S_COMMENT, L"/*", L"*/"); // /* */ で囲まれた部分
	}
	virtual void onStartStyle(int id) override {
		// コメント内部だったら無視する
		if (comment_depth_ > 0) {
			return;
		}
		switch (id) {
		case S_YELLOW:
			box_->pushFontAttr();
			box_->setFontColor(Color32(0xFF, 0x00, 0xFF, 0xFF));
			break;
		case S_RED:
			box_->pushFontAttr();
			box_->setFontColor(Color32(0xFF, 0x00, 0x00, 0xFF));
			break;
		case S_BOLD:
			box_->pushFontAttr();
			box_->setFontStyle(KFont::BOLD);
			break;
		case S_BIG:
			box_->pushFontAttr();
			box_->setFontStyle(KFont::OUTLINE);
			box_->setFontSize(36);
			break;
		case S_WEAK:
			box_->pushFontAttr();
			box_->setFontSize(18);
			box_->setFontColor(Color32(0x77, 0x77, 0x77, 0xFF));
			break;
		case S_RUBY:
			// ルビ範囲の始まり
			is_ruby_ = true;
			break;
		case S_COMMENT:
			// コメント部分
			comment_depth_++;
			break;
		}
	}
	virtual void onEndStyle(int id, const wchar_t *text, const wchar_t *more) override {
		if (id == 0) {
			return;
		}
		if (id == S_COMMENT) {
			comment_depth_--;
			return;
		}
		// コメント内部だったら無視する
		if (comment_depth_ > 0) {
			return;
		}
		if (id == S_RUBY) {
			// ルビが終わった。ルビの書式では前半に対象文字列。後半にルビ文字列が入っている
			is_ruby_ = false;

			// ルビ範囲を決める＆自動改行禁止にするため、対象文字列をグループ化する
			int group_id = box_->beginGroup();

			// ルビ対象文字列を追加する
			for (int i=0; text[i]; i++) {
				box_->addChar(text[i]);
			}
			
			// ルビ文字を設定する
			if (more[0]) {
				box_->pushFontAttr();
				box_->setFontSize(12); // 文字サイズ小さく
				box_->setFontStyle(KFont::NORMAL); // 通常スタイルにする
				box_->setRuby(group_id, more, wcslen(more));
				box_->popFontAttr();
			} else {
				// ルビ文字が省略されている。
				// この場合は、ルビなしの単なる自動改行禁止文字列として扱われることになる
			}

			box_->endGroup();
			return;
		}
		box_->popFontAttr();
	}
	virtual void onChar(wchar_t chr) override {
		// コメント内部の場合は何もしない
		if (comment_depth_ > 0) {
			return;
		}
		// ルビの処理は最後に一括で行うので。ここでは何もしない
		if (is_ruby_) {
			return;
		}
		// 改行文字を処理
		if (chr == L'\n') {
			box_->newLine();
			return;
		}
		// 印字可能な文字だった場合はテキストボックスに追加
		if (K_iswprint(chr)) {
			box_->addChar(chr);
		}
	}
};

void Test_textbox2() {
	const wchar_t *s = 
		L"[[ウィキペディア]]は[(百科事典|ひゃっかじてん)]/*ルビを振るにはこうする*/を"
		L"<(作成|さくせい)>する<<(プロジェクト)>>/*この部分は自動改行されない。（ルビ付き文字列は自動改行されないため、ルビの指定を省略すれば自動改行されない普通の文字列を組むことができる）*/です。"
		L"その[[記事]]に[(執筆者独自|しっぴつしゃどくじ)]の[意見]や研究内容が含まれてはならず{（Wikipedia:独自研究は載せないを参照）}、"
		L"その記事の内容は、信頼できる文献を参照することによって検証可能でなければなりません{（Wikipedia:検証可能性を参照）}。"
		L"したがって、記事の執筆者は、複数の信頼できる<[検証可能]>な文献を参照し、その内容に即して記事を執筆することが要求されます。"
		L"<<一方で>>、(参考文献|さんこうぶんけん)に掲載されている文章をそのまま引き写すことは、剽窃（盗作）であり、"
		L"場合によっては著作権の侵害という法律上の問題も生じることから、"
		L"各執筆者は、[[<<独自の表現>>]]で/*ここはコメント*/記事を[執筆]しなければなりません。//これはコメント\n"
		L"#これはコメントです\n";

	// テキストボックスを用意する
	KTextBox box;
	KFont font;
//	font.loadFromFileName(L"C:\\windows\\fonts\\msgothic.ttc", 0); // ＭＳ ゴシック
//	font.loadFromFileName(L"C:\\windows\\fonts\\msgothic.ttc", 1); // MS UI Gothic
//	font.loadFromFileName(L"C:\\windows\\fonts\\msgothic.ttc", 2); // ＭＳ Ｐ ゴシック
	font.loadFromFileName(L"C:\\windows\\fonts\\meiryo.ttc"); // メイリオ
	box.setFont(font);
	box.setFontSize(22);
	box.setAutoWrapWidth(400);

	// パーサーを実行する
	SampleParser mydok(&box);
	mydok.parse(s);

	// この時点で、テキストボックスにはパーサーで処理された文字列が入っている。
	// 画像ファイルに保存する
	KImage img = box.exportImage(NULL);
	std::string png = img.saveToMemory();
	FileOutput("~text2.png").write(png);
}
#pragma endregion // Test


} // namespace
