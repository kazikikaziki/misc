#include "KEdgeFile.h"
#include "KFile.h"
#include "KImage.h"
#include "KLog.h"
#include "KNum.h"
#include "KRef.h"
#include "KConv.h"
#include "KZlib.h"
#include "KStd.h"
#include "mo_cstr.h"

#define EDGE2_STRING_ENCODING "JPN"  // Edge2 は日本語環境で使う事を前提とする

namespace mo {


bool g_edge_export_unzipped_image = false; // 未圧縮の Edge ファイルイメージを出力する？（テスト用）


static KImage _MakeImage(int width, int height, int bitdepth) {
	if (bitdepth == 32) {
		return KImage(width, height, KImage::RGBA32);
	}
	if (bitdepth == 24) {
		return KImage(width, height, KImage::RGB24);
	}
	if (bitdepth == 8) {
		return KImage(width, height, KImage::INDEX8);
	}
	Log_assert(0);
	return KImage();
}

#pragma region KEdgePalette
KEdgePalette::KEdgePalette() {
	data_b_ = 0;
	data_c_ = 0;
	data_d_ = 0;
	data_e_ = 0;
	bgr_colors_.resize(256 * 3);
	data_f_.resize(256);
	data_g_.resize(256);
	data_h_.resize(256*6);
	data_i_.resize(256);
	data_j_.resize(0);
	data_k_ = 0;
	data_l_ = 0;
	data_m_ = 0;
}
KEdgePalette::KEdgePalette(const KEdgePalette *copy) {
	*this = *copy;
}
#pragma endregion // KEdgePalette

#pragma region KEdgeLayer
KEdgeLayer::KEdgeLayer() {
	delay_ = 10;
	label_u8_.clear();
	label_color_rgbx_[0] = 255;
	label_color_rgbx_[1] = 255;
	label_color_rgbx_[2] = 255;
	label_color_rgbx_[3] = 0;
	is_grouped_ = false;
	is_visibled_ = true;
	data_a_ = 4;
	data_b_ = 1;
	data_c_ = 0;
	data_d_ = 0;
	data_e_.clear();
	data_f_ = 1;
	data_g_ = 2;
	data_h_ = 1;
}

/// レイヤーをコピーする
/// @param source       コピー元レイヤー
/// @param sharePixels  ピクセルバッファの参照を共有する
KEdgeLayer::KEdgeLayer(const KEdgeLayer *source, bool sharePixels) {
	delay_ = source->delay_;
	label_u8_ = source->label_u8_;
	memcpy(label_color_rgbx_, source->label_color_rgbx_, 4);
	is_grouped_ = source->is_grouped_;
	is_visibled_ = source->is_visibled_;
	data_a_ = source->data_a_;
	data_b_ = source->data_b_;
	data_c_ = source->data_c_;
	data_d_ = source->data_d_;
	data_e_.assign(source->data_e_.begin(), source->data_e_.end());
	data_f_ = source->data_f_;
	data_g_ = source->data_g_;
	data_h_ = source->data_h_;
	if (sharePixels) {
		// 参照コピー
		image_rgb24_ = source->image_rgb24_;
	} else {
		// 実体コピー
		image_rgb24_ = source->image_rgb24_.clone();
	}
}
KEdgeLayer::~KEdgeLayer() {
}
#pragma endregion // KEdgeLayer


#pragma region KEdgePage
KEdgePage::KEdgePage() {
	width_ = 256;
	height_ = 256;
	delay_ = 10;
	is_grouped_ = false;
	label_u8_.clear();
	layers_.clear();
	label_color_rgbx_[0] = 255;
	label_color_rgbx_[1] = 255;
	label_color_rgbx_[2] = 255;
	label_color_rgbx_[3] = 0;
	data_a_ = 0;
	data_b_ = 1;
	data_c_ = 0;
	data_d_ = (uint32_t)(-1);
	data_e_ = (uint32_t)(-1);
	data_f_ = 0;
	data_g_ = 0;
	data_h_ = 0;
	data_i_ = 1;
	data_j_ = 2;
	data_k_ = 1;
	ani_xparam_.dest = 0;
	ani_xparam_.is_relative = 0;
	ani_yparam_.dest = 0;
	ani_yparam_.is_relative = 0;
}
KEdgePage::KEdgePage(const KEdgePage *source, bool copy_layers) {
	delay_ = source->delay_;
	label_u8_ = source->label_u8_;
	is_grouped_ = source->is_grouped_;
	memcpy(label_color_rgbx_, source->label_color_rgbx_, 4);
	width_ = source->width_;
	height_ = source->height_;
	data_a_ = source->data_a_;
	data_b_ = source->data_b_;
	data_c_ = source->data_c_;
	data_d_ = source->data_d_;
	data_e_ = source->data_e_;
	data_f_ = source->data_f_;
	data_g_ = source->data_g_;
	data_h_ = source->data_h_;
	data_i_ = source->data_i_;
	data_j_ = source->data_j_;
	data_k_ = source->data_k_;
	ani_xparam_ = source->ani_xparam_;
	ani_yparam_ = source->ani_yparam_;
	if (copy_layers) {
		int numlayers = source->getLayerCount();
		for (int i=0; i<numlayers; i++) {
			const KEdgeLayer *src_layer = source->getLayer(i);
			addCopyLayer(src_layer, true);
		}
	}
}
KEdgePage::~KEdgePage() {
	for (auto it=layers_.begin(); it!=layers_.end(); it++) {
		KEdgeLayer *layer = *it;
		delete layer;
	}
}
KEdgeLayer * KEdgePage::addLayer() {
	KEdgeLayer *layer = new KEdgeLayer();
	layers_.push_back(layer);
	return layer;
}
KEdgeLayer * KEdgePage::addCopyLayer(const KEdgeLayer *source, bool share_pixels) {
	KEdgeLayer *layer = new KEdgeLayer(source, share_pixels);
	layers_.push_back(layer);
	return layer;
}
KEdgeLayer * KEdgePage::addLayerAt(int index) {
	KEdgeLayer *layer = new KEdgeLayer();
	if (index < 0) {
		layers_.insert(layers_.begin(), layer);
	} else if (index < (int)layers_.size()) {
		layers_.insert(layers_.begin() + index, layer);
	} else {
		layers_.push_back(layer);
	}
	return layer;
}
void KEdgePage::delLayer(int index) {
	if (index < 0) return;
	if (index >= (int)layers_.size()) return;
	delete layers_[index];
	layers_.erase(layers_.begin() + index);
}
KEdgeLayer * KEdgePage::getLayer(int index) {
	if (index < 0) return NULL;
	if (index >= (int)layers_.size()) return NULL;
	return layers_[index];
}
const KEdgeLayer * KEdgePage::getLayer(int index) const {
	if (index < 0) return NULL;
	if (index >= (int)layers_.size()) return NULL;
	return layers_[index];
}
int KEdgePage::getLayerCount() const {
	return (int)layers_.size();
}
#pragma endregion // KEdgePage


#pragma region KEdgeDocument
KEdgeDocument::KEdgeDocument() {
	clear();
}
KEdgeDocument::KEdgeDocument(const KEdgeDocument *source, bool copy_pages) {
	bool copy_palettes = true;
	K_assert(source);
	compression_flag_ = source->compression_flag_;
	bit_depth_ = source->bit_depth_;
	{
		fg_palette_.color_index = source->fg_palette_.color_index;
		fg_palette_.data_a = source->fg_palette_.data_a;
		fg_palette_.data_c = source->fg_palette_.data_c;
		memcpy(fg_palette_.bgr_colors, source->fg_palette_.bgr_colors, 3);
		memcpy(fg_palette_.data_b, source->fg_palette_.data_b, 6);

		bg_palette_.color_index = source->bg_palette_.color_index;
		bg_palette_.data_a = source->bg_palette_.data_a;
		bg_palette_.data_c = source->bg_palette_.data_c;
		memcpy(bg_palette_.bgr_colors, source->bg_palette_.bgr_colors, 3);
		memcpy(bg_palette_.data_b, source->bg_palette_.data_b, 6);
	}
	data_d_ = source->data_d_;
	data_d2_ = source->data_d2_;
	data_d3_.assign(source->data_d3_.begin(), source->data_d3_.end());
	data_e_.assign(source->data_e_.begin(), source->data_e_.end());
	data_a_ = source->data_a_;
	data_b_ = source->data_b_;
	data_c_ = source->data_c_;
	data_f_ = source->data_f_;
	data_g_ = source->data_g_;
	data_h_ = source->data_h_;
	data_i_ = source->data_i_;
	data_j_ = source->data_j_;
	data_k_ = source->data_k_;

	if (copy_pages) {
		int numpages = source->getPageCount();
		for (int i=0; i<numpages; i++) {
			const KEdgePage *src = source->getPage(i);
			addCopyPage(src);
		}
	}
	if (copy_palettes) {
		K_assert(palettes_.empty());
		for (size_t i=0; i<source->palettes_.size(); i++) {
			KEdgePalette *p = source->palettes_[i];
			palettes_.push_back(new KEdgePalette(p));
		}
	}
}
KEdgeDocument::~KEdgeDocument() {
	clear();
}
void KEdgeDocument::clear() {
	for (auto it=palettes_.begin(); it!=palettes_.end(); ++it) {
		delete (*it);
	}
	for (auto it=pages_.begin(); it!=pages_.end(); ++it) {
		delete (*it);
	}
	pages_.clear();
	palettes_.clear();
	compression_flag_ = 256;
	bit_depth_ = 24;
	fg_palette_.color_index = 0;
	bg_palette_.color_index = 0;
	memset(&fg_palette_, 0, sizeof(fg_palette_));
	memset(&bg_palette_, 0, sizeof(bg_palette_));
	fg_palette_.data_a = 255;
	fg_palette_.data_c = 0;
	bg_palette_.data_a = 255;
	bg_palette_.data_c = 0;
	data_a_ = 0;
	data_b_ = 0;
	data_c_ = 0;
	data_d_ = 1;
	data_d2_ = 1;
	data_d3_.clear();
	data_e_.clear();
	data_f_ = 0;
	data_g_ = 0;
	data_h_ = 0;
	data_i_ = 256;
	data_j_ = 1;
	data_k_ = 0;
}
bool KEdgeDocument::loadFromFile(FileInput &file) {
	if (file.empty()) return false;
	KEdgeRawReader reader;
	if (reader.read(this, file) != 0) {
		clear();
		return false;
	}
	return true;
}
bool KEdgeDocument::loadFromFileName(const Path &filename) {
	bool success = false;
	FileInput file;
	if (file.open(filename)) {
		success = loadFromFile(file);
	}
	return success;
}
void KEdgeDocument::saveToFile(FileOutput &file) {
	KEdgeRawWriter writer;
	writer.write(this, file);
}
void KEdgeDocument::saveToFileName(const Path &filename) {
	FileOutput file;
	if (file.open(filename)) {
		saveToFile(file);
	}
}

// Make document for Edge2 Application ( http://takabosoft.com/edge2 )
// All pages need to have least 1 layer.
void KEdgeDocument::autoComplete() {
	if (bit_depth_ == 0) {
		bit_depth_ = 0;
	}

	// Edge needs least one palette
	if (palettes_.empty()) {
		palettes_.push_back(new KEdgePalette());
	}

	// Edge needs least one page
	if (pages_.empty()) {
		pages_.push_back(new KEdgePage());
	}

	for (size_t p=0; p<pages_.size(); p++) {
		KEdgePage *page = pages_[p];

		if (page->width_==0 || page->height_==0) {
			int DEFAULT_SIZE = 8;
			page->width_  = (uint16_t)DEFAULT_SIZE;
			page->height_ = (uint16_t)DEFAULT_SIZE;
		}

		// Page needs least one layer
		if (page->layers_.empty()) {
			KEdgeLayer *layer = new KEdgeLayer();
			page->layers_.push_back(layer);
		}

		// Make empty image if not exists
		for (size_t l=0; l<page->layers_.size(); l++) {
			KEdgeLayer *layer = page->layers_[l];
			if (layer->image_rgb24_.empty()) {
				layer->image_rgb24_ = _MakeImage(page->width_, page->height_, bit_depth_);
			}
		}
	}
}

KEdgePage * KEdgeDocument::addPage() {
	KEdgePage *page = new KEdgePage();
	pages_.push_back(page);
	return page;
}
KEdgePage * KEdgeDocument::addCopyPage(const KEdgePage *source) {
	KEdgePage *page = new KEdgePage(source, true);
	pages_.push_back(page);
	return page;
}
KEdgePage * KEdgeDocument::addPageAt(int index) {
	KEdgePage *page = new KEdgePage();
	if (index < 0) {
		pages_.insert(pages_.begin(), page);
	} else if (index < (int)pages_.size()) {
		pages_.insert(pages_.begin() + index, page);
	} else {
		pages_.push_back(page);
	}
	return page;
}
void KEdgeDocument::delPage(int index) {
	if (index < 0) return;
	if (index >= (int)pages_.size()) return;
	delete pages_[index];
	pages_.erase(pages_.begin() + index);
}
int KEdgeDocument::getPageCount() const {
	return (int)pages_.size();
}
KEdgePage * KEdgeDocument::getPage(int index) {
	if (index < 0) return NULL;
	if (index >= (int)pages_.size()) return NULL;
	return pages_[index];
}
const KEdgePage * KEdgeDocument::getPage(int index) const {
	if (index < 0) return NULL;
	if (index >= (int)pages_.size()) return NULL;
	return pages_[index];
}
uint8_t * KEdgeDocument::getPalettePointer(int index) {
	if (0 <= index && index < (int)palettes_.size()) {
		return (uint8_t *)&palettes_[index]->bgr_colors_[0];
	}
	return NULL;
}
int KEdgeDocument::getPaletteCount() const {
	return (int)palettes_.size();
}
KEdgePalette * KEdgeDocument::getPalette(int index) {
	return palettes_[index];
}
const KEdgePalette * KEdgeDocument::getPalette(int index) const {
	return palettes_[index];
}
std::string KEdgeDocument::expotyXml() const {
	std::string xml_u8;

	int numPages = getPageCount();
	xml_u8 += K_sprintf("<Edge pages='%d'>\n", numPages);
	for (int p=0; p<numPages; p++) {
		const KEdgePage *page = getPage(p);
		int numLayers = page->getLayerCount();
		xml_u8 += K_sprintf("  <Page label='%s' labelColor='#%02x%02x%02x' delay='%d' w='%d' h='%d' layers='%d'>\n", 
			page->label_u8_.c_str(), 
			page->label_color_rgbx_[0], page->label_color_rgbx_[1], page->label_color_rgbx_[2],
			page->delay_, page->width_, page->height_, numLayers);
		for (int l=0; l<numLayers; l++) {
			const KEdgeLayer *layer = page->getLayer(l);
			xml_u8 += K_sprintf("    <Layer label='%s' labelColor='#%02x%02x%02x' delay='%d'/>\n",
				layer->label_u8_.c_str(), 
				layer->label_color_rgbx_[0], layer->label_color_rgbx_[1], layer->label_color_rgbx_[2],
				layer->delay_);
		}
		xml_u8 += "  </Page>\n";
	}

	int numPal = getPaletteCount();
	xml_u8 += K_sprintf("  <Palettes count='%d'>\n", numPal);
	for (int i=0; i<numPal; i++) {
		const KEdgePalette *pal = getPalette(i);
		xml_u8 += K_sprintf("    <Pal label='%s'/>\n", pal->name_u8.c_str());
	}
	xml_u8 += "  </Palettes>\n";

	xml_u8 += "</Edge>\n";
	return xml_u8;
}

/// パレットを画像化したものを作成する
/// 横幅は 16 ドット、高さは 16 * パレット数になる
/// EDGE 背景色に該当するピクセルのみアルファ値が 0 になるが、それ以外のピクセルのアルファは必ず 255 になる
KImage KEdgeDocument::exportPaletteImage() const {
	if (palettes_.empty()) return KImage();
	KImage pal(PALETTE_IMAGE_SIZE, PALETTE_IMAGE_SIZE * palettes_.size());
	uint8_t *dest = pal.lockData();
	for (size_t p=0; p<palettes_.size(); p++) {
		const auto &bgr = palettes_[p]->bgr_colors_;
		for (int i=0; i<256; i++) {
			uint8_t b = bgr[i*3+0];
			uint8_t g = bgr[i*3+1];
			uint8_t r = bgr[i*3+2];
			uint8_t a = (bg_palette_.color_index == i) ? 0 : 255;
			dest[(256 * p + i) * 4 + 0] = r;
			dest[(256 * p + i) * 4 + 1] = g;
			dest[(256 * p + i) * 4 + 2] = b;
			dest[(256 * p + i) * 4 + 3] = a;
		}
	}
	pal.unlockData();
	return pal;
}

/// Edge レイヤーの画像を、パレット番号が取得可能な形式で書き出す。
/// RGBそれぞれにパレット番号を、A には常に255をセットするため、パレット変換しないで表示した場合は奇妙なグレースケール画像に見える
/// 書き出し時のフォーマットは RGBA32 ビットになる
/// 元画像がパレットでない場合は NULL を返す
KImage KEdgeDocument::exportSurfaceRaw(int pageindex, int layerindex, int transparent_index) const {
	KImage output;
	const KEdgePage *page = getPage(pageindex);
	if (page) {
		const KEdgeLayer *layer = page->getLayer(layerindex);
		if (layer && bit_depth_ == 8) {
			int w = page->width_;
			int h = page->height_;
			const uint8_t *p_src = layer->image_rgb24_.getData();
			output = KImage(w, h);
			uint8_t *p_dst = output.lockData();
			for (int y=0; y<h; y++) {
				for (int x=0; x<w; x++) {
					uint8_t i = p_src[y * w + x];
					p_dst[(y * w + x) * 4 + 0] = i; // パレットインデックスをそのままRGB値として書き込む
					p_dst[(y * w + x) * 4 + 1] = i;
					p_dst[(y * w + x) * 4 + 2] = i;
					p_dst[(y * w + x) * 4 + 3] = (transparent_index == i) ? 0 : 255;
				}
			}
			output.unlockData();
		}
	}
	return output;
}
static KImage _ImageFromRGB24(int w, int h, const uint8_t *pixels, const uint8_t *transparent_color_BGR) {
	if (w <= 0 || h <= 0 || pixels == NULL) { Log_error("invalid argument"); return KImage(); }
	KEdgeBin buf(w * h * 4, '\0');
	uint8_t *buf_p = (uint8_t*)&buf[0];
	const uint8_t *t = transparent_color_BGR;
	const int srcPitch = w * 3;
	const int dstPitch = w * 4;
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			const uint8_t r = pixels[srcPitch*y + x*3 + 2]; // r
			const uint8_t g = pixels[srcPitch*y + x*3 + 1]; // g
			const uint8_t b = pixels[srcPitch*y + x*3 + 0]; // b
			const uint8_t a = (t && b==t[0] && g==t[1] && r==t[2]) ? 0 : 255;
			buf_p[dstPitch*y + x*4 + 0] = r;
			buf_p[dstPitch*y + x*4 + 1] = g;
			buf_p[dstPitch*y + x*4 + 2] = b;
			buf_p[dstPitch*y + x*4 + 3] = a;
		}
	}
	return KImage(w, h, KImage::RGBA32, buf_p);
}

static KImage _ImageFromIndexed(int w, int h, const uint8_t *indexed_format_pixels, const uint8_t *palette_colors_BGR, int transparent_color_index) {
	if (w <= 0 || h <= 0) { Log_error("invalid argument"); return KImage(); }
	K_assert(indexed_format_pixels);
	K_assert(palette_colors_BGR);
	const int dstPitch = w * 4;
	KEdgeBin buf(w * h * 4, '\0');
	uint8_t *buf_p = (uint8_t*)&buf[0];
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			const uint8_t idx = indexed_format_pixels[w * y + x];
			if (idx == transparent_color_index) {
				buf_p[dstPitch*y + x*4 + 0] = 0; // r
				buf_p[dstPitch*y + x*4 + 1] = 0; // g
				buf_p[dstPitch*y + x*4 + 2] = 0; // b
				buf_p[dstPitch*y + x*4 + 3] = 0; // a
			} else {
				buf_p[dstPitch*y + x*4 + 0] = palette_colors_BGR[idx * 3 + 2]; // r
				buf_p[dstPitch*y + x*4 + 1] = palette_colors_BGR[idx * 3 + 1]; // g
				buf_p[dstPitch*y + x*4 + 2] = palette_colors_BGR[idx * 3 + 0]; // b
				buf_p[dstPitch*y + x*4 + 3] = 255; // a
			}
		}
	}
	return KImage(w, h, KImage::RGBA32, buf_p);
}

/// Edge レイヤーの画像を KImage に書き出す
/// 書き出し時のフォーマットは常に RGBA32 ビットになる（Edgeの背景色を透過色にする）
/// 失敗した場合は NULL を返す
KImage KEdgeDocument::exportSurface(int pageindex, int layerindex, int paletteindex) const {
	KImage output;
	const KEdgePage *page = getPage(pageindex);
	if (page) {
		const KEdgeLayer *layer = page->getLayer(layerindex);
		if (layer) {
			// Make surface
			const uint8_t *p_src = layer->image_rgb24_.getData();
			int w = page->width_;
			int h = page->height_;
			if (bit_depth_ == 24) {
				output = _ImageFromRGB24(w, h, p_src, bg_palette_.bgr_colors);
			} else if (bit_depth_ == 8) {
				const uint8_t *colors = (uint8_t *)&palettes_[paletteindex]->bgr_colors_[0];
				output = _ImageFromIndexed(w, h, p_src, colors, bg_palette_.color_index);
			}
		}
	}
	return output;
}

/// 4枚の元画像とチャンネルを指定し、抽出した４つのチャンネル画像を合成して一枚の画像にする。
/// @arg src_surface_r 画像の src_channel_r チャンネル画像を、Rチャンネルとして使う。
/// @arg src_surface_g 画像の src_channel_g チャンネル画像を、Gチャンネルとして使う。
/// @arg src_surface_b 画像の src_channel_b チャンネル画像を、Bチャンネルとして使う。
/// @arg src_surface_a 画像の src_channel_a チャンネル画像を、Aチャンネルとして使う。
///
/// @param src_r  Rチャンネルとして使う元画像
/// @param src_g  Gチャンネルとして使う元画像
/// @param src_b  Bチャンネルとして使う元画像
/// @param src_a  Aチャンネルとして使う元画像
/// @param off_r  src_r から色を取り出す時の各ピクセル先頭からのオフセットバイト数。1ピクセルが4バイトで構成されているとき、0 から 3 までの値を指定できる
/// @param off_g  （略）
/// @param off_b  （略）
/// @param off_a  （略）
static KImage _ImageFromChannels(const KImage *src_r, const KImage *src_g, const KImage *src_b, const KImage *src_a, int off_r, int off_g, int off_b, int off_a) {
	K_assert(src_r);
	K_assert(src_g);
	K_assert(src_b);
	K_assert(src_a);
	if(off_r < 0 || 4 <= off_r) { Log_error("Invalid offset bytes for R channel"); return KImage(); }
	if(off_g < 0 || 4 <= off_g) { Log_error("Invalid offset bytes for G channel"); return KImage(); }
	if(off_b < 0 || 4 <= off_b) { Log_error("Invalid offset bytes for B channel"); return KImage(); }
	if(off_a < 0 || 4 <= off_a) { Log_error("Invalid offset bytes for A channel"); return KImage(); }
	// すべてのサーフェスでサイズが一致することを確認
	const int w = src_r->getWidth();
	const int h = src_r->getHeight();
	if (w <= 0 || h <= 0) { Log_error("Invalid image size"); return KImage(); }
	const bool size_r_ok = (src_r->getWidth()==w && src_r->getHeight()==h);
	const bool size_g_ok = (src_g->getWidth()==w && src_g->getHeight()==h);
	const bool size_b_ok = (src_b->getWidth()==w && src_b->getHeight()==h);
	const bool size_a_ok = (src_a->getWidth()==w && src_a->getHeight()==h);
	if (!size_r_ok || !size_g_ok || !size_b_ok || !size_a_ok) { Log_error("Source images have different sizes"); return KImage(); }
	// 準備
	KImage out(w, h, KImage::RGBA32);
	// RGBAそれぞれのチャンネルを合成する
	const uint8_t *src_p_r = src_r->getData();
	const uint8_t *src_p_g = src_g->getData();
	const uint8_t *src_p_b = src_b->getData();
	const uint8_t *src_p_a = src_a->getData();
	uint8_t *dstP = out.lockData();
	K_assert(dstP);
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			const int i = (w * y + x) * 4;
			dstP[i + 0] = src_p_r[i + off_r];
			dstP[i + 1] = src_p_g[i + off_g];
			dstP[i + 2] = src_p_b[i + off_b];
			dstP[i + 3] = src_p_a[i + off_a];
		}
	}
	out.unlockData();
	return out;
}





/// Edge レイヤーの画像をアルファチャンネルつきで KImage に書き出す
/// 書き出し時のフォーマットは常に RGBA32 ビットで、アルファ値の入力にはアルファマスクレイヤーを指定する
/// マスクレイヤーはアルファ値をグレースケールで記録したもので、変換元のレイヤーと同じページ内にある
/// マスクレイヤーのインデックスに -1 が指定されている場合は、Edgeの背景色を透過させる
/// 失敗した場合は NULL を返す
KImage KEdgeDocument::exportSurfaceWithAlpha(int pageindex, int layerindex, int maskindex, int paletteindex) const {
	if (maskindex < 0) {
		// アルファマスクが省略されている
		return exportSurface(pageindex, layerindex, paletteindex);

	} else {
		KImage rgb = exportSurface(pageindex, layerindex, paletteindex);
		KImage mask = exportSurface(pageindex, maskindex, 0);
		KImage output = _ImageFromChannels(&rgb, &rgb, &rgb, &mask, 0, 1, 2, 0);
		return output;
	}
}

/// 指定された色に最も近いカラーインデックスを返す
/// 検索できない場合は defaultindex を返す
int KEdgeDocument::findColorIndex(int paletteindex, uint8_t r, uint8_t g, uint8_t b, int defaultindex) const {
	if (paletteindex < 0 || (int)palettes_.size() <= paletteindex) return defaultindex;

	int result = defaultindex;
	int min_diff = 255;
	const KEdgePalette *pal = palettes_[paletteindex];
	for (int i=0; i<256; i++) {
		int pal_b = (int)pal->bgr_colors_[i*3 + 0];
		int pal_g = (int)pal->bgr_colors_[i*3 + 1];
		int pal_r = (int)pal->bgr_colors_[i*3 + 2];

		int diff = abs(pal_r - r) + abs(pal_g - g) + abs(pal_b - b);
		if (diff < min_diff) {
			min_diff = diff;
			result = i;
		}
	}
	return result;
}

/// KImage で指定された画像を Edge レイヤーに書き出す
/// 書き込み先の Edge レイヤーにパレットが設定されている場合は、適当に減色してパレットに対応させる
/// 書き込みに成功すれば true を返す
bool KEdgeDocument::writeImageToLayer(int pageindex, int layerindex, int paletteindex, const KImage *image) {
	if (image == NULL) {
		Log_error("E_INVALID_ARGUMENT");
		return false;
	}
	if (image->getBytesPerPixel() != 3) {
		Log_errorf("E_EDGE_INVALID_BYTES_PER_PIXEL: %d", image->getBytesPerPixel());
		return false;
	}

	KEdgePage *page = getPage(pageindex);
	if (page == NULL) {
		Log_errorf("E_EDGE_INVALID_PAGE_INDEX: PAGE=%d/%d", pageindex, getPageCount());
		return false;
	}

	KEdgeLayer *layer = page->getLayer(layerindex);
	if (layer == NULL) {
		Log_errorf("E_EDGE_INVALID_LAYER_INDEX: PAGE=%d, LAYER=%d/%d", pageindex, layerindex, page->getLayerCount());
		return false;
	}

	// コピー範囲を決定
	const int w = Num_min((int)page->width_, image->getWidth());
	const int h = Num_min((int)page->height_, image->getHeight());

	if (bit_depth_ == 24) {
		// RGB24ビットカラー

		// コピー先のバッファ
		uint8_t *dst_p = layer->image_rgb24_.lockData();
		// RGB値を直接コピーする。アルファチャンネルは無視する
		const uint8_t *src_p = image->getData();
		int src_data_size = image->getDataSize();
		const int dst_pitch = page->width_ * 3;
		const int src_pitch = image->getPitch();
		memset(dst_p, 0, src_data_size); // アラインメントによる余白も含めてクリア
		for (int y=0; y<h; y++) {
			for (int x=0; x<w; x++) {
				const uint8_t r = src_p[src_pitch*y + x*3 + 0];
				const uint8_t g = src_p[src_pitch*y + x*3 + 1];
				const uint8_t b = src_p[src_pitch*y + x*3 + 2];
				// EdgeにはBGRの順番で保存する
				dst_p[dst_pitch*y + x*3 + 0] = b;
				dst_p[dst_pitch*y + x*3 + 1] = g;
				dst_p[dst_pitch*y + x*3 + 2] = r;
			}
		}
		layer->image_rgb24_.unlockData();
		return true;
	}

	if (bit_depth_ == 8) {
		// インデックスカラー

		// コピー先のバッファ
		uint8_t *dst_p = layer->image_rgb24_.lockData();
		// パレットを考慮してコピーする
		const uint8_t *src_p = image->getData();
		int src_data_size = image->getDataSize();
		const int dst_pitch = page->width_;
		const int src_pitch = image->getPitch();
		memset(dst_p, 0, src_data_size); // アラインメントによる余白も含めてクリア
		for (int y=0; y<h; y++) {
			for (int x=0; x<w; x++) {
				const uint8_t r = src_p[src_pitch*y + 3*x + 0];
				const uint8_t g = src_p[src_pitch*y + 3*x + 1];
				const uint8_t b = src_p[src_pitch*y + 3*x + 2];
				dst_p[dst_pitch*y + x] = (uint8_t)findColorIndex(paletteindex, r, g, b, 0);
			}
		}
		layer->image_rgb24_.unlockData();
		return true;
	}

	Log_errorf("E_EDGE_INVALID_BIT_DEPTH: DEPTH=%d, PAGE=%d, LAYER=%d", bit_depth_, pageindex, layerindex);
	return false;
}
#pragma endregion // KEdgeDocument

#pragma region KEdgePalFile
KEdgePalFile::KEdgePalFile() {
	memset(bgr_, 0, sizeof(bgr_));
	memset(data_, 0, sizeof(data_));
}
KImage KEdgePalFile::exportImage(bool index0_for_transparent) const {
	KImage img(16, 16);
	uint8_t *dst = img.lockData();
	for (int i=0; i<256; i++) {
		dst[i*4 + 0] = bgr_[i*3+2]; // r
		dst[i*4 + 1] = bgr_[i*3+1]; // g
		dst[i*4 + 2] = bgr_[i*3+0]; // b
		dst[i*4 + 3] = 255; // a
	}
	if (index0_for_transparent) {
		dst[0*4 + 3] = 0; // a
	}
	img.unlockData();
	return img;
}
#pragma endregion // KEdgePalFile

#pragma region KEdgePalReader
bool KEdgePalReader::loadFromFileName(const Path &filename) {
	bool ok = false;
	FileInput file;
	if (file.open(filename)) {
		ok = loadFromFile(file);
	}
	return ok;
}
bool KEdgePalReader::loadFromMemoty(const void *data, size_t size) {
	bool ok = false;
	FileInput file;
	if (file.open(data, size)) {
		ok = loadFromFile(file);
	}
	return ok;
}
bool KEdgePalReader::loadFromFile(FileInput &file) {
	if (file.empty()) {
		return false;
	}
	// 識別子
	{
		char sign[12];
		file.read(sign, sizeof(sign));
		if (strcmp(sign, "EDGE2 PAL") != 0) {
			return false; // sign not matched
		}
	}
	// 展開後のデータサイズ
	uint32_t uzsize;
	file.read(&uzsize, 4);
	K_assert(uzsize > 0);

	// 圧縮データ
	int zsize = file.size() - file.tell();
	KEdgeBin zbin = file.readBin(zsize);
	K_assert(zsize > 0);
	K_assert(! zbin.empty());

	// 展開
	KEdgeBin bin = KZlib::uncompress_zlib(zbin.data(), zsize, uzsize);
	K_assert(! bin.empty());
	K_assert(bin.size() == uzsize);

	KChunkedFileReader::init(bin.data(), bin.size());

	uint16_t a;
	readChunk(0x03e8, 2, &a);

	// パレットをすべて読みだす
	while (!eof()) {
		KEdgePalFile pal;
		std::string name_mb;
		readChunk(0x03e9, &name_mb);
		pal.name_u8 = KConv::ansiToUtf8(name_mb, EDGE2_STRING_ENCODING);
		readChunk(0x03ea, 256*3, pal.bgr_);
		readChunk(0x03eb, 256, pal.data_);
		items_.push_back(pal);
	}
	return true;
}
std::string KEdgePalReader::exportXml() { // UTF8
	std::string xml_u8 = "<PalFile>\n";
	for (size_t i=0; i<items_.size(); i++) {
		xml_u8 += K_sprintf("  <Pal label='%s'/><!-- %d -->\n", items_[i].name_u8.c_str(), i);
	}
	xml_u8 += "</PalFile>\n";
	return xml_u8;
}

#pragma endregion // KEdgePalReader

#pragma region KEdgeRawReader
KEdgeRawReader::KEdgeRawReader() {
}
// Edge2 バイナリファイルからデータを読み取る
int KEdgeRawReader::read(KEdgeDocument *e, FileInput &file) {
	uint16_t zflag;
	KEdgeBin uzbin = unzip(file, &zflag);

	if (uzbin.empty()) return -1;

	// EDGE を得る
	readFromUncompressedBuffer(e, uzbin.data(), uzbin.size());
	e->compression_flag_ = zflag;
	return 0;
}

KEdgeBin KEdgeRawReader::unzip(FileInput &file, uint16_t *zflag) {
	// 識別子
	{
		char sign[6];
		file.read(sign, sizeof(sign));
		if (strcmp(sign, "EDGE2") != 0) {
			return KEdgeBin(); // signature doest not matched
		}
	}
	// 圧縮フラグ
	file.read(zflag, 2);

	// 展開後のデータサイズ
	uint32_t uzsize;
	file.read(&uzsize, 4);
	K_assert(uzsize > 0);

	// 圧縮データ
	int zsize = file.size() - file.tell();
	KEdgeBin zbin = file.readBin(zsize);
	K_assert(zsize > 0);
	K_assert(! zbin.empty());

	// 展開
	KEdgeBin uzbin = KZlib::uncompress_zlib(zbin.data(), zbin.size(), uzsize);
	K_assert(! uzbin.empty());
	K_assert(uzbin.size() == uzsize);

	return uzbin;
}

void KEdgeRawReader::scanLayer(KEdgeDocument *e, KEdgePage *page, KEdgeLayer *layer) {
	std::string label_mb;
	readChunk(0x03e8, &label_mb);
	layer->label_u8_ = KConv::ansiToUtf8(label_mb, EDGE2_STRING_ENCODING);

	readChunk(0x03e9, 4, &layer->data_a_);
	readChunk(0x03ea, 1, &layer->is_grouped_);
	readChunk(0x03eb, 1, &layer->data_b_);
	readChunk(0x03ec, 1, &layer->data_c_);
	readChunk(0x03ed, 1, &layer->is_visibled_);
	{
		layer->image_rgb24_ = _MakeImage(page->width_, page->height_, e->bit_depth_);
		uint8_t *p = layer->image_rgb24_.lockData();
		K_assert(p);
		uint32_t len = page->width_ * page->height_ * e->bit_depth_ / 8;
		readChunk(0x03ee, len, p); // 8bit palette or 24bit BGR
		layer->image_rgb24_.unlockData();
	}
	readChunk(0x03ef, 1, &layer->data_d_);
	readChunk(0x03f0, 0, &layer->data_e_); // 子チャンクを含む
	readChunk(0x03f1, 4, layer->label_color_rgbx_);
	{
		uint32_t raw_delay = 0;
		readChunk(0x07d0, 4, &raw_delay);
		layer->delay_ = raw_delay / 100;
	}
	readChunk(0x07d1, 1, &layer->data_f_);
	readChunk(0x07d2, 1, &layer->data_g_);
	readChunk(0x07d3, 1, &layer->data_h_);
}

void KEdgeRawReader::scanPage(KEdgeDocument *e, KEdgePage *page) {
	uint16_t numlayers;

	std::string label_mb;
	readChunk(0x03e8, &label_mb);
	page->label_u8_ = KConv::ansiToUtf8(label_mb, EDGE2_STRING_ENCODING);

	readChunk(0x03e9, 4, &page->data_a_);
	readChunk(0x03ea, 1, &page->is_grouped_);
	readChunk(0x03eb, 1, &page->data_b_);
	readChunk(0x03ec, 1, &page->data_c_);
	readChunk(0x03ed, 2, &page->width_);
	readChunk(0x03ee, 2, &page->height_);
	readChunk(0x03ef, 4, &page->data_d_);
	readChunk(0x03f0, 4, &page->data_e_);
	readChunk(0x03f1, 4, page->label_color_rgbx_);
	readChunk(0x07d0, 1, &page->data_f_);
	readChunk(0x07d1, 4, &page->data_g_);
	readChunk(0x07d2, 2, &numlayers);
	// レイヤ
	for (uint16_t i=0; i<numlayers; i++) {
		openChunk(0x07d3);
		KEdgeLayer *layer = new KEdgeLayer();
		scanLayer(e, page, layer);
		page->layers_.push_back(layer);
		closeChunk();
	}
	readChunk(0x07d4, 4, &page->data_h_);
	{
		uint32_t raw_delay = 0;
		readChunk(0x0bb8, 4, &raw_delay);
		page->delay_ = raw_delay / 100;
	}
	readChunk(0x0bb9, 1, &page->data_i_);
	readChunk(0x0bba, 1, &page->data_j_);
	readChunk(0x0bbb, 5, &page->ani_xparam_);
	readChunk(0x0bbc, 5, &page->ani_yparam_);
	readChunk(0x0bbd, 1, &page->data_k_);
}

void KEdgeRawReader::scanEdge(KEdgeDocument *e) {
	e->pages_.clear();
	e->palettes_.clear();

	uint16_t numpages;
	readChunk(0x03e8, 1, &e->bit_depth_);
	readChunk(0x03e9, 1, &e->data_a_);
	readChunk(0x03ea, 1, &e->data_b_);
	readChunk(0x03eb, 1, &e->data_c_);

	openChunk(0x03ed);
	{
		readChunk(0x3e8, 1, &e->data_d_);
		readChunk(0x3e9, 1, &e->data_d2_);
	}
	closeChunk();

	// Optional chunk
	if (checkChunkId(0x03ee)) {
		readChunk(0x03ee, 0, &e->data_d3_);
	}
	if (checkChunkId(0x03ef)) {
		readChunk(0x03ef, 0, &e->data_d4_);
	}

	readChunk(0x0fa1, 0, &e->data_e_);

	readChunk(0x07d0, 2, &numpages);
	readChunk(0x07d1, 4, &e->data_f_);
	readChunk(0x07d2, 1, &e->data_g_);

	// Pages
	for (uint16_t i=0; i<numpages; ++i) {
		openChunk(0x07d3);
		KEdgePage *page = new KEdgePage();
		scanPage(e, page);
		e->pages_.push_back(page);
		closeChunk();
	}

	uint16_t numpals;
	readChunk(0x07d4, 4, &e->data_h_);
	readChunk(0x0bb8, 2, &numpals);
	readChunk(0x0bb9, 2, &e->data_i_);
	readChunk(0x0bba, 4, &e->data_j_);

	// パレット（3 * 256バイト * 任意の個数）
	for (uint16_t i=0; i<numpals; ++i) {
		openChunk(0x0bbb);

		KEdgePalette *pal = new KEdgePalette();
		
		std::string name_mb;
		readChunk(0x03e8, 0, &name_mb);
		pal->name_u8 = KConv::ansiToUtf8(name_mb, EDGE2_STRING_ENCODING);
		
		readChunk(0x03e9, 4,     &pal->data_b_);
		readChunk(0x03ea, 1,     &pal->data_c_);
		readChunk(0x03eb, 1,     &pal->data_d_);
		readChunk(0x03ec, 1,     &pal->data_e_);
		readChunk(0x03ed, 256*3, &pal->bgr_colors_);
		readChunk(0x03ef, 256,   &pal->data_f_);
		readChunk(0x03f0, 256,   &pal->data_g_);
		readChunk(0x03f1, 256*6, &pal->data_h_);
		readChunk(0x03f2, 256,   &pal->data_i_);
		readChunk(0x03f3, 0,     &pal->data_j_); // 子チャンクを含む
		readChunk(0x07d0, 4,     &pal->data_k_); // Delay?
		readChunk(0x07d1, 1,     &pal->data_l_);
		readChunk(0x07d2, 1,     &pal->data_m_);
		e->palettes_.push_back(pal);

		closeChunk();
	}
	readChunk(0x0bbc, 4, &e->data_k_);

	// 前景色
	openChunk(0x0bbf);
	{
		readChunk(0x03e8, 1, &e->fg_palette_.color_index);
		readChunk(0x03e9, 3,  e->fg_palette_.bgr_colors);
		readChunk(0x03ea, 1, &e->fg_palette_.data_a);
		readChunk(0x03eb, 6,  e->fg_palette_.data_b);
		readChunk(0x03ec, 1, &e->fg_palette_.data_c);
	}
	closeChunk();

	// 背景色
	openChunk(0x0bc0);
	{
		readChunk(0x03e8, 1, &e->bg_palette_.color_index);
		readChunk(0x03e9, 3,  e->bg_palette_.bgr_colors);
		readChunk(0x03ea, 1, &e->bg_palette_.data_a);
		readChunk(0x03eb, 6,  e->bg_palette_.data_b);
		readChunk(0x03ec, 1, &e->bg_palette_.data_c);
	}
	closeChunk();
}

// 非圧縮の Edge2 バイナリデータからデータを読み取る
// @param buf バイナリ列(ZLibで展開後のデータであること)
// @param len バイナリ列のバイト数
void KEdgeRawReader::readFromUncompressedBuffer(KEdgeDocument *e, const void *buf, size_t len) {
	init(buf, len);
	scanEdge(e);
}
#pragma endregion // KEdgeRawReader

#pragma region KEdgeRawWriter
KEdgeRawWriter::KEdgeRawWriter() {
	clear();
}
void KEdgeRawWriter::write(KEdgeDocument *e, FileOutput &file) {
	K_assert(e);

	// 無圧縮 Edge バイナリを作成する
	writeUncompressedEdge(e);
	
	// データを圧縮し、Edge2 ツールで扱える形式にしたものを得る
	writeCompressedEdge(file);
}
void KEdgeRawWriter::writeLayer(KEdgePage *page, int layerindex, int bitdepth) {
	K_assert(page);
	KEdgeLayer *layer = page->getLayer(layerindex);
	K_assert(layer);

	std::string label_mb = KConv::utf8ToAnsi(layer->label_u8_, EDGE2_STRING_ENCODING);
	writeChunkN(0x03e8, label_mb.c_str());
	writeChunk4(0x03e9, layer->data_a_);
	writeChunk1(0x03ea, layer->is_grouped_);
	writeChunk1(0x03eb, layer->data_b_);
	writeChunk1(0x03ec, layer->data_c_);
	writeChunk1(0x03ed, layer->is_visibled_?1:0);
	{
		// write RGB pixels
		uint32_t pixels_size = page->width_ * page->height_ * bitdepth/8;
		uint8_t * pixels_ptr = layer->image_rgb24_.lockData();
		if (pixels_ptr) {
			K_assert(pixels_size == (uint32_t)layer->image_rgb24_.getDataSize());
			writeChunkN(0x03ee, pixels_size, pixels_ptr);
			layer->image_rgb24_.unlockData();
		} else {
			// 画像が Empty 状態になっている
			writeChunkN(0x03ee, pixels_size, NULL);
		}
	}
	writeChunk1(0x03ef, layer->data_d_);
	writeChunkN(0x03f0, layer->data_e_);
	writeChunkN(0x03f1, 4, layer->label_color_rgbx_);
	writeChunk4(0x07d0, layer->delay_ * 100); // delay must be written x100
	writeChunk1(0x07d1, layer->data_f_);
	writeChunk1(0x07d2, layer->data_g_);
	writeChunk1(0x07d3, layer->data_h_);
}

void KEdgeRawWriter::writePage(KEdgeDocument *e, int pageindex) {
	K_assert(e);
	KEdgePage *page = e->getPage(pageindex);
	K_assert(page);
	K_assert(page->width_ > 0);
	K_assert(page->height_ > 0);

	std::string label_mb = KConv::utf8ToAnsi(page->label_u8_, EDGE2_STRING_ENCODING);
	writeChunkN(0x03e8, label_mb.c_str());
	writeChunk4(0x03e9, page->data_a_);
	writeChunk1(0x03ea, page->is_grouped_);
	writeChunk1(0x03eb, page->data_b_);
	writeChunk1(0x03ec, page->data_c_);
	writeChunk2(0x03ed, page->width_);
	writeChunk2(0x03ee, page->height_);
	writeChunk4(0x03ef, page->data_d_);
	writeChunk4(0x03f0, page->data_e_);
	writeChunkN(0x03f1, 4, page->label_color_rgbx_);
	writeChunk1(0x07d0, page->data_f_);
	writeChunk4(0x07d1, page->data_g_);

	uint16_t numLayers = (uint16_t)page->getLayerCount();
	writeChunk2(0x07d2, numLayers);

	// Layers
	K_assert(numLayers > 0);
	for (uint16_t i=0; i<numLayers; i++) {
		openChunk(0x07d3);
		writeLayer(page, i, e->bit_depth_);
		closeChunk();
	}

	writeChunk4(0x07d4, page->data_h_);
	writeChunk4(0x0bb8, page->delay_ * 100); // delay must be written x100
	writeChunk1(0x0bb9, page->data_i_);
	writeChunk1(0x0bba, page->data_j_);
	writeChunkN(0x0bbb, 5, &page->ani_xparam_);
	writeChunkN(0x0bbc, 5, &page->ani_yparam_);
	writeChunk1(0x0bbd, page->data_k_);
}

void KEdgeRawWriter::writeEdge(KEdgeDocument *e) {
	K_assert(e);
	K_assert(e->bit_depth_==8 || e->bit_depth_==24);

	writeChunk1(0x03e8, e->bit_depth_);
	writeChunk1(0x03e9, e->data_a_);
	writeChunk1(0x03ea, e->data_b_);
	writeChunk1(0x03eb, e->data_c_);

	openChunk(0x03ed);
	{
		writeChunk1(0x03e8, e->data_d_);
		writeChunk1(0x03e9, e->data_d2_);
	}
	closeChunk();

	if (e->data_d3_.size() > 0) {
		writeChunkN(0x03ee, e->data_d3_);
	} else {
		// データサイズゼロとして書くのではなく
		// チャンク自体を書き込まない
	}

	writeChunkN(0x0fa1, e->data_e_);

	uint16_t numPages = (uint16_t)e->getPageCount();
	writeChunk2(0x07d0, numPages);
	writeChunk4(0x07d1, e->data_f_);
	writeChunk1(0x07d2, e->data_g_);

	// Pages
	K_assert(numPages > 0);
	for (uint16_t i=0; i<numPages; i++) {
		openChunk(0x07d3);
		writePage(e, i);
		closeChunk();
	}

	writeChunk4(0x07d4, e->data_h_);
	
	uint16_t numPalettes = (uint16_t)e->getPaletteCount();
	writeChunk2(0x0bb8, numPalettes);
	writeChunk2(0x0bb9, e->data_i_);
	writeChunk4(0x0bba, e->data_j_);

	// Palettes
	K_assert(numPalettes > 0);
	for (uint16_t i=0; i<numPalettes; i++) {
		openChunk(0x0bbb);
		const KEdgePalette *pal = e->palettes_[i];

		std::string name_mb = KConv::utf8ToAnsi(pal->name_u8, EDGE2_STRING_ENCODING);
		writeChunkN(0x03e8, name_mb.c_str());
		writeChunk4(0x03e9, pal->data_b_);
		writeChunk1(0x03ea, pal->data_c_);
		writeChunk1(0x03eb, pal->data_d_);
		writeChunk1(0x03ec, pal->data_e_);
		writeChunkN(0x03ed, pal->bgr_colors_);
		writeChunkN(0x03ef, pal->data_f_);
		writeChunkN(0x03f0, pal->data_g_);
		writeChunkN(0x03f1, pal->data_h_);
		writeChunkN(0x03f2, pal->data_i_);
		writeChunkN(0x03f3, pal->data_j_);
		writeChunk4(0x07d0, pal->data_k_);
		writeChunk1(0x07d1, pal->data_l_);
		writeChunk1(0x07d2, pal->data_m_);
		closeChunk();
	}

	writeChunk4(0x0bbc, e->data_k_);

	// 前景色
	openChunk(0x0bbf);
	{
		writeChunk1(0x03e8,    e->fg_palette_.color_index);
		writeChunkN(0x03e9, 3, e->fg_palette_.bgr_colors);
		writeChunk1(0x03ea,    e->fg_palette_.data_a);
		writeChunkN(0x03eb, 6, e->fg_palette_.data_b);
		writeChunk1(0x03ec,    e->fg_palette_.data_c);
	}
	closeChunk();
		
	// 背景色
	openChunk(0x0bc0);
	{
		writeChunk1(0x03e8,    e->bg_palette_.color_index);
		writeChunkN(0x03e9, 3, e->bg_palette_.bgr_colors);
		writeChunk1(0x03ea,    e->bg_palette_.data_a);
		writeChunkN(0x03eb, 6, e->bg_palette_.data_b);
		writeChunk1(0x03ec,    e->bg_palette_.data_c);
	}
	closeChunk();
}

void KEdgeRawWriter::writeCompressedEdge(FileOutput &file) const {
	// 圧縮前のサイズ
	uint32_t uncompsize = size();

	KEdgeBin zbuf = KZlib::compress_zlib(data(), size(), 1);

	// ヘッダーを出力
	uint16_t compflag = 256;
	file.write("EDGE2\0", 6);
	file.write((char *)&compflag, 2);
	file.write((char *)&uncompsize, 4);

	// 圧縮データを出力
	file.write(zbuf.data(), zbuf.size());
}

void KEdgeRawWriter::writeUncompressedEdge(KEdgeDocument *e) {
	K_assert(e);

	// 初期化
	clear();
		
	// 無圧縮 Edge データを作成
	writeEdge(e);
}
#pragma endregion // KEdgeRawWriter









#pragma region KEdgeWriter
KEdgeWriter::KEdgeWriter() {
	doc_ = new KEdgeDocument();
	doc_->palettes_.push_back(new KEdgePalette());
	doc_->bit_depth_ = 24;
}
KEdgeWriter::~KEdgeWriter() {
	delete (doc_);
}
void KEdgeWriter::clear() {
	delete(doc_);
	//
	doc_ = new KEdgeDocument();
	doc_->palettes_.push_back(new KEdgePalette());
	doc_->bit_depth_ = 24;
}
void KEdgeWriter::addPage(const KImage &img, int delay) {
	if (img.empty()) return;
	int img_w = img.getWidth();
	int img_h = img.getHeight();
	{
		KEdgePage *page = doc_->addPage();
		page->width_  = (uint16_t)img_w;
		page->height_ = (uint16_t)img_h;
		page->delay_ = delay;
		{
			KEdgeLayer *layer = page->addLayer();
			if (img.getBytesPerPixel() == 4) { // 32ビット画像から24ビット画像へ
				KImage image_rgb24(img_w, img_h, KImage::RGB24);
				int src_p = img.getPitch();
				const uint8_t *sbuf = img.getData();
				int dst_p = image_rgb24.getPitch();
				uint8_t *dbuf = image_rgb24.lockData();
				for (int y=0; y<img_h; y++) {
					for (int x=0; x<img_w; x++) {
						uint8_t r = sbuf[src_p * y + 4 * x + 0];
						uint8_t g = sbuf[src_p * y + 4 * x + 1];
						uint8_t b = sbuf[src_p * y + 4 * x + 2];
					//	uint8_t a = sbuf[src_p * y + 4 * x + 3];

						// edge へは BGR 順で保存することに注意
						dbuf[dst_p * y + 3 * x + 0] = b;
						dbuf[dst_p * y + 3 * x + 1] = g;
						dbuf[dst_p * y + 3 * x + 2] = r;
					}
				}
				layer->image_rgb24_ = image_rgb24;
			}
		}
	}
}
void KEdgeWriter::saveToStream(FileOutput &file) {
	K_assert(doc_);
	doc_->saveToFile(file);
}
#pragma endregion // KEdgeWriter



} // namespace


