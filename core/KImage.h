#pragma once
#include "KColor.h"
#include <vector>
#include <memory>
#include <inttypes.h>

namespace mo {

/// 画像データを保持・管理するためのクラスで、画像ファイルのセーブとロード、基本的な画像処理に対応する。
/// ブレンド関数を指定しての合成など、もう少し高度なことが行いたい場合は KImageUtils を使う
/// @see KImageUtils
/// @note
/// 代入演算子 = で代入した場合は、単なる参照コピー（ポインタのコピーと同義）になる。
/// 本当の意味でのコピーを作りたい場合は KImage::clone() を使う
class KImage {
public:
	enum Format {
		NOFMT,  ///< フォーマット情報なし
		RGBA32, ///< R8 G8 B8 A8（4バイト/ピクセル）
		RGB24,  ///< R8 G8 B8（3バイト/ピクセル）
		BGR24,  ///< B8 G8 R8（3バイト/ピクセル）
		INDEX8, ///< インデックス画像（1バイト/ピクセル）
	};

	/// コンストラクタ
	/// 何のデータも保持しない。画像サイズは 0 x 0 になり、 empty() は true を返す
	KImage();

	/// コンストラクタ
	/// 指定されたサイズと色で初期化する。
	/// @see create(int, int, const Color32 &)
	KImage(int w, int h, const Color32 &color);

	/// コンストラクタ。
	/// 指定されたサイズで初期化する。
	/// @see create(int, int, Format, const void *) を参照
	KImage(int w, int h, Format fmt=RGBA32, const void *data=NULL);

	/// 画像サイズと色を指定して画像を初期化する
	/// @param w, h  画像サイズ
	/// @param color 色
	void create(int w, int h, const Color32 &color=Color32(0,0,0,0));

	/// 画像サイズとフォーマット、ピクセルデータを指定して画像を初期化する
	/// @param w, h 画像サイズ
	/// @param fmt  フォーマット
	/// @param data ピクセルデータ。w, h, fmt で指定したサイズとフォーマットに従っていないといけない。
	///             NULL を指定した場合は 0 で初期化される
	void create(int w, int h, Format fmt, const void *data);

	/// 画像を廃棄する。普通はこの関数を呼ぶ必要はないが、
	/// 明示的に empty() の状態にしたい場合に用いる
	void destroy();

	/// 画像情報を保持していなければ true を返す
	/// この場合 getWidth() と getHeight() は 0 になり、 getData() は NULL になる
	bool empty() const;

	/// 横ピクセル数
	int getWidth() const;

	/// 縦ピクセル数
	int getHeight() const;
	
	/// 1行分のピクセルを表現するのに何バイト使うかを返す。
	/// getBytesPerPixel() * getWidth() と一致するとは限らない
	int getPitch() const;

	/// ピクセルフォーマットを返す
	Format getFormat() const;

	/// １ピクセルを表現するのに何バイト使うかを返す。
	/// RGBA32 フォーマットなら 4 になる
	int getBytesPerPixel() const;

	/// データのアドレスを返す（読み取り専用）
	/// @see lockData()
	/// @see getOffsetBytes()
	const uint8_t * getData() const;

	/// データをスレッドロックし、アドレスを返す
	/// @see unlockData()
	/// @see getOffsetBytes()
	uint8_t * lockData();

	/// データのスレッドロックを解除する
	void unlockData();

	/// データのバイト数を返す
	int getDataSize() const;

	/// 画像のコピーを作成する
	/// @note 複製には必ず clone() を用いる。代入記号 = は参照コピーになる
	KImage clone() const;

	/// 画像をロードする。
	/// このライブラリでは画像のロードについて外部ライブラリ stb_image.h を利用しているため、
	/// 受け入れ可能フォーマットは stb_image.h に依存する。
	/// ライブラリが通常オプションでビルドされているなら、JPEG, PNG, BMP, GIF, PSD, PIC, PNM　に対応する。
	/// @see https://github.com/nothings/stb
	bool loadFromMemory(const void *data, size_t size);
	bool loadFromMemory(const std::string &bin);

	/// 画像の png フォーマット表現を返す。
	/// @param png_compress_level PNGの圧縮レベルを指定する。0で無圧縮、9で最大圧縮
	std::string saveToMemory(int png_compress_level=1) const;

	/// source の部分画像を、書き込み座標 (x, y) を左上とする矩形にコピーする
	/// @note RGBA32 フォーマットでのみ利用可能
	void copyRect(int x, int y, const KImage &source, int sx, int sy, int sw, int sh);
	
	/// source 全体を、書き込み座標 (x, y) を左上とする矩形にコピーする
	/// @note RGBA32 フォーマットでのみ利用可能
	void copyImage(int x, int y, const KImage &source);

	/// ピクセル色を得る。
	/// 範囲外座標を指定した場合は Color32::ZERO を返す
	/// @note RGBA32 フォーマットでのみ利用可能
	Color32 getPixel(int x, int y) const;

	/// ピクセル色を設定する。
	/// @note RGBA32 フォーマットでのみ利用可能
	void setPixel(int x, int y, const Color32 &color);

	/// getPixel() と同じだが、範囲チェックを行わない分高速になる。
	/// （ただしデバッグビルドでは assert によるチェックが入る）
	Color32 getPixelFast(int x, int y) const;

	/// setPixel() と同じだが、範囲チェックを行わない分高速になる。
	/// （ただしデバッグビルドでは assert によるチェックが入る）
	void setPixelFast(int x, int y, const Color32 &value);

	/// x,y,w,h で指定された範囲を color で塗りつぶす。
	/// fillRect(0, 0, getWidth(), getHeight(), color) とすれば
	/// fill(color) と同じになる
	/// @note RGBA32 フォーマットでのみ利用可能
	void fillRect(int x, int y, int w, int h, const Color32 &color);

	/// 画像を color で塗りつぶす
	/// fill(Color32::ZERO) にすると 0 で塗りつぶすことになる
	/// @note RGBA32 フォーマットでのみ利用可能
	void fill(const Color32 &color);

	/// 指定されたチャンネルだけ抜き出す
	/// @param channel ピクセル内でのバイトオフセット。
	///                フォーマットが RGBA32 であれば、0=R, 1=G, 2=B, 3=A になる
	/// @return チャンネル値をグレースケールで表現したRGBA32画像を返す。
	KImage getChannel(int channel) const;

	/// ピクセル座標を指定して、そのピクセルまでのオフセットバイト数を得る。
	/// このバイト数を getData() や lockData() で取得したアドレスに足すと、
	/// 目的ピクセルのアドレスになる
	int getOffsetBytes(int x, int y) const;

	class Impl {
	public:
		virtual ~Impl() {}
		virtual uint8_t * buffer() = 0;
		virtual uint8_t * lock() = 0;
		virtual void unlock() = 0;
		virtual void info(int *w, int *h, Format *fmt) = 0;
	};

private:
	void getInfo(int *w, int *h, Format *f) const;

	std::shared_ptr<Impl> impl_;
};

class KImageUtils {
public:
	/// 指定範囲の中にRGB>0なピクセルが存在するなら true を返す
	static bool hasNonBlackPixel(const KImage &img, int x, int y, int w, int h);

	/// 指定範囲の中に不透明なピクセルが存在するなら true を返す
	static bool hasOpaquePixel(const KImage &img, int x, int y, int w, int h);

	/// 画像をセル分割したとき、それぞれのセルがRGB>0なピクセルを含むかどうかを調べる
	/// @param img 画像
	/// @param cellsize セルの大きさ（ピクセル単位、正方形）
	/// @param cells[out] セル内のピクセル状態。全ピクセルのRGBが0なら false, 一つでも非ゼロピクセルが存在するなら true
	/// @param xcells, ycells[out] 取得した縦横のセルの数。NULLでもよい。画像が中途半端な大きさだった場合は余白が拡張されることに注意
	static void scanNonBlackCells(const KImage &img, int cellsize, std::vector<int> *cells, int *xcells, int *ycells);

	/// 画像をセル分割したとき、それぞれのセルが不透明ピクセルを含むかどうかを調べる
	/// @param img 画像
	/// @param cellsize セルの大きさ（ピクセル単位、正方形）
	/// @param cells[out] セル内のピクセル状態。全ピクセルの Alpha が 0 なら false, 一つでも Alpha > 0 なピクセルが存在するなら true
	/// @param xcells, ycells[out] 取得した縦横のセルの数。NULLでもよい。画像が中途半端な大きさだった場合は余白が拡張されることに注意
	static void scanOpaqueCells(const KImage &image, int cellsize, std::vector<int> *cells, int *xcells, int *ycells);

	/// png のヘッダだけ読み取り、画像サイズを得る
	/// @param png_first_24bytes png ファイルの最初の24バイト部分を指定する
	static bool readPngImageSizeFromFile(const void *png_first_24bytes, int *w, int *h);

	struct CopyRange {
		int dst_x, dst_y;
		int src_x, src_y;
		int cpy_w, cpy_h;
	};

	/// 指定範囲が画像範囲を超えないように調整する
	static bool adjustRect(int img_w, int img_h, int *_x, int *_y, int *_w, int *_h);
	static bool adjustRect(int dst_w, int dst_h, int src_w, int src_h, CopyRange *range);
	static bool adjustRect(const KImage &dst, const KImage &src, CopyRange *range);

	/// ユーザー定義の方法で画像を重ねる
	/// @param image 元の画像
	/// @param owner 重ねる画像
	/// @param pred ピクセルごとの合成計算を定義したオブジェクト。
	///   以下のオペレータが実装されたクラスを指定する。
	///   Color32 operator()(const Color32 &dst, const Color32 &src) const;
	///
	///   定義済みのクラスは以下の通り
	///   - KBlendFuncMin
	///   - KBlendFuncMax
	///   - KBlendFuncAdd
	///   - KBlendFuncSub
	///   - KBlendFuncAlpha
	template <typename T> static void blend(KImage &image, int x, int y, const KImage &over, const T &pred);

	/// #blend の pred に KBlendFuncAlpha を与えたもの
	static void blendAlpha(KImage &image, const KImage &over);

	/// #blend の pred に KBlendFuncAlpha を与えたもの
	static void blendAlpha(KImage &image, int x, int y, const KImage &over, const Color32 &color=Color32::WHITE);

	/// #blend の pred に KBlendFuncAdd を与えたもの
	static void blendAdd(KImage &image, const KImage &over);

	/// #blend の pred に KBlendFuncAdd を与えたもの
	static void blendAdd(KImage &image, int x, int y, const KImage &over, const Color32 &color=Color32::WHITE);
	
	/// X方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurX(KImage &image);
	
	/// X方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurX(KImage &dst, const KImage &src);

	/// Y方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurY(KImage &image);

	/// Y方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurY(KImage &dst, const KImage &src);

	/// 画像サイズを 1/2 にする
	static void halfScale(KImage &image);

	/// 画像サイズを 2 倍にする
	static void doubleScale(KImage &image);

	/// ピクセルごとに定数 mul を乗算する
	static void mul(KImage &image, float mul);

	/// ピクセルごとにカラー定数 color を乗算合成する
	/// @param color image の各ピクセルに対して乗算合成する色。
	///              単純な乗算ではないことに注意。
	///              乗算数 color の各値は 0 ～ 255 になっているが、それを 0.0 ～ 1.0 と解釈して乗算する。
	///              例えば color に Color32(127,127,127,127) を指定するのは、ピクセルのRGBA各値を 0.5 倍するのと同じ
	static void mul(KImage &image, const Color32 &color);

	/// 画像を dx, dy だけずらす
	static void offset(KImage &image, int dx, int dy);

	/// 水平線を描画する
	static void horzLine(KImage &image, int x0, int x1, int y, const Color32 &color);

	/// 垂直線を描画する
	static void vertLine(KImage &image, int x, int y0, int y1, const Color32 &color);

	/// 透明部分だけを残し、不透明部分をすべて color に置き換える
	static void silhouette(KImage &image, const Color32 &color);

	/// RGB を反転する。Alphaは変更しない
	static void inv(KImage &image);

	/// RGB グレースケール化した値を Alpha に適用し、RGBを fill で塗りつぶすことで、
	/// グレースケール画像をアルファチャンネルに変換する
	static void rgbToAlpha(KImage &image, const Color32 &fill=Color32::WHITE);
	
	/// パーリンノイズ画像（正方形）を作成する
	static KImage perlin(int size=256, int xwrap=4.0f, int ywrap=4.0f, float mul=1.0f);
};

#pragma region BlendFunc
/// min 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncMin {
	Color32 operator()(const Color32 &dst, const Color32 &src) const {
		return Color32::getmin(dst, src);
	}
};

/// max 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncMax {
	Color32 operator()(const Color32 &dst, const Color32 &src) const {
		return Color32::getmax(dst, src);
	}
};

/// add 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncAdd {
	Color32 color_; ///< src に乗算する色
	KBlendFuncAdd(Color32 color=Color32::WHITE) {
		color_ = color;
	}
	Color32 operator()(const Color32 &dst, const Color32 &src) const {
		return Color32::addAlpha(dst, src, color_);
	}
};

/// sub 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncSub {
	Color32 color_; ///< src に乗算する色
	KBlendFuncSub(Color32 color=Color32::WHITE) {
		color_ = color;
	}
	Color32 operator()(const Color32 &dst, const Color32 &src) const {
		return Color32::subAlpha(dst, src, color_);
	}
};

/// alpha 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncAlpha {
	Color32 color_; ///< src に乗算する色
	KBlendFuncAlpha(Color32 color=Color32::WHITE) {
		color_ = color;
	}
	Color32 operator()(const Color32 &dst, const Color32 &src) const {
		return Color32::blendAlpha(dst, src, color_);
	}
};
#pragma endregion // BlendFunc


/// KImageUtils::blend 実装部
template <typename T> void KImageUtils::blend(KImage &image, int x, int y, const KImage &over, const T &pred) {
	CopyRange range;
	range.dst_x = x;
	range.dst_y = y;
	range.src_x = 0;
	range.src_y = 0;
	range.cpy_w = over.getWidth();
	range.cpy_h = over.getHeight();
	adjustRect(image, over, &range);

	for (int yy=0; yy<range.cpy_h; yy++) {
		for (int xx=0; xx<range.cpy_w; xx++) {
			Color32 src = over .getPixelFast(range.src_x + xx, range.src_y + yy);
			Color32 dst = image.getPixelFast(range.dst_x + xx, range.dst_y + yy);
			Color32 out = pred(dst, src);
			image.setPixelFast(range.dst_x + xx, range.dst_y + yy, out);
		}
	}
}



void Test_imageutils();


} // namespace

