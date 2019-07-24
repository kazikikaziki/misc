#pragma once
#include "KKeyboard.h"
#include "KWindow.h"
#include "system.h"

namespace mo {

class Engine;

class IDebugMenuCallback {
public:
	struct Params {
		int menu_id;
		int item_id;
		bool enabled;
		bool checked;
		bool do_default;

		Params() {
			menu_id = 0;
			item_id = 0;
			enabled = false;
			checked = false;
			do_default = false;
		}
	};
	/// ���j���[���\�������Ƃ��ɌĂ΂��
	virtual void onDebugMenuSystemMenuOpen(IDebugMenuCallback::Params *p) {}

	/// ���j���[���ڂ��\�������Ƃ��ɌĂ΂��
	virtual void onDebugMenuSystemMenuItemShow(IDebugMenuCallback::Params *p) {}

	/// ���j���[���ڂ��I�����ꂽ���ɌĂ΂��
	virtual void onDebugMenuSystemMenuItemClicked(IDebugMenuCallback::Params *p) {}
};

/// �f�o�b�O���j���[��ǉ�����
class DebugMenuSystem : public System, public IWindowCallback {
public:
	DebugMenuSystem();
	virtual ~DebugMenuSystem();
	void init(Engine *engine);
	virtual void onEnd() override;
	void addCallback(IDebugMenuCallback *cb);
	void removeCallback(IDebugMenuCallback *cb);
	void addUserMenuItem(const wchar_t *text, int id=0, KKeyboard::Key key=KKeyboard::NONE, KKeyboard::ModifierKeys mods=0);
	void addDebugVisibleMenu();
	void addReloadMenu();
	void addTimeControlMenu();
	void addHelpMenu();
	void addFullScreenMenu();
	void addSparator();

	// IWindowCallback
	virtual void onWindowEvent(WindowEvent *e) override;

private:
	struct Item {
		Item() {
			menu_id = 0;
			item_id = 0;
		}
		int menu_id;
		int item_id;
	};
	struct Shortcut {
		Shortcut() {
			key = KKeyboard::NONE;
			mods = 0;
			item_id = 0;
		}
		KKeyboard::Key key;
		KKeyboard::ModifierKeys mods;
		int item_id;
	};
	int addSysMenuItem(const wchar_t *text, int item_id = 0, const wchar_t *icon_res = NULL);
	bool addKeyboardShortcut(int item_id, KKeyboard::Key key, KKeyboard::ModifierKeys mods);
	/// ���j���[���J�����Ƃ��Ă���
	void onMenuOpen(int menu_id);
	const Item * findItemById(int item_id);
	/// ���j���[���ڂ��\������钼�O�ɌĂ΂��B
	/// ���j���[���ڂ̃`�F�b�N�̗L���� Enabled �̏�ԂȂǂ͂��̃^�C�~���O�ōX�V����
	void onMenuItemShow(const Item *item);
	/// ���j���[���ڂ��N���b�N�܂��̓V���[�g�J�b�g�L�[�ɂ���đI�����ꂽ���ɌĂ΂��
	void onMenuItemClick(const Item *item);
	// ���j���[���I�����ꂽ���̃f�t�H���g�������s��
	void doDefaultMenuClick(int item_id);

	std::vector<Shortcut> shortcuts_;
	std::vector<Item> menuitems_;
	std::vector<IDebugMenuCallback *> cb_;
	Engine *engine_;
	int inspector_style_;
	bool initialized_;
};


} // namespace
