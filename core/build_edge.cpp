#include "build_edge.h"
#include "KFile.h"
#include "KEdgeFile.h"
#include "KLog.h"
#include "KStd.h"
#include "KToken.h"
#include "mo_cstr.h"

namespace mo {

/// Edge ファイルのページラベル、レイヤーラベルの色
static const Color32 EC_WHITE = Color32(255, 255, 255, 255);
static const Color32 EC_GRAY  = Color32(192, 192, 192, 255);
static const Color32 EC_GREEN = Color32(196, 255, 196, 255);
static const Color32 EC_BLUE  = Color32(191, 223, 255, 255);
static const Color32 EC_RED   = Color32(255, 168, 168, 255);
static const Color32 EC_IGNORE = EC_RED;


/// 無視をあらわす色かどうかを判定
bool GameEdgeBuilder::isIgnorableLabelColor(const Color32 &color) {
	return color == EC_IGNORE;
}

/// 無視可能なレイヤーならば true を返す（レイヤーのラベルの色によって判別）
bool GameEdgeBuilder::isIgnorableLayer(const KEdgeLayer *layer) {
	Log_assert(layer);
	return isIgnorableLabelColor(Color32(
		layer->label_color_rgbx_[0],
		layer->label_color_rgbx_[1],
		layer->label_color_rgbx_[2],
		255
	));
}

/// 無視可能なページならば true を返す（ページのラベルの色によって判別）
bool GameEdgeBuilder::isIgnorablePage(const KEdgePage *page) {
	Log_assert(page);
	return isIgnorableLabelColor(Color32(
		page->label_color_rgbx_[0],
		page->label_color_rgbx_[1],
		page->label_color_rgbx_[2],
		255
	));
}

/// 無視可能なレイヤーを全て削除する
void GameEdgeBuilder::removeIgnorableLayers(KEdgePage *page) {
	Log_assert(page);
	for (int i=page->getLayerCount()-1; i>=0; i--) {
		const KEdgeLayer *layer = page->getLayer(i);
		if (isIgnorableLayer(layer)) {
			page->delLayer(i);
		}
	}
}

/// 無視可能なページを全て削除する
void GameEdgeBuilder::removeIgnorablePages(KEdgeDocument *edge) {
	Log_assert(edge);
	for (int i=edge->getPageCount()-1; i>=0; i--) {
		const KEdgePage *page = edge->getPage(i);
		if (isIgnorablePage(page)) {
			edge->delPage(i);
		}
	}
}

/// 無視可能なページ、レイヤーを全て削除する
void GameEdgeBuilder::removeIgnorableElements(KEdgeDocument *edge) {
	Log_assert(edge);
	removeIgnorablePages(edge);
	for (int i=0; i<edge->getPageCount(); i++) {
		KEdgePage *page = edge->getPage(i);
		removeIgnorableLayers(page);
	}
}

bool GameEdgeBuilder::parseLabel(const Path &label, EdgeLayerLabel *out) {
	K_assert(out);
	out->numcmds = 0;

	// コマンド解析

	// "#レイヤー名@コマンド" の書式に従う

	// レイヤー名だけが指定されている例
	// "#face"
	//   name = "face" 
	//   cmds = ""
	//
	// 何も認識しない例
	// "face"
	//   name = "" // 先頭が # でない場合は名前省略
	//   cmds = "" // @ が含まれていないのでコマンド無し
	//
	// コマンドだけを認識する例
	// "@blend=add"
	// "face@blend=add"
	//   name = "" // 先頭が # でない場合は名前省略
	//   cmds = "blend=add"
	//
	// レイヤー名とコマンドを認識する例
	// "#face@blend=add"
	//   name = "face" 
	//   cmds = "blend=add"
	//
	// レイヤー名と複数のコマンドを認識する例
	// "#face@blend=add@se=none"
	//   name = "face" 
	//   cmds = "blend=add@se=none"

	// 区切り文字 @ で分割
	KToken tok(label.u8(), "@");

	// 最初の部分が # で始まっているならレイヤー名が指定されている
	if (K_strstarts(tok.get(0), "#")) {
		out->name = Path(tok.get(0) + 1); // "#" の次の文字から
	}

	// 右部分
	out->numcmds = 0;
	if (tok.size() > 1) {
		if (tok.size()-1 < EdgeLayerLabel::MAXCMDS) {
			out->numcmds = tok.size()-1;
		} else {
			Log_errorf(u8"E_EDGE_COMMAND: 指定されたコマンド '%s' の個数が、設定可能な最大数 %d を超えています", 
				label.u8(), EdgeLayerLabel::MAXCMDS);
			out->numcmds = EdgeLayerLabel::MAXCMDS;
		}
		for (int i=1; i<out->numcmds; i++) {
			out->cmd[i] = tok.get(i);
		}
	}
	return true;
}
bool GameEdgeBuilder::loadFromFile(KEdgeDocument *edge, FileInput &file, const Path &debugname) {
	if (edge && edge->loadFromFile(file)) {
		// Edge ドキュメントに含まれる無視属性のページやレイヤーを削除する。
		// 無視属性のページは元から存在しないものとしてページ番号を再割り当てすることに注意。
		// 無視属性のレイヤーも同じ
		removeIgnorableElements(edge);
		return true;
	} else {
		Log_errorf("E_EDGE_FAIL: Filed to load: %s", debugname.u8());
		return false;
	}
}
bool GameEdgeBuilder::loadFromFileInMemory(KEdgeDocument *edge, const void *data, size_t size, const Path &name) {
	Log_assert(edge);
	bool ret = false;
	FileInput file;
	if (file.open(data, size)) {
		ret = loadFromFile(edge, file, name);
	} else {
		Log_errorf("E_MEM_READER_FAIL: %s", name.u8());
	}
	return ret;
}
bool GameEdgeBuilder::loadFromFileName(KEdgeDocument *edge, const Path &filename) {
	Log_assert(edge);
	bool ret = false;
	FileInput file;
	if (file.open(filename)) {
		ret = loadFromFile(edge, file, filename);
	} else {
		Log_errorf("E_EDGE_FAIL: STREAM ERROR: %s", filename.u8());
	}
	return ret;
}

} // namespace
