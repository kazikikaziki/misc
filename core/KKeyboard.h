#pragma once

namespace mo {

class KKeyboard {
public:
	enum Key {
		NONE,
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		DIG_0, DIG_1, DIG_2, DIG_3, DIG_4, DIG_5, DIG_6, DIG_7, DIG_8, DIG_9,
		SPACE, APOSTROPHE, COMMA, MINUS, PERIOD, SLASH, SEMICOLON, EQUAL, BRACKET_L, BRACKET_R, BACKSLASH,
		NUMPAD_0, NUMPAD_1, NUMPAD_2, NUMPAD_3, NUMPAD_4,
		NUMPAD_5, NUMPAD_6, NUMPAD_7, NUMPAD_8, NUMPAD_9,
		NUMPAD_DOT, NUMPAD_ADD, NUMPAD_SUB, NUMPAD_MUL, NUMPAD_DIV,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		LEFT, RIGHT, UP, DOWN, PAGE_UP, PAGE_DOWN, HOME, END,
		BACKSPACE, TAB, ENTER, INSERT, DEL,
		ESCAPE, SNAPSHOT,
		SHIFT, SHIFT_L, SHIFT_R,
		CTRL, CTRL_L, CTRL_R,
		ALT, ALT_L, ALT_R,
		KEY_ENUM_MAX,
	};

	enum Modifier {
		M_SHIFT = 1,
		M_CTRL  = 2,
		M_ALT   = 4,
	};
	typedef int ModifierKeys;

	static ModifierKeys getModifiers();

	/// キーボードのキーが押されている状態かどうか
	static bool isKeyDown(Key k);

	/// キーボードのキーの文字列表現を返す
	/// 名前が未定義の場合は空文字列 "" を返す。（NULLは返さない）
	static const char * getKeyName(Key k);

	/// 名前を指定してキーを得る。大小文字は区別しない
	static bool findKeyByName(const char *name, Key *k);

	static Key getKeyFromVirtualKey(int vKey);
};

} // namespace

