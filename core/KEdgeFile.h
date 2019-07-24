#pragma once
#include <inttypes.h>
#include <stack>
#include "KRef.h"
#include "KChunkedFile.h"
#include "KImage.h"

/// ドット絵ツール Edge2 の標準ファイル形式 .edg ファイルを読み書きするためのクラス群
/// http://takabosoft.com/edge2


namespace mo {


/// パレットをサーフェスとして保存する時の画像サイズ
/// ひとつのパレットは、16x16ドットの RGBA32 で画像化される
static const int PALETTE_IMAGE_SIZE = 16;


class FileInput;
class FileOutput;
class KEdgePalette;
class KEdgeMemory;
class KEdgeLayer;
class KEdgePage;
class KEdgeDocument;
class Path;

typedef std::string KEdgeBin;

class KEdgePalette {
public:
	KEdgePalette();
	KEdgePalette(const KEdgePalette *copy);
public:
	std::string name_u8;
	uint32_t data_b_;
	uint8_t  data_c_;
	uint8_t  data_d_;
	uint8_t  data_e_;
	uint32_t data_k_;
	uint8_t  data_l_;
	uint8_t  data_m_;
	KEdgeBin bgr_colors_;
	KEdgeBin data_f_;
	KEdgeBin data_g_;
	KEdgeBin data_h_;
	KEdgeBin data_i_;
	KEdgeBin data_j_;
};

class KEdgeLayer {
public:
	KEdgeLayer();
	KEdgeLayer(const KEdgeLayer *source, bool sharePixels);
	~KEdgeLayer();
public:
	uint32_t delay_;
	uint8_t  is_grouped_;
	uint8_t  is_visibled_;
	uint8_t  label_color_rgbx_[4];
	uint32_t data_a_;
	uint8_t  data_b_;
	uint8_t  data_c_;
	uint8_t  data_d_;
	uint8_t  data_f_;
	uint8_t  data_g_;
	uint8_t  data_h_;
	KEdgeBin data_e_;
	std::string label_u8_;
	KImage image_rgb24_;
};

class KEdgePage {
public:
#pragma pack(push, 1)
	struct AniParam {
		uint32_t dest;
		uint8_t is_relative;
	};
#pragma pack(pop)
	KEdgePage();
	KEdgePage(const KEdgePage *source, bool copy_layers);
	~KEdgePage();
	KEdgeLayer * addLayer();
	KEdgeLayer * addCopyLayer(const KEdgeLayer *source, bool share_pixels);
	KEdgeLayer * addLayerAt(int index);
	void delLayer(int index);
	KEdgeLayer * getLayer(int index);
	const KEdgeLayer * getLayer(int index) const;
	int getLayerCount() const;
public:
	uint32_t delay_;
	uint8_t  is_grouped_;
	uint8_t  label_color_rgbx_[4];
	uint16_t width_;
	uint16_t height_;
	uint32_t data_a_;
	uint8_t  data_b_;
	uint8_t  data_c_;
	uint32_t data_d_;
	uint32_t data_e_;
	uint8_t  data_f_;
	uint32_t data_g_;
	uint32_t data_h_;
	uint8_t  data_i_;
	uint8_t  data_j_;
	uint8_t  data_k_;
	std::string label_u8_;
	std::vector<KEdgeLayer *> layers_;
	AniParam ani_xparam_;
	AniParam ani_yparam_;
};

class KEdgeDocument {
public:
	struct Pal {
		uint8_t color_index;
		uint8_t bgr_colors[3];
		uint8_t data_a;
		uint8_t data_b[6];
		uint8_t data_c;
	};
	KEdgeDocument();
	KEdgeDocument(const KEdgeDocument *source, bool copy_pages);
	~KEdgeDocument();
	void clear();

	bool loadFromFile(FileInput &file);
	bool loadFromFileName(const Path &filename);
	void saveToFile(FileOutput &file);
	void saveToFileName(const Path &filename);

	void autoComplete();
	KEdgePage * addPage();
	KEdgePage * addCopyPage(const KEdgePage *source);
	KEdgePage * addPageAt(int index);
	void delPage(int index);
	int getPageCount() const;
	KEdgePage * getPage(int index);
	const KEdgePage * getPage(int index) const;
	uint8_t * getPalettePointer(int index);
	int getPaletteCount() const;
	KEdgePalette * getPalette(int index);
	const KEdgePalette * getPalette(int index) const;
	KImage exportSurfaceRaw(int pageindex, int layerindex, int transparent_index) const;
	KImage exportSurface(int pageindex, int layerindex, int paletteindex) const;
	KImage exportSurfaceWithAlpha(int pageindex, int layerindex, int maskindex, int paletteindex) const;
	KImage exportPaletteImage() const;
	std::string expotyXml() const; // XML(utf8)
	int findColorIndex(int paletteindex, uint8_t r, uint8_t g, uint8_t b, int defaultindex) const;
	bool writeImageToLayer(int pageindex, int layerindex, int paletteindex, const KImage *surface);
public:
	uint16_t compression_flag_;
	uint8_t  bit_depth_;
	Pal      fg_palette_;
	Pal      bg_palette_;
	uint8_t  data_a_;
	uint8_t  data_b_;
	uint8_t  data_c_;
	uint8_t  data_d_;
	uint8_t  data_d2_;
	KEdgeBin data_d3_;
	KEdgeBin data_d4_;
	KEdgeBin data_e_;
	uint32_t data_f_;
	uint8_t  data_g_;
	uint32_t data_h_;
	uint16_t data_i_;
	uint32_t data_j_;
	uint32_t data_k_;
	std::vector<KEdgePalette *> palettes_;
	std::vector<KEdgePage *> pages_;
};

/// EDGE でエクスポートできるパレットファイル .PAL に対応
class KEdgePalFile {
public:
	KEdgePalFile();

	/// パレットをRGBA32ビット画像としてエクスポートする
	/// img: 書き出し用画像。16x16の大きさを持っていないといけない
	/// index0_for_transparent: インデックス[0]を完全透過として扱う
	KImage exportImage(bool index0_for_transparent) const;

	/// BGR 配列 256 色ぶん
	uint8_t bgr_[256*3];

	/// 不明な 256 バイトのデータ。透過色？
	uint8_t data_[256];

	/// パレットのラベル名
	std::string name_u8;
};

class KEdgePalReader: public KChunkedFileReader {
public:
	bool loadFromFile(FileInput &file);
	bool loadFromFileName(const Path &filename);
	bool loadFromMemoty(const void *data, size_t size);

	std::string exportXml(); // UTF8
	std::vector<KEdgePalFile> items_;
};

class KEdgeRawReader: public KChunkedFileReader {
public:
	static KEdgeBin unzip(FileInput &file, uint16_t *zflag=NULL);

	KEdgeRawReader();

	// Edge2 バイナリファイルからデータを読み取る
	int read(KEdgeDocument *e, FileInput &file);

private:
	void scanLayer(KEdgeDocument *e, KEdgePage *page, KEdgeLayer *layer);
	void scanPage(KEdgeDocument *e, KEdgePage *page);
	void scanEdge(KEdgeDocument *e);
	void readFromUncompressedBuffer(KEdgeDocument *e, const void *buf, size_t len);
};

class KEdgeRawWriter: public KChunkedFileWriter {
public:
	KEdgeRawWriter();
	void write(KEdgeDocument *e, FileOutput &file);
private:
	void writeLayer(KEdgePage *page, int layerindex, int bitdepth);
	void writePage(KEdgeDocument *e, int pageindex);
	void writeEdge(KEdgeDocument *e);
	void writeCompressedEdge(FileOutput &file) const;
	void writeUncompressedEdge(KEdgeDocument *e);
};

class KEdgeWriter {
	KEdgeDocument *doc_;
public:
	KEdgeWriter();
	~KEdgeWriter();
	void clear();
	void addPage(const KImage &img, int delay);
	void saveToStream(FileOutput &file);
};


} // namespace
