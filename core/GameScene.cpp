#include "GameScene.h"
//
#include "GameAction.h"
#include "inspector.h"
#include "Engine.h"
#include "KImgui.h"
#include "KStd.h"

namespace mo {

static const int SCENE_ID_LIMIT = 100;



const KNamedValues * Scene::getParams() const {
	return &params_;
}
void Scene::setParams(const KNamedValues *new_params) {
	KNamedValues old_params = params_;
	params_.assign(new_params);
	onSetParams(&params_, new_params, &old_params);
}



SceneSystem::SceneSystem() {
	curr_.clear();
	next_.clear();
	//
	cb_ = NULL;
	clock_ = 0;
	reload_ = false;
	engine_ = NULL;
}
SceneSystem::~SceneSystem() {
	onEnd();
}
void SceneSystem::setCallback(ISceneCallback *cb) {
	cb_ = cb;
}
void SceneSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine_ = engine;
}
void SceneSystem::onStart() {
#ifndef NO_IMGUI
	Log_assert(engine_);
	Inspector *inspector = engine_->getInspector();
	if (inspector) {
		SystemEditorParams *editorP = inspector->getSystemEditorParams(this);
		Log_assert(editorP);
	//	editorP->sort_primary = mo_EDITORPRIORITY_BASIC;
	}
#endif // !NO_IMGUI
}
void SceneSystem::onEnd() {
	if (curr_.scene) {
		callSceneExit();
	}
	for (auto it=scenelist_.begin(); it!=scenelist_.end(); ++it) {
		if (*it) delete *it;
	}
	scenelist_.clear();
	curr_.clear();
	next_.clear();
	engine_ = NULL;
}
void SceneSystem::onFrameStart() {
	if (curr_.scene) {
		curr_.scene->onSceneUpdate();
	}
}
void SceneSystem::onInspectorGui() {
#ifndef NO_IMGUI
	const char *nullname = "(NULL)";
	const char *s = curr_.scene ? typeid(*curr_.scene).name() : nullname;
	ImGui::Text("Clock: %d", clock_);
	ImGui::Text("Current scene: %s", s);
	if (ImGui::TreeNode("Current scene params")) {
		if (curr_.scene) {
			PathList keys;
			const KNamedValues *nv = curr_.scene->getParams();
			nv->getKeys(keys, true);
			for (auto it=keys.cbegin(); it!=keys.cend(); ++it) {
				const Path &key = *it;
				ImGui::Text("%s: %s", key.u8(), nv->getString(key).u8());
			}
		}
		ImGui::TreePop();
	}
	for (size_t i=0; i<scenelist_.size(); i++) {
		Scene *scene = scenelist_[i];
		const char *name = scene ? typeid(*scene).name() : nullname;
		ImGui::PushID(ImGui_ID(i));
		if (ImGui::Button(K_sprintf("%d", i).c_str())) {
			setNextScene(i, NULL, NULL);
		}
		ImGui::SameLine();
		ImGui::Text("%s", name);
		ImGui::PopID();
	}
	ImGui::Separator();
	if (curr_.scene) {
		curr_.scene->onInspectorGui();
	}
#endif // !NO_IMGUI
}
int SceneSystem::getClock() const {
	return clock_;
}
Scene * SceneSystem::getScene(SceneId id) {
	if (id < 0) return NULL;
	if (id >= (int)scenelist_.size()) return NULL;
	return scenelist_[id];
}
Scene * SceneSystem::getCurrentScene() {
	return curr_.scene;
}
int SceneSystem::getCurrentSceneId() {
	for (size_t i=0; i<scenelist_.size(); i++) {
		if (scenelist_[i] == curr_.scene) {
			return i;
		}
	}
	return -1;
}
void SceneSystem::addScene(SceneId id, Scene *scene) {
	Log_assert(id >= 0);
	Log_assert(id < SCENE_ID_LIMIT);
	if (getScene(id)) {
		Log_warningf("W_SCENE_OVERWRITE: Scene with the same id '%d' already exists. The scene will be overwritten by new one.", id);
		if (curr_.scene == getScene(id)) {
			Log_warningf("W_DESTROY_RUNNING_SCENE: Running scene with id '%d' will immediatly destroy.", id);
		}
		if (scenelist_[id]) delete scenelist_[id];
	}
	if ((int)scenelist_.size() <= id) {
		scenelist_.resize(id + 1);
	}
	scenelist_[id] = scene;
}
#define TRAC(obj)  Log_infof("%s: '%s'", __FUNCTION__, obj ? typeid(*obj).name() : "(NULL)");

void SceneSystem::callSceneEnter() {
	TRAC(curr_.scene);
	if (curr_.scene) {
		curr_.scene->setParams(&curr_.params);

		// onSceneEnter でさらにシーンが変更される可能性に注意
		curr_.scene->onSceneEnter(curr_.action);
		Ref_drop(curr_.action);
	}
}
void SceneSystem::callSceneExit() {
	TRAC(curr_.scene);
	if (curr_.scene) {
		curr_.scene->onSceneExit();
	}
}
void SceneSystem::setNextScene(SceneId id, const KNamedValues *params, IAct *action) {
	Scene *scene = getScene(id);
	if (next_.scene) {
		Log_warningf("W_SCENE_OVERWRITE: Queued scene '%s' will be overwritten by new posted scene '%s'",
			typeid(*next_.scene).name(),
			(scene ? typeid(*scene).name() : "(NULL)")
		);
		next_.scene = NULL;
		Ref_drop(next_.action);
	}
	next_.scene = scene;
	Ref_grab(action);
	next_.action = action;
	next_.id = id;
	next_.params.assign(params);
}
void SceneSystem::restart() {
	setNextScene(curr_.id, &curr_.params, NULL);
}
void SceneSystem::postReload() {
	reload_ = true;
}
void SceneSystem::processReloading() {
	// リロード要求がある場合はリロードする
	// ただし、ロード処理実行中の場合は無視する
	if (reload_) {
		restart();
		reload_ = false;
		Log_info("Reload: end");
	}
}
void SceneSystem::processSwitching() {
	// シーン切り替えが指定されていればそれを処理する
	if (next_.scene == NULL) {
		if (curr_.scene) {
			SceneId id = curr_.scene->onQueryNextScene();
			setNextScene(id, NULL, NULL);
		}
	}
	if (next_.scene) {
		if (cb_) {
			// シーン切り替え通知
			// ここで next.params が書き換えられる可能性に注意
			cb_->onSceneSystemSceneChanging(curr_.id, curr_.scene, next_.id, &next_.params);
		}
		if (curr_.scene) {
			callSceneExit();
			Ref_drop(curr_.action);
			curr_.scene = NULL;
			curr_.id = -1;
		}
		clock_ = 0;
		curr_ = next_;
		next_.clear();
		callSceneEnter();

	} else {
		clock_++;
	}
}

} // namespace
