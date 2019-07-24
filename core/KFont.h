#pragma once
#include "KImage.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace mo {

class FileInput;
class Path;

typedef void * UserFontTexture;

class UserFontTextureCallback {
public:
	virtual void on_update_texture(UserFontTexture tex, const KImage &image) = 0;
	virtual UserFontTexture on_new_texture(int w, int h) = 0;
};

/// フォント１文字の情報
/// 単位はすべてピクセル。
struct KGlyph {
	// GLYPH METRICS
	//
	//            |
	//            |   left     right
	//            |    +---------+ top
	//            |    |         |
	//            |    |         |
	//            |    |         |
	//            |    |         |
	//  baseline -O----+---------+----A---->
	//            |    |         | 
	//            |    +---------+ bottom
	//            |
	//            V
	// O: origin
	// A: next origin
	// advance: A - O
	//
	KGlyph() {
		advance = 0;
		left = top = right = bottom = 0;
		texture = (UserFontTexture)0;
		u0 = v0 = u1 = v1 = 0.0f;
	}

	/// 文字AABBの幅（ピクセル単位）
	int w() const { return right - left; }

	/// 文字AABBの高さ（ピクセル単位）
	int h() const { return bottom - top; }

	/// AABB 左端（オリジン基準）
	/// ※オリジン左端を超える可能性 (left < 0) に注意。(Aの左下や V の左上など）
	int left;

	/// AABB 右端（オリジン基準）
	int right;

	/// AABB 上端（オリジン基準、Y軸下向き）
	/// ※ベースライン下側 (top > 0) になる可能性に注意（アンダースコアなど）
	int top;

	/// AABB 下端（オリジン基準、Y軸下向き）
	/// ベースラインよりも上側 (bottom < 0) になる可能性に注意（ハットマーク ^ など）
	int bottom;

	/// 標準の文字送り量
	/// 現在のオリジンから次の文字のオリジンまでの長さ
	float advance;

	/// このグリフに関連付けられたユーザー定義のテクスチャ
	UserFontTexture texture;

	/// テクスチャ上でのグリフ範囲
	float u0, v0, u1, v1;
};

/// フォント全体のサイズ情報
/// 単位はすべてピクセル。
struct FontMetrics {
	// FONT METRICS
	//
	//            |
	//            A----+--------+ ascent (文字の最高)
	//            |    |        |
	//            |    |        |
	//            |    |        |
	//            |    |        |
	//  baseline -O----+--------+------------>
	//            |    |        |
	//            |    +--------+ descent (文字の最低点)
	//            |             |
	//            |             +-- linegap (最低点から行の基底までの余白)
	//            |             |
	//            B---------------------------
	//            |
	//            V
	// A-B: line height (行の高さ)
	//
	FontMetrics() {
		ascent = 0;
		descent = 0;
		linegap = 0;
		lineheight = 0;
		em_height = 0;
	}
	float ascent;
	float descent;
	float linegap;
	float lineheight;
	float em_height;
};

class KFont {
public:
	static void setUserTextureCallback(UserFontTextureCallback *cb);

	/// 文字スタイル
	enum Style {
		NORMAL,         ///< 通常文字
		BOLD,           ///< 太字
		OUTLINE,        ///< アウトライン（塗りつぶしなし）
		OUTLINE_FILLED, ///< アウトライン（塗りつぶしあり）
	};

	/// TrueType または OpenType フォントに格納されている名前情報の識別番号。
	///
	/// Naming Table
	/// http://www.microsoft.com/typography/otspec/name.htm
	///
	/// True Type Font のフォーマット
	/// https://www48.atwiki.jp/sylx/pages/19.html
	///
	/// 使用例（ＭＳ ゴシック）
	/// KFont font;
	/// font.loadFromFileName("c:\\windows\\fonts\\msgothic.ttc", 0);
	/// font.getNameInJapanese(NID_FAMILY, buf);
	///
	/// 取得例（ＭＳ ゴシック）
	///
	/// NID_COPYRIGHT : "(c) 2017 data:株式会社リコー typeface:リョービイマジクス(株)" 
	/// NID_FAMILY    : "ＭＳ ゴシック"
	/// NID_SUBFAMILY : "標準"
	/// NID_FONTID    : "Microsoft:ＭＳ ゴシック"
	/// NID_FULLNAME  : "ＭＳ ゴシック"
	/// NID_VERSION   : "Version 5.11"
	/// NID_POSTSCRIPT: "MS-Gothic"
	/// NID_TRADEMARK : "ＭＳ ゴシックは米国 Microsoft Corporation の米国及びその他の国の登録商標または商標です。"
	///
	enum NameId {
		NID_COPYRIGHT  = 0,
		NID_FAMILY     = 1,
		NID_SUBFAMILY  = 2,
		NID_FONTID     = 3,
		NID_FULLNAME   = 4,
		NID_VERSION    = 5,
		NID_POSTSCRIPT = 6,
		NID_TRADEMARK  = 7,
	};
	enum Flag {
		F_BOLD       = 1,
		F_ITALIC     = 2,
		F_UNDERSCORE = 4,
	};
	typedef int Flags;

	KFont();
	KFont(const KFont &other);
	~KFont();
	KFont & operator = (const KFont &other);
	bool operator == (const KFont &other) const;

	bool loadFromFile(FileInput &file, int ttc_index=0);
	bool loadFromFileName(const Path &filename, int ttc_index=0);
	bool loadFromFileNameInSystemFontDirectory(const Path &filename, int ttc_index=0);
	bool loadFromMemory(const void *data, int size, int ttc_index=0);
	bool empty() const;
	int id() const;


	/// フォントフラグを取得する
	Flags getFlags() const;

	/// TrueType または OpenType フォントに格納されている英語の名前情報を name にセットして true を返す。
	/// 名前が取得できなかった場合は何もせずに false を返す。
	/// 比較的長い文字列が格納されている場合もあるので注意する事。
	/// 例えば Arial (arial.ttc) の CopyRight 表記は複数行から成り、改行文字を含めて271バイトある
	bool getNameInEnglish(NameId nid, wchar_t *name) const;

	/// TrueType または OpenType フォントに格納されている日本語の名前情報を返す
	bool getNameInJapanese(NameId nid, wchar_t *name) const;

	/// chr で指定した文字の、サイズ height における字形を得る
	KGlyph getGlyph(wchar_t chr, float fontsize, Style style=NORMAL, bool with_alpha=true, const Color32 &primary=Color32::WHITE, const Color32 &secondary=Color32::BLACK) const;

	/// 指定されたフォントサイズにおける行情報
	FontMetrics getMetrics(float fontsize) const;

	/// chr で指定した文字の、サイズ height における字形を img に書き込む。
	/// img のビット深度は 8 固定つまり１ピクセル＝１バイトとして書き込む。
	/// ビットマップフォントの場合 height で指定したサイズのグリフが得られるとは限らない。
	/// 指定された文字を含んでいない場合は何もせずに false を返す
	/// img, gm は NULL でもよい
	KImage getGlyphImage8(wchar_t chr, float fontsize) const;
	KImage getGlyphImage32(wchar_t chr, float fontsize, Style style=NORMAL, bool with_alpha=true, const Color32 &primary=Color32::WHITE, const Color32 &secondary=Color32::BLACK) const;

	/// 文字 chr1 と chr2 を並べるときの文字送り調整量をピクセル単位で返す。
	/// chr1 の FontGlyph::advance で指定された文字送り量から、さらに追加するピクセル数を返す。
	/// カーニングしない場合は 0 を返し、文字間を詰める必要があるなら負の値を返す。
	/// 文字送り調整量はフォントサイズ height_in_pixel に比例する
	float getKerningAdvanve(wchar_t chr1, wchar_t chr2, float fontsize) const;


	/// Implement Interface
	class Impl {
	public:
		virtual ~Impl() {}

		/// サイズ fontsize における行情報
		virtual bool get_metrics(float fontsize, FontMetrics *metrics) const = 0;

		/// chr で指定した文字の、サイズ fontsize における字形を img に書き込む。
		/// img のフォーマットは KImage::INDEX8（１ピクセル＝１バイト）のみ対応する。
		/// ビットマップフォントの場合 fontsize で指定したサイズのグリフが得られるとは限らない。
		/// 指定された文字を含んでいない場合は何もせずに false を返す
		/// glyph, image は NULL でもよい
		virtual bool get_glyph(wchar_t chr, float fontsize, KGlyph *glyph, KImage *image) const = 0;

		/// 文字 chr1 と chr2 を並べるときの文字送り調整量をピクセル単位で返す。
		/// 調整量とは、chr1 の文字送り慮う advance から、さらにプラスする値のこと。
		/// この値は 0 なら標準文字送り量のままで、正の値なら広くなり、負の値なら狭くなる。
		/// 文字送り調整量はフォントサイズ fontsize に比例する
		virtual float get_kerning(wchar_t chr1, wchar_t chr2, float fontsize) const { return 0.0f; }

		/// NameId で指定された情報文字列（日本語）を得る。文字列が存在しないか未対応の場合は false を返す
		virtual bool get_name_jp(KFont::NameId nid, wchar_t *name) const { return false; }

		/// NameId で指定された情報文字列（英語）を得る。文字列が存在しないか未対応の場合は false を返す
		virtual bool get_name_en(KFont::NameId nid, wchar_t *name) const { return false; }

		/// フォントスタイルのフラグ
		virtual Flags get_flags() const { return 0; }
	};

private:
	std::shared_ptr<Impl> impl_;
};





class FontFile {
public:
	/// フォントファイルを開く
	bool loadFromFile(FileInput &file);

	/// フォントファイルを開く
	bool loadFromFileName(const Path &filename);

	/// システムフォントフォルダ内にあるフォントファイルを開く
	bool loadFromFileNameInSystemFontDirectory(const Path &filename);

	/// フォントファイルを開く
	bool loadFromMemory(const void *data, int size);

	/// フォントファイルが含んでいるフォントの数を返す
	int getCount() const;

	/// フォントファイルが含んでいるフォントを返す
	/// ttc_index にはフォント番号を指定する。複数のフォントを含んでいない場合は 0 を指定する
	KFont getFont(int ttc_index) const;

	/// Implement Interface
	class Impl {
	public:
		virtual ~Impl() {}
		virtual bool on_load(const void *data, int size) = 0;
		virtual int  on_query_size() const = 0;
		virtual bool on_query_font(KFont *font, int ttc_index) const = 0;
	};

private:
	std::shared_ptr<Impl> impl_;
};

void Test_font();
void Test_font_printInfo(const wchar_t *filename);

} // namespace

