#include "KImagePack.h"
//
#include "KImage.h"
#include "KXmlFile.h"
#include "KNum.h"
#include "KFile.h"
#include "KStd.h"
#include "KToken.h"
#include "KVideoDef.h"
#include "mo_cstr.h"
#include <inttypes.h>

namespace mo {

/// a ÷ b の端数切り上げ整数を返す
static inline int _CeilDiv(int a, int b) {
	return (int)ceilf((float)a / b);
}

/// x <= 2^n を満たす最小の 2^n を返す
static inline int _CeilToPow2(int x) {
	int s = 1;
	while (s < x) {
		s <<= 1;
	}
	return s;
}



#pragma region CImgPackW
CImgPackW::CImgPackW(int cellsize) {
	init(cellsize);
}
void CImgPackW::init(int cellsize) {
	itemlist_.clear();
	cellsize_ = cellsize;
	cellspace_ = 0;
	total_cells_ = 0;
}
void CImgPackW::addImage(const KImage &img, const SImgExtraData *extra) {
	SImgPackItem item;
	item.img = img;
	item.xcells = 0;
	item.ycells = 0;
	item.pack_offset = total_cells_;
	if (extra) item.extra = *extra;
	KImageUtils::scanOpaqueCells(img, cellsize_, &item.cells, &item.xcells, &item.ycells);
	itemlist_.push_back(item);
	total_cells_ += item.cells.size();
}
int CImgPackW::getNumberOfCellsRequired() const {
	return total_cells_;
}
bool CImgPackW::getBestSize(int *w, int *h) const {
	K_assert(w && h);
	const int BIGNUM = 1000000;
	int max_width = 1024 * 4;
	int best_w = 0;
	int best_h = 0;
	int best_loss = BIGNUM;
	int cellarea = cellsize_ + cellspace_ * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
	int min_w_pow2 = _CeilToPow2(cellarea); // とりあえず１セルが収まるような最低の 2^n から調べていく
	for (int w_pow2=min_w_pow2; w_pow2<=max_width; w_pow2*=2) {
		int xcells = w_pow2 / cellarea; // 横セル数
		int req_ycells = _CeilDiv(total_cells_, xcells); // 必要な縦セル数（端数切り上げ）
		int req_height = req_ycells * cellarea; // 必要な縦ピクセル数
		int actual_height = _CeilToPow2(req_height); // 必要な縦ピクセル数を満たし、かつ 2^n になるような縦ピクセル数
		int actual_ycells = actual_height / cellarea; // 実際に縦に配置できるセル数
		int actual_total = xcells * actual_ycells; // 実際に配置できるセル総数
		if (actual_total < total_cells_) {
			continue; // 足りない
		}
		int loss = actual_total - total_cells_; // 無駄になったセル数
		if (loss < best_loss) {
			best_w = w_pow2;
			best_h = actual_height;
			best_loss = loss;

		} else if (loss == best_loss) {
			// ロス数が同じ場合、縦横の和が小さい方を選ぶ
			if (w_pow2 + actual_height < best_w + best_h) {
				best_w = w_pow2;
				best_h = actual_height;
			//	best_loss = loss;
			}
		}
	}
	if (best_w && best_h) {
		*w = best_w;
		*h = best_h;
		return true;
	}
	return false;
}
bool CImgPackW::isCapacityOK(int dest_width, int dest_height) const {
	int cellarea = cellsize_ + cellspace_ * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
	int xcells = dest_width  / cellarea; // 端数切捨て
	int ycells = dest_height / cellarea;
	return total_cells_ <= xcells * ycells;
}
KImage CImgPackW::getPackedImage() const {
	// 画像を格納するのに最適なテクスチャサイズを得る
	int w = 0;
	int h = 0;
	getBestSize(&w, &h);

	KImage dest(w, h);
	int cellarea = cellsize_ + cellspace_ * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
	int dest_xcells = w / cellarea; // 端数切捨て
	for (auto it=itemlist_.begin(); it!=itemlist_.end(); ++it) {
		const SImgPackItem &item = *it;
		for (size_t c=0; c<item.cells.size(); c++) {
			int orig_idx = item.cells[c];
			int orig_col = orig_idx % item.xcells;
			int orig_row = orig_idx / item.xcells;
			int orig_x = cellsize_ * orig_col;
			int orig_y = cellsize_ * orig_row;

			int pack_idx = item.pack_offset + c;
			int pack_col = pack_idx % dest_xcells;
			int pack_row = pack_idx / dest_xcells;
			int pack_x = cellarea * pack_col;
			int pack_y = cellarea * pack_row;

			dest.copyRect(pack_x+cellspace_, pack_y+cellspace_, item.img, orig_x, orig_y, cellsize_, cellsize_);
		}
	}
	return dest;
}
void CImgPackW::getMetaString(std::string *_meta) const {
	K_assert(_meta);
	std::string &meta = *_meta;
	char s[256];
	sprintf(s, "<pack cellsize='%d' cellspace='%d' numimages='%d'>\n", cellsize_, cellspace_, itemlist_.size());
	meta = s;

	meta += "<extra>" + extra_ + "</extra>\n";

	for (auto it=itemlist_.begin(); it!=itemlist_.end(); ++it) {
		const SImgPackItem &item = *it;
		sprintf(s, "<img w='%d' h='%d' offset='%d' numcells='%d' page='%d' layer='%d' blend='%d' data0='%d'>",
			item.img.getWidth(),
			item.img.getHeight(),
			item.pack_offset, 
			item.cells.size(),
			item.extra.page,
			item.extra.layer,
			item.extra.blend,
			item.extra.data0
		);
		meta += s;
		for (size_t c=0; c<item.cells.size(); c++) {
			int src_idx = item.cells[c];
			sprintf(s, "%d ", src_idx);
			meta += s;
		}
		meta += "</img>\n";
	}
	meta += "</pack>\n";
}
void CImgPackW::setExtraString(const char *data) {
	extra_ = data;
}
void CImgPackW::saveToFileNames(const Path &imagefile, const Path &metafile) const {
	K_assert(!imagefile.empty());
	K_assert(!metafile.empty());

	// パック画像を保存
	KImage teximg = getPackedImage();
	std::string png = teximg.saveToMemory(1/*FASTEST*/);
	FileOutput(imagefile).write(png);
	
	// パック情報を保存
	std::string meta;
	getMetaString(&meta);

	FileOutput file;
	if (file.open(metafile)) {
		file.write(meta);
	}
}
#pragma endregion // CImgPackW



#pragma region CImgPackR
CImgPackR::CImgPackR() {
	cellsize_ = 0;
	cellspace_ = 0;

	// ピクセルとの境界線をぴったりとってしまうと、計算誤差によって隣のピクセルを取得してしまう可能性がある。
	// 少しだけピクセル内側を指すようにする
	// セルの余白を設定しているなら、この値は 0 でも大丈夫。
	// セル余白をゼロにして調整値を設定するか、
	// セル余白を設定して調整値を 0 にするか、どちらかで対処する
	adj_ = 0.01f;

}
void CImgPackR::clear() {
	itemlist_.clear();
	cellsize_ = 0;
	cellspace_ = 0;
	extra_.clear();
}
void CImgPackR::loadFromMetaString(const char *xml_u8) {
	clear();

	K_assert(xml_u8);
	using namespace mo;

	tinyxml2::XMLDocument *xdoc = KXml::createXmlDocument(xml_u8, Path(__FUNCTION__));
	if (xdoc == NULL) return;

	tinyxml2::XMLElement *xpack = xdoc->FirstChildElement("pack");
	K_assert(xpack);

	cellsize_ = xpack->IntAttribute("cellsize");
	K_assert(cellsize_ > 0);

	cellspace_ = xpack->IntAttribute("cellspace");
	K_assert(cellspace_ >= 0);

	tinyxml2::XMLElement *xextra = xpack->FirstChildElement("extra");
	if (xextra) {
		const char *s = xextra->GetText();
		if (s) extra_ = s;
	}

	int cellarea = cellsize_ + cellspace_ * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
	int numimages = xpack->IntAttribute("numimages");
	for (tinyxml2::XMLElement *ximg=xpack->FirstChildElement("img"); ximg!=NULL; ximg=ximg->NextSiblingElement("img")) {
		int w = ximg->IntAttribute("w");
		int h = ximg->IntAttribute("h");

		SImgPackItem item;
		item.pack_offset = ximg->IntAttribute("offset");
		item.img.create(w, h);
		item.xcells  = w / cellarea;
		item.ycells  = h / cellarea;
		item.extra.page  = ximg->IntAttribute("page");
		item.extra.layer = ximg->IntAttribute("layer");
		item.extra.blend = ximg->IntAttribute("blend", -1); // Blendのデフォルト値は -1 (Blend_Invalid) であることに注意
		item.extra.data0 = ximg->IntAttribute("data0");
		int numcells = ximg->IntAttribute("numcells");
		const char *text = ximg->GetText(); // ascii 文字だけだとわかりきっているので、文字列コード考慮しない
		KToken tok(text);
		K_assert((int)tok.size() == numcells);
		item.cells.resize(numcells);
		for (int i=0; i<numcells; i++) {
			int idx = mo::c_strtoi(tok[i]);
			item.cells[i] = idx;
		}
		itemlist_.push_back(item);
	}
	K_assert((int)itemlist_.size() == numimages);

	KXml::deleteXml(xdoc);
}
void CImgPackR::loadFromMetaFileName(const wchar_t *metafilename) {
	K_assert(metafilename);
	using namespace mo;
	FileInput file;
	file.open(metafilename);
	std::string text = file.readBin();
	loadFromMetaString(text.c_str());
}
int CImgPackR::getImageCount() const {
	return (int)itemlist_.size();
}
void CImgPackR::getImageSize(int index, int *w, int *h) const {
	K_assert(w && h);
	const SImgPackItem &item = itemlist_[index];
	// メタデータをロードした時、画像サイズを img.width, img.hegiht に入れてあるのでその値を使う。
	// ちなみに img.rega は NULL のままである。
	// 詳細は ImgPack_ReadPackInfoFromMetaText を参照せよ
	if (w) *w = item.img.getWidth();
	if (h) *h = item.img.getHeight();
}
void CImgPackR::getImageExtra(int index, SImgExtraData *extra) const {
	K_assert(0 <= index && index < (int)itemlist_.size());
	const SImgPackItem &item = itemlist_[index];
	if (extra) *extra = item.extra;
}
void CImgPackR::getImage(const KImage &pack_img, int index, KImage &orig_img) const {
	K_assert(cellsize_ > 0);
	K_assert(!pack_img.empty());
	K_assert(pack_img.getFormat() == KImage::RGBA32);
	K_assert(orig_img.getFormat() == KImage::RGBA32);

	int cellarea = cellsize_ + cellspace_ * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
	orig_img.fill(Color32::ZERO);
	const SImgPackItem &item = itemlist_[index];
	int pack_xcells = pack_img.getWidth() / cellarea;
	int orig_xcells = item.img.getWidth() / cellsize_;
	for (size_t i=0; i<item.cells.size(); i++) {
		int pack_idx = item.pack_offset + i;
		int pack_col = pack_idx % pack_xcells;
		int pack_row = pack_idx / pack_xcells;
		int pack_x = cellarea * pack_col;
		int pack_y = cellarea * pack_row;
		int orig_idx = item.cells[i];
		int orig_col = orig_idx % orig_xcells;
		int orig_row = orig_idx / orig_xcells;
		int orig_x = cellsize_ * orig_col;
		int orig_y = cellsize_ * orig_row;
		orig_img.copyRect(orig_x, orig_y, pack_img, pack_x+cellspace_, pack_y+cellspace_, cellsize_, cellsize_);
	}
}
int CImgPackR::getVertexCount(int index) const {
	if (index < 0 || (int)itemlist_.size() <= index) {
		return 0;
	}
	const SImgPackItem &item = itemlist_[index];
	return item.cells.size() * 6; // 1セルにつき2個の三角形(=6頂点)が必要
}
void CImgPackR::getVertexArray(int pack_w, int pack_h, int index, Vertex *vertices, int max_count) const {
	K_assert(cellsize_ > 0);
	K_assert(pack_w > 0);
	K_assert(pack_h > 0);
	K_assert(vertices);
	K_assert(max_count >= 0);
	K_assert(0 <= index && index < (int)itemlist_.size());
	const SImgPackItem &item = itemlist_[index];
	const size_t max_cells = max_count / 6; // 書き込み可能なセルの数（1セルを書き込むために6頂点が必要）
	const size_t num_write_cells = Num_min(item.cells.size(), max_cells); // 実際に書き込むセルの数

	// 頂点座標を設定
	const int orig_xcells = _CeilDiv(item.img.getWidth(), cellsize_); // 元画像の横セル数（端数切り上げ）
	const int orig_height = item.img.getHeight();
	for (size_t i=0; i<num_write_cells; i++) {
		int orig_idx = item.cells[i];
		int orig_col = orig_idx % orig_xcells;
		int orig_row = orig_idx / orig_xcells;
		int orig_x = cellsize_ * orig_col;
		int orig_y = cellsize_ * orig_row;

		// セルの各頂点の座標（ビットマップ座標系 左上基準 Y軸下向き）
		int bmp_x0 = orig_x;
		int bmp_y0 = orig_y;
		int bmp_x1 = orig_x + cellsize_;
		int bmp_y1 = orig_y + cellsize_;

		// セルの各頂点の座標（ワールド座標系）
		int x0 = bmp_x0;
		int x1 = bmp_x1;
	#if 1
		// 左下基準、Y軸上向き
		int yTop = orig_height - bmp_y0;
		int yBtm = orig_height - bmp_y1;
	#else
		// 左上基準 Y軸下向き
		int yTop = bmp_y0;
		int yBtm = bmp_y1;
	#endif
		// 利便性のため、6頂点のうちの最初の4頂点は、左上を基準に時計回りに並ぶようにする
		// この処理は画像復元の処理に影響することに注意 (VideoBank::exportSpriteImage)
		//
		// 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になる
		// |      |
		// |      |
		// 3------2
		vertices[i*6+0].pos = Vec3(x0, yTop, 0); // 0:左上
		vertices[i*6+1].pos = Vec3(x1, yTop, 0); // 1:右上
		vertices[i*6+2].pos = Vec3(x1, yBtm, 0); // 2:右下
		vertices[i*6+3].pos = Vec3(x0, yBtm, 0); // 3:左下
		vertices[i*6+4].pos = Vec3(x0, yTop, 0); // 0:左上
		vertices[i*6+5].pos = Vec3(x1, yBtm, 0); // 2:右下
	}

	// テクスチャ座標を設定
	const int cellarea = cellsize_ + cellspace_ * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
	const int pack_xcells = pack_w / cellarea; // パック画像に入っている横セル数
	for (size_t i=0; i<num_write_cells; i++) {
		int pack_idx = item.pack_offset + i;
		int pack_col = pack_idx % pack_xcells;
		int pack_row = pack_idx / pack_xcells;
		int pack_x = cellarea * pack_col + cellspace_;
		int pack_y = cellarea * pack_row + cellspace_;

		// セルの各頂点の座標（ビットマップ座標系 Y軸下向き）
		int bmp_x0 = pack_x;
		int bmp_y0 = pack_y;
		int bmp_x1 = pack_x + cellsize_;
		int bmp_y1 = pack_y + cellsize_;

		// セルの各頂点のUV座標
		// adj_ ピクセルだけ内側を取るようにする（adj_ は 1未満の微小数）
		float u0 = (float)(bmp_x0 + adj_) / pack_w;
		float v0 = (float)(bmp_y0 + adj_) / pack_h;
		float u1 = (float)(bmp_x1 - adj_) / pack_w;
		float v1 = (float)(bmp_y1 - adj_) / pack_h;

		// 頂点の順番については getMeshPositionArray を参照
		vertices[i*6+0].tex = Vec2(u0, v0); // 0:左上
		vertices[i*6+1].tex = Vec2(u1, v0); // 1:右上
		vertices[i*6+2].tex = Vec2(u1, v1); // 2:右下
		vertices[i*6+3].tex = Vec2(u0, v1); // 3:左下
		vertices[i*6+4].tex = Vec2(u0, v0); // 0:左上
		vertices[i*6+5].tex = Vec2(u1, v1); // 2:右下
	}

	// 色は一律白に
	for (size_t i=0; i<num_write_cells*6; i++) {
		vertices[i].dif = Color32::WHITE;
	}
}
const char * CImgPackR::getExtraString() const {
	return extra_.c_str();
}
#pragma endregion // CImgPackR


} // namespace

