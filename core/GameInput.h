#pragma once
#include "system.h"
#include "KJoystick.h"
#include "KKeyboard.h"
#include "KMouse.h"
#include "Screen.h" // MouseHits
#include <unordered_set>

namespace mo {

class InputSystem;
class CButtonAssign; // internal class
class Engine;
class Path;
typedef int ButtonAssignId;

/// ボタンID
typedef int ButtonId;
static const ButtonId ButtonId_Null = 0;
static inline int  button_cmp(const ButtonId &a, const ButtonId &b) { return a - b; }
static inline bool button_equals(const ButtonId &a, const ButtonId &b) { return a == b; }
static inline bool button_isnull(const ButtonId &a) { return button_equals(a, ButtonId_Null); }

class InputCo {
public:
	InputCo();
	bool getButtonDown(ButtonId btn_id) const; ///< ボタンが押された瞬間であれば true を返す
	int getButtonState(ButtonId btn_id) const; ///< ボタンが押されていれば、そのボタンの値を返す
	void setUserInputEnabled(bool value);
	bool isUserInputEnabled() const;

	/// getButtonState や getButtonDown が返す値を上書きする
	/// イベント操作などで、プレイヤーの入力に介入したい時に使う。
	/// 例えば、プレイヤーが何も操作していないときでも setOverrite(BUTTON1, 1) としておけば、
	/// getButtonState(BUTTON1) が 1 を返すようになる
	void setOverwrite(ButtonId btn_id, int value);

	/// 入力値の介入をやめる
	void clearOverwrite(ButtonId btn_id);

	struct _PrivateBtn {
		_PrivateBtn() {
			last = 0;
			curr = 0;
			raw_value = 0;
			overwrite_value = 0;
			overwrite_enabled = false;
		}
		int last;            // 直前フレームでのボタン値
		int curr;            // このフレームでのボタン値
		int raw_value;       // InputSystem から配信されたボタン値
		int overwrite_value; // ボタンの上書き値
		bool overwrite_enabled;
	};

	std::unordered_map<ButtonId, _PrivateBtn> private_buttons_;
	bool user_input_enabled_;
	bool enabled_;
};

/// キー入力状態を管理するためのクラス
///
/// ゲームで使うキー（押された、離されたタイミングを監視したいキー）を定義する
///
/// 仮想ボタンにキーボードやゲームパッドのキーを割り当てる。
/// 仮想ボタン名はユーザーが自由につけてよい名前で、同じ名前の仮想ボタンが複数ある場合は入力がマージされる
/// 例えばボタン "shot" に対してキー "JOY_B1" と "Z" を割り当てた場合は、Zキーを押しても、パッドのボタン１を押しても同じ "shot" に対する入力と見なされる
/// 逆に、同じ仮想ボタンに対して複数の入力があった場合は、絶対値の大きい方を採用する。
///

/// ＵＩ操作用の方向キーと、プレイヤー操作用の方向キーは別々に定義することもできる
/// ＵＩ操作用の方向キーではキーリピートを有効にして操作性をあげ、プレイヤー操作用の方向キーではキーリピートを無効にしておく、など
/// 例えば以下のようにしておき、ＵＩ操作するときには getButtonDown(BTN_UI_LEFT) をチェックし、プレイヤーが動き出す瞬間などは getButtonDown(BTN_PLAYER_LEFT) をチェックする
///
/// addButton(BTN_UI_LEFT, "UI left"); // UI操作用の仮想ボタン BTN_UI_LEFT を追加
/// addButton(BTN_PLAYER_LEFT, "Player left"); // アクション操作用の仮想ボタン BTN_PLAYER_LEFT を追加
/// assignKeyboard(BTN_UI_LEFT, -1, KeyCode_Left, InputSystem::REPEAT); // BTN_UI_LEFT に対してキーを割り当てる
/// assignKeyboard(BTN_PLAYER_LEFT, -1, KeyCode_Left, 0); // BTN_PLAYER_LEFT に対してキーを割り当てる
/// 


class IInputCallback {
public:
	/// キーアサインをリセットした
	/// 現時点では、これはインスペクター上で [RESET] が押された場合のみ呼ばれる
	virtual void onInputSystemResetAssigns(InputSystem *is) {}

	/// キーアサインを追加した
	/// InputSystem::addSystemKeyboard
	/// InputSystem::assignAliasButton
	/// InputSystem::assignKeyboard
	/// InputSystem::assignKeyboardSequence
	/// InputSystem::assignJoystick
	/// InputSystem::assignJoystickSequence
	/// InputSystem::assignMouse
	virtual void onInputSystemButtonAssignAdded(InputSystem *is, ButtonAssignId assign_id) {}

	/// 既存のキーアサインを変更した
	/// InputSystem::changeKeyboardAssign
	/// InputSystem::changeJoystickAssign
	virtual void onInputSystemButtonAssignChanged(InputSystem *is, ButtonAssignId assign_id) {}

	/// キーアサインを削除した
	/// InputSystem::removeAssign
	virtual void onInputSystemButtonAssignRemove(InputSystem *is, ButtonAssignId assign_id) {}

	/// ボタンが有効化された
	/// InputSystem::setButtonEnabled
	virtual void onInputSystemButtonEnabled(InputSystem *is, ButtonId button_id) {}

	/// ボタンが無効化された
	/// InputSystem::setButtonEnabled
	virtual void onInputSystemButtonDisabled(InputSystem *is, ButtonId button_id) {}
};

class JoystickBuffer; // internal

class InputSystem: public System, public IScreenCallback {
public:
	static const int DEFAULT_KEYREPEAT_DELAY_FRAMES = 20;
	static const int DEFAULT_KEYREPEAT_INTERVAL_FRAMES = 5;

	enum Flag {
		/// ウィンドウが非アクティブな時でも入力を受け付ける。
		///
		/// たいていの場合、キーボード入力はウィンドウがアクティブな場合のみにして、
		/// ジョイスティック入力は非アクティブでも反応するようにするとうまくいく。
		BACKGROUND = 1,

		/// キーリピートを有効にする。（ジョイスティックの場合、キーリピートをエミュレートする）
		///
		/// キーリピートの待機と間隔を設定するには setKeyRepeatingInFrames を使う。
		/// システムに設定されたキーリピートの待機と間隔を取得するには Platform::getExtraInfo を使う
		REPEAT = 2,

		/// 修飾キー(Alt, Shift, Ctrl)を排他する。
		///
		/// 例えば [Enter] と [Alt + Enter] のボタンが登録されていた場合、
		/// Alt を押しながら Enter を押すと [Alt + Enter] ボタンと [Enter] ボタンの両方が反応してしまう。
		/// このとき、[Enter] ボタン側に NO_MODIFIER フラグを入れておくと、Alt キーが押されているときは [Enter] が反応しなくなる。
		NO_MODIFIER = 4,

		/// 方向キーの入力を厳格に調べる。
		/// 例えば右キー入力を受け付けるとき、デフォルト状態では斜め右上や右下でも受け付けてしまう。
		/// 右上でも右下でもなく、右を入力した場合のみ受け付けたい場合は、このフラグを付ける
		STRICT_DIRECTION = 8,
	};
	typedef int Flags;

	InputSystem();
	virtual ~InputSystem();
	void init(Engine *engine);
	void attach(EID e);
	InputCo * getComponent(EID e);
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual void onDetach(EID e) override;
	virtual bool onIsAttached(EID e) override { return _getNode(e) != NULL; }
	virtual void onAppFrameUpdate() override;
	virtual void onFrameUpdate() override;
	virtual void onInspectorGui() override;
	virtual void onUpdateGui() override;

	void clearState();
	void setCallback(IInputCallback *cb);

	/// 仮想ボタンを追加する。
	/// id 仮想ボタンの識別番号。ボタンごとに異なる番号にする必要がある。
	///    既に addButtton で追加済みの id を指定した場合は古い設定を上書きする
	/// name 仮想ボタンの名前。インスペクターなどで表示するために使う
	void addButton(ButtonId btn, const Path &name);

	/// 仮想ボタンの名前を得る
	/// ボタンが未定義の場合は空文字列を返す
	const Path & getButtonName(ButtonId btn) const;

	/// 指定された名前を持つ仮想ボタンを返す
	/// 名前を持つ仮想ボタンが複数存在する場合、最初に見つかったものを返す
	ButtonId getButtonByName(const Path &name) const;

	/// ボタンが定義されているかどうか
	bool hasButton(ButtonId btn) const;

	/// キーアサインをすべて削除する
	void clearKeyAssigns();

	/// 仮想ボタンにキーボードのキーを割り当てる
	/// button_value には仮想ボタンをチェックしたときに帰ってくる値を指定する。普通は 1.0 だが、方向キーなどに -1.0 を割り当てることができる
	/// @param btn    addButton で登録済みの仮想ボタン
	/// @param button_value 入力が成立したときの仮想ボタンの値（キーを最大まで押した時に getButton(), getButtonF() が返す値を指定する）
	/// @param key          このボタンに関連付けるキーボードのキー番号
	/// @return キーアサイン識別番号
	ButtonAssignId assignKeyboard(ButtonId btn, int button_value, KKeyboard::Key key, Flags flags);
	
	/// 仮想ボタンにキー配列（コマンド入力）を割り当てる
	/// @param btn    addButton で登録済みの仮想ボタン
	/// @param button_value 入力が成立したときの仮想ボタンの値（コマンド入力に成功した時に getButton(), getButtonF() が返す値を指定する）
	/// @param cmd          コマンド入力配列。末尾が KeyCode_Null で終わっていること
	/// @param timeout      入力制限時間（フレーム数）
	/// @return キーアサイン識別番号
	ButtonAssignId assignKeyboardSequence(ButtonId btn, int button_value, KKeyboard::Key *cmd, int timeout, Flags flags);

	/// 仮想ボタンにマウスボタンを割り当てる
	/// @param btn    addButton で登録済みの仮想ボタン
	/// @param button_value 入力が成立したときの仮想ボタンの値（キーを最大まで押した時に getButton(), getButtonF() が返す値を指定する）
	/// @param mouse_btn    このボタンに関連付けるマウスのボタン番号
	/// @param flasg        入力フラグ。MouseKey_Wheel を選んだときのみ、BACKGROUND を指定できる
	/// @return キーアサイン識別番号
	ButtonAssignId assignMouse(ButtonId btn, int button_value, KMouse::Button mouse_btn, Flags flags);

	/// 仮想ボタンにジョイスティック（ゲームパッド）のボタンを割り当てる
	/// @param btn      addButton で登録済みの仮想ボタン
	/// @param button_value   入力が成立したときの仮想ボタンの値（キーを最大まで押した時に getButton(), getButtonF() が返す値を指定する。
	/// @param joystick_btn   このボタンに割り当てるジョイスティック（ゲームパッド）のボタン番号
	/// @param joystick_index このボタンに割り当てるジョイスティック（ゲームパッド）のインデックス番号
	/// @return キーアサイン識別番号
	int assignJoystick(ButtonId btn, int button_value, KJoystick::Button joystick_btn, int joystick_index, Flags flags);

	/// 仮想ボタンにジョイスティック（ゲームパッド）のボタン配列（コマンド入力）を割り当てる
	/// 使い方は addKeyboardCommand と同じ
	/// cmd の末尾は JoyKey_Null にする
	/// @return キーアサイン識別番号
	ButtonAssignId assignJoystickSequence(ButtonId btn, int button_value, KJoystick::Button *cmd, int timeout, int joystick_index, Flags flags);

	/// システム入力用の仮想ボタンにキーボードのキーを割り当てる。
	/// addKeyboard とは異なり、デバッグ目的などでゲーム進行が停止している間でも状態が更新される
	/// @return キーアサイン識別番号
	ButtonAssignId addSystemKeyboard(ButtonId btn, KKeyboard::Key key, Flags flags);

	/// 仮想ボタンに別の仮想ボタンを割り当てる。（仮想ボタンの別名を作る行為に相当）
	/// @param btn 仮想ボタン
	/// @param other_btn 割り当てる仮想ボタン
	/// @return キーアサイン識別番号
	ButtonAssignId assignAliasButton(ButtonId btn, ButtonId other_btn);

	/// 仮想ボタンを有効、無効にする。無効化されたボタンは、押されていないようにふるまう
	void setButtonEnabled(ButtonId btn, bool value);

	/// 仮想ボタンが登録済みかつ有効ならば true を返す
	bool isButtonEnabled(ButtonId btn) const;

	/// キーアサインを削除する
	/// @param assign_id キーアサイン識別番号
	void removeAssign(ButtonAssignId assign_id);

	/// アサインされているボタンとキーボードキーの組み合わせを取得する
	/// キーアサイン識別番号が存在しないか、キーボードに関連付けられていない場合は false を返す
	/// @param assign_id キーアサイン識別番号
	bool getKeyboardAssign(ButtonAssignId assign_id, ButtonId *btn, KKeyboard::Key *keycode);

	/// アサインされているボタンとジョイスティックボタンの組み合わせを取得する
	/// キーアサイン識別番号が存在しないか、ジョイスティックに関連付けられていない場合は false を返す
	/// @param assign_id キーアサイン識別番号
	bool getJoystickAssign(ButtonAssignId assign_id, ButtonId *btn, KJoystick::Button *joybtn, int *joyindex);

	/// アサインされているボタンとマウスボタンの組み合わせを取得する
	/// キーアサイン識別番号が存在しないか、マウスボタンに関連付けられていない場合は false を返す
	/// @param assign_id キーアサイン識別番号
	bool getMouseAssign(ButtonAssignId assign_id, ButtonId *btn, KMouse::Button *mb);

	/// キーボードのキーアサインを変更する
	/// @param assign_id キーアサイン識別番号。addKeyboard などの戻り値か、findKeyboardAssign で取得できる
	/// @param new_key 新しく割り当てるキー
	void changeKeyboardAssign(ButtonAssignId assign_id, KKeyboard::Key key);

	/// ジョイスティックのキーアサインを変更する
	/// @param assign_id キーアサイン識別番号。addKeyboard などの戻り値か、findKeyboardAssign で取得できる
	/// @param new_key 新しく割り当てるキー
	void changeJoystickAssign(ButtonAssignId assign_id, KJoystick::Button key);

	/// キーアサインを一時的に有効・無効にする
	void setAssignEnabled(ButtonAssignId assign_id, bool value);

	/// キーアサインが有効かどうか
	bool isAssignEnabled(ButtonAssignId assign_id) const;

	/// 入力状態をリセットする。
	/// 入力状態がゼロになるめ、たとえボタン押しっぱなしの状態であっても getButtonDown が true になる
	void resetButtonState(ButtonId btn);
	void resetAllButtonStates();

	/// 仮想ボタンの状態をデジタル値で得る
	///
	/// 指定されたボタンが押されているかどうかを 0 か 1 で返す。
	/// ボタンが軸入力（スティックなど）に関連づけられている場合、軸の傾きを -1, 0, 1 の値で返す。
	/// 仮想ボタンに対して複数のキーが割り当てられている場合、絶対値の最も大きなものを返す
	///
	/// @param btn ボタンID。（ボタンIDは addKeyboard などで設定する）
	///
	int getState(ButtonId btn) const;

	/// 仮想ボタン状態をアナログ値で得る
	///
	/// 指定されたボタンが押されているかどうか（あるいは少しだけ押されているかどうか）を 0.0 以上 1.0 以下（未満ではない）で返す
	/// ボタンが軸入力（スティックなど）に関連づけられている場合、軸の傾きを -1.0 以上 1.0 以下（未満ではない）の値で返す
	/// 仮想ボタンに対して複数のキーが割り当てられている場合、絶対値の最も大きなものを返す
	///
	/// @param btn ボタンID。（ボタンIDは addKeyboard などで設定する）
	///
	float getStateF(ButtonId btn) const;

	/// 仮想ボタンが押された瞬間かどうかを調べる
	/// 指定されたボタンが押された瞬間ならば true を返す
	/// ボタンが軸入力（スティックなど）に関連づけられている場合、軸がニュートラル位置から傾き始めた瞬間ならば true を返す
	/// 未登録のキーに対して判定したい場合は getKeyDown を使う
	///
	/// @param btn ボタンID。（ボタンIDは addKeyboard などで設定する）
	///
	bool getButtonDown(ButtonId btn) const;
	bool getButtonDownEx(ButtonId btn, bool dont_care_button_enable) const;

	/// 仮想ボタンが離された瞬間かどうかを調べる
	/// 指定されたボタンが離された瞬間ならば true を返す
	/// ボタンが軸入力（スティックなど）に関連づけられている場合、軸がニュートラル位置に戻った瞬間ならば true を返す
	///
	/// @param btn ボタンID。（ボタンIDは addKeyboard などで設定する）
	///
	bool getButtonUp(ButtonId btn) const;

	/// キーボードキーの文字表現を返す
	Path getKeyboardName(KKeyboard::Key key) const;

	/// ゲームパッドキーの文字表現を返す
	Path getJoystickName(KJoystick::Button joy) const;

	bool getKeyboardFromName(const Path &s, KKeyboard::Key *key) const;

	bool getJoystickFromName(const Path &s, KJoystick::Button *btn) const;

	/// 現在押されているキーボードのキーを最大 size 個 keys にセットする。
	/// 実際にセットしたキーの数を返す
	int getPressedKeyboardKeys(KKeyboard::Key *keys, int size) const;

	/// ジョイスティックの番号が有効かどうか調べる
	bool isJoystickConnected(int joystick_index) const;

	/// Joystick で現在押されているボタンをすべて取得する。
	/// ボタンを順番に調べ、見つかった順に keys に追加する。size 個に達したら関数を終了する。
	/// keys に追加したボタンの数を返す
	/// ※ボタンだけを調べる。軸、方向キーは調べない
	int getPressedJoyButtons(KJoystick::Button *keys, int size, int joystick_index=0) const;

	/// マウスカーソルが移動しているなら移動量を dx, dy にセットして true を返す
	/// 移動距離が trigger_delta ピクセル以内だった場合は停止状態とみなして false を返す
	bool getMouseMoveDelta(int *dx, int *dy, int trigger_delta=1) const;

	/// マウスカーソルの位置を返す（ウィンドウクライアント座標系）
	Vec3 getMousePointInWindow() const;

	/// マウスホイールの回転値を取得する
	int getMouseWheelDelta() const;

	/// マウスボタンの状態を直接取得する
	bool getMouseDown(KMouse::Button mb) const; ///< 押された瞬間なら true
	bool getMouseUp(KMouse::Button mb) const;   ///< 離された瞬間なら true
	bool isMousePressed(KMouse::Button mb) const; ///< 押された状態なら true

	/// キーボードのキーの状態を直接取得する
	bool getKeyDown(KKeyboard::Key key) const;   ///< 押された瞬間なら true
	bool getKeyUp(KKeyboard::Key key) const;     ///< 離された瞬間なら true
	bool isKeyPressed(KKeyboard::Key key) const; ///< 押された状態なら true

	/// ゲームパッドのボタンの状態を直接取得する
	bool getJoyButtonDown(KJoystick::Button key, int joystick_index=0) const;
	bool getJoyButtonUp(KJoystick::Button key, int joystick_index=0) const;
	bool isJoyButtonPressed(KJoystick::Button key, int joystick_index=0) const;
	float isJoyButtonAnalog(KJoystick::Button key, int joystick_index=0) const;

	/// 現在の実行環境から、キーリピートの設定を取得する
	void getSystemKeyRepeatingInFrames(int *delay_frames, int *interval_frames) const;

	/// 疑似キーリピートの設定をフレーム単位で取得
	void getKeyRepeatingInFrames(int *delay, int *interval) const;

	/// 疑似キーリピートをフレーム単位で設定する
	void setKeyRepeatingInFrames(int delay, int interval);

	/// 簡易的なキーコンフィグを表示する。
	/// このメソッドは onUpdateGui から呼び出すこと
	void updateKeyConfigGui(int w);
	/// GUIで表示するためのアサイン名
	void setKeyConfigText(ButtonAssignId assign_id, const Path &text);
	const Path & getKeyConfigText(ButtonAssignId assign_id) const;

private:
	struct _Node {
		EID e;
		InputCo *co;
	};
	struct _VirtualBtn {
		ButtonId btn_id;
		Path name;
	};
	class _KBButtonEnumerator {
	public:
		int ord_;
		KKeyboard::Key result_;

		_KBButtonEnumerator() {
			ord_ = 0;
			result_ = KKeyboard::NONE;
		}
		bool found(KKeyboard::Key k) {
			if (ord_ == 0) {
				result_ = k;
			}
			ord_--;
			return ord_ >= 0;
		}
	};
	_Node * _getNode(EID e);
	int _newAssignId();
	int getButtonIndexById(ButtonId id) const;
	int getButtonIndexByName(const Path &name) const;
	int getAssignIndexById(ButtonAssignId id) const;

	_VirtualBtn * _newButton(ButtonId id);
	bool _isKeyRepeatTrigger(int frame) const;
	int _getRawState(const CButtonAssign *btn, int key, bool *key_repeated) const;
	int _getRawJoyState(const CButtonAssign *btn, int key, bool *key_repeated) const;
	int _getRawKBState(const CButtonAssign *btn, int key, bool *key_repeated) const;
	int _getRawMouseState(const CButtonAssign *btn, int key, bool *key_repeated) const;
	int _getSeqState(CButtonAssign *btn, bool *key_repeated);
	void _updateButton(CButtonAssign *btn);
	int _getButtonCount() const;
	bool _getKeyboardAssign(ButtonId button_id, _KBButtonEnumerator *cb, int max_alias_depth) const;
	bool _isButtonDown(ButtonId button_id, bool dont_case_button_enable) const;
	bool _isButtonUp(ButtonId button_id) const;

	std::unordered_map<EID , _Node> nodes_;
	std::vector<CButtonAssign *> assigns_;
	std::vector<_VirtualBtn> buttons_;
	JoystickBuffer *joy_[KJoystick::MAX_CONNECTION];
	mutable std::unordered_map<ButtonId, int> stats_;
	std::unordered_set<ButtonId> active_buttons_;
	int keyrepeat_delay_;
	int keyrepeat_interval_;
	int inspector_sort_;
	ButtonAssignId new_assign_id_;
	IInputCallback *cb_;
	Engine *engine_;
	int game_time_;
	int wheel_last_;
	int wheel_curr_;
	int mouse_last_;
	int mouse_curr_;
	Vec3 mpos_curr_;
	Vec3 mpos_last_;
	bool keys_last_[KKeyboard::KEY_ENUM_MAX];
	bool keys_curr_[KKeyboard::KEY_ENUM_MAX];
};


}
