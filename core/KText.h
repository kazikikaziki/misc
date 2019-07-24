#pragma once
#include "KFont.h"
#include "KVideoDef.h"
#include <vector>

namespace mo {

class KTextBox {
public:
	struct Attr {
		Color32 glyph_color1; ///< テクスチャ上のグリフの第一描画色。普通は Color32::WHITE 固定でよい（色は color によって乗算される）
		Color32 glyph_color2; ///< テクスチャ上のグリフの第二描画色。普通は Argb23::BLACK 固定でよい。アウトライン文字（塗りつぶしあり）の場合に使う

		Color32 color; ///< 文字色
		KFont font;   ///< フォント
		KFont::Style style; ///< 文字スタイル
		float size;  ///< 文字サイズ（ピクセル単位でフォントの標準高さを指定）
		float pitch; ///< 文字間の調整量（標準文字送りからの調整値。0で標準と同じ）
		int userdata;

		Attr() {
			glyph_color1 = Color32::WHITE;
			glyph_color2 = Color32::BLACK;
			color = Color32::WHITE;
			style = KFont::NORMAL;
			pitch = 0.0f;
			size = 20.0f;
			userdata = 0;
		}
	};
	struct Anim {
		/// 文字表示アニメの進行度。0.0 で文字が出現した瞬間、1.0で完全に文字が表示された状態になる
		float progress;

		Anim() {
			progress = 0.0f;
		}
	};
	struct Char {
		wchar_t code;
		Attr attr;
		Anim anim;
		KGlyph glyph; ///< この文字のグリフ情報
		Vec2 pos;    ///< テキストボックス原点からの相対座標。文字がグループ化されている場合は親文字からの相対座標
		int parent;  ///< 文字がグループ化されている場合、親文字のインデックス。グループ化されていない場合は -1

		Char() {
			code = L'\0';
			parent = -1;
		}
		bool no_glyph() const {
			return glyph.u0 == glyph.u1 || glyph.v0 == glyph.v1;
		}
	};

	KTextBox();
	void clear();

	/// 自動改行幅。0だと改行しない
	void setAutoWrapWidth(int w);
	
	/// カーソル位置をスタックに入れる
	void pushCursor();

	/// カーソル位置を復帰する
	void popCursor();

	/// カーソル位置を移動する
	void setCursor(const Vec2 &pos);

	/// カーソル位置を得る
	Vec2 getCursor() const;

	/// 現在の文字属性をスタックに入れる
	void pushFontAttr();

	/// 文字属性を復帰する
	void popFontAttr();

	/// フォントを設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// 注意：これから追加される文字に対する設定なので、追加済みテキストの状態は変わらない
	void setFont(KFont &font);

	/// 文字色を設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// 注意：これから追加される文字に対する設定なので、追加済みテキストの状態は変わらない
	void setFontColor(const Color32 &color);

	/// テクスチャ上で用意される文字色
	void setTextureGlyphColor(const Color32 &primary, const Color32 &secondary);

	/// 文字スタイルを設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// 注意：これから追加される文字に対する設定なので、追加済みテキストの状態は変わらない
	void setFontStyle(KFont::Style style);

	/// 文字サイズを設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// 文字サイズはピクセル単位で指定するが、必ずしも適切な行の高さと一致するわけではない。
	/// 行の推奨サイズ (LineHeight) を取得したければ KFont::getMetrics を使う。
	/// 注意：この設定は、次に追加した文字から適用される
	void setFontSize(float size);

	/// 文字ピッチ（標準文字送り量からのオフセット）を設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// 注意：この設定は、次に追加した文字から適用される
	void setFontPitch(float pitch);

	/// 現在のスタイルにユーザー定義のデータを関連付ける
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// 注意：この設定は、次に追加した文字から適用される
	void setUserData(int userdata);

	/// 行の高さを設定する。
	/// @note これは文字属性ではない。pushFontAttr, popFontAttr の影響を受けない
	/// 注意：この設定は、次の改行から適用される
	void setLineHeight(float height);

	/// カーニング
	void setKerning(bool value);

	float getLineHeight() const;

	/// 文字を追加する
	/// 改行したい時は明示的に newLine を呼ぶ
	void addChar(wchar_t code);

	void addString(const wchar_t *text);

	/// グループ化を開始する。
	/// ここから endGroup() を呼ぶまでの間に addChar した文字が１グループとして扱われる。
	/// グループ化すると途中で自動改行されなくなる
	/// このグループのグループ番号を返す
	int beginGroup();

	/// グループ化を終了する
	void endGroup();

	/// 指定されたグループにルビを振る。
	/// group_id ルビを振る対象となるグループ番号。このグループの文字列の上側中央寄せでルビ文字を設定する
	/// text ルビ文字列
	/// count ルビ文字列の長さ
	/// ルビ文字のサイズやスタイルには現在の文字スタイルがそのまま適用されるので、
	/// 事前にルビ用のサイズやスタイルを適用しておく。呼び出し例は以下の通り
	/// 
	/// textbox.pushFontAttr(); // 現在の文字設定を退避
	/// textbox.setFontSize(rubyfontsize); // ルビ文字のサイズ
	/// textbox.setFontStyle(KFont::NORMAL); // ルビ文字には通常スタイルの文字を適用
	/// textbox.setRuby(groupid, rubytext, rubysize); // ルビを設定
	/// textbox.popFontAttr(); // 文字設定戻す
	/// 
	void setRuby(int group_id, const wchar_t *text, int count);

	/// 改行する
	void newLine();

	/// インデックスを指定して文字情報を得る
	const Char & getCharByIndex(int index) const;

	/// このボックスが含んでいる文字数を得る
	int getCharCount() const;

	/// 指定した範囲にあるテキストのバウンドボックスを得る
	bool getBoundRect(int start, int count, Vec2 *out_topleft, Vec2 *out_bottomright) const;

	/// 指定したグループに文字列のバウンドボックスを得る
	bool getGroupBoundRect(int parent, Vec2 *out_topleft, Vec2 *out_bottomright) const;

	/// 指定した範囲にあるテキストを x を中心にした中央揃えになるように移動する
	bool horzCentering(int start, int count, float x);
	
	/// テキストを画像として取得する
	struct ExportDesc {
		ExportDesc() {
			xsize = 0;
			ysize = 0;
			margin_l = margin_r = margin_top = margin_btm = 4;
			show_margin = true;
			show_rowlines = true;
			show_boundbox = true;
			show_wrap = true;
		}
		int xsize;
		int ysize;
		int margin_l;
		int margin_r;
		int margin_top;
		int margin_btm;
		bool show_margin;
		bool show_rowlines;
		bool show_boundbox;
		bool show_wrap;
	};
	KImage exportImage(const ExportDesc *desc);

	/// 文字の実際の位置（テキストボックス原点からの相対座標）を返す
	Vec2 getCharPos(int index) const;

	/// テキストを描画するために必要な頂点数を返す (TriangleList)
	int getMeshVertexCount() const;

	/// テキストを描画の対象になる文字の数（コントロール文字を除外した文字数）
	int getMeshCharCount() const;

	/// テキストを描画するための頂点配列を作成する（ワールド座標系 左下原点 Y軸上向き）
	void getMeshVertexArray(Vertex *vertices, int max_count) const;

	/// 頂点座標配列を作成する（ワールド座標系 左下原点 Y軸上向き）
	/// buffer 座標配列の書き込み先
	/// max_bytes 書き込み可能な最大バイト数
	/// stride_bytes 次の座標までのバイト数（座標の書き込み先の先頭から、次の座標先頭までのバイト数）
	void getMeshPositionArray(void *buffer, int max_bytes, int stride_bytes) const;

	/// UV座標配列を作成する（テクスチャ座標系 左上原点 Y軸下向き）
	/// buffer 座標配列の書き込み先
	/// max_bytes 書き込み可能な最大バイト数
	/// stride_bytes 次の座標までのバイト数（座標の書き込み先の先頭から、次の座標先頭までのバイト数）
	void getMeshTexCoordArray(void *buffer, int max_bytes, int stride_bytes) const;

	/// カラー座標配列を作成する（RGBA float*4）
	/// buffer 座標配列の書き込み先
	/// max_bytes 書き込み可能な最大バイト数
	/// stride_bytes 次の座標までのバイト数（座標の書き込み先の先頭から、次の座標先頭までのバイト数）
	void getMeshColorArray(void *buffer, int max_bytes, int stride_bytes) const;

	/// index 番目の文字を描画するために必要なテクスチャを返す
	UserFontTexture getMeshTexture(int index) const;

	float getCharProgress(int index) const;
	void setCharProgress(int index, float progress);

	/// 文字の出現アニメを有効にするかどうか。
	/// true にすると setCharProgress での設定が有効になり、onGlyphMesh が呼ばれるようになる
	void setCharProgressEnable(bool value);
	
	/// 自動改行位置を決める時、code 位置の直前で改行してもよいか問い合わせるときに呼ばれる。
	/// 基本的には chr.code が行頭禁則文字であれば true を返すように実装する
	virtual bool dontBeStartOfLine(wchar_t code);

	/// 自動改行位置を決める時、code 位置の直後で改行してもよいか問い合わせるときに呼ばれる。
	/// 基本的には chr.code が行末禁則文字であれば true を返すように実装する
	virtual bool dontBeLastOfLine(wchar_t code);

	/// 独自のカーニングを施す場合に使う
	virtual void onKerning(wchar_t a, wchar_t b, float size, float *kerning) {}

	/// 文字を描画する直前に呼ばれる。
	/// chr.progress の値に応じて、実際の文字の色や矩形を設定する。
	/// setCharProgressEnable が true の場合のみ呼ばれる
	/// left, top, right, bottom にはこれから表示しようとしてる文字の矩形座標が入る。
	/// 必要に応じて、これらの値を書き換える
	virtual void onGlyphMesh(int index, const Char &chr, float *left, float *top, float *right, float *bottom) const {}

private:
	std::vector<Vec2> curstack_;
	std::vector<Attr> attrstack_;
	std::vector<Char> sequence_;
	Attr curattr_;
	float curx_;
	float cury_;
	float lineheight_;
	float auto_wrap_width_;
	int curlinestart_; // 現在の行頭にある文字のインデックス
	int curparent_;
	int rowcount_;
	bool progress_animation_; // true にした場合、progress の値によって文字の出現アニメーションが適用される
	bool kerning_; // カーニング使う
};


class KTextParser {
public:
	struct TEXT {
		const wchar_t *ws;
		int len;
	};

	KTextParser();

	/// 一行書式を追加する
	/// id 書式を識別するための番号.
	/// token 書式の開始記号。2文字以上でもよい。この記号から行末までがこの書式の適用範囲になる。例: "#" "//" 
	void addStyle(int id, const wchar_t *start);

	/// 範囲指定の書式を追加する
	/// id 書式を識別するための番号。
	/// start 書式の始まりを表す記号。2文字以上でもよい。例: "[" "[[" "<"
	/// end 書式の終了を表す記号。2文字以上でもよい。例: "]" "]]" ">"
	void addStyle(int id, const wchar_t *start, const wchar_t *end);

	/// パーサーを実行する
	void parse(const wchar_t *text);

	/// 書式開始記号を見つけたときに呼ばれる
	virtual void onStartStyle(int id) {}

	/// 書式終了記号を見つけたときに呼ばれる
	/// id 書式番号
	/// text 書式対象文字列。文字列が区切り文字 | で区切られている場合、最初の区切り文字よりも前半の部分が入る
	/// more 書式対象文字列が区切り文字 | で区切られている場合、最初の区切り文字よりも後ろの部分が入る
	virtual void onEndStyle(int id, const wchar_t *text, const wchar_t *more) {}

	/// 普通の文字を見つけたときに呼ばれる
	virtual void onChar(wchar_t chr) {}

private:
	struct STYLE {
		static const int MAXTOKENLEN = 8;
		int id;
		wchar_t start_token[MAXTOKENLEN];
		wchar_t end_token[MAXTOKENLEN];
	};
	/// スタイル開始
	/// stype スタイル開始位置などの情報
	/// pos スタイル開始の文字位置
	void startStyle(const STYLE *style, const wchar_t *pos);

	/// スタイル終了
	/// stype スタイル開始位置などの情報
	/// pos スタイル終了の文字位置
	void endStyle(const STYLE *style, const wchar_t *pos);

	const STYLE * isStart(const wchar_t *text) const;
	const STYLE * isEnd(const wchar_t *text) const;
	std::vector<STYLE> styles_;

	struct MARK {
		const STYLE *style;
		const wchar_t *start;
	};
	std::vector<MARK> stack_;
	wchar_t escape_;
};


void Test_textbox1();
void Test_textbox2();

}