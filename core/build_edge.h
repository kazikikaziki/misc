#pragma once
#include "KPath.h"

namespace mo {

class KEdgeDocument;
class KEdgeLayer;
class KEdgePage;
class FileInput;
class Color32;

struct EdgeLayerLabel {
	static const int MAXCMDS = 4;
	Path name;
	Path cmd[MAXCMDS];
	int numcmds;

	EdgeLayerLabel() {
		numcmds = 0;
	}
};

/// ゲームにEDGEドキュメントを使う時の操作関数群。
/// EDGEドキュメントをゲーム用のアニメファイルとして使う場合に必要な操作を行う
/// ※EDGEドキュメントはドット絵ツール Edge2 の標準ファイル形式。詳細は以下を参照
/// http://takabosoft.com/edge2
///
class GameEdgeBuilder {
public:
	/// 無視をあらわす色かどうかを判定
	static bool isIgnorableLabelColor(const Color32 &color);

	/// 無視可能なレイヤーならば true を返す（レイヤーのラベルの色によって判別）
	static bool isIgnorableLayer(const KEdgeLayer *layer);

	/// 無視可能なページならば true を返す（ページのラベルの色によって判別）
	static bool isIgnorablePage(const KEdgePage *page);

	/// 無視可能なレイヤーを全て削除する
	static void removeIgnorableLayers(KEdgePage *page);

	/// 無視可能なページを全て削除する
	static void removeIgnorablePages(KEdgeDocument *edge);

	/// 無視可能なページ、レイヤーを全て削除する
	static void removeIgnorableElements(KEdgeDocument *edge);

	/// ページまたはレイヤーのラベル文字列に式が含まれていれば、それを読み取る
	static bool parseLabel(const Path &label, EdgeLayerLabel *out);

	/// Edgeドキュメントをロードする
	/// ※使用後は drop で参照カウンタを捨てる必要がある
	static bool loadFromFileName(KEdgeDocument *edge, const Path &filename);
	static bool loadFromFile(KEdgeDocument *edge, FileInput &file, const Path &debugname);
	static bool loadFromFileInMemory(KEdgeDocument *edge, const void *data, size_t size, const Path &debugname);
};


}
