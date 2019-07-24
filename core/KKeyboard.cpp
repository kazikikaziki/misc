#include "KKeyboard.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include <vector>

#ifdef _WIN32
#	define K_stricmp _stricmp
#else
#	define K_stricmp strcasecmp
#endif

namespace mo {

namespace {

#ifdef _WIN32
class Win32Keyboard {
public:
	Win32Keyboard() {
		initTable();
	}
	KKeyboard::Key getKeyCode(int vKey) const {
		for (size_t i=0; i<KKeyboard::KEY_ENUM_MAX; i++) {
			if (keymap_[i] == vKey) {
				return (KKeyboard::Key)i;
			}
		}
		return KKeyboard::NONE;
	}
	bool isKeyDown(KKeyboard::Key key) const {
		int vKey = keymap_[key];
		SHORT stat = GetKeyState(vKey);
	//	SHORT stat = GetAsyncKeyState(vKey);
		return (stat & 0x8000) != 0;
	}

private:
	void initTable() {
		ZeroMemory(keymap_, sizeof(keymap_));
		auto t = keymap_;
		t[KKeyboard::NONE      ] = 0;
		t[KKeyboard::BACKSPACE ] = VK_BACK;
		t[KKeyboard::TAB       ] = VK_TAB;
		t[KKeyboard::ENTER     ] = VK_RETURN;
		t[KKeyboard::ESCAPE    ] = VK_ESCAPE;
		t[KKeyboard::SPACE     ] = ' ';
		t[KKeyboard::APOSTROPHE] = VK_OEM_7; // For US keyboard ' or "
		t[KKeyboard::COMMA     ] = ',';
		t[KKeyboard::MINUS     ] = '-';
		t[KKeyboard::PERIOD    ] = '.';
		t[KKeyboard::SLASH     ] = '/';
		t[KKeyboard::DIG_0     ] = '0';
		t[KKeyboard::DIG_1     ] = '1';
		t[KKeyboard::DIG_2     ] = '2';
		t[KKeyboard::DIG_3     ] = '3';
		t[KKeyboard::DIG_4     ] = '4';
		t[KKeyboard::DIG_5     ] = '5';
		t[KKeyboard::DIG_6     ] = '6';
		t[KKeyboard::DIG_7     ] = '7';
		t[KKeyboard::DIG_8     ] = '8';
		t[KKeyboard::DIG_9     ] = '9';
		t[KKeyboard::SEMICOLON ] = ';';
		t[KKeyboard::EQUAL     ] = '=';
		t[KKeyboard::A         ] = 'A';
		t[KKeyboard::B         ] = 'B';
		t[KKeyboard::C         ] = 'C';
		t[KKeyboard::D         ] = 'D';
		t[KKeyboard::E         ] = 'E';
		t[KKeyboard::F         ] = 'F';
		t[KKeyboard::G         ] = 'G';
		t[KKeyboard::H         ] = 'H';
		t[KKeyboard::I         ] = 'I';
		t[KKeyboard::J         ] = 'J';
		t[KKeyboard::K         ] = 'K';
		t[KKeyboard::L         ] = 'L';
		t[KKeyboard::M         ] = 'M';
		t[KKeyboard::N         ] = 'N';
		t[KKeyboard::O         ] = 'O';
		t[KKeyboard::P         ] = 'P';
		t[KKeyboard::Q         ] = 'Q';
		t[KKeyboard::R         ] = 'R';
		t[KKeyboard::S         ] = 'S';
		t[KKeyboard::T         ] = 'T';
		t[KKeyboard::U         ] = 'U';
		t[KKeyboard::V         ] = 'V';
		t[KKeyboard::W         ] = 'W';
		t[KKeyboard::X         ] = 'X';
		t[KKeyboard::Y         ] = 'Y';
		t[KKeyboard::Z         ] = 'Z';
		t[KKeyboard::BRACKET_L ] = '[';
		t[KKeyboard::BACKSLASH ] = '\\';
		t[KKeyboard::BRACKET_R ] = ']';
		t[KKeyboard::DEL       ] = VK_DELETE;
		t[KKeyboard::NUMPAD_0  ] = VK_NUMPAD0;
		t[KKeyboard::NUMPAD_1  ] = VK_NUMPAD1;
		t[KKeyboard::NUMPAD_2  ] = VK_NUMPAD2;
		t[KKeyboard::NUMPAD_3  ] = VK_NUMPAD3;
		t[KKeyboard::NUMPAD_4  ] = VK_NUMPAD4;
		t[KKeyboard::NUMPAD_5  ] = VK_NUMPAD5;
		t[KKeyboard::NUMPAD_6  ] = VK_NUMPAD6;
		t[KKeyboard::NUMPAD_7  ] = VK_NUMPAD7;
		t[KKeyboard::NUMPAD_8  ] = VK_NUMPAD8;
		t[KKeyboard::NUMPAD_9  ] = VK_NUMPAD9;
		t[KKeyboard::NUMPAD_DOT] = VK_DECIMAL;
		t[KKeyboard::NUMPAD_ADD] = VK_ADD;
		t[KKeyboard::NUMPAD_SUB] = VK_SUBTRACT;
		t[KKeyboard::NUMPAD_MUL] = VK_MULTIPLY;
		t[KKeyboard::NUMPAD_DIV] = VK_DIVIDE;
		t[KKeyboard::F1        ] = VK_F1;
		t[KKeyboard::F2        ] = VK_F2;
		t[KKeyboard::F3        ] = VK_F3;
		t[KKeyboard::F4        ] = VK_F4;
		t[KKeyboard::F5        ] = VK_F5;
		t[KKeyboard::F6        ] = VK_F6;
		t[KKeyboard::F7        ] = VK_F7;
		t[KKeyboard::F8        ] = VK_F8;
		t[KKeyboard::F9        ] = VK_F9;
		t[KKeyboard::F10       ] = VK_F10;
		t[KKeyboard::F11       ] = VK_F11;
		t[KKeyboard::F12       ] = VK_F12;
		t[KKeyboard::LEFT      ] = VK_LEFT;
		t[KKeyboard::UP        ] = VK_UP;
		t[KKeyboard::RIGHT     ] = VK_RIGHT;
		t[KKeyboard::DOWN      ] = VK_DOWN;
		t[KKeyboard::INSERT    ] = VK_INSERT;
		t[KKeyboard::PAGE_UP   ] = VK_PRIOR;
		t[KKeyboard::PAGE_DOWN ] = VK_NEXT;
		t[KKeyboard::HOME      ] = VK_HOME;
		t[KKeyboard::END       ] = VK_END;
		t[KKeyboard::SNAPSHOT  ] = VK_SNAPSHOT;
		t[KKeyboard::SHIFT     ] = VK_SHIFT;
		t[KKeyboard::SHIFT_L   ] = VK_LSHIFT;
		t[KKeyboard::SHIFT_R   ] = VK_RSHIFT;
		t[KKeyboard::CTRL      ] = VK_CONTROL;
		t[KKeyboard::CTRL_L    ] = VK_LCONTROL;
		t[KKeyboard::CTRL_R    ] = VK_RCONTROL;
		t[KKeyboard::ALT       ] = VK_MENU;
		t[KKeyboard::ALT_L     ] = VK_LMENU;
		t[KKeyboard::ALT_R     ] = VK_RMENU;
	}

	int keymap_[KKeyboard::KEY_ENUM_MAX];
};

static Win32Keyboard s_KB;
#endif

class KeyNames {
public:
	const char * get(KKeyboard::Key k) {
		static const char *s_empty = "";
		prepare();
		if (0 <= k && k < KKeyboard::KEY_ENUM_MAX) {
			return table_[k].c_str();
		} else {
			return s_empty;
		}
	}
	bool parse(const char *s, KKeyboard::Key *k) {
		prepare();
		if (s && s[0]) {
			for (size_t i=0; i<KKeyboard::KEY_ENUM_MAX; i++) {
				const char *n = table_[i].c_str();
				if (K_stricmp(n, s) == 0) {
					if (k) *k = (KKeyboard::Key)i;
					return true;
				}
			}
		}
		return false;
	}

private:
	void prepare() {
		if (!table_.empty()) {
			return;
		}
		auto &t = table_;
		t.resize(KKeyboard::KEY_ENUM_MAX);
		t[KKeyboard::NONE          ] = "";
		t[KKeyboard::BACKSPACE     ] = "Backspace";
		t[KKeyboard::TAB           ] = "Tab";
		t[KKeyboard::ENTER         ] = "Enter"; // Return
		t[KKeyboard::ESCAPE        ] = "Escape"; // ESC
		t[KKeyboard::SPACE         ] = "Space";
		t[KKeyboard::APOSTROPHE    ] = "Apostrophe"; // Single Quot
		t[KKeyboard::COMMA         ] = "Comma";
		t[KKeyboard::MINUS         ] = "Minus";
		t[KKeyboard::PERIOD        ] = "Period"; // Dot
		t[KKeyboard::SLASH         ] = "Slash";
		t[KKeyboard::DIG_0         ] = "0";
		t[KKeyboard::DIG_1         ] = "1";
		t[KKeyboard::DIG_2         ] = "2";
		t[KKeyboard::DIG_3         ] = "3";
		t[KKeyboard::DIG_4         ] = "4";
		t[KKeyboard::DIG_5         ] = "5";
		t[KKeyboard::DIG_6         ] = "6";
		t[KKeyboard::DIG_7         ] = "7";
		t[KKeyboard::DIG_8         ] = "8";
		t[KKeyboard::DIG_9         ] = "9";
		t[KKeyboard::SEMICOLON     ] = "Semicolon"; 
		t[KKeyboard::EQUAL         ] = "Equal";
		t[KKeyboard::A             ] = "A";
		t[KKeyboard::B             ] = "B";
		t[KKeyboard::C             ] = "C";
		t[KKeyboard::D             ] = "D";
		t[KKeyboard::E             ] = "E";
		t[KKeyboard::F             ] = "F";
		t[KKeyboard::G             ] = "G";
		t[KKeyboard::H             ] = "H";
		t[KKeyboard::I             ] = "I";
		t[KKeyboard::J             ] = "J";
		t[KKeyboard::K             ] = "K";
		t[KKeyboard::L             ] = "L";
		t[KKeyboard::M             ] = "M";
		t[KKeyboard::N             ] = "N";
		t[KKeyboard::O             ] = "O";
		t[KKeyboard::P             ] = "P";
		t[KKeyboard::Q             ] = "Q";
		t[KKeyboard::R             ] = "R";
		t[KKeyboard::S             ] = "S";
		t[KKeyboard::T             ] = "T";
		t[KKeyboard::U             ] = "U";
		t[KKeyboard::V             ] = "V";
		t[KKeyboard::W             ] = "W";
		t[KKeyboard::X             ] = "X";
		t[KKeyboard::Y             ] = "Y";
		t[KKeyboard::Z             ] = "Z";
		t[KKeyboard::BRACKET_L     ] = "LeftBracket";
		t[KKeyboard::BACKSLASH     ] = "Backslash";
		t[KKeyboard::BRACKET_R     ] = "RightBracket";
		t[KKeyboard::DEL           ] = "Delete"; // Del
		t[KKeyboard::NUMPAD_0      ] = "Numpad0";
		t[KKeyboard::NUMPAD_1      ] = "Numpad1";
		t[KKeyboard::NUMPAD_2      ] = "Numpad2";
		t[KKeyboard::NUMPAD_3      ] = "Numpad3";
		t[KKeyboard::NUMPAD_4      ] = "Numpad4";
		t[KKeyboard::NUMPAD_5      ] = "Numpad5";
		t[KKeyboard::NUMPAD_6      ] = "Numpad6";
		t[KKeyboard::NUMPAD_7      ] = "Numpad7";
		t[KKeyboard::NUMPAD_8      ] = "Numpad8";
		t[KKeyboard::NUMPAD_9      ] = "Numpad9";
		t[KKeyboard::NUMPAD_DOT    ] = "NumpadPeriod"; // Dot
		t[KKeyboard::NUMPAD_ADD    ] = "NumpadPlus";   // Add, Plus
		t[KKeyboard::NUMPAD_SUB    ] = "NumpadMinus";  // Sub, Minus
		t[KKeyboard::NUMPAD_MUL    ] = "NumpadStar";   // Mul, Star, Asterisk
		t[KKeyboard::NUMPAD_DIV    ] = "NumpadSlash";  // Div, Slash
		t[KKeyboard::F1            ] = "F1";
		t[KKeyboard::F2            ] = "F2";
		t[KKeyboard::F3            ] = "F3";
		t[KKeyboard::F4            ] = "F4";
		t[KKeyboard::F5            ] = "F5";
		t[KKeyboard::F6            ] = "F6";
		t[KKeyboard::F7            ] = "F7";
		t[KKeyboard::F8            ] = "F8";
		t[KKeyboard::F9            ] = "F9";
		t[KKeyboard::F10           ] = "F10";
		t[KKeyboard::F11           ] = "F11";
		t[KKeyboard::F12           ] = "F12";
		t[KKeyboard::LEFT          ] = "Left";
		t[KKeyboard::UP            ] = "Up";
		t[KKeyboard::RIGHT         ] = "Right";
		t[KKeyboard::DOWN          ] = "Down";
		t[KKeyboard::INSERT        ] = "Insert";
		t[KKeyboard::PAGE_UP       ] = "PageUp";   // PGUP
		t[KKeyboard::PAGE_DOWN     ] = "PageDown"; // PGDN
		t[KKeyboard::HOME          ] = "Home";
		t[KKeyboard::END           ] = "End";
		t[KKeyboard::SNAPSHOT      ] = "PrintScreen"; // Snapshot PrintScreen, PrtSc
		t[KKeyboard::SHIFT         ] = "Shift";
		t[KKeyboard::SHIFT_L       ] = "LeftShift";
		t[KKeyboard::SHIFT_R       ] = "RightShift";
		t[KKeyboard::CTRL          ] = "Ctrl";
		t[KKeyboard::CTRL_L        ] = "LeftCtrl";
		t[KKeyboard::CTRL_R        ] = "RightCtrl";
		t[KKeyboard::ALT           ] = "Alt";
		t[KKeyboard::ALT_L         ] = "LeftAlt";
		t[KKeyboard::ALT_R         ] = "RightAlt";
	}
	std::vector<std::string> table_;
};

static KeyNames s_Names;

} // private namespace

bool KKeyboard::isKeyDown(Key k) {
#ifdef _WIN32
	return s_KB.isKeyDown(k);
#else
	return false;
#endif
}
KKeyboard::Key KKeyboard::getKeyFromVirtualKey(int vKey) {
#ifdef _WIN32
	return s_KB.getKeyCode(vKey);
#else
	return (Key)0;
#endif
}
KKeyboard::ModifierKeys KKeyboard::getModifiers() {
	ModifierKeys flags = 0;
	if (isKeyDown(SHIFT)) flags |= M_SHIFT;
	if (isKeyDown(CTRL))  flags |= M_CTRL;
	if (isKeyDown(ALT))   flags |= M_ALT;
	return flags;
}
const char * KKeyboard::getKeyName(Key k) {
	return s_Names.get(k);
}
bool KKeyboard::findKeyByName(const char *name, Key *key) {
	return s_Names.parse(name, key);
}

} // namespace
