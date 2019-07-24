#ifndef NO_IMGUI
#include "inspector_entity.h"
//
#include "GameHierarchy.h"
#include "GameCamera.h"
#include "GameRender.h"
#include "Engine.h"
#include "inspector.h"
#include "KImgui.h"
#include "KNum.h"
#include "KLog.h"


namespace mo {

CEntityInspector::CEntityInspector() {
	engine_ = NULL;
	hierarchy_ = false;
	solo_ = 0;
	mute_ = 0;
}
void CEntityInspector::init(Engine *engine) {
	engine_ = engine;
}
bool CEntityInspector::isVisible(EID e) const {
	if (solo_) {
		if (hierarchy_) {
			return isSelectedInHierarchy(e);
		} else {
			return isSelected(e);
		}
	}
	if (mute_) {
		if (hierarchy_) {
			return !isSelectedInHierarchy(e);
		} else {
			return !isSelected(e);
		}
	}
	return true;
}
int CEntityInspector::getSelectedCount() const {
	return (int)selected_entities_.size();
}
EID CEntityInspector::getSelected(int index) {
	if (index < 0 || (int)selected_entities_.size() <= index) {
		return NULL;
	}
	return selected_entities_[index];
}
void CEntityInspector::select(EID e) {
	if (e) selected_entities_.push_back(e);
}
void CEntityInspector::unselectAll(){
	selected_entities_.clear();
}
bool CEntityInspector::isSelected(EID e) const {
	if (e == NULL) return false;
	auto it = std::find(selected_entities_.begin(), selected_entities_.end(), e);
	if (it == selected_entities_.end()) return false;
	return true;
}
bool CEntityInspector::isSelectedInHierarchy(EID e) const {
	HierarchySystem *hs = engine_->findSystem<HierarchySystem>();
	while (e) {
		if (isSelected(e)) {
			return true;
		}
		e = hs->getParent(e);
	}
	return false;
}
void CEntityInspector::guiEntityBrowser() {
	EID selected_e = getSelected(0);
	if (selected_e == NULL) {
		ImGui::Text("** No entity selected");
		return;
	}
	HierarchySystem *hs = engine_->findSystem<HierarchySystem>();
	if (!hs->exists(selected_e)) {
		ImGui::TextColored(GuiColor_WarningText, "** Entiy %d(0x%X) does not exist.", EID_toint(selected_e), EID_toint(selected_e));
		return;
	}

	updateEntityAttribute(selected_e);
	{
		bool solo = (solo_ == selected_e);
		if (ImGui::Checkbox("Solo", &solo)) {
			if (solo) {
				mute_ = NULL;
				solo_ = selected_e;
			} else {
				solo_ = NULL;
			}
		}
	}
	ImGui::SameLine();
	{
		bool mute = (mute_ == selected_e);
		if (ImGui::Checkbox("Mute", &mute)) {
			if (mute) {
				solo_ = NULL;
				mute_ = selected_e;
			} else {
				mute_ = NULL;
			}
		}
	}
	ImGui::SameLine();
	ImGui::Checkbox("Hierarchy", &hierarchy_);

	// Components
	ImGui::Dummy(ImVec2(0, 8));
	for (int i=0; i<engine_->getSystemCount(); i++) {
		System *s = engine_->getSystem(i);
		if (s->isAttached(selected_e)) {
			ImGui::PushID(s);
			int flags = ImGuiTreeNodeFlags_DefaultOpen;
			if (ImGui::CollapsingHeader(typeid(*s).name(), flags)) {
				ImGui::Indent();
				s->onEntityInspectorGui(selected_e);
				ImGui::Unindent();
				ImGui::Dummy(ImVec2(0, 8));
			}
			ImGui::PopID();
		}
	}
}
void CEntityInspector::updateEntityAttribute(EID e) {
	HierarchySystem *hs = engine_->findSystem<HierarchySystem>();
	Log_assert(hs);

	RenderSystem *rs = engine_->findSystem<RenderSystem>();
	Log_assert(rs);

	if (1) {
		auto name = hs->getName(e);
		auto path = hs->getNameInTree(e);
		int order = hs->getSerialIndex(e);
		int self_layer = hs->getLayer(e);
		int tree_layer = hs->getLayerInTree(e);
		ImGui::Text("Name: %s", name.u8());
		ImGui::Text("Path: %s", path.u8());
		ImGui::Text("ID: %d(0x%X)", EID_toint(e), EID_toint(e));
		ImGui::Text("Tag: %d", hs->getTag(e));
		if (self_layer >= 0) {
			ImGui::Text("Layer: %d", self_layer);
		} else {
			ImGui::Text("Layer: %d (inherited)", tree_layer);
		}
		ImGui::Text("Order In Hierarchy: %d", order);

		CameraSystem *cs = engine_->findSystem<CameraSystem>();
		EID camera_e = NULL;
		if (cs) {
			camera_e = cs->getCameraForEntity(e);
		}

		if (cs == NULL) {
			ImGui::TextColored(GuiColor_WarningText, "No CameraSystem installed");
		} else if (camera_e == NULL) {
			ImGui::TextColored(GuiColor_WarningText, "Pos in camera: NO CAMERA FOR THIS ENTITY.");
		} else {
			View view;
			cs->getView(camera_e, &view);

			Matrix4 world_matrix;
			hs->getLocalToWorldMatrix(e, &world_matrix);
			Matrix4 matrix = world_matrix * view.transform_matrix;
			Matrix4 proj = view.projection_matrix;
			Vec3 pos = (matrix * proj).transformZero();
			if (Num_in_range(pos.x, -1.0f, 1.0) && Num_in_range(pos.y, -1.0f, 1.0f)) {
				ImGui::TextColored(GuiColor_ActiveText, "Pos in view: %.2f %.2f", pos.x, pos.y);
			} else {
				ImGui::TextColored(GuiColor_WarningText, "Pos in view: %.2f %.2f (OUT OF VIEW)", pos.x, pos.y);
			}
		}
	}
	if (1) {
		bool b = hs->isEnabled(e);
		if (ImGui::Checkbox("Enabled", &b)) {
			hs->setEnabled(e, b);
		}
	}
	if (1) {
		bool b = hs->isPaused(e);
		if (ImGui::Checkbox("Paused", &b)) {
			hs->setPaused(e, b);
		}
	}
	if (1) {
		bool b = hs->isVisible(e);
		if (ImGui::Checkbox("Visible", &b)) {
			hs->setVisible(e, b);
		}
	}
}


} // namespace

#endif // !NO_IMGUI
