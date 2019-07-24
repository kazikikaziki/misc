#include "GameAction.h"
#include "Engine.h"
#include "GameAnimation.h"
#include "GameRender.h"
#include "inspector.h"
#include "GameHierarchy.h"
#include "GameCamera.h"
#include "KImgui.h"
#include "mo_cstr.h"
#include "KClock.h"
#include <algorithm>

namespace mo {





class ActionCo {
public:
	ActionCo();
	virtual ~ActionCo();
	void setDefaultActionName(const Path &action_name, const KNamedValues *params=NULL);
	const Path & getDefaultActionName() const;
	void clearAction();
	/// 現在実行中のアクションを返す
	Action * getCurrentAction();
	/// 新しいアクションを割り込み実行する（現在のアクションは中断される）
	void interrupt(Action *action);
	/// 次のアクションを予約する（現在のアクションが終わってから実行される）
	void postNext(Action *action);

	void onInspector();
	Action root_action_;
	KNamedValues default_action_params_;

	bool act_exists(IAct *act) const {
		IAct *tmp = act_;
		while (tmp) {
			if (tmp == act) return true;
			tmp = tmp->next_;
		}
		return false;
	}
	bool act_waiting(IAct *act) const {
		return act && (act_ != act) && act_exists(act);
	}
	IAct * act_get() {
		return act_;
	}
	void act_clear() {
		while (act_) {
			act_next();
		}
	}

	// 新しいアクションを予約する。実行中のアクションがなければ直ちに開始する
	void act_post(IAct *act) {
		if (act == NULL) return;
		if (act_) {
			IAct *tmp = act_;
			while (tmp->next_) tmp = tmp->next_;
			tmp->next_ = act;
			tmp->next_->grab();
		} else {
			act_ = act;
			act_->grab();
			if (act_->paused_) {
				act_->onResume();
			} else {
				act_->co_ = this;
				act_->_onEnterAction();
			}
		}
	}

	// newact を先頭とする新しいアクションシーケンスを割り込ませる。
	// 実行中のアクションは中断される
	void act_interrupt(IAct *act) {
		if (act == NULL) return;
		IAct *tmp = NULL;
		if (act_) {
			tmp = act_;
			act_->paused_ = true;
			act_->onPause();
			act_ = NULL;
		}
		act_post(act);
		act_post(tmp);
		Ref_drop(tmp);
	}

	// 現在のアクションを終了させ、次のアクションに遷移する
	void act_next() {
		if (act_ == NULL) return;
		IAct *tmp = act_->next_;
		act_->_onExitAction();
		act_->drop();
		act_ = NULL;
		act_post(tmp);
		Ref_drop(tmp);
	}

	// アクションを更新する
	void act_step() {
		if (act_ == NULL) return;
		if (act_->_onStepAction()) return;
		act_next();
	}
	Engine *engine_;
	EID entity_;
	IAct *act_;
	bool enabled_;

//private:
	Path default_action_name_;
};




#pragma region IAct
IAct::IAct() {
	co_ = NULL;
	next_ = NULL;
	paused_ = false;
}
void IAct::_onEnterAction() {
	onEnterAction();
}
void IAct::_onExitAction() {
	onExitAction();
}
bool IAct::_onStepAction() {
	return onStepAction();
}
EID IAct::getEntity() {
	return co_ ? co_->entity_ : NULL;
}
ActionCo * IAct::getActionComponent() {
	return co_;
}
#pragma endregion // IAct




#pragma region ActionCo
ActionCo::ActionCo() {
	entity_ = NULL;
	act_ = NULL;
	engine_ = NULL;
}
ActionCo::~ActionCo() {
	act_clear();

	/// @todo
	/// root_action.bound_ == true のままデストラクタに入ってしまっていて、 assert で引っかかる。
	/// root_action の onEnterAction / onExitAction 呼び出しの整合性はチェックしない。
	/// 時間があるときにきちんと修正すること。そもそもアクションのヒエラルキー自体、いらないかもしれない
	root_action_.bound_ = false; 
	
	root_action_.clearChildActions();
}
void ActionCo::clearAction() {
	root_action_.clearChildActions();
}
void ActionCo::onInspector() {
#ifndef NO_IMGUI
	ImGui::Text("Action:");
	ImGui::Indent();
	{
		Action *act = &root_action_;
		int i = 0;
		while (act) {
			ImGui::Separator();
			ImGui::Text("[%d] %s", i, typeid(*act).name());
			act->onInspector();
			act = act->child_actions_.empty() ? NULL : act->child_actions_.back();
			i++;
		}
	}
	{
		IAct *act = act_;
		while (act) {
			ImGui::Text("%s", typeid(*act).name());
			act->onInspector();
			act = act->next_;
		}
	}
	ImGui::Unindent();
#endif // !NO_IMGUI
}
void ActionCo::setDefaultActionName(const Path &action_name, const KNamedValues *params) {
	default_action_name_ = action_name;
	default_action_params_.assign(params);
}
const Path & ActionCo::getDefaultActionName() const {
	return default_action_name_;
}
Action * ActionCo::getCurrentAction() {
	// 実行中のアクションを得る
	Action *act = root_action_.getCurrentTerminalAction();
	// ルートアクションが返った場合、子アクションは実行されていない。
	// この場合、実質的に何のアクションも実行されていないので NULL を返す
	if (act == &root_action_) {
		return NULL;
	}
	return act;
}
/// 新しいアクションを割り込み実行する（現在のアクションは中断される）
void ActionCo::interrupt(Action *action) {
	root_action_.interruptChildAction(action);
}

/// 次のアクションを予約する（現在のアクションが終わってから実行される）
void ActionCo::postNext(Action *action) {
	root_action_.addChildAction(action);
}

#pragma endregion

#pragma region Action
Action::Action() {
	entity_ = NULL;
	child_action_changed_ = true;
	child_action_interrupted_ = false;
	next_sibling_action_ = NULL;
	next_sibling_action_immediate_ = false;
	abort_curr_action_ = false;
	flags_ = ayActionFlag_None;
	bound_ = false;
	parent_action_ = NULL;
	engine_ = NULL;
}
Action::~Action() {
	Log_assert(bound_ == false);
	clearChildActions();
	if (next_sibling_action_) {
		delete next_sibling_action_;
	}
}
void Action::init(Engine *engine, EID e, bool recursive) {
	Log_assert(e);
	if (debug_name_.empty()) {
		wchar_t s[256];
		mbstowcs(s, typeid(*this).name(), 256);
		debug_name_ = s;
	}
	entity_ = e;
	engine_ = engine;
	if (recursive) {
		for (auto it=child_actions_.begin(); it!=child_actions_.end(); ++it) {
			auto act = *it;
			act->init(engine, e);
		}
	}
}
// 実行中のアクションを返す。サブアクションが実行中なら、再帰的に実行中アクションをたどっていく。
// サブアクションが存在しないなら自分自身を返す
Action * Action::getCurrentTerminalAction() {
	Action *act = this;
	while (! act->child_actions_.empty()) {
		act = act->child_actions_.back();
	}
	return act;
}
// 次に実行すべきアクションを設定する
void Action::setNextSiblingAction(Action *action, bool abort_curr_action) {
	if (next_sibling_action_) {
		delete next_sibling_action_;
	}
	next_sibling_action_ = action;
	next_sibling_action_immediate_ = false;
	abort_curr_action_ = abort_curr_action;
	if (next_sibling_action_) {
		next_sibling_action_->parent_action_ = parent_action_;
	}
}

// 次に実行するべきアクションを設定する。設定されたアクションは現在のフレーム内で適用される。副作用に注意すること
void Action::setNextSiblingActionInThisFrame(Action *action, bool abort_curr_action) {
	if (next_sibling_action_) {
		delete next_sibling_action_;
	}
	next_sibling_action_ = action;
	next_sibling_action_immediate_ = true;
	abort_curr_action_ = abort_curr_action;
	if (next_sibling_action_) {
		next_sibling_action_->parent_action_ = parent_action_;
	}
}

// 現在実行中のサブアクションを中断し、最優先サブアクションを追加する
// 最優先サブアクション終了後に、中断されていたサブアクションが再開する
void Action::interruptChildAction(Action *action) {
	Log_assert(action);
	Log_assert(entity_);
	action->init(engine_, entity_);
	action->parent_action_ = this;
	child_actions_.push_back(action);
	child_action_changed_ = true;
	child_action_interrupted_ = true;
}
// サブアクションの実行待ちキューの末尾にサブアクションを追加する
void Action::addChildAction(Action *act, bool init) {
	Log_assert(act);
	if (init) {
		Log_assert(entity_);
		act->init(engine_, entity_);
	}
	act->parent_action_ = this;
	if (child_actions_.empty()) {
		child_action_changed_ = true;
	}
	child_actions_.insert(child_actions_.begin(), act);
}
bool Action::childActionInProgress() {
	return !child_actions_.empty();
}
bool Action::currentActionHasFlag(ayActionFlag flag) {
	Action *act = getCurrentTerminalAction();
	Log_assert(act);
	return (act->flags_ & flag) != 0;
}
void Action::clearChildActions() {
	for (size_t i=0; i<child_actions_.size(); i++) {
		child_actions_[i]->_onExitAction();
		delete child_actions_[i];
	}
	child_actions_.clear();
}
// コマンドを送る。
// 終端フラグが設定されなけれ、アクション階層を上に向かいながらコマンドを順次適用する。
// ※親エンティティのアクションという意味ではなく、エンティティでのアクション階層内の処理であることに注意
void Action::command(int cmd, int *args) {
	bool term = false;
	Action *act = this;
	while (act && !term) {
		act->onCommand(cmd, args, &term);
		act = act->parent_action_;
	}
}
bool Action::stepChildAction() {
	if (child_actions_.empty()) {
		return false;
	}
	// アクション取得
	Action *curr = child_actions_.back();
	Log_assert(curr);
	Log_assert(curr->parent_action_ == this);
	// 前回実行したアクションと異なっていれば開始メソッドを呼ぶ
	if (child_action_changed_) {
		// 新しいアクションを初期化する。
		// 初期化（開始）が拒否された場合は、新しいアクションを削除する
		if (curr->flags_ & ayActionFlag_Dead) {
			curr->_onExitAction();
			delete curr;
			child_actions_.pop_back();
			child_action_changed_ = true;
			child_action_interrupted_ = false;
			return true;
		}
		curr->_onEnterAction();
		// 割り込みアクションだった場合は、
		// 割り込まれたアクションに対して新しいアクションの開始を通知する
		if (child_action_interrupted_ && child_actions_.size() >= 2) {
			Action *act = child_actions_[child_actions_.size() - 2]; // 最後尾よりも一つ手前のアクションが、中断されたアクション
			bool can_resume = true;
			act->onInterrupt(curr, &can_resume);
			if (can_resume) {
				act->flags_ |= ayActionFlag_Yield; // 再開待ち
			} else {
				act->flags_ |= ayActionFlag_Dead; // 再開できない。このアクションは終わった
			}
		}
	}
	// アクション実行
	child_action_changed_ = false;
	child_action_interrupted_ = false;
	if (curr->_onStepAction()) {
		// 現在のアクションはまだ削除しない
		Action *next = curr->next_sibling_action_;
		curr->next_sibling_action_ = NULL;
		if (next) {
			// 次に実行すべきアクションが指定されているが、現在のアクションはまだ終了していない
			if (curr->abort_curr_action_) {
				// 強制終了あり
				// 現在のアクションを終了し、割り込みアクションを挿入する
				curr->_onExitAction();
				child_actions_.pop_back();
				delete curr;
				next->init(engine_, entity_);
				child_actions_.push_back(next);
				child_action_changed_ = true;
				child_action_interrupted_ = false; // 中断ではない

			} else {
				// 強制終了なし
				// 現在のアクションを中断し、割り込みアクションを挿入する
				next->init(engine_, entity_);
				child_actions_.push_back(next);
				child_action_changed_ = true;
				child_action_interrupted_ = true;
			}

		} else {
			// 割り込みアクションなし。
			// 現在のアクションを継続する
		}
	} else {
		// 現在のアクションは終了し、削除してよい
		Action *next = curr->next_sibling_action_;
		bool immed = curr->next_sibling_action_immediate_;
		curr->next_sibling_action_ = NULL;
		// 現在のアクションを終了する
		curr->_onExitAction();
		child_actions_.pop_back();
		delete curr;
		child_action_changed_ = true;
		child_action_interrupted_ = false;
		if (next) {
			// 次に実行すべきアクションが指定されている
			next->init(engine_, entity_);
			child_actions_.push_back(next);

			if (immed) {
				// すぐに反映させる。副作用に注意せよ
				next->_onEnterAction();
				child_action_changed_ = false; // 既に onEnterAction を呼んだ
			}
		} else {
			// 次に実行すべきアクションなし
		}
	}
	return true;
}
#pragma endregion










#pragma region ActionSystem


bool ActionSystem::_SortActionPred::operator()(const _Node *n1, const _Node *n2) const {
	Log_assert(n1);
	Log_assert(n1->co);
	Log_assert(n2);
	Log_assert(n2->co);
	Log_assert(hierarchy_);
	if (sort_ == 0) {
		Path s1 = hierarchy_->getName(n1->e);
		Path s2 = hierarchy_->getName(n2->e);
		return s1.compare(s2) < 0;
	}
	if (sort_ == 1) {
		Action *act1 = n1->co->getCurrentAction();
		Action *act2 = n2->co->getCurrentAction();
		Path s1 = act1 ? typeid(*act1).name() : Path::Empty;
		Path s2 = act2 ? typeid(*act2).name() : Path::Empty;
		return s1.compare(s2) < 0;
	}
	if (sort_ == 2) {
		float t1 = n1->debug_time_spent_msec;
		float t2 = n2->debug_time_spent_msec;
		return t1 < t2;
	}
	Path s1 = hierarchy_->getName(n1->e);
	Path s2 = hierarchy_->getName(n2->e);
	return s1.compare(s2) < 0;
}

ActionSystem::ActionSystem() {
	cb_ = NULL;
	hs_ = NULL;
	debug_sort_ = 0;
	engine_ = NULL;
}
ActionSystem::~ActionSystem() {
	onEnd();
}
void ActionSystem::init(Engine *engine, IActionSystemCallback *cb) {
	Log_trace;
	engine->addSystem(this);
	engine_ = engine;
	cb_ = cb;
	debug_sort_ = 0;
}
void ActionSystem::onEnd() {
	cb_ = NULL;
	hs_ = NULL;
	debug_sort_ = 0;
	engine_ = NULL;
}
void ActionSystem::onStart() {
	Log_assert(engine_ && "init() not called");
	hs_ = engine_->findSystem<HierarchySystem>();
	Log_assert(hs_ && "No HierarchySystem found");
}
ActionCo * ActionSystem::getComponent(EID e) {
	_Node *node = _getNode(e);
	return node ? node->co : NULL;
}
bool ActionSystem::onAttach(EID e, const char *type) {
	if (type==NULL || strcmp(type, "Action") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void ActionSystem::setDefaultActionName(EID e, const Path &action_name, const KNamedValues *params) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->setDefaultActionName(action_name, params);
	}
}
const Path & ActionSystem::getDefaultActionName(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->getDefaultActionName();
	}
	return Path::Empty;
}
void ActionSystem::clearAction(EID e) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->clearAction();
	}
}
Action * ActionSystem::getCurrentAction(EID e) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->getCurrentAction();
	}
	return NULL;
}
void ActionSystem::interrupt(EID e, Action *action) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		node->co->interrupt(action);
	}
}
void ActionSystem::postNext(EID e, Action *action) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		node->co->postNext(action);
	}
}






bool ActionSystem::act_exists(EID e, IAct *act) const {
	const _Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->act_exists(act);
	}
	return false;
}
bool ActionSystem::act_waiting(EID e, IAct *act) const {
	const _Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->act_waiting(act);
	}
	return false;
}
IAct * ActionSystem::act_get(EID e) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->act_get();
	}
	return NULL;
}
void ActionSystem::act_clear(EID e) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		node->co->act_clear();
	}
}
void ActionSystem::act_post(EID e, IAct *act) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		node->co->act_post(act);
	}
}
void ActionSystem::act_interrupt(EID e, IAct *act) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		node->co->act_interrupt(act);
	}
}
void ActionSystem::act_next(EID e) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		node->co->act_next();
	}
}
void ActionSystem::act_step(EID e) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		node->co->act_step();
	}
}
// アクションに対してコマンドを送る
void ActionSystem::command(EID e, int cmd, int *args) {
	_Node *node = _getNode(e);
	if (node) {
		Action *action = node->co->getCurrentAction();
		if (action) {
			action->command(cmd, args);
		}
	}
}
bool ActionSystem::setParameter(EID e, const char *key, const char *val) {
	_Node *node = _getNode(e);
	if (node == NULL) {
		return false;
	}
	if (strcmp(key, "action") == 0) {
		node->co->default_action_name_ = val;
		return true;
	}
	return false;
}
void ActionSystem::attach(EID e, Action *act) {
	if (e == NULL) return;
	if (_getNode(e) == NULL) {
		_Node node;
		node.e = e;
		node.co = new ActionCo();
		node.co->entity_ = e;
		node.co->engine_ = engine_;
		node.co->root_action_.init(engine_, e);
		node.co->root_action_._onEnterAction();
		nodes_[e] = node;
	}
	if (act) {
		_Node *n = _getNode(e);
		Log_assert(n);
		Log_assert(n->co);
		// 既に attach 済みのアクションに対して重複 attach を実行したときのことを考慮する
		// attach メソッドに渡された act は即座に実行されることを想定しているハズなので、
		// 「古いアクションが終わってから act が実行される」だと事故る可能性がある（というか事故った）
		// 既存のアクションを削除し、act が新しいアクションとして即座に始まるようにしておく
		n->co->clearAction();

		n->co->postNext(act);//  <-- 必ず nodes_ に登録してから呼び出す
		updateEntity(e);
	}
}
void ActionSystem::onDetach(EID e) {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		_Node &node = it->second;
		Log_assert(node.co);
		node.co->root_action_.clearChildActions();
		node.co->root_action_._onExitAction();
		delete node.co;
		nodes_.erase(it);
	}
}
void ActionSystem::onFrameUpdate() {
	KClock clock;
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		clock.reset();
		updateEntity(it->second);
		float t = clock.getTimeMsecF();
		it->second.debug_time_spent_msec = t;
	}
}
void ActionSystem::onInspectorGui() {
#ifndef NO_IMGUI
	Log_assert(engine_);
	Log_assert(hs_);
	if (ImGui::TreeNode("Actions")) {
		if (ImGui::Button("ent-name")) debug_sort_ = 0;
		ImGui::SameLine(); if (ImGui::Button("act-name")) debug_sort_ = 1;
		ImGui::SameLine(); if (ImGui::Button("time-once")) debug_sort_ = 2;

		tmp_nodes_.clear();
		for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
			tmp_nodes_.push_back(&it->second);
		}
		std::sort(tmp_nodes_.begin(), tmp_nodes_.end(), _SortActionPred(debug_sort_, hs_));
		for (auto it=tmp_nodes_.begin(); it!=tmp_nodes_.end(); ++it) {
			const _Node *node = *it;
			Log_assert(node->co);
			Action *act = node->co->getCurrentAction();
			Path ename = hs_->getName(node->e);
			Path aname = act ? typeid(*act).name() : "(none)";
			ImGui::Text("%s: %s (%.2fms)", ename.u8(), aname.u8(), node->debug_time_spent_msec);
			if (ImGui::IsItemHovered()) {
				Path path = hs_->getNameInTree(node->e);
				ImGui::SetTooltip("EntityPath: %s", path.u8());
			}
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
void ActionSystem::onEntityInspectorGui(EID e) {
#ifndef NO_IMGUI
	_Node *node = _getNode(e);
	if (node == NULL) return;
	Log_assert(node->co);
	ImGui::Checkbox("Enabled", &node->co->enabled_);
	node->co->onInspector();
#endif // !NO_IMGUI
}
Action * ActionSystem::createAction(const Path &act_name, const KNamedValues *params) {
	if (act_name.empty()) {
		return NULL; // アクション未指定
	}
	if (cb_ == NULL) {
		return NULL; // アクション作成用のコールバックが指定されていない
	}
	Action *act = cb_->onActionSystem_NewAction(act_name, params);
	if (act == NULL) {
		Log_errorf("E_ACTION: No action named '%s'", act_name.u8());
		return NULL; // アクションをロードできない
	}
	return act;
}
ActionSystem::_Node * ActionSystem::_getNode(EID e) {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		return &it->second;
	}
	return NULL;
}
const ActionSystem::_Node * ActionSystem::_getNode(EID e) const {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		return &it->second;
	}
	return NULL;
}
void ActionSystem::updateEntity(EID e) {
	_Node *node = _getNode(e);
	if (node) {
		updateEntity(*node);
	}
}
IAct * ActionSystem::getCurrentAct(EID e) {
	_Node *node = _getNode(e);
	if (node && node->co) {
		return node->co->act_;
	}
	return NULL;
}
void ActionSystem::updateEntity(_Node &node) {
	Log_assert(engine_);
	Log_assert(hs_);
	Log_assert(node.co);
	if (hs_->isEnabledInTree(node.e) && !hs_->isPausedInTree(node.e)) {
		ActionCo *act_co = node.co;
		if (act_co->getCurrentAction() == NULL) {
			// アクションが実行されていない。
			// デフォルトアクションがあるなら、それを設定する
			Action *def_action = createAction(act_co->getDefaultActionName(), &act_co->default_action_params_);
			if (def_action) {
				act_co->postNext(def_action);
			}
		}
		act_co->root_action_._onStepAction(); // アクションコンポーネントのルートアクションは常に実行し続ける
		act_co->act_step();
	}
}
#pragma endregion // ActionSystem

} // namespace
