#pragma once
#include "GameId.h"
#include "KPath.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <typeinfo> // typeid

namespace mo {


class System {
public:
	System() {
	}
	virtual ~System() {}

	/// 一番最初の更新時に一度だけ呼ばれる
	virtual void onStart() {}

	/// ゲームエンジンの終了時に呼ばれる
	virtual void onEnd() {}

	virtual bool setParameter(EID e, const char *key, const char *val) { return false; }
	virtual bool getParameter(EID e, const char *key, char *val) { return false; }
	
	/// フレームの最初で呼ばれる
	///（デバッグ機能などでゲーム進行が強制停止している場合は呼ばれない）
	virtual void onFrameStart() {}

	/// フレームの更新時に呼ばれる
	///（デバッグ機能などでゲーム進行が強制停止している場合は呼ばれない）
	virtual void onFrameUpdate() {}

	/// フレームの更新時に、すべてのシステムの onFrameUpdate が実行された後で呼ばれる
	///（デバッグ機能などでゲーム進行が強制停止している場合は呼ばれない）
	virtual void onFrameLateUpdate() {}

	/// フレームの最後に呼ばれる
	/// （更新と描画を完了した後、一番最後に呼ばれる）
	virtual void onFrameEnd() {}

	/// 毎フレームの更新時に呼ばれる
	/// onFrameUpdate とは異なり、デバッグ機能などでゲームを強制停止している間にも呼ばれ続ける
	virtual void onAppFrameUpdate() {}

	/// 毎フレームの描画直前に呼ばれる
	/// onAppFrameUpdate の呼び出し回数と onAppRenderUpdate の回数は必ずしも一致しない。
	/// デバイスロストやウィンドウ最小化、フレームスキップなどで描画不可能、描画不要と判定されている間は呼ばれない
	virtual void onAppRenderUpdate() {}

	/// システム用のインスペクターを更新する時に呼ばれる
	virtual void onInspectorGui() {}

	/// エンティティ用のインスペクターを更新する時に呼ばれる
	virtual void onEntityInspectorGui(EID e) {}

	/// エンティティ e に対して、type で指定された機能をアタッチする。
	/// type が NULL だった場合は条件なしでアタッチする。
	/// type に文字列が指定されている場合、それに従った機能をアタッチする。
	/// 未知の文字列が指定された場合は何もせずに false を返す。
	/// e が NULL だった場合は何もせずに（エラーを出さずに）false を返す。
	virtual bool onAttach(EID e, const char *type) { return false; }

	/// エンティティをデタッチしたときに呼ばれる
	virtual void onDetach(EID e) {}

	/// エンティティがアタッチされているか問い合わせあったときに呼ばれる
	virtual bool onIsAttached(EID e) { return false; }

	bool isAttached(EID e) {
		return onIsAttached(e);
	}
};


} // namespace
