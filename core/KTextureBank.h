#pragma once
#include "KVideoDef.h"
#include "KPath.h"
#include "GameId.h"
#include <unordered_map>
#include <mutex>

namespace mo {

class KImage;
class FileInput;
class VideoBank;

class IVideoBankCallback {
public:
	/// SpriteRenderer の setModifier で値が指定されている場合に呼ばれる。
	/// modifier の値に応じて、テクスチャーを動的に生成する。
	/// @param newtex      動的に生成されたテクスチャ。サイズは basetex と同じで、中身は basetex のコピーで初期化されている
	/// @param basetexname 元になるテクスチャの名前
	/// @param basetex     元になるテクスチャ。一番簡単な処理方法は、basetex の内容を newtex にコピーすることである
	/// @param modifier    適用するエフェクトの種類。何番のときに何のエフェクトをかけるかはユーザー定義。（ただし0はエフェクトなしとして予約済み）
	/// @param data        関連付けられているエンティティ
	virtual void on_videobank_modifier(TEXID newtex, const Path &basetexname, TEXID basetex, int modifier, EID data) = 0;
};

class TextureBank {
public:
	IVideoBankCallback *cb_;

	enum Flag {
		// 同名のテクスチャが存在した場合に上書きする
		F_OVERWRITE_ANYWAY = 1,

		// 使用目的の異なる同名テクスチャが存在する場合に、テクスチャを再生成して上書きする
		// （例：レンダーテクスチャを取得しようとしているが、同名の通常テクスチャがあった）
		F_OVERWRITE_USAGE_NOT_MATCHED = 2,

		// サイズの異なる同名テクスチャが存在する場合に、テクスチャを再生成して上書きする
		//（例：256x256のレンダーテクスチャを取得しようとしたが、同名でサイズの異なるレンダーテクスチャが既に存在している）
		F_OVERWRITE_SIZE_NOT_MATCHED = 4,
	};
	typedef int Flags;

	TextureBank();
	virtual ~TextureBank();
	TEXID addTextureFromFile(const Path &name, FileInput &file);
	TEXID addTextureFromSize(const Path &name, int w, int h);
	TEXID addTextureFromImage(const Path &name, const KImage &image);
	TEXID addTextureFromTexture(const Path &name, TEXID source);
	TEXID addRenderTexture(const Path &name, int w, int h, Flags flags=0);
	TEXID findTexture(const Path &name, int modifier=0, bool should_exist=true, EID data=NULL);

	// 純粋に、指定された名前に完全に一致するテクスチャを持っているかどうかだけ調べる。勝手にロードしたり、別の名前での検索を試みたりしない
	TEXID findTextureRaw(const Path &name, bool should_exist) const;
	Path getName(const TEXID tex) const;
	/// 指定されたテクスチャリソースを所持しているかどうか
	/// オンデマンドロードや動的構築などの副作用なしに、本当に持っているかどうかだけを調べる
	int getCount();
	PathList getNames();
	PathList getRenderTexNames();

	void clearAssets();
	void delAsset(const Path &name);
	bool hasAsset(const Path &name) const;
	void clearModifierCaches();

	// 2^nサイズのテクスチャを強制する
	bool pow2_texture_only_;
private:

	/// 指定されたテクスチャがパレット適用済みテクスチャであれば、元のテクスチャ名とパレット名を得る。
	/// パレット適用済みテクスチャでない場合は、何もせずに false を返す
	bool parseName(const Path &name, Path *based_tex, Path *applied_pal) const;

	std::unordered_map<Path, TEXID > items_;
	mutable std::recursive_mutex mutex_;
};

} // namespace
