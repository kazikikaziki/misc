#pragma once
#ifndef NO_IMGUI
#include "GameId.h"
#include <vector>

namespace mo {

class Engine;

class CEntityInspector {
public:
	CEntityInspector();
	void init(Engine *engine);
	bool isVisible(EID e) const;
	int getSelectedCount() const;
	EID getSelected(int index);
	void select(EID e);
	void unselectAll();
	bool isSelected(EID e) const;
	bool isSelectedInHierarchy(EID e) const;
	void guiEntityBrowser();
	void updateEntityAttribute(EID e);
private:
	Engine *engine_;
	std::vector<EID> selected_entities_;
	EID solo_;
	EID mute_;
	bool hierarchy_;
};

}

#endif // !NO_IMGUI
