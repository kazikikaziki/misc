#pragma once
#include "system.h"
#include "KNamedValues.h"

namespace mo {

class IAct;
class Engine;
class KNamedValues;
	
class Scene {
public:
	explicit Scene() {}
	virtual ~Scene() {}
	virtual int onQueryNextScene() = 0;
	virtual void onSceneUpdate() = 0;

	/// シーン開始する。
	/// main_action にはシーン管理用のアクションを指定する。非NULLが指定された場合は参照カウンタを増やす。NULLの場合はデフォルトのシーン処理を行う
	virtual void onSceneEnter(IAct *main_action) {}
	virtual void onSceneExit() {}
	virtual void onInspectorGui() {}

	/// パラメータがセットされるときに呼ばれる
	virtual void onSetParams(KNamedValues *new_params, const KNamedValues *specified_params, const KNamedValues *old_params) {}

	const KNamedValues * getParams() const;
	void setParams(const KNamedValues *new_params);

private:
	KNamedValues params_;
};

typedef int SceneId;

class ISceneCallback {
public:
	/// curr_id 現在のシーン番号
	/// curr_scene 現在のシーン
	/// next_id 次のシーン番号
	/// next_params 次のシーンに設定されるパラメータ群
	virtual void onSceneSystemSceneChanging(SceneId curr_id, Scene *curr_scene, int next_id, KNamedValues *next_params) = 0;
};

class SceneSystem: public System {
public:
	SceneSystem();
	virtual ~SceneSystem();
	void init(Engine *engine);
	void setCallback(ISceneCallback *cb);
	void processReloading();
	void processSwitching();
	void restart();
	int getClock() const;
	Scene * getScene(SceneId id);
	Scene * getCurrentScene();
	void addScene(SceneId id, Scene *scene);
	void setNextScene(SceneId id, const KNamedValues *params=NULL, IAct *action=NULL);
	void postReload();
	int getCurrentSceneId();
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual void onFrameStart() override;
	virtual void onInspectorGui() override;

private:
	struct _SceneDesc {
		Scene *scene;
		IAct *action;
		KNamedValues params;
		SceneId id;

		void clear() {
			scene = NULL;
			action = NULL;
			id = -1;
			params.clear();
		}
	};

	void callSceneEnter();
	void callSceneExit();

	std::vector<Scene *> scenelist_;
	_SceneDesc curr_;
	_SceneDesc next_;
	int clock_;
	bool reload_;
	ISceneCallback *cb_;
	Engine *engine_;
};


}

