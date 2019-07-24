#pragma once
#ifndef NO_IMGUI
namespace mo {

class Engine;

class CCoreEngineInspector {
public:
	CCoreEngineInspector();
	void init(Engine *engine);
	void updatePanel();
	void updateAppPanel();
	void updateTimePanel();
	void updateMousePanel();
	void updateWindowPanel();

private:
	Engine *engine_;
};

}

#endif // !NO_IMGUI
