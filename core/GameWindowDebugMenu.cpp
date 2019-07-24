#include "GameWindowDebugMenu.h"
#include "GameScene.h"
#include "inspector.h"
#include "Engine.h"
#include "Screen.h"
#include "KLog.h"
#include "KMenu.h"
#include "KWindow.h"

namespace mo {

#pragma region DebugMenuSystem

enum MID {
	MID_NONE,
	MID_DEBUGHELP,
	MID_GAMEHELP,
	MID_TOGGLE_SCREEN_MODE,
	MID_FPS_VISIBLE,
	MID_INSPECTOR_VISIBLE,
	MID_SCENE_RESTART,
	MID_PLAY,
	MID_STEP,
	MID_PAUSE,
};


DebugMenuSystem::DebugMenuSystem() {
	inspector_style_ = 0;
	engine_ = NULL;
}
DebugMenuSystem::~DebugMenuSystem() {
	onEnd();
}
void DebugMenuSystem::init(Engine *engine) {
	Log_trace;
	inspector_style_ = 0;
	engine_ = engine;
	engine_->addSystem(this);
	engine_->addWindowCallback(this);
	if (1) {
		addFullScreenMenu();
	}
	if (1) {
		addHelpMenu();
	}
}
void DebugMenuSystem::onEnd() {
	if (engine_) {
		engine_->removeWindowCallback(this);
		engine_ = NULL;
	}
}
void DebugMenuSystem::onWindowEvent(WindowEvent *e) {
	switch (e->type) {
	case WindowEvent::MENU_OPEN:
		onMenuOpen(e->menu_ev.menu_id);
		break;

	case WindowEvent::MENU_CLICK:
		{
			const Item *item = findItemById(e->menu_ev.command_id);
			onMenuItemClick(item);
			break;
		}
	
	case WindowEvent::KEY_DOWN:
		{
			// �V���[�g�J�b�g�e�[�u���Ɣ�r����
			for (auto it=shortcuts_.begin(); it!=shortcuts_.end(); ++it) {
				const Shortcut &shortcut = *it;
				if (e->key_ev.key == shortcut.key) {
					if (e->key_ev.mod_flags == shortcut.mods) {
						const Item *item = findItemById(shortcut.item_id);
						onMenuItemClick(item);
						return;
					}
				}
			}
			break;
		}
	}
}
void DebugMenuSystem::addCallback(IDebugMenuCallback *cb) {
	if (cb) cb_.push_back(cb);
}
void DebugMenuSystem::removeCallback(IDebugMenuCallback *cb) {
	for (int i = (int)cb_.size() - 1; i >= 0; i--) {
		if (cb_[i] == cb) {
			cb_.erase(cb_.begin() + i);
		}
	}
}
void DebugMenuSystem::addFullScreenMenu() {
	addSparator();
	addUserMenuItem(L"�t���X�N���[�� Alt+Enter", MID_TOGGLE_SCREEN_MODE, KKeyboard::ENTER, KKeyboard::M_ALT);
}
void DebugMenuSystem::addSparator() {
	KMenu *menu = engine_->getWindow()->getMenu();
	if (menu) {
		menu->addSeparator();
	}
}
void DebugMenuSystem::addUserMenuItem(const wchar_t *text, int item_id, KKeyboard::Key key, KKeyboard::ModifierKeys mods) {
	if (addSysMenuItem(text, item_id)) {
		if (key != KKeyboard::NONE) {
			addKeyboardShortcut(item_id, key, mods);
		}
	}
}
int DebugMenuSystem::addSysMenuItem(const wchar_t *text, int item_id, const wchar_t *icon_res) {
	KMenu *menu = engine_->getWindow()->getMenu();
	if (menu == NULL) return -1;

	if (item_id && findItemById(item_id) != NULL) {
		// ����ID�̃��j���[���ڂ����ɑ��݂���
		Log_errorf("E_MENU_ITEM_ALREADY_EXISTS: %d", item_id);
		return -1;
	}
	Item item;
	item.menu_id = 0; // 0 = SYSTEM MENU
	item.item_id = item_id;
	menuitems_.push_back(item);
	//

	int idx = menu->addItem(text, item_id);
	if (idx >= 0 && icon_res) {
		menu->setItemBitmapFromResource(idx, icon_res);
	}
	return idx;
}
bool DebugMenuSystem::addKeyboardShortcut(int item_id, KKeyboard::Key key, KKeyboard::ModifierKeys mods) {
	if (item_id == 0) {
		// ���j���[ID 0 �͂ǂ̃��j���[���ڂ��w���Ȃ�
		Log_errorf("E_INVALID_MENU_ITEM_ID: %d", item_id);
		return false;
	}
	const Item *item = findItemById(item_id);
	if (item == NULL) {
		// �֘A�t���郁�j���[���ڂ����݂��Ȃ�
		Log_errorf("E_MISSING_MENU_ITEM_ID: %d", item_id);
		return false;
	}
	Shortcut sc;
	sc.key = key;
	sc.mods = mods;
	sc.item_id = item_id;
	shortcuts_.push_back(sc);
	return true;
}
void DebugMenuSystem::addDebugVisibleMenu() {
	KMenu *menu = engine_->getWindow()->getMenu();
	if (menu) {
		addSparator();

		addSysMenuItem(L"FPS\tShift+F1", MID_FPS_VISIBLE);
		addKeyboardShortcut(MID_FPS_VISIBLE, KKeyboard::F1, KKeyboard::M_SHIFT);

		int idx = addSysMenuItem(L"�f�o�b�O���j���[\tShift+ESC", MID_INSPECTOR_VISIBLE);
		menu->setItemBitmapFromResource(idx, L"ICO_PROP");
		addKeyboardShortcut(MID_INSPECTOR_VISIBLE, KKeyboard::ESCAPE, KKeyboard::M_SHIFT);
	}
}
void DebugMenuSystem::addReloadMenu() {
	KMenu *menu = engine_->getWindow()->getMenu();
	if (menu) {
		addSparator();
		addSysMenuItem(L"�V�[�����Z�b�g\tF5", MID_SCENE_RESTART);
		addKeyboardShortcut(MID_SCENE_RESTART, KKeyboard::F5, 0);
	}
}
void DebugMenuSystem::addTimeControlMenu() {
	KMenu *menu = engine_->getWindow()->getMenu();
	if (menu) {
		addSparator();
		if (1) {
			int idx = addSysMenuItem(L"�ꎞ��~\tF1", MID_PAUSE);
			menu->setItemBitmapFromResource(idx, L"ICO_PAUSE");
			//	addKeyboardShortcut(MID_PAUSE, KeyCode_F1, NULL); MID_STEP �Ƌ��p
		}
		if (1) {
			int idx = addSysMenuItem(L"�X�e�b�v\tF1", MID_STEP);
			menu->setItemBitmapFromResource(idx, L"ICO_NEXT");
			addKeyboardShortcut(MID_STEP, KKeyboard::F1, 0);
		}
		if (1) {
			int idx = addSysMenuItem(L"�ĊJ\tF2", MID_PLAY);
			menu->setItemBitmapFromResource(idx, L"ICO_PLAY");
			addKeyboardShortcut(MID_PLAY, KKeyboard::F2, 0);
		}
	}
}
void DebugMenuSystem::addHelpMenu() {
}
void DebugMenuSystem::onMenuOpen(int menu_id) {
	// �J�����Ƃ��Ă��郁�j���[�Ɋ܂܂�Ă��郁�j���[��񋓂��A�C�x���g���Ă�
	for (size_t i = 0; i < menuitems_.size(); i++) {
		const Item *item = &menuitems_[i];
		if (item->menu_id == menu_id) {
			onMenuItemShow(item);
		}
	}
}
const DebugMenuSystem::Item * DebugMenuSystem::findItemById(int item_id) {
	for (size_t i = 0; i < menuitems_.size(); i++) {
		Item *item = &menuitems_[i];
		if (item->item_id == item_id) {
			return item;
		}
	}
	return NULL;
}
void DebugMenuSystem::onMenuItemShow(const Item *item) {
	Log_assert(engine_);
	KMenu *menu = engine_->getWindow()->getMenu();
	Log_assert(menu);

	Inspector *inspector = engine_->getInspector();

	// �R�[���o�b�N�p�̃p�����[�^���쐬����
	IDebugMenuCallback::Params p;
	p.menu_id = item->menu_id;
	p.item_id = item->item_id;
	p.enabled = menu->isItemEnabled(menu->getIndexOfCommandId(item->item_id));
	p.checked = menu->isItemChecked(menu->getIndexOfCommandId(item->item_id));
	p.do_default = true;

	// ���݂̏󋵂ɉ����āA���j���[���ڂ̑������X�V���Ă���
	IDebugMenuCallback::Params p_old = p; // COPY
	{
		switch (item->item_id) {
		case MID_FPS_VISIBLE:
			p.checked = inspector && inspector->isStatusTipVisible();
			break;
		case MID_INSPECTOR_VISIBLE:
			p.checked = inspector && inspector->isVisible();
			break;
		case MID_PAUSE:
			p.checked = engine_->isPaused();
			break;
		}
	}

	// �R�[���o�b�N
	for (size_t i = 0; i < cb_.size(); i++) {
		cb_[i]->onDebugMenuSystemMenuItemShow(&p);
	}

	// ���j���[���ڂ̑������ω����Ă���΁A����𔽉f������
	if (p.enabled != p_old.enabled) {
		menu->setItemEnabled(menu->getIndexOfCommandId(item->item_id), p.enabled);
	}
	if (p.checked != p_old.checked) {
		menu->setItemChecked(menu->getIndexOfCommandId(item->item_id), p.checked);
	}
}
void DebugMenuSystem::onMenuItemClick(const Item *item) {
	KMenu *menu = engine_->getWindow()->getMenu();
	Log_assert(menu);

	// ���[�U�[��`�̃R�[���o�b�N���ݒ肳��Ă���΁A������Ăяo��
	IDebugMenuCallback::Params p;
	p.menu_id = item->menu_id;
	p.item_id = item->item_id;
	p.enabled = menu->isItemEnabled(menu->getIndexOfCommandId(item->item_id));
	p.checked = menu->isItemChecked(menu->getIndexOfCommandId(item->item_id));
	p.do_default = true;

	// �R�[���o�b�N
	IDebugMenuCallback::Params p_old = p; // COPY
	for (size_t i = 0; i < cb_.size(); i++) {
		cb_[i]->onDebugMenuSystemMenuItemClicked(&p);
	}

	// �p�����[�^���ύX����Ă���΂���𔽉f������
	if (p.enabled != p_old.enabled) {
		menu->setItemEnabled(menu->getIndexOfCommandId(item->item_id), p.enabled);
	}
	if (p.checked != p_old.checked) {
		menu->setItemEnabled(menu->getIndexOfCommandId(item->item_id), p.checked);
	}

	// ����̃��j���[����
	if (p.do_default) {
		doDefaultMenuClick(item->item_id);
	}
}
void DebugMenuSystem::doDefaultMenuClick(int item_id) {
	Log_assert(engine_);

	KMenu *menu = engine_->getWindow()->getMenu();
	Log_assert(menu);
	switch (item_id) {
	case MID_STEP:
		engine_->playStep();
		break;
	case MID_PLAY:
		engine_->play();
		break;
	case MID_PAUSE:
		engine_->pause();
		break;
	case MID_SCENE_RESTART:
		{
			auto sys = engine_->findSystem<SceneSystem>();
			if (sys) {
				sys->postReload();
				g_engine->play(); // �ꎞ��~���Ȃ�΁A�������������
			}
			break;
		}
	case MID_TOGGLE_SCREEN_MODE:
		{
			if (engine_->getStatus(Engine::ST_VIDEO_IS_FULLSCREEN)) {
				engine_->restoreWindow();
			}
			else {
				engine_->setFullscreen();
			}
			break;
		}
	case MID_FPS_VISIBLE:
		{
			Inspector *inspector = engine_->getInspector();
			if (inspector) {
				bool val = !inspector->isStatusTipVisible();
				inspector->setStatusTipVisible(val);
			} else {
				Log_warning("DebugMenuSystem::doDefaultMenuClick: NO_INSPECTOR");
			}
			break;
		}
	case MID_INSPECTOR_VISIBLE:
		{
			Inspector *inspector = engine_->getInspector();
			if (inspector) {
				inspector_style_ = (inspector_style_ + 1) % 3;
				switch (inspector_style_) {
				case 0:
					inspector->setVisible(false);
					break;

				case 1:
					inspector->setOverwrappingWindowMode(true);
					inspector->setVisible(true);
					break;

				case 2:
					inspector->setOverwrappingWindowMode(false);
					inspector->setVisible(true);
					break;
				}
				menu->setItemChecked(menu->getIndexOfCommandId(MID_INSPECTOR_VISIBLE), inspector->isVisible());
			} else {
				Log_warning("DebugMenuSystem::doDefaultMenuClick: NO_INSPECTOR");
			}
			break;
		}
	}
}

#pragma endregion // DebugMenuSystem

} // namespace
