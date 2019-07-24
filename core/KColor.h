#pragma once
#include <inttypes.h>

namespace mo {

static const float MAX_COLOR_ERROR = 0.5f / 255.0f;

class Color32;

class Color {
public:
	static const Color WHITE; ///< Color(1.0f, 1.0f, 1.0f, 1.0f) を定数化したもの
	static const Color BLACK; ///< Color(0.0f, 0.0f, 0.0f, 1.0f) を定数化したもの
	static const Color ZERO;  ///< Color(0.0f, 0.0f, 0.0f, 0.0f) を定数化したもの。コンストラクタによる初期状態 Color() と同じ

public:
	static Color lerp(const Color &color0, const Color &color1, float t);

	/// 各要素ごとに大きい方の値を選択した結果を返す
	static Color getmax(const Color &dst, const Color &src);

	/// 各要素ごとに小さい方の値を選択した結果を返す
	static Color getmin(const Color &dst, const Color &src);

	/// 単純加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color add(const Color &dst, const Color &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color addAlpha(const Color &dst, const Color &src);
	static Color addAlpha(const Color &dst, const Color &src, const Color &factor);
	
	/// 単純減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color sub(const Color &dst, const Color &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color subAlpha(const Color &dst, const Color &src);
	static Color subAlpha(const Color &dst, const Color &src, const Color &factor);

	/// ARGB の成分ごとの乗算
	static Color mul(const Color &dst, const Color &src);

	/// アルファブレンドを計算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color blendAlpha(const Color &dst, const Color &src);
	static Color blendAlpha(const Color &dst, const Color &src, const Color &factor);

public:
	Color();
	Color(float _r, float _g, float _b, float _a);
	Color(const Color &rgb, float _a);
	Color(const Color32 &argb32);

	Color32 toColor32() const;

	float * floats() const;

	/// RGBA 値がゼロならば true を返す
	float grayscale() const;
	/// 各成分の値を 0 以上 1 以下にクリップした結果を返す
	Color clamp() const;
	Color operator + (const Color &op) const;
	Color operator + (float k) const;
	Color operator - (const Color &op) const;
	Color operator - (float k) const;
	Color operator - () const;
	Color operator * (const Color &op) const;
	Color operator * (float k) const;
	Color operator / (float k) const;
	Color operator / (const Color &op) const;
	bool operator == (const Color &op) const;
	bool operator != (const Color &op) const;
	Color & operator += (const Color &op);
	Color & operator += (float k);
	Color & operator -= (const Color &op);
	Color & operator -= (float k);
	Color & operator *= (const Color &op);
	Color & operator *= (float k);
	Color & operator /= (const Color &op);
	Color & operator /= (float k);

	bool equals(const Color &other, float tolerance=1.0f/255) const;

	float r, g, b, a;
};


/// Color と同じくRGBA色を表すが、それぞれの色要素を float ではなく符号なし8ビット整数で表す。
/// 各メンバーは B, G, R, A の順番で並んでおり、Direct3D9 で使われている 32 ビット整数の ARGB 表記
/// 0xAARRGGBB のリトルエンディアン形式と同じになっている。
/// そのため、Color32 をそのまま頂点配列の DWORD として使う事ができる
/// @see Color
/// @see Vertex
class Color32 {
public:
	static const Color32 WHITE; ///< Color32(255, 255, 255, 255) を定数化したもの
	static const Color32 BLACK; ///< Color32(0, 0, 0, 255) を定数化したもの
	static const Color32 ZERO;  ///< Color32(0, 0, 0, 0) を定数化したもの。コンストラクタによる初期状態 Color32() と同じ

	/// 各要素ごとに大きい方の値を選択した結果を返す
	static Color32 getmax(const Color32 &dst, const Color32 &src);

	/// 各要素ごとに小さい方の値を選択した結果を返す
	static Color32 getmin(const Color32 &dst, const Color32 &src);

	/// 単純加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color32 add(const Color32 &dst, const Color32 &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color32 addAlpha(const Color32 &dst, const Color32 &src);
	static Color32 addAlpha(const Color32 &dst, const Color32 &src, const Color32 &factor);
	
	/// 単純減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color32 sub(const Color32 &dst, const Color32 &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color32 subAlpha(const Color32 &dst, const Color32 &src);
	static Color32 subAlpha(const Color32 &dst, const Color32 &src, const Color32 &factor);

	/// ARGB の成分ごとの乗算
	static Color32 mul(const Color32 &dst, const Color32 &src);
	static Color32 mul(const Color32 &dst, float factor);

	/// アルファブレンドを計算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static Color32 blendAlpha(const Color32 &dst, const Color32 &src);
	static Color32 blendAlpha(const Color32 &dst, const Color32 &src, const Color32 &factor);

	/// 線形補完した色を返す。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param t ブレンド係数。0 なら dst 100% になり、1 なら src 100% になる
	static Color32 lerp(const Color32 &x, const Color32 &y, float t);

	static Color32 fromARGB32(uint32_t argb32);
	static Color32 fromXRGB32(uint32_t argb32);

	Color32();
	Color32(const Color32 &other);

	/// コンストラクタ。
	/// 0 以上 255 以下の値で R, G, B, A を指定する。
	/// 範囲外の値を指定した場合でも丸められないので注意 (0xFF との and を取る）
	///
	/// @note
	/// 符号なし32ビットでの順番 A, R, G, B とは引数の順番が異なるので注意すること。
	/// これは Color::Color(float, float, float, float) の順番と合わせてあるため。
	Color32(int _r, int _g, int _b, int _a);

	/// 0xAARRGGBB 形式の符号なし32ビット整数での色を指定する。
	/// Color32(0x11223344) は Color32(0x44, 0x11, 0x22, 0x33) と同じ
	explicit Color32(uint32_t argb);

	Color32(const Color &color);
	bool operator == (const Color32 &other) const;
	bool operator != (const Color32 &other) const;

	Color toColor() const;

	/// uint32_t での色表現（AARRGGBB）を返す
	uint32_t toUInt32() const;

	/// NTSC 加重平均法によるグレースケール値（256段階）を返す
	uint8_t grayscale() const;

	/// リトルエンディアンで uint32_t にキャストしたときに ARGB の順番になるようにする
	/// 0xAARRGGBB <==> *(uint32_t*)(&color32_value)
	uint8_t b, g, r, a;
};

};