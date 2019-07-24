#pragma once
#include "KFont.h"
#include "KRef.h"
#include "KPath.h"
#include "KVec3.h"
#include "system.h"
#include <mutex>

namespace mo {

class Engine;
class IFontLoader;
class ActionSystem;
class RenderSystem;

static const char *NANO_FONT_NAME = "nano";

enum EHorzAlign {
	HORZALIGN_LEFT   = -1,
	HORZALIGN_CENTER =  0,
	HORZALIGN_RIGHT  =  1,
};

enum EVertAlign {
	VERTALIGN_TOP    = -1,
	VERTALIGN_CENTER =  0,
	VERTALIGN_BOTTOM =  1,
};

class TextSystem: public System {
public:
	TextSystem();
	virtual ~TextSystem();
	void init(Engine *engine);

	virtual void onStart() override;
	virtual void onEnd() override;
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onAppRenderUpdate() override;
	void attach(EID e);
	void setText(EID e, const char *text_u8);
	void setText(EID e, const wchar_t *text);
	void setFont(EID e, KFont &font);
	void setFont(EID e, const Path &alias); // alias = TextSystem に登録したフォント名
	void setFontSize(EID e, float value);
	void setFontStyle(EID e, KFont::Style value, const Color32 &primary_color=Color32::WHITE, const Color32 &secondary_color=Color32::BLACK);
	void setFontPitch(EID e, float value);
	void setHorzAlign(EID e, EHorzAlign align);
	void setVertAlign(EID e, EVertAlign align);
	void setColor(EID e, const Color &color);
	void setAlpha(EID e, float alpha);
	Color getColor(EID e) const;
	float getAlpha(EID e) const;
	void updateMesh(EID e);
	void getAabb(EID e, Vec3 *minpoint, Vec3 *maxpoint) const;
	Vec3 getAabbSize(EID e) const;

	/// フォントを作成して追加する
	/// @param alias このフォントにつける名前
	///
	/// @param filename フォントファイル名またはフォント名
	///    空文字列を指定した場合は、システムにインストールされているフォントから name に一致するものを探す。
	///
	///    例) NANO_FONT_NAME 定数を指定すると、組み込みの極小ビットマップフォントを作成する。
	///      "arialbd.ttf" を指定すると Arial フォントになる。
	///      "msgothic.ttc" を指定すると MSゴシックファミリーになる
	///      "ＭＳ ゴシック" を指定するとMSゴシックになる（ＭＳは全角だがスペースは半角なので注意！）
	///
	/// @param ttc_index
	///    TTC フォントが複数のフォントを含んでいる場合、そのインデックスを指定する。
	///    例えば MS ゴシックフォント msgothic.ttc をロードする場合は以下の値を指定する
	///    0 -- MSゴシック
	///    1 -- MS UI ゴシック
	///    2 -- MS P ゴシック
	///
	/// @return
	///    新しく追加したフォントを返す。
	///    既に同名フォントが追加済みである場合は、そのフォントを返す。
	///    どちらの場合でも、得られるフォントは借りた参照であり、呼び出し側で参照カウンタを減らす必要はない
	///    フォントの追加に失敗した場合は NULL を返す
	///
	/// @code
	///  addFont(NANO_FONT_NAME, NANO_FONT_NAME, 0);
	///  addFont("msgothic",    "msgothic.ttc", 0);
	///  addFont("msgothic_ui", "msgothic.ttc", 1);
	///  addFont("msgothic_p",  "msgothic.ttc", 2);
	/// @endcode
	KFont addFont(const Path &alias, const Path &filename=Path::Empty, int ttc_index=0);

	/// 指定されたフォントを追加する。内部で参照カウンタを増やす
	/// 古い同名フォントがある場合は、古いフォントを削除してから追加する。
	/// 追加に失敗した場合は false を返す
	/// @param alias このフォントにつける名前
	/// @param font フォントオブジェクト
	bool addFont(const Path &alias, KFont &font);

	void deleteFont(const Path &alias);

	/// 指定された名前のフォントを返す。
	/// フォントがない場合、fallback 引数にしたがって NULL または代替フォントを返す
	/// @param alias フォント名(addFontでフォントを追加するときに指定した、ユーザー定義のフォント名）
	/// @param fallback  フォントが見つからなかったとき、代替フォントを返すなら true. NULL を返すなら false
	KFont getFont(const Path &alias, bool fallback=true) const;

	/// なんでも良いから適当なフォントを取得したい時に使う。
	/// setDefaultFont によってデフォルトフォントが設定されていれば、それを返す。
	/// 未設定の場合は、最初に追加されたフォントを返す。
	/// フォントが全くロードされていない場合は、Arial フォントを返す
	KFont getDefaultFont() const;

	/// ロード済みの内部フォント一覧を得る
	const PathList & getNames() const;

	/// フォントが見つからなかった場合に使う代替フォントを指定する。
	/// このフォントは既に addFont で追加済みでなければならない。
	/// @param alias フォント名(addFontでフォントを追加するときに指定した、ユーザー定義のフォント名）
	void setDefaultFont(const Path &alias);

private:
	mutable std::unordered_map<Path, KFont> fonts_;

	/// デフォルトフォント。
	/// フォントが全くロードされていない時でも利用可能なフォント。
	/// 何もしなくても、必要な時に自動的にロードされる
	mutable KFont system_default_;

	/// フォールバックフォント。
	/// 指定されたフォントが見つからない場合に使う代替フォント名。
	/// （フォント名は getFont で指定されたもの）
	Path fallback_;

	PathList names_;
	Engine *engine_;
	ActionSystem *as_;
	RenderSystem *rs_;
};

} // namespace
