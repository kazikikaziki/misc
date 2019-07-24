#pragma once
#include <string>
#include "KPath.h"
namespace mo {

class Sprite;

class IBuildCallback {
public:
	struct SPRITEPARAMS {
		Sprite *sprite;
		Path sprite_path;
		Path original_path;
		Path image_path;

		/// 元画像ファイル image_path が複数の画像を含んでいる場合、その何番目の画像を使ったか
		int image_index;
	};

	/// スプライトを保存する直前に呼ばれる。
	/// 必要に応じて p->sprite フィールドを書き換えることができる
	virtual void onBuildSprite(SPRITEPARAMS *p) {}


	/// XMLを構文解析する直前に呼ばれる。
	/// XMLテキストに対してプリプロセッサを実行するために使う。ユーザーは必要に応じて xml_text を自由に改変し、true を返すこと。
	/// false を返すとプリプロセッサが失敗したとみなし、ビルドがエラー終了する
	virtual bool onBuildPreprocessXml(std::string *xml_u8, const Path &debug_name) { return true; }


	virtual void onPack(const Path &name, bool *deny) {}

};

} // namespace
