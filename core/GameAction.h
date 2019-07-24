#pragma once
#include "Engine.h"
#include "GameHierarchy.h"
#include "KLog.h"
#include "KRef.h"

namespace mo {

class CameraSystem;
class HierarchySystem;
class Engine;

enum ayActionFlag {
	ayActionFlag_None           = 0x00,
	ayActionFlag_MustBeGrounded = 0x01, // 設置時専用アクション（落下アニメによって中断される可能性がある）
	ayActionFlag_ZeroSpeed      = 0x02, // 速度をゼロに設定する
	ayActionFlag_Yield          = 0x04, // 割り込みされて、再開待ち状態
	ayActionFlag_Dead           = 0x08, // 削除待ち状態
	ayActionFlag_Brake          = 0x10, // ブレーキをかける
};
typedef int ayActionFlags;

class Action {
	EID entity_;
	Engine *engine_;
public:
	bool bound_;

	Action();
	virtual ~Action();
	virtual void onEnterAction() {}
	virtual void onExitAction() {}
	void _onEnterAction() {
		this->onEnterAction();
		bound_ = true;
	}
	void _onExitAction() {
		this->onExitAction();
		bound_ = false;
	}
	bool _onStepAction() {
		if (bound_) {
			return onStepAction();
		} else {
			Log_warning("Action has been detached");
			return false; // 何らかの原因でバインドが解除されている
		}
	}
	virtual bool onStepAction() { return stepChildAction(); }
	virtual void onInterrupt(Action *action, bool *can_resume) {} // 優先度の高いアクションによって、このアクションが中断されようとしてる
	virtual void onCommand(int cmd, int *args, bool *terminate) {}
	virtual void onInspector() {}

	// コマンドを送る。
	// 終端フラグが設定されなけれ、アクション階層を上に向かいながらコマンドを順次適用する。
	// ※親エンティティのアクションという意味ではなく、エンティティでのアクション階層内の処理であることに注意
	void command(int cmd, int *args);

	void init(Engine *engine, EID e, bool recursive=true);
	// アクション実行
	bool stepChildAction();
	bool childActionInProgress();
	void interruptChildAction(Action *action);
	void setNextSiblingAction(Action *action, bool abort_curr_action=false);
	void setNextSiblingActionInThisFrame(Action *action, bool abort_curr_action=false);
	void addChildAction(Action *action, bool init=true);
	void clearChildActions();
	bool currentActionHasFlag(ayActionFlag flag);
	Action * getCurrentTerminalAction(); // 現在実行中の末端アクション（子を持たないアクション）
	Action * getCurrentChildAction() { return child_actions_.empty() ? NULL : child_actions_.back(); }
	Action * getParentAction() { return parent_action_; }
	EID getEntity() { return entity_; }
	const EID getEntity() const { return entity_; }
	//
	//
	ayActionFlags flags_;
	std::vector<Action *> child_actions_;
	Path debug_name_;
	Action * next_sibling_action_;
	Action * parent_action_;
	bool next_sibling_action_immediate_;
	bool child_action_changed_; // 実行すべきアクションが変わった
	bool child_action_interrupted_;
	bool abort_curr_action_;
};


class ActionCo;

class IAct: public virtual KRef {
public:
	IAct();
	virtual void onEnterAction() {}
	virtual void onExitAction() {}
	virtual bool onStepAction() { return true; }
	virtual void onPause() {}
	virtual void onResume() {}
	virtual void onGizmo() {}
	virtual void onInspector() {}

	EID getEntity();
	ActionCo * getActionComponent();
	void _onEnterAction();
	void _onExitAction();
	bool _onStepAction();

	ActionCo *co_;
	IAct *next_;
	bool paused_;
};


class IActionSystemCallback {
public:
	virtual Action * onActionSystem_NewAction(const Path &action_name, const KNamedValues *params) = 0;
};

class ActionSystem: public System {
public:
	ActionSystem();
	virtual ~ActionSystem();
	void init(Engine *engine, IActionSystemCallback *cb);
	void attach(EID e, Action *act=NULL);
	ActionCo * getComponent(EID e);
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual bool setParameter(EID e, const char *key, const char *val) override;
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onFrameUpdate() override;
	virtual bool onIsAttached(EID e) override { return _getNode(e) != NULL; }
	virtual void onDetach(EID e) override;
	virtual void onInspectorGui() override;
	virtual void onEntityInspectorGui(EID e) override;

	/// エンティティ e に対してコマンドを送る（Action::onCommand）
	/// アクション階層を上にたどりながらコマンドを実行する。
	/// ※親エンティティのアクションという意味ではいことに注意！
	void command(EID e, int cmd, int *args);

	void setDefaultActionName(EID e, const Path &action_name, const KNamedValues *params=NULL);
	const Path & getDefaultActionName(EID e) const;
	void clearAction(EID e);

	/// 現在実行中のアクションを返す
	Action * getCurrentAction(EID e);

	template <class T> T getCurrentActionT(EID e) {
		return dynamic_cast<T>(getCurrentAction(e));
	}

	/// 新しいアクションを割り込み実行する（現在のアクションは中断される）
	void interrupt(EID e, Action *action);
	/// 次のアクションを予約する（現在のアクションが終わってから実行される）
	void postNext(EID e, Action *action);


	bool act_exists(EID e, IAct *act) const;
	bool act_waiting(EID e, IAct *act) const;
	IAct * act_get(EID e);
	void act_clear(EID e);

	// 新しいアクションを予約する。実行中のアクションがなければ直ちに開始する
	void act_post(EID e, IAct *act);

	// newact を先頭とする新しいアクションシーケンスを割り込ませる。
	// 実行中のアクションは中断される
	void act_interrupt(EID e, IAct *act);


	// 現在のアクションを終了させ、次のアクションに遷移する
	void act_next(EID e);

	// アクションを更新する
	void act_step(EID e);

	Action * createAction(const Path &act_name, const KNamedValues *params);
	void updateEntity(EID e);
	IAct * getCurrentAct(EID e);

private:
	struct _Node {
		_Node() {
			e = NULL;
			co = NULL;
			debug_time_spent_msec = 0;
		}
		EID e;
		ActionCo *co;
		float debug_time_spent_msec;
	};

	class _SortActionPred {
	public:
		int sort_;
		HierarchySystem *hierarchy_;
		_SortActionPred(int s, HierarchySystem *h) { 
			sort_ = s;
			hierarchy_ = h;
		}
		bool operator()(const _Node *n1, const _Node *n2) const;
	};

	_Node * _getNode(EID e);
	const _Node * _getNode(EID e) const;
	void updateEntity(_Node &node);
	std::unordered_map<EID, _Node> nodes_;
	std::vector<_Node *> tmp_nodes_;
	HierarchySystem *hs_;
	CameraSystem *camera_sys_;
	IActionSystemCallback *cb_;
	Engine *engine_;
	int debug_sort_;
};

} // namespace


