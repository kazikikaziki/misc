#include "KImage.h"
#include <mutex> // グローバル変数の保護用


#if 1
#	include "KLog.h"
#	define IMGASSERT(x)   Log_assert(x)
#	define IMGERROR(msg)  Log_error(msg)
#else
#	include <assert.h>
#	define IMGASSERT(x)   assert(x)
#	define IMGERROR(msg)  printf("%s\n", msg)
#endif



// stb_image
// https://github.com/nothings/stb/blob/master/stb_image.h
// license: public domain
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_perlin.h"

// ヘッダではなく、ソースファイルとしてとりこんでいることに注意
// ここを削除すると、当然ながらリンカエラーが発生する
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// RGBA32フォーマットでのバイト数
#define IMG32_BYTES_PER_PIXEL  4

// 整数だけで色計算するなら 1
#define INT_CALC 1

namespace mo {

#pragma region Functions
static int _min(int a, int b) {
	return (a < b) ? a : b;
}
static int _max(int a, int b) {
	return (a > b) ? a : b;
}
static float _clampf(float t) {
	return (t < 0.0f) ? 0.0f : ((t < 1.0f) ? t : 1.0f);
}
static int _clamp(int t, int a, int b) {
	if (t < a) return a;
	if (t < b) return t;
	return b;
}

/// a ÷ b の端数切り上げ整数を返す
static int _ceil_div(int a, int b) {
	IMGASSERT(b > 0);
	return (int)ceilf((float)a / b);
}

/// エンディアン変換 Big Endian <==> Little Endian
static void _swap_endian32(void *data32) {
	IMGASSERT(data32);
	uint8_t *p = (uint8_t *)data32;
	uint8_t t;
	t = p[0]; p[0]=p[3]; p[3] = t; // p[0] <==> p[3]
	t = p[1]; p[1]=p[2]; p[2] = t; // p[1] <==> p[2]
}

static std::string _png_file_from_rgba32(const void *data, int w, int h, int level) {
	static std::mutex s_mutex;
	std::string bin;

	// 圧縮レベルの指定にはグローバル変数を使うため、スレッドロックしておく
	s_mutex.lock();

	// 圧縮レベル
	int old_level = stbi_write_png_compression_level;
	stbi_write_png_compression_level = level;

	// 圧縮
	int stride = IMG32_BYTES_PER_PIXEL * w;
	int len;
	unsigned char *png = stbi_write_png_to_mem((const unsigned char *)data, stride, w, h, IMG32_BYTES_PER_PIXEL, &len);
	if (png) {
		bin.assign((const char *)png, len);
		STBIW_FREE(png);
	}

	// 圧縮レベル復帰
	stbi_write_png_compression_level = old_level;

	// スレッドロックを解除
	s_mutex.unlock();

	return bin;
}
#pragma endregion // Functions



#pragma region KImageUtils
/// 指定範囲の中にRGB>0なピクセルが存在するなら true を返す
bool KImageUtils::hasNonBlackPixel(const KImage &img, int x, int y, int w, int h) {
	IMGASSERT(img.getData());
	IMGASSERT(0 <= x && x+w <= img.getWidth());
	IMGASSERT(0 <= y && y+h <= img.getHeight());
	IMGASSERT(img.getFormat() == KImage::RGBA32);
	for (int yi=y; yi<y+h; yi++) {
		for (int xi=x; xi<x+w; xi++) {
			const Color32 color = img.getPixelFast(xi, yi);
			if (color.r || color.g || color.b) return true;
		}
	}
	return false;
}

/// 指定範囲の中に不透明なピクセルが存在するなら true を返す
bool KImageUtils::hasOpaquePixel(const KImage &img, int x, int y, int w, int h) {
	IMGASSERT(img.getData());
	IMGASSERT(0 <= x && x+w <= img.getWidth());
	IMGASSERT(0 <= y && y+h <= img.getHeight());
	IMGASSERT(img.getFormat() == KImage::RGBA32);
	for (int yi=y; yi<y+h; yi++) {
		for (int xi=x; xi<x+w; xi++) {
			const Color32 color = img.getPixelFast(xi, yi);
			if (color.a) return true;
		}
	}
	return false;
}


/// 画像をセル分割したとき、それぞれのセルがRGB>0なピクセルを含むかどうかを調べる
/// img 画像
/// cellsize セルの大きさ（ピクセル単位、正方形）
/// cells 有効なピクセルを含むセルのインデックス配列
void KImageUtils::scanNonBlackCells(const KImage &img, int cellsize, std::vector<int> *cells, int *xcells, int *ycells) {
	IMGASSERT(img.getData());
	IMGASSERT(img.getWidth() >= 0);
	IMGASSERT(img.getHeight() >= 0);
	int xcount = _ceil_div(img.getWidth(), cellsize);
	int ycount = _ceil_div(img.getHeight(), cellsize);
	cells->clear();
	for (int yi=0; yi<ycount; yi++) {
		for (int xi=0; xi<xcount; xi++) {
			int x = cellsize * xi;
			int y = cellsize * yi;
			int w = cellsize;
			int h = cellsize;
			adjustRect(img.getWidth(), img.getHeight(), &x, &y, &w, &h);
			if (hasNonBlackPixel(img, x, y, w, h)) {
				cells->push_back(xcount * yi + xi);
			}
		}
	}
	if (xcells) *xcells = xcount;
	if (ycells) *ycells = ycount;
}

/// 画像をセル分割したとき、それぞれのセルが不透明ピクセルを含むかどうかを調べる
/// img 画像
/// cellsize セルの大きさ（ピクセル単位、正方形）
/// cells 有効なピクセルを含むセルのインデックス配列
void KImageUtils::scanOpaqueCells(const KImage &img, int cellsize, std::vector<int> *cells, int *xcells, int *ycells) {
	IMGASSERT(!img.empty());
	IMGASSERT(img.getWidth() >= 0);
	IMGASSERT(img.getHeight() >= 0);
	int xcount = _ceil_div(img.getWidth(), cellsize);
	int ycount = _ceil_div(img.getHeight(), cellsize);
	cells->clear();
	for (int yi=0; yi<ycount; yi++) {
		for (int xi=0; xi<xcount; xi++) {
			int x = cellsize * xi;
			int y = cellsize * yi;
			int w = cellsize;
			int h = cellsize;
			adjustRect(img.getWidth(), img.getHeight(), &x, &y, &w, &h);
			if (hasOpaquePixel(img, x, y, w, h)) {
				cells->push_back(xcount * yi + xi);
			}
		}
	}
	if (xcells) *xcells = xcount;
	if (ycells) *ycells = ycount;
}

/// png のヘッダだけ読み取り、画像サイズを得る
bool KImageUtils::readPngImageSizeFromFile(const void *png_first_24bytes, int *w, int *h) {
	IMGASSERT(png_first_24bytes);
	IMGASSERT(w && h);

	// PNG ファイルフォーマット
	// https://www.setsuki.com/hsp/ext/png.htm

	// Portable Network Graphics
	// https://ja.wikipedia.org/wiki/Portable_Network_Graphics

	const char *SIGN = "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A";
	const char *IHDR = "IHDR";

	// offset [size]
	// 0x0000 [8] --- png signature (89 50 4E 47 0D 0A 1A 0A)
	// 0x0008 [4] --- IHDR chunk length
	// 0x000C [4] --- IHDR chunk type ("IHDR")
	// 0x0010 [4] --- image width (big endian!!)
	// 0x0014 [4] --- image height

	// png signature
	const uint8_t *ptr = (const uint8_t *)png_first_24bytes;
	if (memcmp(ptr+0x0000, SIGN, 8) != 0) {
		return false; // png ではない
	}

	// IHDR chunk type "IHDR"
	if (memcmp(ptr+0x000C, IHDR, 4) != 0) {
		return false; // 不正なヘッダ
	}

	uint32_t w32 = *(uint32_t*)(ptr+0x0010); // image width in BIG-ENDIGAN
	uint32_t h32 = *(uint32_t*)(ptr+0x0014); // image height in BIG-ENDIGAN
	_swap_endian32(&w32);
	_swap_endian32(&h32);

	*w = w32;
	*h = h32;
	return true;
}
#pragma endregion // KImageUtils



class CImageImpl: public KImage::Impl {
public:
	std::vector<uint8_t> buf_;
	int w_;
	int h_;
	KImage::Format fmt_;

	CImageImpl(int w, int h, KImage::Format fmt) {
		w_ = w;
		h_ = h;
		fmt_ = fmt;
		switch (fmt_) {
		case KImage::RGBA32: buf_.resize(w_ * h_ * 4); break;
		case KImage::RGB24:  buf_.resize(w_ * h_ * 3); break;
		case KImage::BGR24:  buf_.resize(w_ * h_ * 3); break;
		case KImage::INDEX8: buf_.resize(w_ * h_ * 1); break;
		default: break;
		}
	}
	virtual void info(int *w, int *h, KImage::Format *f) override {
		if (w) *w = w_;
		if (h) *h = h_;
		if (f) *f = fmt_;
	}
	virtual uint8_t * buffer() override {
		return buf_.data();
	}
	virtual uint8_t * lock() override {
		return buf_.data();
	}
	virtual void unlock() override {
	}
};


#pragma region KImage
KImage::KImage() {
	create(0, 0, NOFMT, NULL);
}
KImage::KImage(int w, int h, KImage::Format fmt, const void *data) {
	create(w, h, fmt, data);
}
KImage::KImage(int w, int h, const Color32 &color) {
	create(w, h, color);
}
void KImage::create(int w, int h, const Color32 &color) {
	create(w, h, RGBA32, NULL);
	fill(color);
}
void KImage::create(int w, int h, KImage::Format fmt, const void *data) {
	impl_ = NULL;
	if (w > 0 && h > 0) {
		impl_ = std::make_shared<CImageImpl>(w, h, fmt);
		if (data) {
			void *buf = impl_->lock();
			memcpy(buf, data, getDataSize());
			impl_->unlock();
		}
	}
}
void KImage::destroy() {
	create(0, 0, NOFMT, NULL);
}
void KImage::getInfo(int *w, int *h, Format *f) const {
	if (impl_) {
		impl_->info(w, h, f);
	}
}
const uint8_t * KImage::getData() const {
	if (impl_) {
		return impl_->buffer();
	}
	return NULL;
}
int KImage::getDataSize() const {
	int w=0, h=0;
	getInfo(&w, &h, NULL);
	return getBytesPerPixel() * w * h;
}
uint8_t * KImage::lockData() {
	if (impl_) {
		return impl_->lock();
	}
	return NULL;
}
void KImage::unlockData() {
	if (impl_) {
		impl_->unlock();
	}
}
bool KImage::empty() const {
	int w=0, h=0;
	getInfo(&w, &h, NULL);
	return w==0 || h==0;
}
int KImage::getWidth() const {
	int w = 0;
	getInfo(&w, NULL, NULL);
	return w;
}
int KImage::getHeight() const {
	int h = 0;
	getInfo(NULL, &h, NULL);
	return h;
}
KImage::Format KImage::getFormat() const {
	Format fmt = NOFMT;
	getInfo(NULL, NULL, &fmt);
	return fmt;
}
int KImage::getPitch() const {
	switch (getFormat()) {
	case NOFMT:  return getDataSize() / getHeight();
	case RGBA32: return getWidth() * 4;
	case RGB24:  return getWidth() * 3;
	case BGR24:  return getWidth() * 3;
	case INDEX8: return getWidth();
	default: return 0;
	}
}
int KImage::getBytesPerPixel() const {
	switch (getFormat()) {
	case NOFMT:  return 0;
	case RGBA32: return 4;
	case RGB24:  return 3;
	case BGR24:  return 3;
	case INDEX8: return 1;
	default: return 0;
	}
}
KImage KImage::clone() const {
	return KImage(getWidth(), getHeight(), getFormat(), getData());
}
std::string KImage::saveToMemory(int png_compress_level) const {
	std::string bin;
	if (getFormat() == RGBA32) {
		bin = _png_file_from_rgba32(getData(), getWidth(), getHeight(), png_compress_level);
	}
	if (bin.empty()) {
		IMGERROR("E_PNG: FAILED TO MAKE PNG IMAGE");
	}
	return bin;
}
bool KImage::loadFromMemory(const std::string &bin) {
	return loadFromMemory(bin.data(), bin.size());
}
bool KImage::loadFromMemory(const void *data, size_t size) {
	int w=0, h=0, comp=0;
	stbi_uc *p = stbi_load_from_memory((const stbi_uc *)data, size, &w, &h, &comp, 4);
	if (p) {
		create(w, h, KImage::RGBA32, p);
		stbi_image_free(p);
		return true;
	}
	IMGERROR("Failed to load image");
	return false;
}
void KImage::copyRect(int x, int y, const KImage &src, int sx, int sy, int sw, int sh) {
	KImage &dst = *this;

	// 同一フォーマット間でのみコピー可能
	if (dst.empty()) return;
	if (src.empty()) return;
	if (dst.getFormat() != src.getFormat()) return;

	// コピー範囲を調整
	KImageUtils::CopyRange range;
	range.dst_x = x;
	range.dst_y = y;
	range.src_x = sx;
	range.src_y = sy;
	range.cpy_w = sw;
	range.cpy_h = sh;
	KImageUtils::adjustRect(dst, src, &range);

	if (getFormat() == KImage::RGBA32) {
		uint8_t *dst_ptr = lockData();
		{
			const uint8_t *src_ptr = src.getData();
			int cpylen = range.cpy_w * IMG32_BYTES_PER_PIXEL;
			for (int yy=0; yy<range.cpy_h; yy++) {
				int dst_offset = dst.getOffsetBytes(range.dst_x, range.dst_y + yy);
				int src_offset = src.getOffsetBytes(range.src_x, range.src_y + yy);
				memcpy(dst_ptr+dst_offset, src_ptr+src_offset, cpylen);
			}
		}
		unlockData();
	}
}
void KImage::copyImage(int x, int y, const KImage &source) {
	if (empty()) return;
	if (source.empty()) return;

	int w=0, h=0; Format f=NOFMT;
	getInfo(&w, &h, &f);
	if (x==0 && y==0 && w==source.getWidth() && h==source.getHeight() && f==source.getFormat()) {
		void *buf = lockData();
		memcpy(buf, source.getData(), source.getDataSize());
		unlockData();
	} else {
		copyRect(x, y, source, 0, 0, source.getWidth(), source.getHeight());
	}
}
Color32 KImage::getPixelFast(int x, int y) const {
	int w=0, h=0; Format f=NOFMT;
	getInfo(&w, &h, &f);
	IMGASSERT(0 <= x && x < w);
	IMGASSERT(0 <= y && y < h);
	IMGASSERT(f == RGBA32);
	uint8_t *ptr = (uint8_t*)getData() + (w * y + x) * 4;
	return Color32(ptr[0], ptr[1], ptr[2], ptr[3]);
}
void KImage::setPixelFast(int x, int y, const Color32 &color) {
	int w=0, h=0; Format f=NOFMT;
	getInfo(&w, &h, &f);
	IMGASSERT(0 <= x && x < w);
	IMGASSERT(0 <= y && y < h);
	IMGASSERT(f == RGBA32);
	uint8_t *ptr = (uint8_t*)getData() + (w * y + x) * 4;
	ptr[0] = color.r;
	ptr[1] = color.g;
	ptr[2] = color.b;
	ptr[3] = color.a;
}
Color32 KImage::getPixel(int x, int y) const {
	int w=0, h=0; Format f=NOFMT;
	getInfo(&w, &h, &f);
	if (x < 0 || w <= x) return Color32();
	if (y < 0 || h <= y) return Color32();
	if (f != RGBA32) return Color32();
	return getPixelFast(x, y);
}
void KImage::setPixel(int x, int y, const Color32 &color) {
	int w=0, h=0; Format f=NOFMT;
	getInfo(&w, &h, &f);
	if (x < 0 || w <= x) return;
	if (y < 0 || h <= y) return;
	if (f != RGBA32) return;
	setPixelFast(x, y, color);
}
void KImage::fillRect(int left, int top, int width, int height, const Color32 &color) {
	// 塗りつぶし範囲を調整
	int x = left;
	int y = top;
	int w = width;
	int h = height;
	KImageUtils::adjustRect(getWidth(), getHeight(), &x, &y, &w, &h);

	if (getFormat() == RGBA32) {
		uint8_t *buf = lockData();
		// 1行目だけまじめに処理する
		{
			uint8_t *line = buf + getOffsetBytes(0, y);
			for (int i=x; i<x+w; i++) {
				uint8_t *p = line + IMG32_BYTES_PER_PIXEL * i;
				p[0] = color.r;
				p[1] = color.g;
				p[2] = color.b;
				p[3] = color.a;
			}
		}
		// 2行目以降は、1行目をコピーする
		{
			void *src = buf + getOffsetBytes(x, y);
			for (int i=y+1; i<y+h; i++) {
				void *dst = buf + getOffsetBytes(x, i);
				memcpy(dst, src, w * IMG32_BYTES_PER_PIXEL);
			}
		}
		unlockData();
		return;
	}
}
void KImage::fill(const Color32 &color) {
	int w=0, h=0;
	getInfo(&w, &h, NULL);
	fillRect(0, 0, w, h, color);
}
int KImage::getOffsetBytes(int x, int y) const {
	int w = getWidth();
	int h = getHeight();
	if (getFormat() == RGBA32) {
		IMGASSERT(0 <= x && x < w);
		IMGASSERT(0 <= y && y < h);
		return (y * w + x) * IMG32_BYTES_PER_PIXEL;
	}
	return 0;
}
KImage KImage::getChannel(int channel) const {
	if (empty()) return KImage();
	int w=0, h=0;
	getInfo(&w, &h, NULL);
	KImage dest(w, h);
	if (channel >= 0 && channel < getBytesPerPixel()) {
		const uint8_t *sbuf = getData();
		uint8_t *dbuf = dest.lockData();
		for (int y=0; y<h; y++) {
			for (int x=0; x<w; x++) {
				const uint8_t *s = sbuf + (w * y + x) * 4;
				uint8_t *d = dbuf + (w * y + x) * 4;
				d[0] = s[channel];
				d[1] = s[channel];
				d[2] = s[channel];
				d[3] = 255;
			}
		}
		dest.unlockData();
	}
	return dest;
}

#pragma endregion // KImage


#pragma region KImageUtils
inline Color32 Color32_mean2(const Color32 &x, const Color32 &y) {
	return Color32(
		(x.r + y.r) / 2,
		(x.g + y.g) / 2,
		(x.b + y.b) / 2,
		(x.a + y.a) / 2
	);
}
inline Color32 Color32_mean3(const Color32 &x, const Color32 &y, const Color32 &z) {
	return Color32(
		(x.r + y.r + z.r) / 3,
		(x.g + y.g + z.g) / 3,
		(x.b + y.b + z.b) / 3,
		(x.a + y.a + z.a) / 3
	);
}
inline Color32 Color32_mean4(const Color32 &x, const Color32 &y, const Color32 &z, const Color32 &w) {
	return Color32(
		(x.r + y.r + z.r + w.r) / 4,
		(x.g + y.g + z.g + w.g) / 4,
		(x.b + y.b + z.b + w.b) / 4,
		(x.a + y.a + z.a + w.a) / 4
	);
}

void KImageUtils::halfScale(KImage &image) {
	int w = image.getWidth()  / 2;
	int h = image.getHeight() / 2;
	KImage tmp(w, h);
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			// (x, y) を左上とする2x2の4ピクセルの平均色を計算する
#if INT_CALC
			Color32 A = image.getPixelFast(x*2,   y*2  );
			Color32 B = image.getPixelFast(x*2+1, y*2  );
			Color32 C = image.getPixelFast(x*2,   y*2+1);
			Color32 D = image.getPixelFast(x*2+1, y*2+1);
			Color32 out = Color32_mean4(A, B, C, D);
			tmp.setPixelFast(x, y, out);
#else
			Color A = image.getPixelFast(x*2,   y*2  );
			Color B = image.getPixelFast(x*2+1, y*2  );
			Color C = image.getPixelFast(x*2,   y*2+1);
			Color D = image.getPixelFast(x*2+1, y*2+1);
			Color out = (A + B + C + D) / 4; // 平均を求める。途中で桁あふれしないように Color32 ではなく Color で計算する
			tmp.setPixelFast(x, y, Color32(out));
#endif
		}
	}
	image = tmp.clone();
}
void KImageUtils::doubleScale(KImage &image) {
	int w = image.getWidth();
	int h = image.getHeight();
	KImage tmp(w*2, h*2);
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			Color32 c = image.getPixelFast(x, y);
			tmp.setPixelFast(x*2,   y*2,   c);
			tmp.setPixelFast(x*2+1, y*2,   c);
			tmp.setPixelFast(x*2,   y*2+1, c);
			tmp.setPixelFast(x*2+1, y*2+1, c);
		}
	}
	image = tmp.clone();
}
void KImageUtils::offset(KImage &image, int dx, int dy) {
	KImage tmp = image.clone();
	image.fill(Color32());
	image.copyImage(dx, dy, tmp);
}
void KImageUtils::mul(KImage &image, const Color32 &color32) {
	int w = image.getWidth();
	int h = image.getHeight();
	KImage tmp = image.clone();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
#if INT_CALC
			const Color32 &src = image.getPixelFast(x, y);
			Color32 out = Color32::mul(src, color32);
			tmp.setPixelFast(x, y, out);
#else
			Color colorf = color32;
			Color out = image.getPixelFast(x, y);
			out.r *= colorf.r;
			out.g *= colorf.g;
			out.b *= colorf.b;
			out.a *= colorf.a;
			tmp.setPixelFast(x, y, out);
#endif
		}
	}
	image = tmp.clone();
}
void KImageUtils::mul(KImage &image, float factor) {
	int w = image.getWidth();
	int h = image.getHeight();
	KImage tmp = image.clone();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
#if INT_CALC
			const Color32 &src = image.getPixelFast(x, y);
			Color32 out = Color32::mul(src, factor);
			tmp.setPixelFast(x, y, out);
#else
			Color out = image.getPixelFast(x, y);
			out *= factor;
			tmp.setPixelFast(x, y, out);
#endif
		}
	}
	image = tmp.clone();
}
void KImageUtils::inv(KImage &image) {
	// RGB を反転する。Alphaは無変更
	int w = image.getWidth();
	int h = image.getHeight();
	KImage tmp = image.clone();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			Color32 out = image.getPixelFast(x, y);
			out.r = 255 - out.r;
			out.g = 255 - out.g;
			out.b = 255 - out.b;
			tmp.setPixelFast(x, y, out);
		}
	}
	image = tmp.clone();
}
void KImageUtils::rgbToAlpha(KImage &image, const Color32 &fill) {
	// RGB グレースケール値を Alpha に変換し、RGBを fill で塗りつぶす
	int w = image.getWidth();
	int h = image.getHeight();
	KImage tmp = image.clone();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			Color32 color = image.getPixelFast(x, y);
			Color32 out;
			out.r = fill.r;
			out.g = fill.g;
			out.b = fill.b;
			out.a = color.grayscale();
			tmp.setPixelFast(x, y, out);
		}
	}
	image = tmp.clone();
}
static void _blur(KImage &dst, const KImage &src, int dx, int dy) {
	if (dx) {
		for (int y=0; y<src.getHeight(); y++) {
			for (int x=1; x+1<src.getWidth(); x++) {
#if INT_CALC
				Color32 val0 = src.getPixelFast(x-1, y);
				Color32 val1 = src.getPixelFast(x  , y);
				Color32 val2 = src.getPixelFast(x+1, y);
				Color32 dstval = Color32_mean3(val0, val1, val2);
				dst.setPixelFast(x, y, dstval);
#else
				Color val0 = src.getPixelFast(x-1, y);
				Color val1 = src.getPixelFast(x  , y);
				Color val2 = src.getPixelFast(x+1, y);
				Color32 dstval = (val0 + val1 + val2) / 3;
				dst.setPixelFast(x, y, dstval);
#endif
			}
		}
	} else {
		for (int y=1; y+1<src.getHeight(); y++) {
			for (int x=0; x<src.getWidth(); x++) {
#if INT_CALC
				Color32 val0 = src.getPixelFast(x, y-1);
				Color32 val1 = src.getPixelFast(x, y  );
				Color32 val2 = src.getPixelFast(x, y+1);
				Color32 dstval = Color32_mean3(val0, val1, val2);
				dst.setPixelFast(x, y, dstval);
#else
				Color val0 = src.getPixelFast(x, y-1);
				Color val1 = src.getPixelFast(x, y  );
				Color val2 = src.getPixelFast(x, y+1);
				Color32 dstval = (val0 + val1 + val2) / 3;
				dst.setPixelFast(x, y, dstval);
#endif
			}
		}
	}
}
void KImageUtils::blurX(KImage &dst, const KImage &src) {
	_blur(dst, src, 1, 0);
}
void KImageUtils::blurY(KImage &dst, const KImage &src) {
	_blur(dst, src, 0, 1);
}
void KImageUtils::blurX(KImage &image) {
	KImage tmp = image.clone();
	blurX(image, tmp);
}
void KImageUtils::blurY(KImage &image) {
	KImage tmp = image.clone();
	blurY(image, tmp);
}
void KImageUtils::silhouette(KImage &image, const Color32 &color) {
	int w = image.getWidth();
	int h = image.getHeight();
	KImage tmp = image.clone();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			Color32 out = image.getPixelFast(x, y);
			out.r = color.r;
			out.g = color.g;
			out.b = color.b;
			out.a = (uint8_t)((int)out.a * (int)color.a / 255);
			tmp.setPixelFast(x, y, out);
		}
	}
	image = tmp.clone();
}
/// 指定範囲が画像範囲を超えないように調整する
bool KImageUtils::adjustRect(int img_w, int img_h, int *_x, int *_y, int *_w, int *_h) {
	IMGASSERT(img_w >= 0);
	IMGASSERT(img_h >= 0);
	IMGASSERT(_x && _y && _w && _h);
	bool changed = false;
	int x0 = *_x;
	int y0 = *_y;
	int x1 = x0 + *_w; // x1は x範囲+1 であることに注意！
	int y1 = y0 + *_h; // y1は y範囲+1 であることに注意！
	if (x0 < 0) { x0 = 0; changed = true; }
	if (y0 < 0) { y0 = 0; changed = true; }
	if (x1 > img_w) { x1 = img_w; changed = true; }
	if (y1 > img_h) { y1 = img_h; changed = true; }
	if (changed) {
		*_x = x0;
		*_y = y0;
		*_w = _max(0, x1 - x0);
		*_h = _max(0, y1 - y0);
		return true;
	}
	return false;
}
bool KImageUtils::adjustRect(const KImage &dst, const KImage &src, CopyRange *range) {
	return adjustRect(dst.getWidth(), dst.getHeight(), src.getWidth(), src.getHeight(), range);
}
bool KImageUtils::adjustRect(int dst_w, int dst_h, int src_w, int src_h, CopyRange *range) {
	IMGASSERT(dst_w >= 0);
	IMGASSERT(dst_h >= 0);
	IMGASSERT(src_w >= 0);
	IMGASSERT(src_h >= 0);
	IMGASSERT(range);
	bool changed = false;

	// 転送元が範囲内に収まるように調整
	if (range->dst_x < 0) {
		int adj = -range->dst_x;
		range->dst_x += adj;
		range->src_x += adj;
		range->cpy_w -= adj;
		changed = true;
	}
	if (range->dst_y < 0) {
		int adj = -range->dst_y;
		range->dst_y += adj;
		range->src_y += adj;
		range->cpy_h -= adj;
		changed = true;
	}
	if (dst_w < range->dst_x + range->cpy_w) {
		int adj = range->dst_x + range->cpy_w - dst_w;
		range->cpy_w -= adj;
		changed = true;
	}
	if (dst_h < range->dst_y + range->cpy_h) {
		int adj = range->dst_y + range->cpy_h - dst_h;
		range->cpy_h -= adj;
		changed = true;
	}

	// 転送先が範囲内に収まるように調整
	if (range->src_x < 0) {
		int adj = -range->src_x;
		range->dst_x += adj;
		range->src_x += adj;
		range->cpy_w -= adj;
		changed = true;
	}
	if (range->src_y < 0) {
		int adj = -range->src_y;
		range->dst_y += adj;
		range->src_y += adj;
		range->cpy_h -= adj;
		changed = true;
	}
	if (src_w < range->src_x + range->cpy_w) {
		int adj = range->src_x + range->cpy_w - src_w;
		range->cpy_w -= adj;
		changed = true;
	}
	if (src_h < range->src_y + range->cpy_h) {
		int adj = range->src_y + range->cpy_h - src_h;
		range->cpy_h -= adj;
		changed = true;
	}

	// 矩形サイズが負の値にならないよう調整
	if (range->cpy_w < 0) {
		range->cpy_w = 0;
		changed = true;
	}
	if (range->cpy_h < 0) {
		range->cpy_h = 0;
		changed = true;
	}
	return changed;
}
void KImageUtils::blendAlpha(KImage &image, const KImage &over) {
	blend(image, 0, 0, over, KBlendFuncAlpha());
}
void KImageUtils::blendAlpha(KImage &image, int x, int y, const KImage &over, const Color32 &color) {
	blend(image, x, y, over, KBlendFuncAlpha(color));
}
void KImageUtils::blendAdd(KImage &image, const KImage &over) {
	blend(image, 0, 0, over, KBlendFuncAdd());
}
void KImageUtils::blendAdd(KImage &image, int x, int y, const KImage &over, const Color32 &color) {
	blend(image, x, y, over, KBlendFuncAdd(color));
}
void KImageUtils::horzLine(KImage &image, int x0, int x1, int y, const Color32 &color) {
	if (y < 0 || image.getHeight() <= y) return;
	x0 = _max(x0, 0);
	x1 = _min(x1, image.getWidth()-1);
	for (int i=x0; i<=x1; i++) {
		image.setPixelFast(i, y, color);
	}
}
void KImageUtils::vertLine(KImage &image, int x, int y0, int y1, const Color32 &color) {
	if (x < 0 || image.getWidth() <= x) return;
	y0 = _max(y0, 0);
	y1 = _min(y1, image.getHeight()-1);
	for (int i=y0; i<=y1; i++) {
		image.setPixelFast(x, i, color);
	}
}
KImage KImageUtils::perlin(int size, int xwrap, int ywrap, float mul) {
	KImage img(size, size);

	uint8_t *dst = img.lockData();
	for (int y=0; y<size; y++) {
		for (int x=0; x<size; x++) {
			float px = (float)xwrap * x / size;
			float py = (float)ywrap * y / size;
			float raw = stb_perlin_noise3(px, py, 0.0f, xwrap, ywrap, 0);

			// ノイズ値は -1 以上 1 以下なので、これを 0 以上 255 以下に再スケールする
			float nor = (raw + 1.0f) / 2.0f; //[-1.0 .. 1.0] --> [0.0 .. 1.0]
			uint8_t value = (uint8_t)_clamp((int)(nor * mul * 255), 0, 255);
			// グレースケール画像として書き込む
			int idx = (size * y + x) * 4;
			dst[idx + 0] = value; // R
			dst[idx + 1] = value; // G
			dst[idx + 2] = value; // B
			dst[idx + 3] = 255;   // A
		}
	}
	img.unlockData();
	return img;
}

#pragma endregion // KImageUtils



void Test_imageutils() {
	{
		const uint32_t data4x4[] = {
			0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000,
			0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000,
			0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
			0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000,
		};
		KImage img(4, 4, KImage::RGBA32, NULL);
		for (int i=0; i<16; i++) {
			img.setPixelFast(i%4, i/4, Color32(data4x4[i]));
		}
		KImageUtils::halfScale(img); // 2x2ごとに混合（単純平均）しサイズを1/2に
		IMGASSERT(img.getWidth() == 2);
		Color32 a = img.getPixelFast(0, 0);
		Color32 b = img.getPixelFast(1, 0);
		Color32 c = img.getPixelFast(0, 1);
		Color32 d = img.getPixelFast(1, 1);
		IMGASSERT(a == Color32(0x3F3F3F3F)); // 0xFFFFFFFF の 1/4
		IMGASSERT(b == Color32(0x7F7F7F7F)); // 0xFFFFFFFF の 1/2
		IMGASSERT(c == Color32(0xFFFFFFFF)); // 0xFFFFFFFF の 1/1
		IMGASSERT(d == Color32(0x7F7F7F7F)); // 0xFFFFFFFF の 1/2
	}

	{
		KImage img(3, 1, KImage::RGBA32, NULL);
		img.setPixelFast(0, 0, Color32(0x00000000));
		img.setPixelFast(1, 0, Color32(0x33333333));
		img.setPixelFast(2, 0, Color32(0xFFFFFFFF));
		KImageUtils::blurX(img); // 3x1の範囲を単純平均
		Color32 a = img.getPixelFast(1, 0); // 中心ピクセルを取る
		IMGASSERT(a == Color32(0x66666666)); // (0x00 + 0x33 + 0xFF)/3
	}

	{
		KImage img(1, 1, Color32(30, 30, 30, 255));
		KImageUtils::mul(img, 2.0f); // RGBA各要素を 2.0 倍し、0..255の範囲に丸める
		Color32 a = img.getPixelFast(0, 0);
		IMGASSERT(a == Color32(60, 60, 60, 255));
	}

	{
		KImage img(1, 1, Color32(30, 30, 30, 255));
		KImageUtils::mul(img, Color32(127, 127, 255, 255/10)); // RGBA各要素を 0.5, 0.5, 1.0, 0.1 倍
		Color32 a = img.getPixelFast(0, 0);
		IMGASSERT(a == Color32(14, 14, 30, 25));
	}

	{
		KImage img(1, 1, Color32(55, 55, 55, 230));
		KImageUtils::inv(img);// RGBを反転。Aは不変
		Color32 a = img.getPixelFast(0, 0);
		IMGASSERT(a == Color32(200, 200, 200, 230));
	}
}


} // namespace
