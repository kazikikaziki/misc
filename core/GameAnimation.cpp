#include "GameAnimation.h"
//
#include "GameAnimationCurve.h"
#include "GameRender.h"
#include "GameHierarchy.h"
#include "Engine.h"
#include "asset_path.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNum.h"
#include "KStd.h"
#include "KXmlFile.h"
#include "mo_cstr.h"
#include <algorithm> // std::sort

#define OLD_FRAME_STEP 0 // アニメクリップのフレームを１進めるときの挙動を昔のままにする（ループ末尾で１フレームのギャップが発生する）

#define CHECK_EID_EXISTS  1 // EID がヒエラルキー上に存在するかをチェックする


namespace mo {

#pragma region AnimationClipImpl
AnimationClip::AnimationClip(const Path &name) {
	Log_assert(!name.empty());
	spritecurve_ = NULL;
	looping_ = false;
	remove_entity_on_finish_ = false;
}
AnimationClip::~AnimationClip() {
	clear();
}
void AnimationClip::clear() {
	looping_ = false;
	remove_entity_on_finish_ = false;
	clearCurves();
}
const Path & AnimationClip::getName() const {
	return name_;
}
void AnimationClip::setSpriteCurve(const SpriteAnimationCurve *curve) {
	Ref_copy(spritecurve_, curve);
}
const SpriteAnimationCurve * AnimationClip::getSpriteCurve() const {
	return spritecurve_;
}
int AnimationClip::getLabelFrame(const Path &label) const {
	int frame = -1;
	if (spritecurve_) {
		frame = spritecurve_->getFrameByLabel(label);
	}
	return frame;
}
int AnimationClip::getTimeLength() const {
	int len = 0;
	if (spritecurve_) {
		int x = spritecurve_->getDuration();
		len = Num_max(x, len);
	}
	return len;
}
void AnimationClip::clearCurves() {
	Ref_drop(spritecurve_);
}
bool AnimationClip::isLooping() const {
	return looping_;
}
void AnimationClip::setLooping(bool value) {
	looping_ = value;
}
void AnimationClip::setRemoveEntityOnFinish(bool val) {
	remove_entity_on_finish_ = val;
}
bool AnimationClip::isRemoveEntityOnFinish() const {
	return remove_entity_on_finish_;
}

#pragma endregion // AnimationClip



#pragma region AnimationSystem
AnimationSystem::AnimationSystem() {
	engine_ = NULL;
	hs_ = NULL;
}
AnimationSystem::~AnimationSystem() {
	onEnd();
}
void AnimationSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine_ = engine;
}
void AnimationSystem::addCallback(IAnimationCallback *cb) {
	cblist_.push_back(cb);
}
void AnimationSystem::removeCallback(IAnimationCallback *cb) {
	auto it = std::find(cblist_.begin(), cblist_.end(), cb);
	if (it != cblist_.end()) {
		cblist_.erase(it);
	}
}
void AnimationSystem::onStart() {
	hs_ = engine_->findSystem<HierarchySystem>();
	Log_assert(hs_);
}
void AnimationSystem::onEnd() {
	clearClips();
	engine_ = NULL;
	hs_ = NULL;
}
bool AnimationSystem::onAttach(EID e, const char *type) {
	if (type==NULL || strcmp(type, "Animation") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void AnimationSystem::attach(EID e, const Path &clip) {
	if (e == NULL) return; // NULL は登録不可
	if (_getNode(e)) return; // すでに登録済み
	if (CHECK_EID_EXISTS) { // ヒエラルキーに登録済みのEIDのみアタッチ可能
		Log_assert(hs_);
		if (!hs_->exists(e)) {
			return;
		}
	}
	_Node node;
	node.e = e;
	node.clip_ani = AnimationElement();
	node.clip_speed = 1.0f;
	node.clip_playing = false;
	node.clip_speed_restore = false;
	node.awake_time = 0;
	nodes_[e] = node;
	if (!clip.empty()) {
		setClip(e, clip);
	}
}
void AnimationSystem::onDetach(EID e) {
	clearCurves(e);

	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		_Node &node = it->second;
		Log_assert(node.numeric_elms.empty()); // <--- clearCurves でクリアされているはず
		nodes_.erase(it);
	}
}
bool AnimationSystem::isClipLoaded(const Path &name) {
	bool ret;
	mutex_.lock();
	{
		ret = (items_.find(name) != items_.end());
	}
	mutex_.unlock();
	return ret;
}
void AnimationSystem::unloadClip(const Path &name) {
	mutex_.lock();
	{
		auto it = items_.find(name);
		if (it != items_.end()) {
			it->second->drop();
			items_.erase(it);
		}
	}
	mutex_.unlock();
}
void AnimationSystem::loadClip(const Path &name, AnimationClip *clip) {
	mutex_.lock();
	{
		if (items_.find(name) != items_.end()) {
			Log_warningf("W_CLIP_OVERWRITE: Resource named '%s' already exists. The resource data will be overwriten by new one", name.u8());
			unloadClip(name);
		}
		Log_assert(items_.find(name) == items_.end());
		items_[name] = clip;
		Log_verbosef("ADD_CLIP: %s", name.u8());
	}
	mutex_.unlock();

}
AnimationClip * AnimationSystem::findClip(const Path &name) const {
	AnimationClip *ret;
	mutex_.lock();
	{
		auto it = items_.find(name);
		ret = (it!=items_.end()) ? it->second : NULL;
	}
	mutex_.unlock();
	return ret;
}
void AnimationSystem::onFrameUpdate() {
	int game_time = engine_->getStatus(Engine::ST_FRAME_COUNT_GAME);

	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		_Node *node = &it->second;

		// シーンツリー単位で Disabled になっている場合は更新不要
		if (!hs_->isEnabledInTree(node->e)) {
			continue;
		}

		// シーンツリー単位で一時停止状態になっている場合は更新不要
		if (hs_->isPausedInTree(node->e)) {
			continue;
		}

		// スリープ解除時刻が負の値になっている場合は、無期限スリープする
		if (node->awake_time < 0) {
			continue;
		}

		// 現在時刻がスリープ解除時刻に達していなければスリープ継続する
		if (game_time < node->awake_time) {
			continue;
		}

		// スプライトアニメの状態を更新
		if (node->clip_ani.curve) {
			node_seek_sprite_frame(node, node->clip_ani.frame + node->clip_speed);
		}

		// スプライトアニメ終了時にエンティティを削除する
		if (node_should_kill_entity(node)) {
			hs_->removeTree(node->e);
		}

		// 数値アニメを更新
		elm_tick(node);

		// 数値アニメに従って、位置や色などの状態を更新する
		elm_update_values(node);

		// 数値アニメ終了時にエンティティを削除する
		elm_remove_finished(node);
	}
}
void AnimationSystem::node_send_animation_commands(const _Node *node) const {
	Log_assert(node);
	if (node->clip_ani.curve && cblist_.size() > 0) {
		const KNamedValues &params = node->clip_ani.curve->getUserSegmentParameters(node->clip_ani.frame);
		for (auto pit=params.cbegin(); pit!=params.cend(); ++pit) {
			const char *key_u8 = pit->first.u8();
			const char *val_u8 = pit->second.u8();
			for (auto cit=cblist_.begin(); cit!=cblist_.end(); ++cit) {
				IAnimationCallback *cb = *cit;
				cb->on_animation_command(node->e, key_u8, val_u8);
			}
		}
	}
}
void AnimationSystem::node_update_commands(_Node *node) {
	Log_assert(node);
	node->parameters.clear();
	if (node->clip_ani.curve) {
		const KNamedValues &params = node->clip_ani.curve->getUserSegmentParameters(node->clip_ani.frame);
		for (auto it=params.cbegin(); it!=params.cend(); ++it) {
			node->parameters.setString(it->first, it->second);
		}
	}
}
// エンティティを削除する必要があるか？
bool AnimationSystem::node_should_kill_entity(const _Node *node) const {
	Log_assert(node);

	// クリップ未指定なら削除しない
	const AnimationClip *clip = findClip(node->clip_curr);
	if (clip == NULL) {
		return false;
	}

	// アニメ再生中なら削除しない
	if (node->clip_playing) {
		return false;
	}

	// アニメ完了かつ削除フラグがあるならエンティティを削除
	if (node->clip_ani.hasFlag(AnimationElement::F_REMOVE_ENTITY_AT_END)) {
		return true;
	}
	return false;
}
void AnimationSystem::elm_tick(_Node *node) {
	Log_assert(node);
	for (auto it=node->numeric_elms.begin(); it!=node->numeric_elms.end(); ++it) {
		AnimationElement &elm = *it;
		float frame_delta = 1.0f;
		Log_assert(elm.curve);
		elm.frame += frame_delta;

		// ループが設定されていて、再生位置が終端を超えていれば先頭に戻る
		if (elm.hasFlag(AnimationElement::F_LOOP)) {
			if (elm.curve->getDuration() <= elm.frame) {
				elm.frame = 0;
				if (elm.cb) elm.cb->on_curve_repeat();
			}
		}
	}
}
void AnimationSystem::elm_update_values(_Node *node) {
	Log_assert(node);
	for (auto it=node->numeric_elms.begin(); it!=node->numeric_elms.end(); ++it) {
		AnimationElement &elm = *it;
		Log_assert(elm.curve);
		elm.curve->animate(node->e, elm.frame);
	}
}
void AnimationSystem::elm_remove_finished(_Node *node) {
	Log_assert(node);
	for (auto it=node->numeric_elms.begin(); it!=node->numeric_elms.end(); /*++it*/) {
		AnimationElement &elm = *it;
		if (elm.curve->getDuration() <= elm.frame) {
			Log_assert(elm.curve);
			elm.curve->end(node->e);
			if (elm.cb) {
				elm.cb->on_curve_done();
				elm.cb->on_curve_detach();
			}
			elm.curve->drop();
			it = node->numeric_elms.erase(it);
			if (elm.hasFlag(AnimationElement::F_REMOVE_ENTITY_AT_END)) {
				hs_->removeTree(node->e); // エンティティを削除
			}
		} else {
			it++;
		}
	}
}
void AnimationSystem::setSleep(EID e, int duration) {
	_Node *node = _getNode(e);
	if (node == NULL) return;

	if (duration >= 0) {
		node->awake_time = engine_->getStatus(Engine::ST_FRAME_COUNT_GAME) + duration;
	} else {
		node->awake_time = -1; // INFINITY
	}
}
bool AnimationSystem::isSleep(EID e) const {
	int game_time = engine_->getStatus(Engine::ST_FRAME_COUNT_GAME);
	const _Node *node = _getNode(e);
	if (node) {
		if (node->awake_time < 0) {
			return true; // 無限スリープ
		}
		if (game_time < node->awake_time) {
			return true; // まだ解除時間に達していない
		}
	}
	return false;
}
void AnimationSystem::playCurve(EID e, AnimationCurve *curve, ICurveCallback *cb) {
	_Node *node = _getNode(e);
	if (node == NULL) return;
	if (curve == NULL) return;

	AnimationElement elm;
	elm.curve = curve;
	elm.curve->grab();
	elm.cb = cb;
	elm.frame = 0;
	node->numeric_elms.push_back(elm);
	elm.curve->start(node->e);
	elm.curve->animate(node->e, elm.frame);
}
void AnimationSystem::clearCurves(EID e) {
	_Node *node = _getNode(e);
	if (node == NULL) return;
	for (auto it=node->numeric_elms.begin(); it!=node->numeric_elms.end(); ++it) {
		AnimationElement &elm = *it;
		if (elm.curve) {
			elm.curve->end(e);
			if (elm.cb) elm.cb->on_curve_detach();
			elm.curve->drop();
		}
	}
	node->numeric_elms.clear();
}
bool AnimationSystem::hasCurve(EID e) const {
	const _Node *node = _getNode(e);
	if (node) {
		for (auto it=node->numeric_elms.begin(); it!=node->numeric_elms.end(); ++it) {
			const AnimationElement &elm = *it;
			if (elm.curve) {
				return true;
			}
		}
	}
	return false;
}



const Path & AnimationSystem::getClip(EID e) const {
	const _Node *node = _getNode(e);
	if (node) {
		return node->clip_curr;
	} else {
		return Path::Empty;
	}
}
void AnimationSystem::setClip(EID e, const Path &name, bool keep) {
	_Node *node = _getNode(e);
	if (node == NULL) return;

	// 同じアニメが既に再生中の場合は何もしない
	if (keep) {
		if (node->clip_curr.compare(name) == 0) {
			return;
		}
	}

	// 再生中アニメの終了イベントを呼ぶ
	if (node->clip_ani.curve) {
		node->clip_ani.curve->end(node->e);
	}

	// 指定されたクリップを得る。
	// クリップ名が指定されているのに見つからなかった場合はエラーログ出す
	const AnimationClip *clip = findClip(name);
	if (!name.empty() && clip == NULL) {
		Log_errorf("E_ANI: Clip not found: '%s'", name.u8());
	}

	if (clip == NULL) {
		// アニメ外す

		// アニメ速度が再生中のクリップにだけ適用されているなら、速度を戻す
		if (node->clip_speed_restore) {
			node->clip_speed = 1.0f;
		}

		node->clip_curr = "";
		node->clip_playing = false;
		node->clip_speed_restore = false;
		node->clip_ani = AnimationElement();
		node_update_commands(node);

	} else {
		// アニメ適用
		const SpriteAnimationCurve *curve = clip->getSpriteCurve();
		Log_assert(curve);

		// アニメ速度が再生中のクリップにだけ適用されているなら、速度を戻す
		if (node->clip_speed_restore) {
			node->clip_speed = 1.0f;
		}

		node->clip_curr = name;
		node->clip_playing = true;
		node->clip_speed_restore = false;
		node->clip_ani = AnimationElement();
		node->clip_ani.curve = curve;

		// アニメで設定されている再生フラグをコピー
		if (clip->isLooping()) {
			node->clip_ani.setFlag(AnimationElement::F_LOOP, true);
		}
		if (clip->isRemoveEntityOnFinish()) {
			node->clip_ani.setFlag(AnimationElement::F_REMOVE_ENTITY_AT_END, true);
		}

		// コマンド更新
		node_update_commands(node);

		// 開始イベントを呼ぶ
		node->clip_ani.curve->start(node->e);

		// アニメ更新関数を通しておく
		node->clip_ani.curve->animate(node->e, node->clip_ani.frame);

		// コマンド送信
		node_send_animation_commands(node);
	}
}
void AnimationSystem::setClip(EID e, const AnimationClip *clip, bool keep) {
	if (clip) {
		setClip(e, clip->getName(), keep);
	} else {
		setClip(e, Path::Empty, keep);
	}
}
bool AnimationSystem::isPlaying(EID e) const {
	const _Node *node = _getNode(e);
	if (node) {
		return node->clip_playing;
	} else {
		return false;
	}
}
void AnimationSystem::node_seek_sprite_frame(_Node *node, float frame) {
	Log_assert(node);
	if (node->clip_ani.curve) {
		int old_page = node->clip_ani.curve->getSegmentIndexByFrame(node->clip_ani.frame);

		node->clip_ani.frame = frame;

		// アニメ全体の長さ
		int total_length = node->clip_ani.curve->getDuration();

		if (node->clip_ani.frame < total_length) {
			node->clip_playing = true;
			node->clip_ani.curve->animate(node->e, node->clip_ani.frame);
			int new_page = node->clip_ani.curve->getSegmentIndexByFrame(node->clip_ani.frame);
			if (old_page != new_page) {
				node_send_animation_commands(node);
			}
			node_update_commands(node);

		} else {
			// 再生位置がアニメ終端に達した。
			// ループまたは終了処理する
			if (node->clip_ani.hasFlag(AnimationElement::F_LOOP)) {
				// 先頭に戻る
				node->clip_ani.frame = 0;
				node->clip_playing = true;
				node->clip_ani.curve->animate(node->e, node->clip_ani.frame);
				node_send_animation_commands(node);
				node_update_commands(node);
			} else {
				node->clip_playing = false;
			}
		}
	}
	if (node_should_kill_entity(node)) {
		hs_->removeTree(node->e); // エンティティを削除
	}
}

void AnimationSystem::seekToBegin(EID e) {
	seekToFrame(e, 0);
}
void AnimationSystem::seekToFrame(EID e, int frame) {
	_Node *node = _getNode(e);
	if (node == NULL) return;
	node_seek_sprite_frame(node, frame);
}
void AnimationSystem::seekToEnd(EID e) {
	_Node *node = _getNode(e);
	if (node == NULL) return;

	// setClip()した直後にseekToEnd() を呼んだ場合、
	// セットしたクリップが clip_path_ ではなく next_clip_path_ にはいっていて、getClip() が NULL を返す場合がある
	const AnimationClip *clip = findClip(node->clip_curr);
	if (clip == NULL) return;

	node_seek_sprite_frame(node, clip->getTimeLength());
}
void AnimationSystem::seekToLabel(EID e, const Path &label) {
	_Node *node = _getNode(e);
	if (node == NULL) return;

	const AnimationClip *clip = findClip(node->clip_curr);
	if (clip == NULL) return;

	const SpriteAnimationCurve *curve = clip->getSpriteCurve();
	if (curve == NULL) return;

	int frame = curve->getFrameByLabel(label);
	if (frame < 0) return;

	node_seek_sprite_frame(node, frame);
}
void AnimationSystem::seekToPage(EID e, int page) {
	_Node *node = _getNode(e);
	if (node == NULL) return;
	
	const AnimationClip *clip = findClip(node->clip_curr);
	if (clip == NULL) return;

	const SpriteAnimationCurve *curve = clip->getSpriteCurve();
	if (curve == NULL) return;

	int frame = curve->getFrameByPage(page);
	if (frame < 0) return;
	
	node_seek_sprite_frame(node, frame);
}


float AnimationSystem::getSpeed(EID e) const {
	const _Node *node = _getNode(e);
	if (node) {
		return node->clip_speed;
	} else {
		return 1.0f;
	}
}
void AnimationSystem::setSpeed(EID e, float speed, bool current_clip_only) {
	_Node *node = _getNode(e);
	if (node) {
		node->clip_speed = speed;
		node->clip_speed_restore = current_clip_only;
	}
}


int AnimationSystem::getCurrentFrame(EID e) const {
	const _Node *node = _getNode(e);
	if (node == NULL) return -1;
	return node->clip_ani.frame;
}
int AnimationSystem::getCurrentFrameInPage(EID e) const {
	const _Node *node = _getNode(e);
	if (node == NULL) return -1;
	
	const AnimationClip *clip = findClip(node->clip_curr);
	if (clip == NULL) return -1;

	const SpriteAnimationCurve *curve = clip->getSpriteCurve();
	if (curve == NULL) return -1;

	int frame = node->clip_ani.frame;
	if (frame < 0) return -1;

	int page = curve->getPageByFrame(frame, true);
	int frame_offset = curve->getFrameByPage(page);
	int frame_in_page = frame - frame_offset;
	return frame_in_page;
}

int AnimationSystem::getCurrentPage(EID e) const {
	const _Node *node = _getNode(e);
	if (node == NULL) return -1;
	
	const AnimationClip *clip = findClip(node->clip_curr);
	if (clip == NULL) return -1;

	const SpriteAnimationCurve *curve = clip->getSpriteCurve();
	if (curve == NULL) return -1;

	int frame = node->clip_ani.frame;
	if (frame < 0) return -1;

	int page = curve->getPageByFrame(frame, true);
	return page;
}

void AnimationSystem::clearCurrentUserParameters(EID e) {
	_Node *node = _getNode(e);
	if (node) node->parameters.clear();
}
void AnimationSystem::setCurrentParameter(EID e, const Path &key, const Path &value) {
	_Node *node = _getNode(e);
	if (node) node->parameters.setString(key, value);
}
const KNamedValues * AnimationSystem::getCurrentUserParameters(EID e) {
	_Node *node = _getNode(e);
	return node ? &node->parameters : NULL;
}
bool AnimationSystem::getCurrentParameterBool(EID e, const Path &name) {
	const _Node *node = _getNode(e);
	if (node == NULL) return false;
	return node->parameters.getBool(name);
}
int AnimationSystem::getCurrentParameterInt(EID e, const Path &name, int def) {
	const _Node *node = _getNode(e);
	if (node == NULL) return 0;
	return node->parameters.getInteger(name, def);
}
const Path & AnimationSystem::getCurrentParameter(EID e, const Path &name) {
	const _Node *node = _getNode(e);
	if (node == NULL) return Path::Empty;
	return node->parameters.getString(name);
}
bool AnimationSystem::queryCurrentParameterInt(EID e, const Path &name, int *value) const {
	const _Node *node = _getNode(e);
	if (node == NULL) return false;
	return node->parameters.queryInteger(name, value);
}
void AnimationSystem::guiClips() const {
#ifndef NO_IMGUI
	if (ImGui::TreeNode(ImGui_ID(0), u8"スプライトアニメ(%d)", items_.size())) {
		// 表示フィルタ
		static char s_filter[256] = {0};
		ImGui::InputText(u8"Filter", s_filter, sizeof(s_filter));

		// 名前リスト
		PathList names;
		names.reserve(items_.size());
		if (s_filter[0]) {
			// フィルターあり
			// 部分一致したものだけリストに入れる
			for (auto it=items_.cbegin(); it!=items_.cend(); ++it) {
				const char *s = it->first.u8();
				if (K_strpos(s, s_filter, 0) >= 0) {
					names.push_back(it->first);
				}
			}
		} else {
			// フィルターなし
			for (auto it=items_.cbegin(); it!=items_.cend(); ++it) {
				names.push_back(it->first);
			}
		}

		// ソート
		std::sort(names.begin(), names.end());

		// リスト表示
		for (auto it=names.cbegin(); it!=names.cend(); ++it) {
			const Path &name = *it;
			if (ImGui::TreeNode(name.u8())) {
				auto cit = items_.find(name);
				Log_assert(cit != items_.end());
				const AnimationClip *clip = cit->second;
				guiClip(clip);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
void AnimationSystem::clearClips() {
	mutex_.lock();
	{
		for (auto it=items_.begin(); it!=items_.end(); ++it) {
			Ref_drop(it->second);
		}
		items_.clear();
	}
	mutex_.unlock();
}
bool AnimationSystem::guiClip(const AnimationClip *clip) const {
#ifndef NO_IMGUI
	if (clip == NULL) {
		ImGui::Text("(NO CLIP)");
		return false;
	}
	ImGui::Text("Length: %d", clip->getTimeLength());
	ImGui::Text("Loop: %s", clip->isLooping() ? "Yes" : "No");
	ImGui::Text("Remove entity at end: %s", clip->isRemoveEntityOnFinish() ? "Yes" : "No");
	if (clip->getSpriteCurve()) {
		const AnimationCurve *curve = clip->getSpriteCurve();
		Log_assert(curve);
		ImGui::Text("%s", typeid(*curve).name());
		if (ImGui::TreeNode(ImGui_ID(-2), "segments")) {
			curve->guiCurve();
			ImGui::TreePop();
		}
	}
#endif // NO_IMGUI
	return false;
}

bool AnimationSystem::guiAnimationState(_Node *node) const {
#ifndef NO_IMGUI
	Log_assert(node);
	AnimationClip *clip = findClip(node->clip_curr);
	if (clip == NULL) {
		ImGui::Text("(NO CLIP)");
		return false;
	}
	bool changed = false;
	ImGui::Text("Frame %d/%d", (int)node->clip_ani.frame, clip->getTimeLength());
	{
		const auto FLAG = AnimationElement::F_LOOP;
		bool a = node->clip_ani.hasFlag(FLAG);
		if (ImGui::Checkbox("Loop", &a)) {
			node->clip_ani.setFlag(FLAG, a);
			changed = true;
		}
	}
	{
		const auto FLAG = AnimationElement::F_REMOVE_ENTITY_AT_END;
		bool a = node->clip_ani.hasFlag(FLAG);
		if (ImGui::Checkbox("Remove entity at end", &a)) {
			node->clip_ani.setFlag(FLAG, a);
			changed = true;
		}
	}
	float spd = node->clip_speed;
	if (ImGui::DragFloat("Speed Scale", &spd)) {
		node->clip_speed = spd;
	}
	if (node->clip_ani.curve) {
		if (ImGui::TreeNode("%s", typeid(*node->clip_ani.curve).name())) {
			node->clip_ani.curve->guiState(node->clip_ani.frame);
			ImGui::TreePop();
		}
	}
	return changed;
#else // !NO_IMGUI
	return false;
#endif // NO_IMGUI
}

bool AnimationSystem::setParameter(EID e, const char *key, const char *val) {
	_Node *node = _getNode(e);
	if (node == NULL) {
		return false;
	}
	if (strcmp(key, "clip") == 0) {
		if (val && val[0]) {
			setClip(e, val);
		} else {
			setClip(e, "");
		}
		return true;
	}
	return false;
}

void AnimationSystem::onEntityInspectorGui(EID e) {
#ifndef NO_IMGUI
	_Node *node = _getNode(e);
	if (node == NULL) return;
	{
		if (ImGui::TreeNode("Clip: %s", node->clip_curr.u8())) {
			guiAnimationState(node);
			ImGui::TreePop();
		}
	}
	if (ImGui::TreeNode("Parameters")) {
		for (auto it=node->parameters.cbegin(); it!=node->parameters.cend(); ++it) {
			ImGui::Text("%s ==> %s", it->first.u8(), it->second.u8());
		}
		ImGui::TreePop();
	}
	if (node->awake_time >= 0) {
		int slp = Num_max(node->awake_time - engine_->getStatus(Engine::ST_FRAME_COUNT_GAME), 0);
		ImGui::Text("Sleep time left: %d", slp);
	} else {
		ImGui::Text("Sleep time left: (INFINITY)");
	}
#endif // !NO_IMGUI
}

void AnimationSystem::onInspectorGui() {
#ifndef NO_IMGUI
	if (ImGui::TreeNode("Bank")) {
		guiClips();
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
#pragma endregion // AnimationSystem



}

