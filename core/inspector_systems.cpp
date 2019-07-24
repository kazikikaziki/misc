#ifndef NO_IMGUI
#include "Engine.h"
#include "inspector_systems.h"
#include "inspector.h"
#include "GameHierarchy.h"
#include "GameAudio.h"
#include "KLog.h"
#include "KNum.h"
#include "KImgui.h"
#include <algorithm>

namespace mo {

// インスペクター上でのシステムの並び順
class SystemSortPred {
public:
	ParamMap &params_;

	SystemSortPred(ParamMap &p): params_(p) {
	}
	bool operator()(System *a, System *b) const {
		if (params_[a].sort_primary == params_[b].sort_primary) {
			return strcmp(typeid(*a).name(), typeid(*b).name()) < 0;
		
		} else {
			return params_[a].sort_primary < params_[b].sort_primary;
		}
	}
};


CEngineSystemInspector::CEngineSystemInspector() {
	engine_ = NULL;
}
void CEngineSystemInspector::init(Engine *engine) {
	engine_ = engine;
}
void CEngineSystemInspector::updatePanel() {
	Log_assert(engine_);
	
	// システムリストをコピー
	tmp_.resize(engine_->getSystemCount());
	for (int i=0; i<engine_->getSystemCount(); i++) {
		tmp_[i] = engine_->getSystem(i);
	}
	if (tmp_.empty()) return;

	// インスペクター上での表示順でソート
	std::sort(tmp_.begin(), tmp_.end(), SystemSortPred(params_));

	int lastkey = params_[tmp_[0]].sort_primary;
	for (auto it=tmp_.begin(); it!=tmp_.end(); ++it) {
		System *sys = *it;

		// エディタ上での表示設定
		const SystemEditorParams &ed_params = params_[sys];

		ImGui::PushID(ImGui_ID(sys));
		if (lastkey != ed_params.sort_primary) {
			ImGui::Separator();
		}

		int time_consumed = engine_->getTimeConsumed(sys);

		// システムの消費時間に応じて色を変える
		// 重いほど赤くなるように
		int fpsreq = engine_->getStatus(Engine::ST_FPS_REQUIRED);
		{
			float k = Num_normalize(time_consumed, 0, 1000/fpsreq);
			float r = Num_lerp(0.0f, 1.0f, k);
			float g = 0.0f;
			float b = Num_lerp(1.0f, 0.0f, k);
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(r, g, b, 1.0f));
		}
		const char *sysname = typeid(*sys).name();

		// クラス名は "class Hoge" のように空白を含んでいるかもしれない。空白よりも後を使う
		const char *sysname_sep = strrchr(sysname, ' ');

		// 空白が見つかったら、その右隣の文字を先頭とする。空白が見つからなかった場合は元文字列を使う
		sysname = sysname_sep ? sysname_sep+1 : sysname;

		if (ImGui::CollapsingHeader(sysname, ed_params.default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Consume %d[msec]", time_consumed);
			}

			ImGui::Indent();
			sys->onInspectorGui();
			ImGui::Unindent();

			// インスペクター上でのシステムタブの状態を更新する
			if (! ed_params.is_open) {
				params_[sys].is_open = true;
			}

		} else {
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Consume %d[msec]", time_consumed);
			}
			// インスペクター上でのシステムタブの状態を更新する
			if (ed_params.is_open) {
				params_[sys].is_open = false;
			}
		}
		ImGui::PopStyleColor();
		lastkey = ed_params.sort_primary;
		ImGui::PopID();
	}
	engine_->clearTimeConsumed();

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Audio")) {
		AudioSystem *aud = engine_->findSystem<AudioSystem>();
		if (aud) {
			aud->onInspectorGui();
		} else {
			ImGui::Text("NO AUDIO");
		}
	}
}

} // namsepsace

#endif // !NO_IMGUI
