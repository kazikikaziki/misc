#pragma once
#include <vector>
#include <string>
#include "KImage.h"
#include "KPath.h"

namespace mo {

struct Vertex;

/// 画像以外の情報
struct SImgExtraData {
	SImgExtraData() {
		page = 0;
		layer = 0;
		blend = -1;
		data0 = 0;
	}

	int page;  /// ページ番号
	int layer; /// レイヤー番号
	int blend; /// Blend
	int data0;  /// User data
};

/// 画像情報
struct SImgPackItem {
	SImgPackItem() {
		xcells = 0;
		ycells = 0;
		pack_offset = 0;
	}

	std::vector<int> cells;
	KImage img;
	int xcells;
	int ycells;
	int pack_offset;
	SImgExtraData extra;
};

/// パック画像を作成する
class CImgPackW {
public:
	CImgPackW(int cellsize=16);
	void init(int cellsize);

	/// パックに画像を追加する
	/// img: 追加する画像
	/// extra: 画像に付随する情報。不要なら NULL を指定する
	void addImage(const KImage &img, const SImgExtraData *extra);

	/// パックを保存する
	/// imagefile: 画像ファイル名（getPackedImageで得られた画像を書き込む）
	/// metafile:  テキストファイル名（getMetaStringで得られたテキストを書き込む）
	void saveToFileNames(const Path &imagefile, const Path &metafile) const;

	/// ユーザー定義の拡張データ文字列を追加する
	void setExtraString(const char *data);

	/// パッキングされた画像を得る
	KImage getPackedImage() const;

	/// パッキング用画像のために必要な画像サイズを得る
	bool getBestSize(int *w, int *h) const;

	/// メタ情報テキストを得る
	void getMetaString(std::string *info) const;

private:
	int getNumberOfCellsRequired() const;
	bool isCapacityOK(int dest_width, int dest_height) const;

	std::string extra_;
	std::vector<SImgPackItem> itemlist_;
	int cellsize_;
	int cellspace_;
	int total_cells_;
};

/// パック画像をロードする
class CImgPackR {
public:
	CImgPackR();
	void clear();

	/// パック画像情報をロードする
	/// メタ文字列は CImgPackW::saveToFileNames で保存されたもの
	void loadFromMetaString(const char *metatext);

	/// パック画像情報をロードする
	/// CImgPackW::saveToFileNames で保存したメタ文字列が入っているファイル名を指定する
	void loadFromMetaFileName(const wchar_t *metafilename);

	/// 画像数を得る
	int getImageCount() const;

	/// インデックスを指定して、その画像の大きさを得る
	void getImageSize(int index, int *w, int *h) const;

	/// インデックスを指定して、その画像の拡張パラメータを得る
	void getImageExtra(int index, SImgExtraData *extra) const;

	/// このパック画像に関連付けられたユーザー定義の文字列を得る。
	/// これは CImgPack::setExtraString() で指定されたものと同じ
	const char * getExtraString() const;

	/// インデックスを指定して、その画像を得る
	/// pack_img  パックされた画像。（CImgPackW::saveToFileNames で保存した画像）
	/// index     取得したい画像のインデックス番号
	/// orig_img  書き込み先の画像を指定する。事前に getImageSize で画像サイズを調べ、
	///           充分なサイズで初期化しておくこと
	void getImage(const KImage &pack_img, int index, KImage &orig_img) const;

	/// index 番目の画像に必要な頂点数を得る (TRIANGLE_LIST)
	int getVertexCount(int index) const;

	/// 頂点座標配列を作成する（ワールド座標系は左下原点のY軸上向き, テクスチャ座標系は左上原点のY軸下向き）
	/// pack_w, pack_h パックされた画像（＝テクスチャ）のサイズ。（CImgPackW::saveToFileNames で保存した画像）
	/// index  番目の画像の座標配列を得る (TRIANGLE_LIST)
	/// buffer 座標配列の書き込み先
	/// max_bytes 書き込み可能な頂点数 (必要な頂点数を調べるには getMeshVertexCount を使う
	void getVertexArray(int pack_w, int pack_h, int index, Vertex *vertices, int max_count) const;


private:
	std::vector<SImgPackItem> itemlist_;
	std::string extra_;
	int cellsize_;
	int cellspace_;
	float adj_;
};


} // namespace
