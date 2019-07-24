#pragma once
#ifndef NO_IMGUI
#include <vector>
#include <unordered_map>

namespace mo {

class Engine;
class System;

struct SystemEditorParams {
	SystemEditorParams() {
		time_consumed = 0;
		sort_primary = 0;
		default_open = false;
		is_open = false;
		always_show_debug = false;
	}

	/// システム更新の所要時間
	uint64_t time_consumed;

	/// インスペクター上にシステムペインを表示する時、
	/// 並び順の決定に使う値。プライマリキー
	int sort_primary;

	/// インスペクター上で、最初からペインが開いているかどうか
	bool default_open;

	/// インスペクター上で、現在ペインが開いているかどうか
	bool is_open;

	/// インスペクターが表示されていなくてもデバッグ情報を描画するかどうか
	bool always_show_debug;
};
typedef std::unordered_map<void *, SystemEditorParams> ParamMap;

class CEngineSystemInspector {
public:
	CEngineSystemInspector();
	void init(Engine *engine);
	void updatePanel();

	SystemEditorParams * getInfo(void *s) {
		return &params_[s];
	}

private:
	ParamMap params_;
	std::vector<System *> tmp_;
	Engine *engine_;
};

} // namespace

#endif // !NO_IMGUI