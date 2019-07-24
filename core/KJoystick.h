#pragma once

namespace mo {

class KJoystick {
public:
	enum {
		MAX_CONNECTION = 2,
	};
	enum Axis {
		X, ///< X軸
		Y, ///< Y軸
		Z, ///< Z軸
		R, ///< X回転
		U, ///< Y回転
		V, ///< Z回転
		POV_X, ///< ハットスイッチX方向
		POV_Y, ///< ハットスイッチY方向
		MAX_AXIS,
	};

	/// ボタンの識別子。
	/// B1 から B16 までは純粋なボタンを表す。
	enum Button {
		NONE = -1,

		B1,   ///< 物理ボタン。ゲームパッドのボタン1に対応 （PS3コントローラ: △）
		B2,   ///< 物理ボタン。ゲームパッドのボタン2に対応 （PS3コントローラ: ○）
		B3,   ///< 物理ボタン。ゲームパッドのボタン3に対応 （PS3コントローラ: ×）
		B4,   ///< 物理ボタン。ゲームパッドのボタン4に対応 （PS3コントローラ: □）
		B5,   ///< 物理ボタン。ゲームパッドのボタン5に対応 （PS3コントローラ: L1）
		B6,   ///< 物理ボタン。ゲームパッドのボタン6に対応 （PS3コントローラ: R1）
		B7,   ///< 物理ボタン。ゲームパッドのボタン7に対応 （PS3コントローラ: L2）
		B8,   ///< 物理ボタン。ゲームパッドのボタン8に対応 （PS3コントローラ: R2）
		B9,   ///< 物理ボタン。ゲームパッドのボタン9に対応 （PS3コントローラ: L3 or SELECT）
		B10,  ///< 物理ボタン。ゲームパッドのボタン10に対応（PS3コントローラ: R3 or L3）
		B11,  ///< 物理ボタン。ゲームパッドのボタン11に対応（PS3コントローラ: SELECT or R3）
		B12,  ///< 物理ボタン。ゲームパッドのボタン12に対応（PS3コントローラ: START or START）
		B13,  ///< 物理ボタン。ゲームパッドのボタン13に対応（PS3コントローラ: PS or なし）
		B14,  ///< 物理ボタン。ゲームパッドのボタン14に対応
		B15,  ///< 物理ボタン。ゲームパッドのボタン15に対応
		B16,  ///< 物理ボタン。ゲームパッドのボタン16に対応
		MAX_BUTTONS, ///< 物理ボタンの最大数

		X_NEGATIVE, ///< 仮想ボタン。X 軸が負の値になると ON になる（PS3コントローラ: 左スティック左）
		X_POSITIVE, ///< 仮想ボタン。X 軸が正の値になると ON になる（PS3コントローラ: 左スティック右）
		Y_NEGATIVE, ///< 仮想ボタン。Y 軸が負の値になると ON になる（PS3コントローラ: 左スティック上）
		Y_POSITIVE, ///< 仮想ボタン。Y 軸が正の値になると ON になる（PS3コントローラ: 左スティック下）
		Z_NEGATIVE, ///< 仮想ボタン。Z 軸が負の値になると ON になる（PS3コントローラ: 右スティック左）
		Z_POSITIVE, ///< 仮想ボタン。Z 軸が正の値になると ON になる（PS3コントローラ: 右スティック左）
		R_NEGATIVE, ///< 仮想ボタン。R 軸が負の値になると ON になる（PS3コントローラ: 本体を奥に傾ける）
		R_POSITIVE, ///< 仮想ボタン。R 軸が正の値になると ON になる（PS3コントローラ: 本体を手前に傾ける）
		U_NEGATIVE, ///< 仮想ボタン。U 軸が負の値になると ON になる（PS3コントローラ: 本体を左に傾ける）
		U_POSITIVE, ///< 仮想ボタン。U 軸が正の値になると ON になる（PS3コントローラ: 本体を右に傾ける）
		V_NEGATIVE, ///< 仮想ボタン。V 軸が負の値になると ON になる（PS3コントローラ: 右スティック上）
		V_POSITIVE, ///< 仮想ボタン。V 軸が正の値になると ON になる（PS3コントローラ: 右スティック下）

		POV_X_NEGATIVE,  ///< 仮想ボタン。ハットスイッチの X 向きが負だった場合に ON になる（PS3コントローラ: 十字キー左）
		POV_X_POSITIVE,  ///< 仮想ボタン。ハットスイッチを X 向きが正だった場合に ON になる（PS3コントローラ: 十字キー右）
		POV_Y_NEGATIVE,  ///< 仮想ボタン。ハットスイッチを Y 向きが負だった場合に ON になる（PS3コントローラ: 十字キー上）
		POV_Y_POSITIVE,  ///< 仮想ボタン。ハットスイッチを Y 向きが正だった場合に ON になる（PS3コントローラ: 十字キー下）
		BUTTON_ENUM_MAX, ///<仮想ボタンも含めたボタンの最大数

		LEFT  = X_NEGATIVE,
		RIGHT = X_POSITIVE,
		UP    = Y_NEGATIVE,
		DOWN  = Y_POSITIVE,
	};

	/// ボタン番号が純粋なボタンを表しているかどうか
	static bool isPushButton(Button btn) { return 0 <= btn && btn < MAX_BUTTONS; }

	/// ボタン番号が軸入力を表しているかどうか
	static bool isAxisButton(Button btn) { return X_NEGATIVE <= btn && btn < BUTTON_ENUM_MAX; }

	/// ジョイスティックが利用可能かどうか調べる
	/// @param id ジョイスティックの識別番号。0 以上 #MAX_CONNECTION 未満の値を指定する
	static bool isConnected(int id);

	/// 軸が利用可能かどうかを返す
	/// @param id ジョイスティックの識別番号。0 以上 #MAX_CONNECTION 未満の値を指定する
	/// ちなみに、「軸の数を返す関数」は、ハットスイッチのを軸に含めるかどうかで混乱するのでダメ
	static bool hasAxis(int id, Axis axis);

	/// ジョイスティックで利用可能なボタン数を返す
	/// @param id ジョイスティックの識別番号。0 以上 #MAX_CONNECTION 未満の値を指定する
	static int getButtonCount(int id);

	/// 指定されたボタンの状態を返す。
	/// この関数は #setUsePovAsAxis による設定の影響を受ける。
	/// 例えば btn に X_NEGATIVE を指定したとしても、#POV_X_NEGATIVE の値を返す可能性がある
	/// @param id ジョイスティックの識別番号。0 以上 #MAX_CONNECTION 未満の値を指定する
	/// @see isButtonDownRaw
	/// @see setUsePovAsAxis
	static bool isButtonDown(int id, Button btn);

	/// 指定された軸の状態を返す。
	/// この関数は #setUsePovAsAxis による設定の影響を受ける。
	/// 例えば axis に X を指定したとしても、POV_X の値を返す可能性がある
	/// @param id ジョイスティックの識別番号。0 以上 #MAX_CONNECTION 未満の値を指定する
	/// @see getAxisRaw
	/// @see setUsePovAsAxis
	static float getAxis(int id, Axis axis);

	/// 指定されたボタンの状態を返す。
	/// この関数は #setUsePovAsAxis による設定の影響を受けない
	/// @param id ジョイスティックの識別番号。0 以上 #MAX_CONNECTION 未満の値を指定する
	static bool isButtonDownRaw(int id, Button btn);

	/// 指定された軸の状態を返す。
	/// この関数は #setUsePovAsAxis による設定の影響を受けない
	/// @param id ジョイスティックの識別番号。0 以上 #MAX_CONNECTION 未満の値を指定する
	static float getAxisRaw(int id, Axis axis);

	/// ハットスイッチの入力を X, Y 軸の入力として扱う
	///
	/// アナログスティックと十字キーの両方ついている場合、
	/// コントローラー設定によって十字キーがXY軸になったり、ハットキーになったりする。
	/// また、PSのコントローラは十字キーがハットキー固定になっている。
	/// そのような時、アナログスティックと十字キーを両方とも X, Y 軸として扱うと便利になる
	static void setUsePovAsAxis(bool value);

	/// ジョイスティックの入力状態を更新する
	static void update();

	/// ジョイスティック軸の文字列表現を返す
	/// 名前が未定義の場合は空文字列 "" を返す。（NULLは返さない）
	static const char * getAxisName(Axis axis);

	/// ジョイスティックボタンの文字列表現を返す
	/// 名前が未定義の場合は空文字列 "" を返す。（NULLは返さない）
	static const char * getButtonName(Button btn);

	/// 名前を指定してボタンを得る。大小文字は区別しない。
	static bool findButtonByName(const char *name, Button *btn);
};

}

