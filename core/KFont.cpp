#include "KFont.h"
#include "KFile.h"
#include "KPath.h"

#ifdef _WIN32
#	include <Shlobj.h> // SHGetFolderPathW
#	include <Windows.h> // GetStringTypeW
#endif

// stb_truetype
// https://github.com/nothings/stb
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define _MAX(a, b) ((a) > (b) ? (a) : (b))


#if 1
#include "KLog.h"
#define FONTASSERT Log_assert
#else
#include <assert.h>
#define FONTASSERT assert
#endif


namespace mo {



static bool _Iswprint(wchar_t wc) {
#ifdef _WIN32
	WORD t = 0;
	GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && !(t & C1_CNTRL);
#else
	// MSVC以外の場合、locale がちゃんと設定されていれば iswprint は期待通りに動作するが、
	// locale 未設定だと ascii と重複する範囲でしか正常動作しないので注意
	assert(0 && "ここのコードは未検証であり、正常動作する保証はない");
	// return iswprint(wc);
	return !iswcntrl(wc);
#endif
}


namespace {

class CNanoFont: public KFont::Impl {
public:
	static const int COL_COUNT = 16;
	static const int ROW_COUNT = 6;
	static const int CHAR_W    = 6;
	static const int CHAR_H    = 9;
	static const int BASELINE  = 6;
	static const int BMP_W     = CHAR_W * COL_COUNT;
	static const int BMP_H     = CHAR_H * 6;
	
	CNanoFont() {
	}
	static const char * get_table_static() {
		static const char *data =
			" !\"#$%&'()*+,-./"
			"0123456789:;<=>?"
			"@ABCDEFGHIJKLMNO"
			"PQRSTUVWXYZ[\\]^_"
			" abcdefghijklmno"
			"pqrstuvwxyz{|}~ ";
		return data;
	}
	static const char * get_bitmap_static() {
		static const char * data =
			//     6     12    18    24    30    36    42    48    54    60    66    72    78    84    90    96
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"        #    # #   # #   ####     #   ##    #      #   #                                        "
			"        #    # #   # #  # #   ## #   #      #     #     #   # # #   #                         # "
			"        #    # #  ##### # #   ## #   #     #      #     #    ###    #                        #  "
			"        #          # #   ###    #    #  #         #     #     #   #####       #####         #   "
			"        #          # #    # #   #   # # #         #     #    ###    #                      #    "
			"                  #####   # #  # ## #  #          #     #   # # #   #    ##          ##   #     "
			"        #          # #  ####  #  ##  ## #          #   #                 ##          ##         " // <== baseline
			"                                                                          #                     "
			"                                                                         #                      "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			" ###   ##    ###   ###  #   # ####   ##   #####  ###   ###                 #         #     ###  "
			"#   #   #   #   # #   # #   # #     #     #   # #   # #   #  ##   ##      #           #   #   # "
			"#  ##   #       #     # #   # #     #         # #   # #   #  ##   ##     #    #####    #      # "
			"# # #   #     ##   ###  ##### ####  ####     #   ###   ####             #               #   ##  "
			"##  #   #    #        #     #     # #   #    #  #   #     #              #    #####    #    #   "
			"#   #   #   #     #   #     #     # #   #   #   #   #     #  ##   ##      #           #         "
			" ###   ###  #####  ###      #  ###   ###    #    ###    ##   ##   ##       #         #      #   " // <== baseline
			"                                                                   #                            "
			"                                                                  #                             "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			" ###   ###  ####   ###  ####  ##### #####  ###  #   # #####  #### #   # #     #   # #   #  ###  "
			"#   # #   # #   # #   # #   # #     #     #     #   #   #      #  #  #  #     ## ## #   # #   # "
			"# ### #   # #   # #     #   # #     #     #     #   #   #      #  #  #  #     ## ## ##  # #   # "
			"# # # #   # ####  #     #   # ####  ####  # ### #####   #      #  ###   #     # # # # # # #   # "
			"# ### ##### #   # #     #   # #     #     #   # #   #   #      #  #  #  #     # # # #  ## #   # "
			"#     #   # #   # #   # #   # #     #     #   # #   #   #      #  #  #  #     #   # #   # #   # "
			" #### #   # ####   ###  ####  ##### #      #### #   # ##### ###   #   # ##### #   # #   #  ###  " // <== baseline
			"                                                                                                "
			"                                                                                                "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"####   ###  ####   ###  ##### #   # #   # #   # #   # #   # #####   ### #   # ###     #         "
			"#   # #   # #   # #   #   #   #   # #   # #   #  # #  #   #     #   #    # #    #    # #        "
			"#   # #   # #   # #       #   #   # #   # # # #  # #  #   #    #    #     #     #   #   #       "
			"####  # # # ####   ###    #   #   # #   # # # #   #    ###    #     #   #####   #               "
			"#     # # # #  #      #   #   #   #  # #  # # #  # #    #    #      #     #     #               "
			"#     #  #  #  #  #   #   #   #   #  # #  # # #  # #    #   #       #   #####   #               "
			"#      ## # #   #  ###    #    ###    #    # #  #   #   #   #####   ###   #   ###         ##### " // <== baseline
			"                                                                                                "
			"                                                                                                "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"            #               #          ##       #       #      #  #     ###                     "
			"            #               #         #         #                 #       #                     "
			"       ###  ####   ####  ####  ###  #####  #### # ##  ###     ##  #   #   #   ####  ####   ###  "
			"          # #   # #     #   # #   #   #   #   # ##  #   #      #  # ##    #   # # # #   # #   # "
			"       #### #   # #     #   # #####   #   #   # #   #   #      #  ##      #   # # # #   # #   # "
			"      #   # #   # #     #   # #       #   #   # #   #   #      #  # ##    #   # # # #   # #   # "
			"       #### ####   ####  ####  ###    #    #### #   # #####    #  #   ##  ### # # # #   #  ###  " // <== baseline
			"                                              #             #  #                                "
			"                                           ###               ##                                 "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"                          #                                          ##   #   ##     ##  #      "
			"                          #                                         #     #     #   #  ##       "
			"####   #### # ###  ###  ##### #   # #   # # # # #   # #   # #####   #     #     #               "
			"#   # #   # ##    #       #   #   # #   # # # #  # #  #   #    #  ##      #      ##             "
			"#   # #   # #      ###    #   #   #  # #  # # #   #    ####   #     #     #     #               "
			"#   # #   # #         #   #   #   #  # #  # # #  # #      #  #      #     #     #               "
			"####   #### #     ####     ##  ####   #    # #  #   #  ###  #####    ##   #   ##                " // <== baseline
			"#         #                                                                                     "
			"#         #                                                                                     "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
		; // <== end of data
		FONTASSERT(sizeof(data) == BMP_W * BMP_H);
		return data;
	}

	virtual bool get_glyph(wchar_t chr, float fontsize, KGlyph *glyph, KImage *image) const override {

		// 文字テーブルから目的の文字を見つける。
		// 見つからなければ失敗
		const char *p = strchr(get_table_static(), (int)chr);
		if (p == NULL) {
			return false;
		}
		
		if (glyph) {
			glyph->advance     = CHAR_W;
			glyph->left        = 0; // ベースライン左端を起点とする文字範囲
			glyph->bottom      = BASELINE - CHAR_H; // 文字範囲の下端。ベースライン基準のＹ軸上向きにするため、ビットマップのＹ座標を反転させる
			glyph->right       = CHAR_W;
			glyph->top         = BASELINE; // 文字範囲の上端。ベースライン基準のＹ軸上向きにするため、ビットマップのＹ座標を反転させる
			
			glyph->texture = 0;
			glyph->u0 = 0;
			glyph->v0 = 0;
			glyph->u1 = 0;
			glyph->v1 = 0;
		}

		if (image) {
			// ビットマップ情報
			const char *bitmap = get_bitmap_static();

			// 画像データを作成する
			KImage img(CHAR_W, CHAR_H, KImage::INDEX8);
			uint8_t *pDst = img.lockData();
			if (pDst) {
				for (int y=0; y<CHAR_H; y++) {
					for (int x=0; x<CHAR_W; x++) {
						char c = bitmap[BMP_W * y + x];
						int idx = CHAR_W * y + x;
						uint8_t val = (c == '#') ? 0xFF : 0x00;
						pDst[idx] = val; // A
					}
				}
				img.unlockData();
			}
			*image = img;
		}
		return true;
	}
};


class CStbFont: public KFont::Impl {
public:
	std::string bin_;
	stbtt_fontinfo info_;

	CStbFont(FileInput &file, int ttc_index, bool *success) {
		FONTASSERT(success);
		memset(&info_, 0, sizeof(info_));
		size_t binsize = file.size();
		bin_ = file.readBin(binsize);
		if (bin_.data()) {
			int offset = stbtt_GetFontOffsetForIndex((unsigned char *)bin_.data(), ttc_index);
			*success = stbtt_InitFont(&info_, (unsigned char *)bin_.data(), offset);
		} else {
			*success = false;
		}
	}

	/// TrueType に埋め込まれている文字列を得る
	/// nid 名前番号
	/// langID 言語番号
	///     1033 (0x0409) = 米国英語
	///     1041 (0x0411) = 日本語
	///     ロケール ID (LCID) の一覧
	///     "https://docs.microsoft.com/ja-jp/previous-versions/windows/scripting/cc392381(v=msdn.10)"
	///
	bool getFontNameString(KFont::NameId nid, int langID, wchar_t *ws) const {
		FONTASSERT(ws);
		int name_bytes = 0;

		// フォントに埋め込まれている文字列を得る。
		// この文字列は2バイトのビッグエンディアンでエンコードされている
		const char *name_u16be = stbtt_GetFontNameString(
			&info_,
			&name_bytes,
			STBTT_PLATFORM_ID_MICROSOFT,
			STBTT_MS_EID_UNICODE_BMP,
			langID,
			nid);

		// ビッグエンディアンで得られた文字列をリトルエンディアンに変換する
		if (name_u16be && name_bytes > 0) {
			int name_len = name_bytes / 2;
			for (int i=0; i<name_len; i++) {
				uint8_t hi = name_u16be[i*2  ];
				uint8_t lo = name_u16be[i*2+1];
				ws[i] = (hi << 8) | lo;
			}
			ws[name_len] = '\0';
		}
		return name_bytes > 0;
	}

	virtual bool get_metrics(float fontsize, FontMetrics *metrics) const override {
		// フォントデザイン単位からピクセルへの変換係数
		const float scale = stbtt_ScaleForPixelHeight(&info_, (float)fontsize);
		if (scale <= 0) return false;

		// フォントデザイン単位(Design Units)と座標系でのメトリクス (Ｙ軸上向き、原点はデザイナーに依存）
		int du_ascent;  // 標準文字の上端
		int du_descent; // 標準文字の下端
		int du_linegap; // 文字下端から行の下端（＝次行の上端）までのスペース
		stbtt_GetFontVMetrics(&info_, &du_ascent, &du_descent, &du_linegap);

		// 大文字の M のサイズ
		int bmp_top;    // 文字AABBの上端。Y軸下向きなので、ベースラインよりも高ければ負の値になる
		int bmp_bottom; // 文字AABBの下端。Y軸下向きなので、ベースラインよりも低ければ正の値になる
		stbtt_GetCodepointBitmapBoxSubpixel(&info_, 'M', scale, scale, 0, 0, NULL, &bmp_top, NULL, &bmp_bottom);

		if (metrics) {
			metrics->ascent = du_ascent * scale;
			metrics->descent = du_descent * scale;
			metrics->linegap = du_linegap * scale;
			metrics->lineheight = (metrics->ascent - metrics->descent) + metrics->linegap;
			metrics->em_height = bmp_bottom - bmp_top + 1;
		}
		return true;
	}

	virtual bool get_glyph(wchar_t chr, float fontsize, KGlyph *glyph, KImage *image) const override {
		if (!_Iswprint(chr)) {
			// 印字可能ではない（改行文字、制御文字など）
			if (glyph) *glyph = KGlyph();
			if (image) *image = KImage();
			return true;
		}

		// フォントデザイン単位からピクセルへの変換係数
		const float scale = stbtt_ScaleForPixelHeight(&info_, (float)fontsize);
		if (scale <= 0) return false;

		// フォントデザイン単位(Design Units)と座標系でのメトリクス (Ｙ軸上向き、原点はデザイナーに依存）
		int du_ascent;  // 標準文字の上端
		int du_descent; // 標準文字の下端
		int du_linegap; // 文字下端から行の下端（＝次行の上端）までのスペース
		stbtt_GetFontVMetrics(&info_, &du_ascent, &du_descent, &du_linegap);

		int du_advance;
		int du_bearing_left;
		stbtt_GetCodepointHMetrics(&info_, chr, &du_advance, &du_bearing_left);

		// 文字ビットマップ座標系でのメトリクス（ベースライン左端を原点、Y軸下向き）
		int bmp_left;   // 文字AABBの左端。du_bering_left * unit_scale と同じ
		int bmp_right;  // 文字AABBの右端
		int bmp_top;    // 文字AABBの上端。Y軸下向きなので、ベースラインよりも高ければ負の値になる
		int bmp_bottom; // 文字AABBの下端。Y軸下向きなので、ベースラインよりも低ければ正の値になる
		stbtt_GetCodepointBitmapBoxSubpixel(&info_, chr, scale, scale, 0, 0, &bmp_left, &bmp_top, &bmp_right, &bmp_bottom);
		FONTASSERT(bmp_left <= bmp_right);
		FONTASSERT(bmp_top <= bmp_bottom);

		if (glyph) {
			glyph->advance = (float)du_advance * scale;
			glyph->left   = bmp_left;
			glyph->bottom = bmp_bottom;
			glyph->right  = bmp_right;
			glyph->top    = bmp_top;
			
			glyph->texture = 0;
			glyph->u0 = 0;
			glyph->v0 = 0;
			glyph->u1 = 0;
			glyph->v1 = 0;
		}

	//	bool no_glyph = stbtt_IsGlyphEmpty(&info_, stbtt_FindGlyphIndex(&info_, chr));
		if (image) {
			int w = bmp_right - bmp_left;
			int h = bmp_bottom - bmp_top;
			if (w == 0 || h == 0) {
				// 字形が存在しない(空白文字や制御文字)
				// ちなみに字形が存在しなくても、空白文字の場合はちゃんと文字送り量 advance が指定されているので、無視してはいけない
				*image = KImage();
			} else {
				KImage img(w, h, KImage::INDEX8);
				uint8_t *pDst = img.lockData();
				stbtt_MakeCodepointBitmapSubpixel(&info_, pDst, w, h, w, scale, scale, 0, 0, chr);
				img.unlockData();
				*image = img;
			}
		}
		return true;
	}
	virtual float get_kerning(wchar_t chr1, wchar_t chr2, float fontsize) const override {
		float scale = stbtt_ScaleForPixelHeight(&info_, fontsize);
		float kern = (float)stbtt_GetCodepointKernAdvance(&info_, chr1, chr2);
		return kern * scale;
	}
	virtual bool get_name_jp(KFont::NameId nid, wchar_t *name) const override {
		return getFontNameString(nid, STBTT_MS_LANG_JAPANESE, name);
	}
	virtual bool get_name_en(KFont::NameId nid, wchar_t *name) const override {
		return getFontNameString(nid, STBTT_MS_LANG_ENGLISH, name);
	}
	virtual KFont::Flags get_flags() const override { 
		// 注意
		// K_stbtt_GetFlags は独自に追加した関数であり、stb_truetype には元々含まれていない。
		// フォントスタイルが Bold か Italic か、などはフラグに保持されているが、
		// stb_truetype にはフラグを単独で取得する関数が用意されていなかった。
		// 比較する関数 (stbtt_FindMatchingFont) ならあるが、これはフラグのほかに名前も比較してしまうため、
		// それを元にフラグだけを返す関数を追加してある。
		// 定義は以下の通り: フラグは STBTT_MACSTYLE_*** で調べる
		//	STBTT_DEF int K_stbtt_GetFlags(stbtt_fontinfo *info) {
		//		return ttSHORT(info->data + info->head + 44);
		//	}
		int flags = K_stbtt_GetFlags(&info_);
		// フラグの値に互換性があることを確認し、stbtt から得られた値をそのまま返す
		FONTASSERT(KFont::F_BOLD == STBTT_MACSTYLE_BOLD);
		FONTASSERT(KFont::F_ITALIC == STBTT_MACSTYLE_ITALIC);
		FONTASSERT(KFont::F_UNDERSCORE == STBTT_MACSTYLE_UNDERSCORE);
		return flags;
	}
};


class CGlyphPack {
public:
	static const int MARGIN = 1;
	static const int TEXSIZE = 1024 * 2;
	
	struct _PAGE {
		UserFontTexture tex;
		KImage img;
		int ymax;
		int xcur, ycur;
	};

	std::vector<_PAGE> pages_;
	std::unordered_map<std::string, KGlyph> table_;
	UserFontTextureCallback *cb_;
	
	
	CGlyphPack() {
		cb_ = NULL;
	}
	
	bool getGlyph(const std::string &key, KGlyph *glyph) {
		auto it = table_.find(key);
		if (it != table_.end()) {
			*glyph = it->second;
			return true;
		}
		return false;
	}

	/// glyph にはテクスチャフィールド以外が埋まったものを指定する。
	/// テクスチャへの追加が成功すれば、テクスチャとUV座標フィールドに値をセットして true を返す
	bool addGlyph(const std::string &key, const KImage &img32, KGlyph *glyph) {
		FONTASSERT(glyph);
		// img32 は余白なしのグリフ画像
		int w = img32.getWidth();
		int h = img32.getHeight();
		_PAGE *page = getPageToWrite(w, h);
		if (page) {
			page->img.copyImage(page->xcur, page->ycur, img32);
			page->ymax = _MAX(page->ymax, page->ycur + h + MARGIN);
			if (cb_) cb_->on_update_texture(page->tex, page->img);
			glyph->texture = page->tex;
			glyph->u0 = (float)(page->xcur + 0) / TEXSIZE;
			glyph->v0 = (float)(page->ycur + 0) / TEXSIZE;
			glyph->u1 = (float)(page->xcur + w) / TEXSIZE;
			glyph->v1 = (float)(page->ycur + h) / TEXSIZE;
			page->xcur += w + MARGIN;
			table_[key] = *glyph;
			return true;
		}
		return false;
	}
	// サイズ w,　h の画像の追加先を得る
	_PAGE * getPageToWrite(int w, int h) {
		if (pages_.size() > 0) {
			_PAGE *page = &pages_.back();
			// 現ページの書き込み位置にそのまま収まる？
			{
				int wspace = page->img.getWidth()  - page->xcur;
				int hspace = page->img.getHeight() - page->ycur;
				if (wspace >= w && hspace >= h) {
					return page;
				}
			}
			// 改行
			page->xcur = 0;
			page->ycur = page->ymax;
			// この状態でもう一度確認
			{
				int wspace = page->img.getWidth()  - page->xcur;
				int hspace = page->img.getHeight() - page->ycur;
				if (wspace >= w && hspace >= h) {
					return page;
				}
			}
		}

		// 新規ページを追加
		if (cb_) {
			_PAGE page;
			page.img = KImage(TEXSIZE, TEXSIZE);
			page.tex = cb_->on_new_texture(TEXSIZE, TEXSIZE);
			page.xcur = 0;
			page.ycur = 0;
			page.ymax = 0;
			pages_.push_back(page);
			return &pages_.back();
		}
		return NULL;
	}
};


class CFontFileImpl: public FontFile::Impl {
public:
	std::string bin_;
	int numfonts_;

	CFontFileImpl() {
		numfonts_ = 0;
	}
	virtual bool on_load(const void *data, int size) override {
		if (data && size > 0) {
			bin_.resize(size);
			memcpy(&bin_[0], data, size);
			numfonts_ = stbtt_GetNumberOfFonts((uint8_t*)&bin_[0]);
		}
		return numfonts_ > 0;
	}
	virtual int on_query_size() const override {
		return numfonts_;
	} 
	virtual bool on_query_font(KFont *font, int ttc_index) const override {
		if (0 <= ttc_index && ttc_index < numfonts_) {
			return font->loadFromMemory(bin_.data(), bin_.size(), ttc_index);
		}
		return false;
	}
};



} // anonymous


#pragma region FontFile
bool FontFile::loadFromFile(FileInput &file) {
	std::string bin = file.readBin();
	return loadFromMemory(bin.data(), bin.size());
}
bool FontFile::loadFromFileName(const Path &filename) {
	FileInput file;
	if (file.open(filename)) {
		return loadFromFile(file);
	}
	return false;
}
bool FontFile::loadFromFileNameInSystemFontDirectory(const Path &filename) {
#ifdef _WIN32
	{
		wchar_t dir[Path::SIZE] = {0};
		if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, dir))) {
			Path fontpath = Path(dir).join(filename);
			return loadFromFileName(fontpath);
		}
		return false;
	}
#else
	return false;
#endif
}
bool FontFile::loadFromMemory(const void *data, int size) {
	impl_ = std::make_shared<CFontFileImpl>();
	if (impl_->on_load(data, size)) {
		return true;
	}
	impl_ = NULL;
	return false;
}
int FontFile::getCount() const {
	if (impl_) {
		return impl_->on_query_size();
	}
	return 0;
}
KFont FontFile::getFont(int ttc_index) const {
	KFont font;
	if (impl_) {
		impl_->on_query_font(&font, ttc_index);
	}
	return font;
}
#pragma endregion // FontFile



static CGlyphPack g_FontAtlas;
static std::string _GetGlyphKey(const KFont &font, wchar_t chr, float fontsize, KFont::Style style, bool with_alpha, const Color32 &primary, const Color32 &secondary) {
	int sizekey = (int)(fontsize * 10); // 0.1刻みで識別できるようにしておく

	char s[256];
	sprintf(s, "%d|%d|%d|%d|%d|x%x|x%x", font.id(), chr, sizekey, style, with_alpha, primary.toUInt32(), secondary.toUInt32());
	return s;
}

#pragma region KFont
KFont::KFont() {
	impl_ = NULL;
}
KFont::KFont(const KFont &other) {
	impl_ = other.impl_;
}
KFont::~KFont() {
}
KFont & KFont::operator = (const KFont &other) {
	impl_ = other.impl_;
	return *this;
}
bool KFont::operator == (const KFont &other) const {
	return impl_ == other.impl_;
}
bool KFont::empty() const {
	return impl_ == NULL;
}
int KFont::id() const {
	return (int)impl_.get();
}
bool KFont::loadFromFile(FileInput &file, int ttc_index) {
	impl_ = NULL;
	if (file.isOpen()) {
		bool success = false;
		impl_ = std::make_shared<CStbFont>(file, ttc_index, &success);
		if (success) {
			return true;
		}
		impl_ = NULL;
	}
	return false;
}
bool KFont::loadFromFileName(const Path &filename, int ttc_index) {
	FileInput file;
	if (file.open(filename)) {
		return loadFromFile(file, ttc_index);
	}
	return false;
}
bool KFont::loadFromFileNameInSystemFontDirectory(const Path &filename, int ttc_index) {
#ifdef _WIN32
	{
		wchar_t dir[Path::SIZE] = {0};
		if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, dir))) {
			Path fontpath = Path(dir).join(filename);
			return loadFromFileName(fontpath, ttc_index);
		}
		return false;
	}
#else
	return false;
#endif
}
bool KFont::loadFromMemory(const void *data, int size, int ttc_index) {
	FileInput file;
	if (file.open(data, size)) {
		return loadFromFile(file, ttc_index);
	}
	return false;
}

#define OUTINE_WIDTH 1
#define BOLD_INFLATE 2

/// 文字画像 (Alpha8) をRGB32に転写したグレースケール不透明画像を作成する
static KImage Glyph_toColor(const KImage &raw, const Color32 &color) {
	if (raw.empty()) return KImage();
	int w = raw.getWidth();
	int h = raw.getHeight();
	KImage img(w, h);
	const uint8_t *src = raw.getData();
	uint8_t *dst = img.lockData();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			int i = w * y + x;
			dst[i*4+0] = src[i] * color.r / 255; // R
			dst[i*4+1] = src[i] * color.g / 255; // G
			dst[i*4+2] = src[i] * color.b / 255; // B
			dst[i*4+3] = 255; // A
		}
	}
	img.unlockData();
	return img;
}

/// 文字画像 (Alpha8) をRGB32に転写したアルファチャンネル画像を作成する
static KImage Glyph_toAlpha(const KImage &raw, const Color32 &color) {
	if (raw.empty()) return KImage();
	int w = raw.getWidth();
	int h = raw.getHeight();
	KImage img(w, h);
	const uint8_t *src = raw.getData();
	uint8_t *dst = img.lockData();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			int i = w * y + x;
			dst[i*4+0] = color.r; // R
			dst[i*4+1] = color.g; // G
			dst[i*4+2] = color.b; // B
			dst[i*4+3] = src[i]; // A
		}
	}
	img.unlockData();
	return img;
}

inline uint8_t uint8_add(uint8_t a, uint8_t b) {
#if 0
	// ADD
	int val = (int)a + (int)b;
#else
	// SCREEN
	int val = (int)a + (int)(255 - a) * b / 255;
#endif
	if (val < 255) {
		return (uint8_t)val;
	} else {
		return 255;
	}
}

/// 文字画像 (Alpha8) を右に1ドット太らせる
static KImage Glyph_bold(const KImage &raw) {
	if (raw.empty()) return KImage();
	FONTASSERT(raw.getFormat() == KImage::INDEX8);
	int sw = raw.getWidth();
	int dw = sw + BOLD_INFLATE;
	int h = raw.getHeight();
	KImage img(dw, h, KImage::INDEX8);
	const uint8_t *src = raw.getData();
	uint8_t *dst = img.lockData();
	switch (BOLD_INFLATE) {
	case 1:
		for (int y=0; y<h; y++) {
			for (int x=0; x<sw; x++) {
				int si = sw * y + x;
				int di = dw * y + x;
				dst[di  ] = uint8_add(dst[di],   src[si]);
				dst[di+1] = uint8_add(dst[di+1], src[si]);
			}
		}
		break;
	case 2:
		for (int y=0; y<h; y++) {
			for (int x=0; x<sw; x++) {
				int si = sw * y + x;
				int di = dw * y + x;
				dst[di  ] = uint8_add(dst[di],   src[si]);
				dst[di+1] = uint8_add(dst[di+1], src[si]);
				dst[di+2] = uint8_add(dst[di+2], src[si]);
			}
		}
		break;
	}
	img.unlockData();
	return img;
}

/// 文字画像 (Alpha8) を上下左右に1ドット太らせる
static KImage Glyph_inflate(const KImage &raw) {
	if (raw.empty()) return KImage();
	FONTASSERT(raw.getFormat() == KImage::INDEX8);
	int w = raw.getWidth();
	int h = raw.getHeight();
	KImage img(w+2, h+2, KImage::INDEX8);
	const uint8_t *src = raw.getData();
	uint8_t *dst = img.lockData();
	int sw = w;
	int dw = w + 2;
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			int si = sw * y + x;
			int dx = 1 + x;
			int dy = 1 + y;
			int di = dw * dy + dx;
			uint8_t src_v = src[si];
			di = dw * (dy-1) + dx;
			dst[di-1] = _MAX(dst[di-1], src_v);
			dst[di  ] = _MAX(dst[di  ], src_v);
			dst[di+1] = _MAX(dst[di+1], src_v);
			di = dw * (dy  ) + dx;
			dst[di-1] = _MAX(dst[di-1], src_v);
			dst[di  ] = _MAX(dst[di  ], src_v);
			dst[di+1] = _MAX(dst[di+1], src_v);
			di = dw * (dy+1) + dx;
			dst[di-1] = _MAX(dst[di-1], src_v);
			dst[di  ] = _MAX(dst[di  ], src_v);
			dst[di+1] = _MAX(dst[di+1], src_v);
		}
	}
	img.unlockData();
	return img;
}

/// 文字画像 (Alpha8) から val を引く
static KImage Glyph_sub(const KImage &raw, int px, int py, const KImage &raw2) {
	if (raw.empty())  return KImage();
	if (raw2.empty()) return raw;

	FONTASSERT(raw.getFormat() == KImage::INDEX8);
	FONTASSERT(raw2.getFormat()== KImage::INDEX8);

	// rawをコピー
	KImage img = raw.clone();

	// raw2 と重なる部分だけ演算する
	const uint8_t *rawbuf = raw2.getData();
	uint8_t *imgbuf = img.lockData();
	int dw = raw.getWidth();
	int sw = raw2.getWidth();
	int sh = raw2.getHeight();
	for (int y=0; y<sh; y++) {
		for (int x=0; x<sw; x++) {
			int dx = px + x;
			int dy = py + y;
			int di = dw * dy + dx;
			int si = sw *  y +  x;
			if (imgbuf[di] > rawbuf[si]) {
				imgbuf[di] -= rawbuf[si];
			} else {
				imgbuf[di] = 0;
			}
		}
	}
	img.unlockData();
	return img;
}

class BlendOutlineFilled {
public:
	Color32 operator()(const Color32 &dst, const Color32 &src) const {
		Color32 out;
		int inv_a = 255 - src.a;
		out.r = (uint8_t)((int)(dst.r * inv_a + src.r * src.a) / 255);
		out.g = (uint8_t)((int)(dst.g * inv_a + src.g * src.a) / 255);
		out.b = (uint8_t)((int)(dst.b * inv_a + src.b * src.a) / 255);
		out.a = dst.a;
		return out;
	}
};

KImage KFont::getGlyphImage8(wchar_t chr, float fontsize) const {
	KImage img8;
	if (impl_) {
		impl_->get_glyph(chr, fontsize, NULL, &img8);
	}
	return img8;
}
KImage KFont::getGlyphImage32(wchar_t chr, float fontsize, Style style, bool with_alpha, const Color32 &primary, const Color32 &secondary) const {
	if (impl_ == NULL) return KImage();

	switch (style) {
	case NORMAL:
		{
			KImage glyph_u8 = getGlyphImage8(chr, fontsize); // 基本グリフ
			if (with_alpha) {
				return Glyph_toAlpha(glyph_u8, primary);
			} else {
				return Glyph_toColor(glyph_u8, primary);
			}
		}
	case BOLD:
		{
			KImage glyph_u8 = getGlyphImage8(chr, fontsize); // 基本グリフ
			KImage bold_u8 = Glyph_bold(glyph_u8);
			if (with_alpha) {
				return Glyph_toAlpha(bold_u8, primary);
			} else {
				return Glyph_toColor(bold_u8, primary);
			}
		}
	case OUTLINE:
		{
			KImage glyph_u8 = getGlyphImage8(chr, fontsize); // 基本グリフ
			KImage bold_u8 = Glyph_inflate(glyph_u8);
			KImage outline_u8 = Glyph_sub(bold_u8, 1, 1, glyph_u8); // 太らせた文字から元の文字を引き、くりぬく
			if (with_alpha) {
				return Glyph_toAlpha(outline_u8, primary);
			} else {
				return Glyph_toColor(outline_u8, primary);
			}
		}
	case OUTLINE_FILLED:
		{
			KImage glyph_u8 = getGlyphImage8(chr, fontsize); // 基本グリフ
			KImage bold_u8 = Glyph_inflate(glyph_u8);
			if (with_alpha) {
				// 背景＋アウトライン部分に当たる画像をアルファ付きで生成する
				KImage out = Glyph_toAlpha(bold_u8, secondary);
				KImage glyph = Glyph_toAlpha(glyph_u8, primary);
				KImageUtils::blend(out, OUTINE_WIDTH, OUTINE_WIDTH, glyph, BlendOutlineFilled());
				return out;
			} else {
				// 背景＋アウトライン部分に当たる画像を不透明画像で生成する
				// そこに重ねるグリフは、重ねる必要があるのでアルファ付きで生成されるが、BlendOutine による合成では
				// ベース画像のアルファは変化しない（完全不透明のままになる）
				KImage out = Glyph_toColor(bold_u8, secondary);
				KImage glyph = Glyph_toAlpha(glyph_u8, primary);
				KImageUtils::blend(out, OUTINE_WIDTH, OUTINE_WIDTH, glyph, BlendOutlineFilled());
				return out;
			}
		}
	default:
		{
			return KImage();
		}
	}
}
float KFont::getKerningAdvanve(wchar_t chr1, wchar_t chr2, float fontsize) const {
	return impl_ ? impl_->get_kerning(chr1, chr2, fontsize) : 0;
}
FontMetrics KFont::getMetrics(float fontsize) const {
	FontMetrics m;
	if (impl_) impl_->get_metrics(fontsize, &m);
	return m;
}
KGlyph KFont::getGlyph(wchar_t chr, float fontsize, Style style, bool with_alpha, const Color32 &primary, const Color32 &secondary) const {
	if (impl_ == NULL) return KGlyph();

	std::string key = _GetGlyphKey(*this, chr, fontsize, style, with_alpha, primary, secondary);
	KGlyph glyph;
	
	// ロード済みならばそれを返す
	if (g_FontAtlas.getGlyph(key, &glyph)) {
		return glyph;
	}

	// グリフ情報を得る。この時点ではテクスチャに関するフィールドが埋まっていない
	impl_->get_glyph(chr, fontsize, &glyph, NULL);
	if (glyph.w() > 0 && style==BOLD) {
		glyph.right += BOLD_INFLATE; // BOLD_INFLATEピクセル太る
		glyph.advance += BOLD_INFLATE; // 文字送りもBOLD_INFLATEピクセル伸びる
	}
	if (glyph.w() > 0 && (style==OUTLINE || style==OUTLINE_FILLED)) {
		glyph.left  -= OUTINE_WIDTH; // 左に1ピクセル太る
		glyph.right += OUTINE_WIDTH; // 右に1ピクセル太る
	//	glyph.advance+=OUTINE_WIDTH*2; // 文字送りも2ピクセル伸びる
		glyph.top+=OUTINE_WIDTH*1; // 高さが上がる
	}

	// グリフ画像を得る
	KImage img32 = getGlyphImage32(chr, fontsize, style, with_alpha, primary, secondary);

	// 登録する
	g_FontAtlas.addGlyph(key, img32, &glyph);

	return glyph;
}
bool KFont::getNameInJapanese(NameId nid, wchar_t *name) const {
	if (impl_) {
		return impl_->get_name_jp(nid, name);
	}
	return false;
}
bool KFont::getNameInEnglish(NameId nid, wchar_t *name) const {
	if (impl_) {
		return impl_->get_name_en(nid, name);
	}
	return false;
}
KFont::Flags KFont::getFlags() const {
	if (impl_) {
		return impl_->get_flags();
	}
	return 0;
}
void KFont::setUserTextureCallback(UserFontTextureCallback *cb) {
	g_FontAtlas.cb_ = cb;
}
#pragma endregion // KFont




static bool Test_getname(KFont &font, KFont::NameId nid, wchar_t *ws) {
	// まず日本語での取得を試みる。
	// ダメだったら英語で取得する
	if (font.getNameInJapanese(nid, ws)) {
		return true;
	}
	if (font.getNameInEnglish(nid, ws)) {
		return true;
	}
	return false;
}

void Test_font_printInfo(const wchar_t *filename) {
#ifdef _WIN32
	FontFile fontfile;
	fontfile.loadFromFileNameInSystemFontDirectory(filename);
	for (int i = 0; i < fontfile.getCount(); i++) {
		KFont font = fontfile.getFont(i);
		wchar_t ws[1024];

		ws[0] = 0;
		_swprintf(ws, L"%s [%d]", filename, i);
		OutputDebugStringW(ws);
		OutputDebugStringW(L"\n");

		ws[0] = 0;
		Test_getname(font, KFont::NID_FAMILY, ws);
		OutputDebugStringW(L"  Family   : ");
		OutputDebugStringW(ws);
		OutputDebugStringW(L"\n");

		ws[0] = 0;
		Test_getname(font, KFont::NID_COPYRIGHT, ws);
		OutputDebugStringW(L"  CopyRight: ");
		OutputDebugStringW(ws);
		OutputDebugStringW(L"\n");

		ws[0] = 0;
		Test_getname(font, KFont::NID_VERSION, ws);
		OutputDebugStringW(L"  Version  : ");
		OutputDebugStringW(ws);
		OutputDebugStringW(L"\n");

		ws[0] = 0;
		Test_getname(font, KFont::NID_TRADEMARK, ws);
		OutputDebugStringW(L"  Trademark: ");
		OutputDebugStringW(ws);
		OutputDebugStringW(L"\n");
	}
	OutputDebugStringW(L"----\n");
	OutputDebugStringW(L"\n");
#endif
}


void Test_font_ex(const Path &font_filename, float fontsize, const Path &output_image_filename) {
	const Color32 BG   = Color(0.0f, 0.3f, 0.0f, 1.0f);
	const Color32 FG   = Color(1.0f, 1.0f, 1.0f, 1.0f);
	const Color32 FG2  = Color(1.0f, 0.0f, 0.0f, 1.0f);
	const Color32 LINE = Color(0.0f, 0.2f, 0.0f, 1.0f);

	KFont font;
	if (font.loadFromFileName(font_filename)) {
		int img_w = 800;
		int img_h = (int)fontsize * 7;
		KImage imgText(img_w, img_h, BG);

		{
			KFont::Style style = KFont::NORMAL;
			const wchar_t *text = L"普通の設定。スタンダード Default";

			int baseline = (int)fontsize*1;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true, FG, FG2);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance;
			}
		}

		{
			KFont::Style style = KFont::BOLD;
			const wchar_t *text = L"太字にしたもの。Bold style.";

			int baseline = (int)fontsize*2;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true, FG, FG2);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance;
			}
		}

		{
			KFont::Style style = KFont::OUTLINE;
			const wchar_t *text = L"アウトライン文字。(KFont::OUTLINE) Outline font.";

			int baseline = (int)fontsize*3;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true, FG, FG2);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance;
			}
		}

		{
			KFont::Style style = KFont::OUTLINE_FILLED;
			const wchar_t *text = L"塗りつぶしアウトライン文字。(KFont::OUTLINE_FILLED) Outline font.";

			int baseline = (int)fontsize*4;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true, FG, FG2);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance + font.getKerningAdvanve(text[i], text[i+1], fontsize);
			}
		}

		{
			KFont::Style style = KFont::NORMAL;
			const wchar_t *text = L"ALTA FIFA Finland Wedding (Kerning off)";

			int baseline = (int)fontsize*5;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true, FG, FG2);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance;
			}
		}

		{
			KFont::Style style = KFont::NORMAL;
			const wchar_t *text = L"ALTA FIFA Finland Wedding (Kerning on)";

			int baseline = (int)fontsize*6;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true, FG, FG2);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance + font.getKerningAdvanve(text[i], text[i+1], fontsize);
			}
		}

		std::string png = imgText.saveToMemory();
		FileOutput(output_image_filename).write(png);
	}
}


void Test_font() {
#ifdef _WIN32
	Test_font_printInfo(L""); // 存在しないファイルでも落ちない
	Test_font_printInfo(L"msgothic.ttc");
	Test_font_printInfo(L"meiryo.ttc");
	Test_font_printInfo(L"arial.ttf");
	Test_font_printInfo(L"tahoma.ttf");
#endif
}


} // namespace
