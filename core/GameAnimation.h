#pragma once
#include "system.h"
#include "GameAnimationCurve.h"
#include <mutex>

namespace mo {

class Engine;
class HierarchySystem;

class AnimationClip: public virtual KRef {
public:
	AnimationClip(const Path &name);
	virtual ~AnimationClip();
	const Path & getName() const;
	void clear();
	void clearCurves();
	int  getTimeLength() const;
	const SpriteAnimationCurve * getSpriteCurve() const;

	/// 指定したラベルがついているフレームを探し、一番最初に見つかったラベルのフレーム位置を返す。
	/// みつからなかった場合は -1 を返す
	int getLabelFrame(const Path &label) const;

	bool isLooping() const;
	void setLooping(bool value);
	void setSpriteCurve(const SpriteAnimationCurve *curve);
	void setRemoveEntityOnFinish(bool val);
	bool isRemoveEntityOnFinish() const;

private:
	const SpriteAnimationCurve *spritecurve_;
	Path name_;
	bool looping_;
	bool remove_entity_on_finish_;
};

class IAnimationCallback {
public:
	/// アニメ進行によって、ユーザー定義コマンドが登録されているフレームに達したときに呼ばれる
	virtual void on_animation_command(EID e, const char *cmd, const char *val) = 0;
};

class AnimationSystem: public System {
public:
	AnimationSystem();
	virtual ~AnimationSystem();
	void init(Engine *engine);
	void addCallback(IAnimationCallback *cb);
	void removeCallback(IAnimationCallback *cb);
	void attach(EID e, const Path &clip="");
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onFrameUpdate() override;
	virtual void onDetach(EID e) override;
	virtual void onInspectorGui() override;
	virtual bool onIsAttached(EID e) override { return _getNode(e) != NULL; }
	virtual void onEntityInspectorGui(EID e) override;
	virtual bool setParameter(EID e, const char *key, const char *val) override;

	/// 数値アニメを設定する
	/// e アタッチ対象のエンティティ
	/// curve 数値アニメーションカーブ
	/// cb アニメ再生中のコールバック
	void playCurve(EID e, AnimationCurve *curve, ICurveCallback *cb=NULL);

	/// 進行中の数値アニメをすべて解除する。
	/// このメソッドは内部で AnimationCurve::end() を呼ぶ。
	/// 途中解除した場合に、現在のアニメ値のままになるのか、
	/// それともアニメ終了値が設定された状態で終わるのかは、
	/// AnimationCurve::end() の実装に依存する。
	/// また、playCurve() で ICurveCallback が指定されていたなら
	/// ICurveCallback::on_curve_detach を呼ぶ
	void clearCurves(EID e);

	/// 進行中の数値アニメがあるかどうか
	bool hasCurve(EID e) const;

	/// エンティティにアタッチされている全てのアニメを duation フレームの間だけ停止する。
	/// @param duration 停止フレーム数。負の値を指定すると無期限に停止する
	void setSleep(EID e, int duration);

	/// 一時停止中かどうか
	bool isSleep(EID e) const;

	/// アニメクリップを再生中かどうか
	bool isPlaying(EID e) const;

	/// アタッチされているアニメクリップを返す
	const Path & getClip(EID e) const;

	/// スプライトアニメをアタッチする
	/// @param name/clip アタッチするアニメクリップ
	/// @param keep 既に同じアニメが設定されている場合の挙動を決める。
	///             true なら既存のアニメがそのまま進行する。false ならリスタートする
	void setClip(EID e, const Path &name, bool keep=false);
	void setClip(EID e, const AnimationClip *clip, bool keep=false);
	
	/// アニメクリップの再生位置を変更する
	void seekToBegin(EID e);
	void seekToEnd(EID e);
	void seekToFrame(EID e, int frame);
	void seekToLabel(EID e, const Path &label);
	
	/// アニメクリップのページ番号を指定してシークする
	///
	/// アニメーションカーブとして SpriteAnimationCurve を含んでいるなら、その情報を元にして
	/// 指定されたページまでシークする。SpriteAnimationCurve を含んでいない場合は何もしない。
	/// 複数の SpriteAnimationCurve が存在する場合は最初に見つかったものを利用する
	void seekToPage(EID e, int page);

	/// アニメ速度の倍率を設定する。
	/// speed; アニメ速度倍率。標準値は 1.0
	/// current_clip_only: 現在再生中のクリップにだけ適用する。
	///                    true にすると、異なるクリップが設定された時に自動的に速度設定が 1.0 に戻る。
	///                    false を指定した場合、再び setSpeed を呼ぶまで速度倍率が変更されたままになる
	void setSpeed(EID e, float speed, bool current_clip_only=false);
	float getSpeed(EID e) const;

	/// アニメクリップを再生中の場合、そのフレーム番号を返す。
	/// 失敗した場合は -1 を返す
	int getCurrentFrame(EID e) const;

	/// アニメクリップを再生中の場合、現在のページの先頭フレームからの相対フレーム番号を返す。
	/// 失敗した場合は -1 を返す
	int getCurrentFrameInPage(EID e) const;

	/// アニメクリップを再生中の場合、そのページ番号を返す。
	/// 失敗した場合は -1 を返す
	int getCurrentPage(EID e) const;

	void clearCurrentUserParameters(EID e);
	void setCurrentParameter(EID e, const Path &key, const Path &value);
	const KNamedValues * getCurrentUserParameters(EID e);

	/// アニメクリップの <Parameters> で指定された値を得る
	bool getCurrentParameterBool(EID e, const Path &name);
	int  getCurrentParameterInt(EID e, const Path &name, int def=0);
	const Path & getCurrentParameter(EID e, const Path &name);
	bool queryCurrentParameterInt(EID e, const Path &name, int *value) const;

	void guiClips() const;
	bool guiClip(const AnimationClip *clip) const;

	void clearClips();
	void unloadClip(const Path &name);
	bool isClipLoaded(const Path &name);
	void loadClip(const Path &name, AnimationClip *clip);
	AnimationClip * findClip(const Path &name) const;

private:
	struct _Node {
		EID e;
		
		AnimationElement clip_ani; ///< クリップアニメの再生情報
		Path clip_curr;            ///< クリップアニメの名前
		float clip_speed;          ///< クリップアニメの再生速度 (1.0 で等倍）
		bool clip_playing;         ///< クリップアニメを再生中かどうか
		bool clip_speed_restore;   ///< 現在のクリップアニメが外されたらアニメ速度を戻す
		KNamedValues parameters;   ///< クリップアニメで定義されているパラメータ文字列

		std::vector<AnimationElement> numeric_elms; ///< 再生中の数値アニメリスト

		/// スリープ解除時刻。
		/// -1だと無期限スリープになる。
		/// 時刻の単位はゲーム時刻で、現在時刻が awake_time の時刻を過ぎている場合はスリープ解除状態であることを意味する
		/// @see Engine::getStatus(Engine::ST_FRAME_COUNT_GAME)
		int awake_time;
	};

	bool guiAnimationState(_Node *node) const;

	// 数値アニメを1フレーム進める
	void elm_tick(_Node *node);

	// 数値アニメ値を反映させる
	void elm_update_values(_Node *node);
	
	// 不要になった数値アニメを削除する
	void elm_remove_finished(_Node *node);

	// エンティティを削除するべきか？
	bool node_should_kill_entity(const _Node *node) const;

	void node_seek_sprite_frame(_Node *node, float frame);
	void node_send_animation_commands(const _Node *node) const;
	void node_update_commands(_Node *node);

	_Node * _getNode(EID e) {
		auto it = nodes_.find(e);
		return (it != nodes_.end()) ? &it->second : NULL;
	}
	const _Node * _getNode(EID e) const {
		auto it = nodes_.find(e);
		return (it != nodes_.end()) ? &it->second : NULL;
	}
	mutable std::recursive_mutex mutex_;
	std::vector<IAnimationCallback *> cblist_;
	std::unordered_map<EID, _Node> nodes_;
	std::unordered_map<Path, AnimationClip *> items_;
	bool broadcast_animation_parameter_;
	Engine *engine_;
	HierarchySystem *hs_;
};


}
