#include "GameInput.h"
#include "Engine.h"
#include "KPlatform.h"
#include "KLog.h"
#include "Screen.h"
#include "KImgui.h"
#include "KNum.h"
#include <algorithm>

namespace mo {

#define MAX_KEY_SEQUENCE_LENGTH 8
#define INPUT_INSPECTOR_COLOR(val) ((val) ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
#define ACTIVE_INPUT_COLOR ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
#define INACTIVE_INPUT_COLOR ImVec4(1.0f, 1.0f, 1.0f, 0.25f)

class CKeyNames {
public:
	CKeyNames() {
	}
	void init() {
		kb_u8pool_.resize(KKeyboard::KEY_ENUM_MAX);
		kb_u8ptr_.resize(KKeyboard::KEY_ENUM_MAX);
		for (size_t i=0; i<KKeyboard::KEY_ENUM_MAX; i++) {
			const char *s = KKeyboard::getKeyName((KKeyboard::Key)i);
			kb_u8pool_[i] = s;
			kb_u8ptr_[i] = kb_u8pool_[i].c_str(); // utf8 の const char * を取得
		}

		js_u8pool_.resize(KJoystick::MAX_BUTTONS);
		js_u8ptr_.resize(KJoystick::MAX_BUTTONS);
		for (size_t i=0; i<KJoystick::MAX_BUTTONS; i++) {
			const char *s = KJoystick::getButtonName((KJoystick::Button)i);
			js_u8pool_[i] = s;
			js_u8ptr_[i] = js_u8pool_[i].c_str(); // utf8 の const char * を取得
		}

		mb_u8pool_.resize(KMouse::ENUM_MAX);
		mb_u8ptr_.resize(KMouse::ENUM_MAX);
		for (size_t i=0; i<KMouse::ENUM_MAX; i++) {
			const char *s = KMouse::getButtonName((KMouse::Button)i);
			mb_u8pool_[i] = s;
			mb_u8ptr_[i] = mb_u8pool_[i].c_str(); // utf8 の const char * を取得
		}
	}
	/// 名前を UTF8 な const char * 配列で表したもの
	/// ImGui で使うためにある
	std::vector<std::string> kb_u8pool_;
	std::vector<std::string> js_u8pool_;
	std::vector<std::string> mb_u8pool_;

	std::vector<const char *> kb_u8ptr_; // キーボード用。文字列の実体は kb_u8pool_ にある
	std::vector<const char *> js_u8ptr_; // ジョイスティック用。文字列の実体は js_u8pool_ にある
	std::vector<const char *> mb_u8ptr_; // マウス用。文字列の実体は mb_u8pool_ にある
};

static CKeyNames g_KeyNames;


/// 実数入力値を -1.0f, 0.0f, 1.0f のどれかに合わせる
static float __SnapF(float raw) {
	if (raw <= -0.5f) return -1.0f;
	if (raw >=  0.5f) return  1.0f;
	return 0.0f;
}
/// 実数入力値を -1, 0, 1 のどれかに合わせる
static int __SnapI(float raw) {
	if (raw <= -0.5f) return -1;
	if (raw >=  0.5f) return  1;
	return 0;
}



enum EDev {
	DEV_NULL,
	DEV_KEYBOARD,
	DEV_MOUSE,
	DEV_JOYSTICK,
};

/// ボタン入力状態を保持するためのクラス
class CButtonAssign {
public:
	CButtonAssign() {
		assign_id = 0;
		button_id = ButtonId_Null;
		alias_of = ButtonId_Null;
		device_type = DEV_NULL;
		device_index = 0;
		key = 0;
		value = 1;
		strict_direction = false;
		keyseq_timeout = 60;
		keyrepeat_enable = false;
		under_app_cycle = false;
		enable_background = false;
		assign_enabled = true;
		no_modifier = false;
		//
		resetState();
	}

	void resetState() {
		_state_curr = 0.0f; // 現フレーム開始時のキー値
		_state_last = 0.0f; // 前フレーム開始時のキー値
		_keypress_timer = 0;
		_keyrepeat_occurred = false;
		_keyseq_buf.clear();
		_keyseq_matched = false;
		_keyseq_time_elapsed = 0;
	}

	/// このボタンに割り当てられた入力デバイス名
	Path getDeviceName() const {
		switch (device_type) {
		case DEV_KEYBOARD: return "keyboard";
		case DEV_MOUSE   : return "mouse";
		case DEV_JOYSTICK: return "joystick";
		}
		return "none";
	}

	/// このボタンに割り当てられた入力デバイスのキー名
	/// seq に 0 以上の値を指定した場合、キーシーケンス上で対応するインデックスのキー名を得る
	Path getKeyName(int seq=-1) const {
		int k;
		if (seq < 0) {
			k = key;
		} else if (seq < (int)keyseq.size()) {
			k = keyseq[seq];
		} else {
			k = KKeyboard::NONE;
		}
		return getKeyNameRaw(k);
	}

	Path getKeyNameRaw(int _key) const {
		switch (device_type) {
		case DEV_NULL    : return "(none)";
		case DEV_KEYBOARD: return Path(KKeyboard::getKeyName((KKeyboard::Key)_key));
		case DEV_MOUSE   : return Path(KMouse::getButtonName((KMouse::Button)_key));
		case DEV_JOYSTICK: return Path(KJoystick::getButtonName((KJoystick::Button)_key));
		}
		return "(---)";
	}

	/// GUIでキーコンフィグを表示する時のラベル文字列
	Path text;

	/// キーシーケンスが定義されている場合、index 番目のキー値を得る
	int getSequence(int index) const {
		if (0 <= index && index < (int)keyseq.size()) {
			return keyseq[index];
		}
		return 0;
	}

	/// このボタンが連続入力によって成り立つ（コマンド入力）ならば true を返す
	bool isSequence() const {
		return !keyseq.empty();
	}

	/// このボタンがエイリアスならば true を返す
	bool isAlias() const {
		return !mo::button_isnull(alias_of);
	}

	/// このキーアサインの識別ID
	ButtonAssignId assign_id;

	/// このボタンのID
	ButtonId button_id;

	/// このボタンに関連付けられているデバイスの種類
	EDev device_type;

	/// このボタンが別のボタンとリンクしている場合のリンク先ボタンＩＤ
	ButtonId alias_of;

	/// このボタンに関連付けられているデバイスが複数ある場合のインデックス番号
	/// この番号はゲームパッドの識別にのみ使う
	int device_index;

	/// このボタンに関連付けられているキーの識別子
	/// このボタンに関連付けられているデバイス (device_type) によって指定する値が異なる。
	/// キーボードならば enum KKeyboard::Key の値を指定する。
	/// マウスならば enum KMouse::Button の値を指定する。
	/// ジョイスティックならば enum KJoystick::Button の値を指定する。
	int key;

	/// ボタンの値。
	/// ボタンが押されている時に state_*** がとるべき値を指定する。
	/// 普通は 1 を指定するが、方向キーなど左右や上下の区別があるものでは -1 を指定する場合もある
	int value;

	/// キーシーケンス（複数キーの連続入力）で入力するべきキーとその順番。
	/// キーを一度離す「ニュートラル」入力も含めて指定する（ニュートラルの場合は 0 を指定する）
	/// また、「最初のキーを押した瞬間」を検出するため、シーケンスの最初は Null で始めておく。
	/// 例）コマンド「左上右」---> {Null, Left, Top, Right}
	/// 例）コマンド「左左」 ---> {Null, Left, Null, Left}
	std::vector<int> keyseq;

	int keyseq_timeout;

	/// 修飾キーが押されているとき、このキー入力を無視するかどうか
	/// true の場合、例えば ALT キーが押されている間は、このキーが反応しなくなる、
	/// 例えば [Enter] と [Alt + Enter] が両方登録されているとき、
	/// [Alt + Enter] を押しても [Enter] が反応しないようにしたい時に使う
	bool no_modifier;

	/// キーボードのキーを押しっぱなしにしたときのキーリピートを受け付けるかどうか
	/// true の場合、キーリピートが発生するたびに InputSystem::getButtonDown() が true を返す
	/// false の場合、最初に押された時にだけ InputSystem::getButtonDown() が true を返す
	bool keyrepeat_enable;

	/// 方向キーが関連付けられている場合、入力方向を厳格にチェックするかどうか
	/// 例えば右キーの入力を調べるとき、右上や右下の入力を排除するなら true、
	/// 右上や右下が入力された場合でも右が入力されたとみなすなら false に設定する
	bool strict_direction;

	/// アプリケーションの更新サイクルに従うかどうか。
	/// デフォルト設定は false で、入力状態はゲームフレーム単位で更新される。
	/// デバッグ用途などでゲームを一時停止した場合は入力も更新されなくなるため、１フレーム単位での正確な入力をテストすることができる。
	/// 例えば全く同じフレームでジャンプボタンと攻撃ボタンを同時に押した場合の挙動をテストするなら、
	/// ゲームを一時停止し、ジャンプボタンと攻撃ボタンを押しっぱなしにした状態で一時停止を解除する。
	/// この値を true に設定すると、アプリケーションのフレーム単位で更新される。
	/// アプリケーションフレームは常に更新され続け、ゲームを一時停止している間でも入力を検知することができる
	bool under_app_cycle;

	/// ゲームウィンドウが非アクティブな場合でも入力を受け付けるかどうか。
	/// キーボードやマウスはウィンドウ非アクティブ状態で無効にし、
	/// ジョイスティックの入力だけを非アクティブでも有効にするとデバッグしやすくなる
	bool enable_background;

	/// キーアサイン単位での有効・無効フラグ
	bool assign_enabled;

	///---------------------------------------
	/// 実行時に変化するフィールド
	/// 設定用ではなく、状態記録、判定用
	///---------------------------------------

	/// キーシーケンスの入力履歴
	struct KEYBUF {
		int time;
		int key;
	};
	std::vector<KEYBUF> _keyseq_buf;

	/// キーシーケンスが成立状態にあるかどうか
	bool _keyseq_matched;

	/// 最後にキーシーケンスが成立したときの所要時間
	int _keyseq_time_elapsed;

	/// ボタンが押されてからの経過時間(F)
	int _keypress_timer;

	/// 現在のボタン値。
	/// ゲームから入力の問い合わせがあった場合にはこの値が返される。
	/// キーが押されていなければ 0.0f になる。最大まで押されていれば value と同じ値になる。
	/// アナログ入力に対応している場合は 0 と value の間を補間した値になる。
	float _state_curr;

	/// 前フレームでのボタン値
	float _state_last;

	/// キーリピートが発生したときのフラグ
	bool _keyrepeat_occurred;
};


#pragma region InputCo
InputCo::InputCo() {
	enabled_ = true;
	user_input_enabled_ = true;
}

/// ボタンが押されていれば true を返す
int InputCo::getButtonState(ButtonId btn_id) const {
	if (! enabled_) return 0;

	auto it = private_buttons_.find(btn_id);
	if (it == private_buttons_.end()) return 0;

	const _PrivateBtn &btn = it->second;
	return btn.curr;
}

/// ボタンが押された瞬間ならば true を返す
bool InputCo::getButtonDown(ButtonId btn_id) const {
	if (! enabled_) return false;

	auto it = private_buttons_.find(btn_id);
	if (it == private_buttons_.end()) return false;

	const _PrivateBtn &btn = it->second;
	return btn.last == 0 && btn.curr != 0;
}

/// ユーザーからの入力を許可するかどうか
/// false をセットした場合、InputSystem のボタン状態を無視し、ボタンが押されていないものとして扱う。
/// （ただし setOverwrite による上書きは機能し続ける）
void InputCo::setUserInputEnabled(bool value) {
	user_input_enabled_ = value;
}
bool InputCo::isUserInputEnabled() const {
	return user_input_enabled_;
}

/// ボタン状態を上書きする。実際の入力とは無関係に getButtonState(btn) が常に value を返すようになる
/// ここでの変更は getButtonDown にも影響を与える
void InputCo::setOverwrite(ButtonId btn_id, int value) {
	auto it = private_buttons_.find(btn_id);
	if (it == private_buttons_.end()) return;

	_PrivateBtn &btn = it->second;
	btn.overwrite_value = value;
	btn.overwrite_enabled = true;
}

/// ボタンの上書き状態を解除する。これ以降 getButtonState は実際の入力を返す
void InputCo::clearOverwrite(ButtonId btn_id) {
	auto it = private_buttons_.find(btn_id);
	if (it == private_buttons_.end()) return;

	_PrivateBtn &btn = it->second;
	btn.overwrite_value = 0;
	btn.overwrite_enabled = false;
}
#pragma endregion


#define MASK_DOWN 0x0F
#define MASK_UP   0xF0
#define ON_DOWN   0x01
#define NOT_DOWN  0x02
#define ON_UP     0x10
#define NOT_UP    0x20

class _BtnSortNamePred {
public:
	bool operator()(CButtonAssign *b1, CButtonAssign *b2) const {
		return button_cmp(b1->button_id, b2->button_id) < 0;
	}
};
class _BtnSortKeyPred {
public:
	bool operator()(CButtonAssign *b1, CButtonAssign *b2) const {
		int cmp1 = b1->getDeviceName().compare(b2->getDeviceName());
		if (cmp1 == 0) {
			int cmp2 = b1->getKeyName().compare(b2->getKeyName());
			return cmp2 < 0;
		} else {
			return cmp1 < 0;
		}
	}
};

class JoystickBuffer {
public:
	struct _STATE {
		_STATE() {
			memset(axis, 0, sizeof(axis));
			memset(buttons, 0, sizeof(buttons));
		}
		float axis[KJoystick::MAX_AXIS];
		bool buttons[KJoystick::MAX_BUTTONS];
	};
	_STATE curr_;
	_STATE prev_;

	void clear() {
		prev_ = _STATE();
		curr_ = _STATE();
	}

	void tick(int id) {
		prev_ = curr_;
		curr_ = _STATE();

		if (KJoystick::isConnected(id)) {
			for (int i=0; i<KJoystick::MAX_AXIS; i++) {
				curr_.axis[i] = KJoystick::getAxis(id, (KJoystick::Axis)i);
			}
			for (int i=0; i<KJoystick::BUTTON_ENUM_MAX; i++) {
				curr_.buttons[i] = KJoystick::isButtonDown(id, (KJoystick::Button)i);
			}
		}
	}
	float getAxisFloat(KJoystick::Axis axis) const {
		return curr_.axis[axis];
	}
	bool isButtonDown(KJoystick::Button btn) const {
		bool s0 = prev_.buttons[(int)btn];
		bool s1 = curr_.buttons[(int)btn];
		return (!s0 && s1); // 前回OFFで、今回ONになっているなら、今押された
	}
	bool isButtonUp(KJoystick::Button btn) const {
		bool s0 = prev_.buttons[(int)btn];
		bool s1 = curr_.buttons[(int)btn];
		return (s0 && !s1); // 前回ONで、今回OFFになっているなら、今離された
	}
	bool isButtonDownHold(KJoystick::Button btn) const {
		return curr_.buttons[(int)btn]; // 現在押されているかどうかだけを見る
	}
	bool getButtonValue(KJoystick::Button btn, bool strict_direction) const {
		if (strict_direction) {
			// 方向厳密の場合は、X, Y 軸の入力に限り、他の軸も一緒に確認する
			switch (btn) {
			case KJoystick::X_NEGATIVE:
			case KJoystick::X_POSITIVE:
				// X 軸の入力時に、Y 軸の入力があったらダメ
				if (curr_.axis[KJoystick::Y] != 0.0f) {
					return false;
				}
				break;

			case KJoystick::Y_NEGATIVE:
			case KJoystick::Y_POSITIVE:
				// Y 軸の入力時に、X 軸の入力があったらダメ
				if (curr_.axis[KJoystick::X] != 0.0f) {
					return false;
				}
				break;

			case KJoystick::POV_X_NEGATIVE:
			case KJoystick::POV_X_POSITIVE:
				// POV_X の入力時に、POV_Y の入力があったらダメ
				if (curr_.axis[KJoystick::POV_Y] != 0.0f) {
					return false;
				}
				break;

			case KJoystick::POV_Y_NEGATIVE:
			case KJoystick::POV_Y_POSITIVE:
				// POV_Y の入力時に、POV_X の入力があったらダメ
				if (curr_.axis[KJoystick::POV_X] != 0.0f) {
					return false;
				}
				break;
			}
			// ここまでで、ダメなパターンは弾いた。
			// これ以降は普通の判定する
		}

		return curr_.buttons[btn];
	}
};



InputSystem::InputSystem() {
	new_assign_id_ = 0;
	inspector_sort_ = 0;
	keyrepeat_delay_ = DEFAULT_KEYREPEAT_DELAY_FRAMES;
	keyrepeat_interval_ = DEFAULT_KEYREPEAT_INTERVAL_FRAMES;
	cb_ = NULL;
	wheel_last_ = 0;
	wheel_curr_ = 0;
	mouse_last_ = 0;
	mouse_curr_ = 0;
	memset(keys_last_, 0, sizeof(keys_last_));
	memset(keys_curr_, 0, sizeof(keys_curr_));
	memset(joy_, 0, sizeof(joy_));
	game_time_ = 0;
	engine_ = NULL;
}
InputSystem::~InputSystem() {
	onEnd();
}
void InputSystem::init(Engine *engine) {
	Log_trace;
	Log_assert(engine);
	engine->addSystem(this);
	engine->addScreenCallback(this);
	engine_ = engine;
}
void InputSystem::onStart() {
	for (int i=0; i<KJoystick::MAX_CONNECTION; i++) {
		Log_assert(joy_[i] == NULL);
		joy_[i] = new JoystickBuffer();
	}
	// キーボードのキーリピート遅延と間隔を取得し、
	// ユーザーが普段使っている設定となるべく同じ感覚でキーリピートが効くようにする
	getSystemKeyRepeatingInFrames(&keyrepeat_delay_, &keyrepeat_interval_);
}
void InputSystem::onEnd() {
	clearKeyAssigns();
	for (int i=0; i<KJoystick::MAX_CONNECTION; i++) {
		if (joy_[i]) {
			delete joy_[i];
			joy_[i] = NULL;
		}
	}
	cb_ = NULL;
}
void InputSystem::clearKeyAssigns() {
	for (size_t i = 0; i < assigns_.size(); i++) {
		delete assigns_[i];
	}
	assigns_.clear();
}
void InputSystem::setCallback(IInputCallback *cb) {
	cb_ = cb;
}
int InputSystem::_newAssignId() {
	return ++new_assign_id_;
}
InputSystem::_Node * InputSystem::_getNode(EID e) {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		return &it->second;
	}
	return NULL;
}
void InputSystem::attach(EID e) {
	if (_getNode(e)) return;
	_Node node;
	node.e = e;
	node.co = new InputCo();
	nodes_[e] = node;
}
void InputSystem::onDetach(EID e) {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		_Node &node = it->second;
		delete node.co;
		nodes_.erase(it);
	}
}
InputCo * InputSystem::getComponent(EID e) {
	_Node *node = _getNode(e);
	return node ? node->co : NULL;
}
bool InputSystem::onAttach(EID e, const char *type) {
	if (type==NULL || strcmp(type, "Input") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void InputSystem::clearState() {
	wheel_last_ = wheel_curr_ = KMouse::getWheel(); // 無回転状態にする。積算値なので 0 にしてはダメ。
	mouse_last_ = mouse_curr_ = 0;
	memset(keys_last_, 0, sizeof(keys_last_));
	memset(keys_curr_, 0, sizeof(keys_curr_));
	stats_.clear();
	for (int i=0; i<KJoystick::MAX_CONNECTION; i++) {
		joy_[i]->clear();
	}
}
void InputSystem::onAppFrameUpdate() {

	bool game_focused = false;
	if (engine_->getStatus(Engine::ST_WINDOW_FOCUSED)) {
		if (ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused()) {
			// ImGui 上にフォーカスがある場合は、
			// ゲーム側で入力を検知しないようにする
		} else {
			game_focused = true;
		}
	}

	wheel_last_ = wheel_curr_;
	wheel_curr_ = KMouse::getWheel();
	mouse_last_ = mouse_curr_;

	if (game_focused) {
		int mouse_buttons[] = {
			KMouse::isButtonDown(KMouse::LEFT  ) ? 1 : 0,
			KMouse::isButtonDown(KMouse::RIGHT ) ? 1 : 0,
			KMouse::isButtonDown(KMouse::MIDDLE) ? 1 : 0,
		};
		mouse_curr_ = 
			mouse_buttons[0] * 1 + 
			mouse_buttons[1] * 2 +
			mouse_buttons[2] * 4;
	} else {
		mouse_curr_ = 0;
	}
	mpos_last_ = mpos_curr_;
	mpos_curr_ = getMousePointInWindow();
	memcpy(keys_last_, keys_curr_, sizeof(keys_last_));
	memset(keys_curr_, 0, sizeof(keys_curr_));
	if (game_focused) {
		for (int i=0; i<KKeyboard::KEY_ENUM_MAX; i++) {
			if (KKeyboard::isKeyDown((KKeyboard::Key)i)) {
				keys_curr_[i] = true;
			}
		}
	}
	for (int i=0; i<KJoystick::MAX_CONNECTION; i++) {
		joy_[i]->tick(i);
	}
	//
	for (size_t i=0; i<assigns_.size(); i++) {
		CButtonAssign *asgn = assigns_[i];
		if (asgn->under_app_cycle) {
			_updateButton(asgn);
		}
	}
}
void InputSystem::onFrameUpdate() {
	stats_.clear();
	game_time_++;

	// システム全体の入力状態を更新
	for (size_t i=0; i<assigns_.size(); i++) {
		CButtonAssign *asgn = assigns_[i];
		if (! asgn->under_app_cycle) {
			_updateButton(asgn);
		}
	}

	// コンポーネントごとの入力状態を更新
	for (auto eit=nodes_.begin(); eit!=nodes_.end(); ++eit) {
		InputCo *input_co = eit->second.co;

		// コンポーネント側の仮想キーテーブルと、システム側の仮想キーテーブルを一致させる
		if (input_co->private_buttons_.size() != buttons_.size()) {
			// 仮想キーが追加されている（または削除）
			// コンポーネント側のキーテーブルを再構築する
			input_co->private_buttons_.clear();
			for (auto bit=buttons_.begin(); bit!=buttons_.end(); ++bit) {
				input_co->private_buttons_[bit->btn_id] = InputCo::_PrivateBtn();
			}
		}

		// 入力更新
		for (auto bit=input_co->private_buttons_.begin(); bit!=input_co->private_buttons_.end(); ++bit) {
			ButtonId btn_id = bit->first;
			InputCo::_PrivateBtn &btn = bit->second;
			btn.last = btn.curr;
			if (input_co->user_input_enabled_) {
				// ユーザーからの入力を受ける
				// ※ここでは、直接 CButtonAssign::state_curr を参照しないこと。
				//   ひとつのボタンに複数の入力が関連付けられている場合に対処できない
				btn.raw_value = getState(btn_id); 
			} else {
				// ユーザーからの入力を受け付けない
				btn.raw_value = 0;
			}
			// 実際にボタン値として使う値を決める
			if (btn.overwrite_enabled) {
				btn.curr = btn.overwrite_value;
			} else {
				btn.curr = btn.raw_value;
			}
		}
	}
}
void InputSystem::onUpdateGui() {
#if 0
	ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
	if (ImGui::Begin("Input")) {
		updateKeyConfigGui(120);
		ImGui::End();
	}
#endif
}

static bool kb_getter(void* data, int idx, const char** out_text) {
	if (0 <= idx && idx < (int)g_KeyNames.kb_u8ptr_.size()) {
		*out_text = g_KeyNames.kb_u8ptr_[idx];
		return true;
	}
	return false;
}
static bool js_getter(void* data, int idx, const char** out_text) {
	if (0 <= idx && idx < (int)g_KeyNames.js_u8ptr_.size()) {
		*out_text = g_KeyNames.js_u8ptr_[idx];
		return true;
	}
	return false;
}
void InputSystem::setKeyConfigText(ButtonAssignId assign_id, const Path &text) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		assigns_[index]->text = text;
	}
}
const Path & InputSystem::getKeyConfigText(ButtonAssignId assign_id) const {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		const CButtonAssign *asgn = assigns_[index];
		// キーアサイン用のテキストが定義されていれば、それを使う
		if (!asgn->text.empty()) {
			return asgn->text;
		}

		// 仮想ボタン名を使う
		return getButtonName(asgn->button_id);
	}
	return Path::Empty;
}

static bool _IsArrowKey(KKeyboard::Key key) {
	return key==KKeyboard::LEFT || key==KKeyboard::RIGHT || key==KKeyboard::UP || key==KKeyboard::DOWN;
}
void InputSystem::updateKeyConfigGui(int col_w) {
#ifndef NO_IMGUI
	const float COL_W = (float)col_w;
	std::vector<CButtonAssign *> kb;
	std::vector<CButtonAssign *> js;
	kb.reserve(assigns_.size());
	js.reserve(assigns_.size());
	for (size_t i=0; i<assigns_.size(); i++) {
		CButtonAssign *asgn = assigns_[i];
		if (asgn->isSequence()) continue; // シーケンスキーはコンフィグ不可
		if (asgn->isAlias()) continue; // エイリアスは単なる別名なのでコンフィグ不可

		// 方向キー、軸に関連付けられたものはコンフィグ不可
		if (asgn->device_type == DEV_KEYBOARD) {
			if (_IsArrowKey((KKeyboard::Key)asgn->key)) {
				continue;
			}
			kb.push_back(asgn);
		}
		if (asgn->device_type == DEV_JOYSTICK) {
			if (KJoystick::isAxisButton((KJoystick::Button)asgn->key)) {
				continue;
			}
			js.push_back(asgn);
		}
	}
	std::sort(kb.begin(), kb.end(), _BtnSortNamePred());
	std::sort(js.begin(), js.end(), _BtnSortNamePred());

	ImGui::Separator();
	if (1) {
		ImGui::Dummy(ImVec2(4.0f, 4.0f));
		ImGui::Text("KKeyboard");
		ImGui::PushID("KKeyboard");
		ImGui::Indent();
		for (size_t i=0; i<kb.size(); i++) {
			auto asgn = kb[i];
		//	Path btnname = getButtonName(asgn->button_id);
			Path btnname = getKeyConfigText(asgn->assign_id);
			Path keyname = asgn->getKeyName();
			ImGui::Text(btnname.u8());
			ImGui::SameLine();
			ImGui::SetCursorPosX(COL_W);

			ImGui::PushID(ImGui_ID(i));
			ImGui::Combo("", &asgn->key, kb_getter, this, KKeyboard::KEY_ENUM_MAX, 20);
			ImGui::PopID();
		}
		ImGui::Unindent();
		ImGui::PopID();
	}

	ImGui::Dummy(ImVec2(16.0f, 16.0f));
	ImGui::Separator();
	if (1) {
		ImGui::Dummy(ImVec2(4.0f, 4.0f));
		ImGui::Text("GamePad");
		ImGui::PushID("GamePad");
		ImGui::Indent();
		for (size_t i=0; i<js.size(); i++) {
			auto asgn = js[i];
		//	Path btnname = getButtonName(asgn->button_id);
			Path btnname = getKeyConfigText(asgn->assign_id);
			Path keyname = asgn->getKeyName();
			ImGui::Text(btnname.u8());
			ImGui::SameLine();
			ImGui::SetCursorPosX(COL_W);
	
			ImGui::PushID(ImGui_ID(i));
			ImGui::Combo("", &asgn->key, js_getter, this, KJoystick::MAX_BUTTONS, 20);
			ImGui::PopID();
		}
		ImGui::Unindent();
		ImGui::PopID();
	}

	ImGui::Dummy(ImVec2(16.0f, 16.0f));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(4.0f, 4.0f));
	if (ImGui::Button("OK")) {
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		if (cb_) cb_->onInputSystemResetAssigns(this);
	}
#endif // !NO_IMGUI
}
void InputSystem::onInspectorGui() {
#ifndef NO_IMGUI
	if (1) {
		Vec3 mpos = getMousePointInWindow();
		ImGui::Text("Mouse in client(%d, %d)", (int)mpos.x, (int)mpos.y);
		ImGui::Text("Wheel pos %d", wheel_curr_);
	}
	if (ImGui::TreeNode("Raw State")) {
		for (int id=0; id<KJoystick::MAX_CONNECTION; id++) {
			if (ImGui::TreeNode(ImGui_ID(id), "Joystick %d", id)) {
				ImGui::PushID(id);
				ImGui::Text("ID: %d", id);
				ImGui::Text("Connected: %s", KJoystick::isConnected(id) ? "Yes" : "No");
				for (int i=0; i<KJoystick::MAX_AXIS; i++) {
					KJoystick::Axis a = (KJoystick::Axis)i;
					if (KJoystick::hasAxis(id, a)) {
						ImGui::Text("Axis(%s): %s",
							KJoystick::getAxisName(a),
							KJoystick::getAxis(id, a)
						);
					} else {
						ImGui::Text("Axis(%s): N/A", 
							KJoystick::getAxisName(a)
						);
					}
				}
				ImGui::Text("ButtonCount: %d", KJoystick::getButtonCount(id));
				for (int i=0; i<KJoystick::getButtonCount(id); i++) {
					KJoystick::Button b = (KJoystick::Button)i;
					ImGui::Text("Button(%s): %s", 
						KJoystick::getButtonName(b),
						KJoystick::isButtonDownRaw(id, b) ? "On" : "Off"
					);
				}
				ImGui::PopID();
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Buttons")) {
		if (ImGui::Button("No sort")) {
			inspector_sort_ = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("By Name")) {
			inspector_sort_ = 1;
		}
		ImGui::SameLine();
		if (ImGui::Button("By Key")) {
			inspector_sort_ = 2;
		}
		std::vector<CButtonAssign *> list(_getButtonCount());
		for (size_t i=0; i<assigns_.size(); i++) {
			list[i] = assigns_[i];
		}
		switch (inspector_sort_) {
		case 0:
			for (size_t i=0; i<list.size(); i++) {
				CButtonAssign *asgn = list[i];
				Path btnname = getButtonName(asgn->button_id);
				Path devname = asgn->getDeviceName();
				Path keyname = asgn->getKeyName();
				if (asgn->isSequence()) {
					keyname = "(seq)";
				}
				if (asgn->isAlias()) {
					float val = getStateF(asgn->button_id);// エイリアスの場合は直接 asgn->stat_curr 参照しない
					ImVec4 color = INPUT_INSPECTOR_COLOR(val);
					ImGui::TextColored(color, "[%s] (alias: %s): %g", btnname.u8(), getButtonName(asgn->alias_of).u8(), val);
				} else {
					float val = asgn->_state_curr;
					ImVec4 color = INPUT_INSPECTOR_COLOR(val);
					ImGui::TextColored(color, "[%s] %s/%s: %g", btnname.u8(), devname.u8(), keyname.u8(), val);
				}
			}
			break;
		case 1: 
			std::sort(list.begin(), list.end(), _BtnSortNamePred());
			for (size_t i=0; i<list.size(); i++) {
				CButtonAssign *asgn = list[i];
				Path btnname = getButtonName(asgn->button_id);
				Path devname = asgn->getDeviceName();
				Path keyname = asgn->getKeyName();
				if (asgn->isSequence()) {
					keyname = "(seq)";
				}
				if (asgn->isAlias()) {
					float val = getStateF(asgn->button_id);// エイリアスの場合は直接 asgn->stat_curr 参照しない
					ImVec4 color = INPUT_INSPECTOR_COLOR(val);
					ImGui::TextColored(color, "[%s] (alias: %s): %g", btnname.u8(), getButtonName(asgn->alias_of).u8(), val);
				} else {
					float val = asgn->_state_curr;
					ImVec4 color = INPUT_INSPECTOR_COLOR(val);
					ImGui::TextColored(color, "[%s] %s/%s: %g", btnname.u8(), devname.u8(), keyname.u8(), val);
				}
			}
			break;
		
		case 2:
			std::sort(list.begin(), list.end(), _BtnSortKeyPred());
			for (size_t i=0; i<list.size(); i++) {
				auto asgn = list[i];
				Path btnname = getButtonName(asgn->button_id);
				Path devname = asgn->getDeviceName();
				Path keyname = asgn->getKeyName();
				if (asgn->isSequence()) {
					keyname = "(seq)";
				}
				if (asgn->isAlias()) {
					float val = getStateF(asgn->button_id);// エイリアスの場合は直接 asgn->stat_curr 参照しない
					ImVec4 color = INPUT_INSPECTOR_COLOR(val);
					ImGui::TextColored(color, "@alias [%s] --> %s: %g", btnname.u8(), getButtonName(asgn->alias_of).u8(), val);
				} else {
					float val = asgn->_state_curr;
					ImVec4 color = INPUT_INSPECTOR_COLOR(val);
					ImGui::TextColored(color, "%s/%s [%s]: %g", devname.u8(), keyname.u8(), btnname.u8(), val);
				}
			}
			break;
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Sequences")) {
		for (size_t i=0; i<assigns_.size(); i++) {
			const CButtonAssign *asgn = assigns_[i];
			if (! asgn->keyseq.empty()) {
				ImGui::PushID(ImGui_ID(i));
				ImGui::Text(getButtonName(asgn->button_id).u8());
				ImGui::Indent();
				Path devname = asgn->getDeviceName();
				if (asgn->_keyseq_time_elapsed > 0) {
					ImGui::Text("%s (time: %d)", devname.u8(), asgn->_keyseq_time_elapsed);
				} else {
					ImGui::Text(devname.u8());
				}
				for (size_t k=0; k<asgn->keyseq.size(); k++) {
					Path keyname = asgn->getKeyName(k);
					if (k < asgn->_keyseq_buf.size()) {
						Path bufname = asgn->getKeyNameRaw(asgn->_keyseq_buf[k].key);
						if (asgn->_keyseq_matched) {
							ImGui::PushStyleColor(ImGuiCol_Text, INPUT_INSPECTOR_COLOR(1));
						} else {
							ImGui::PushStyleColor(ImGuiCol_Text, ACTIVE_INPUT_COLOR);
						}
						ImGui::Text("[%d] %s | %s", k, keyname.u8(), bufname.u8());
						ImGui::PopStyleColor();
					} else {
						ImGui::PushStyleColor(ImGuiCol_Text, INACTIVE_INPUT_COLOR);
						ImGui::Text("[%d] %s | ---", k, keyname.u8());
						ImGui::PopStyleColor();
					}
				}
				ImGui::Unindent();
				ImGui::PopID();
			}
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
void InputSystem::addButton(ButtonId id, const Path &name) {
	_VirtualBtn *btn = _newButton(id);
	Log_assert(btn);
	if (name.empty()) {
		btn->name = Path::fromFormat("Button%d", buttons_.size());
	} else {
		btn->name = name;
	}
}
const Path & InputSystem::getButtonName(ButtonId id) const {
	int index = getButtonIndexById(id);
	if (index >= 0) {
		return buttons_[index].name;
	} else {
		return Path::Empty;
	}
}
ButtonId InputSystem::getButtonByName(const Path &name) const {
	int index = getButtonIndexByName(name);
	if (index >= 0) {
		return buttons_[index].btn_id;
	} else {
		return ButtonId_Null;
	}
}
bool InputSystem::hasButton(ButtonId id) const {
	return getButtonIndexById(id) >= 0;
}
void InputSystem::setAssignEnabled(ButtonAssignId assign_id, bool value) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		assigns_[index]->assign_enabled = value;
	}
}
bool InputSystem::isAssignEnabled(ButtonAssignId assign_id) const {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		return assigns_[index]->assign_enabled;
	}
	return false;
}
void InputSystem::setButtonEnabled(ButtonId button_id, bool value) {
	if (isButtonEnabled(button_id) != value) {
		if (value) {
			active_buttons_.insert(button_id);
			if (cb_) cb_->onInputSystemButtonEnabled(this, button_id);
		} else {
			active_buttons_.erase(button_id);
			if (cb_) cb_->onInputSystemButtonDisabled(this, button_id);
		}
	}
}
bool InputSystem::isButtonEnabled(ButtonId button_id) const {
	return active_buttons_.find(button_id) != active_buttons_.end();
}
ButtonAssignId InputSystem::addSystemKeyboard(ButtonId button_id, KKeyboard::Key key, Flags flags) {
	if (! hasButton(button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: undefined button id (%d)", button_id);
		return (ButtonAssignId)(-1);
	}
	CButtonAssign *asgn = new CButtonAssign();
	asgn->assign_id = _newAssignId();
	asgn->button_id = button_id;
	asgn->device_type = DEV_KEYBOARD;
	asgn->device_index = 0;
	asgn->key = key;
	asgn->value = 1;
	asgn->under_app_cycle = true;
	asgn->strict_direction = (flags & STRICT_DIRECTION) != 0;
	asgn->keyrepeat_enable = (flags & REPEAT) != 0;
	asgn->enable_background = (flags & BACKGROUND) != 0;
	asgn->no_modifier = (flags & NO_MODIFIER) != 0;
	asgn->assign_enabled = true;
	assigns_.push_back(asgn);
	active_buttons_.insert(button_id);
	if (cb_) cb_->onInputSystemButtonAssignAdded(this, asgn->assign_id);
	return asgn->assign_id;
}
ButtonAssignId InputSystem::assignAliasButton(ButtonId button_id, ButtonId other_button_id) {
	if (! hasButton(button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: undefined button id (%d)", button_id);
		return (ButtonAssignId)(-1);
	}
	if (! hasButton(other_button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: undefined button id (%d)", other_button_id);
		return (ButtonAssignId)(-1);
	}
	CButtonAssign *asgn = new CButtonAssign();
	asgn->assign_id = _newAssignId();
	asgn->button_id = button_id;
	asgn->alias_of = other_button_id;
	asgn->assign_enabled = true;
	assigns_.push_back(asgn);
	active_buttons_.insert(button_id);
	if (cb_) cb_->onInputSystemButtonAssignAdded(this, asgn->assign_id);
	return asgn->assign_id;
}
ButtonAssignId InputSystem::assignKeyboard(ButtonId button_id, int button_value, KKeyboard::Key key, Flags flags) {
	if (! hasButton(button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: undefined button id (%d)", button_id);
		return (ButtonAssignId)(-1);
	}
	if (button_value == 0) {
		Log_warningf("E_KEYBOARD_ASSIGN: Btn=%d Val=%d KeyCode=%d", button_id, button_value, key);
	}
	if (key == KKeyboard::NONE) {
		Log_warningf("E_KEYBOARD_ASSIGN: Btn=%d Val=%d KeyCode=%d", button_id, button_value, key);
	}
	CButtonAssign *asgn = new CButtonAssign();
	asgn->assign_id = _newAssignId();
	asgn->button_id = button_id;
	asgn->device_type = DEV_KEYBOARD;
	asgn->key = key;
	asgn->value = button_value;
	asgn->strict_direction = (flags & STRICT_DIRECTION) != 0;
	asgn->keyrepeat_enable = (flags & REPEAT) != 0;
	asgn->enable_background = (flags & BACKGROUND) != 0;
	asgn->no_modifier = (flags & NO_MODIFIER) != 0;
	asgn->assign_enabled = true;
	assigns_.push_back(asgn);
	active_buttons_.insert(button_id);
	if (cb_) cb_->onInputSystemButtonAssignAdded(this, asgn->assign_id);
	return asgn->assign_id;
}
ButtonAssignId InputSystem::assignKeyboardSequence(ButtonId button_id, int button_value, KKeyboard::Key *cmd, int timeout, Flags flags) {
	if (! hasButton(button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: %d", button_id);
		return (ButtonAssignId)(-1);
	}
	if (button_value == 0) {
		Log_warningf("E_KEYBOARD_ASSIGN: Btn=%d Val=%d Seq=(%p)", button_id, button_value, (void*)cmd);
	}
	if (cmd == NULL) {
		Log_warningf("E_KEYBOARD_ASSIGN: Btn=%d Val=%d Seq=(%p)", button_id, button_value, (void*)cmd);
	}
	CButtonAssign *asgn = new CButtonAssign();
	asgn->assign_id = _newAssignId();
	asgn->button_id = button_id;
	asgn->device_type = DEV_KEYBOARD;
	for (int i=0; cmd[i]!=KKeyboard::NONE; i++) {
		if (i>0) asgn->keyseq.push_back(0); // Neutral
		asgn->keyseq.push_back(cmd[i]);
	}
	asgn->keyseq_timeout = timeout;
	asgn->value = button_value;
	asgn->strict_direction = (flags & STRICT_DIRECTION) != 0;
	asgn->keyrepeat_enable = (flags & REPEAT) != 0;
	asgn->enable_background = (flags & BACKGROUND) != 0;
	asgn->no_modifier = (flags & NO_MODIFIER) != 0;
	asgn->assign_enabled = true;
	assigns_.push_back(asgn);
	active_buttons_.insert(button_id);
	if (cb_) cb_->onInputSystemButtonAssignAdded(this, asgn->assign_id);
	return asgn->assign_id;
}
ButtonAssignId InputSystem::assignJoystick(ButtonId button_id, int button_value, KJoystick::Button joystick_btn, int joystick_index, Flags flags) {
	if (! hasButton(button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: %d", button_id);
		return (ButtonAssignId)(-1);
	}
	if (button_value == 0) {
		Log_warningf("E_JOYSTICK_ASSIGN: Btn=%d Val=%d JoyKey=%d", button_id, button_value, joystick_btn);
	}
	if (joystick_btn == 0) {
		Log_warningf("E_JOYSTICK_ASSIGN: Btn=%d Val=%d JoyKey=%d", button_id, button_value, joystick_btn);
	}
	CButtonAssign *asgn = new CButtonAssign();
	asgn->assign_id = _newAssignId();
	asgn->button_id = button_id;
	asgn->device_type = DEV_JOYSTICK;
	asgn->device_index = joystick_index;
	asgn->key = joystick_btn;
	asgn->value = button_value;
	asgn->strict_direction = (flags & STRICT_DIRECTION) != 0;
	asgn->keyrepeat_enable = (flags & REPEAT) != 0;
	asgn->enable_background = (flags & BACKGROUND) != 0;
	asgn->no_modifier = (flags & NO_MODIFIER) != 0;
	asgn->assign_enabled = true;
	assigns_.push_back(asgn);
	active_buttons_.insert(button_id);
	if (cb_) cb_->onInputSystemButtonAssignAdded(this, asgn->assign_id);
	return asgn->assign_id;
}
ButtonAssignId InputSystem::assignJoystickSequence(ButtonId button_id, int button_value, KJoystick::Button *cmd, int timeout, int joystick_index, Flags flags) {
	if (! hasButton(button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: %d", button_id);
		return (ButtonAssignId)(-1);
	}
	if (button_value == 0) {
		Log_warningf("E_JOYSTICK_ASSIGN: Btn=%d Val=%d Seq=(%p)", button_id, button_value, cmd);
	}
	if (cmd == NULL) {
		Log_warningf("E_JOYSTICK_ASSIGN: Btn=%d Val=%d Seq=(%p)", button_id, button_value, cmd);
	}
	CButtonAssign *asgn = new CButtonAssign();
	asgn->assign_id = _newAssignId();
	asgn->button_id = button_id;
	asgn->device_type = DEV_JOYSTICK;
	asgn->device_index = joystick_index;
	for (int i=0; cmd[i] != KJoystick::NONE; i++) {
		if (i>0) asgn->keyseq.push_back(0); // Neutral
		asgn->keyseq.push_back(cmd[i]);
		Log_assert(i < MAX_KEY_SEQUENCE_LENGTH);
	}
	asgn->keyseq_timeout = timeout;
	asgn->value = button_value;
	asgn->strict_direction = (flags & STRICT_DIRECTION) != 0;
	asgn->keyrepeat_enable = (flags & REPEAT) != 0;
	asgn->enable_background = (flags & BACKGROUND) != 0;
	asgn->no_modifier = (flags & NO_MODIFIER) != 0;
	asgn->assign_enabled = true;
	assigns_.push_back(asgn);
	active_buttons_.insert(button_id);
	if (cb_) cb_->onInputSystemButtonAssignAdded(this, asgn->assign_id);
	return asgn->assign_id;
}
ButtonAssignId InputSystem::assignMouse(ButtonId button_id, int button_value, KMouse::Button mouse_btn, Flags flags) {
	if (! hasButton(button_id)) {
		Log_errorf("E_UNDEFINED_BUTTON_ID: %d", button_id);
		return (ButtonAssignId)(-1);
	}
	if (button_value == 0) {
		Log_error("E_MOUSE_ASSIGN: Invalid button value"); // 0 を指定しても意味がない（ボタンが押されたら 0 を返すことになるが、これはボタンが押されていない時の戻り値 0 と区別できない）
	}
	CButtonAssign *asgn = new CButtonAssign();
	asgn->assign_id = _newAssignId();
	asgn->button_id = button_id;
	asgn->device_type = DEV_MOUSE;
	asgn->key = mouse_btn;
	asgn->value = button_value;
	asgn->enable_background = (flags & BACKGROUND) != 0;
	asgn->no_modifier = (flags & NO_MODIFIER) != 0;
	asgn->assign_enabled = true;
	assigns_.push_back(asgn);
	active_buttons_.insert(button_id);
	if (cb_) cb_->onInputSystemButtonAssignAdded(this, asgn->assign_id);
	return asgn->assign_id;
}
bool InputSystem::getKeyboardAssign(ButtonAssignId assign_id, ButtonId *btn, KKeyboard::Key *keycode) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		const CButtonAssign *asgn = assigns_[index];
		if (asgn->device_type == DEV_KEYBOARD) {
			if (btn) *btn = asgn->button_id;
			if (keycode) *keycode = (KKeyboard::Key)asgn->key;
			return true;
		}
	}
	return false;
}
bool InputSystem::getJoystickAssign(ButtonAssignId assign_id, ButtonId *btn, KJoystick::Button *joybtn, int *joyindex) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		const CButtonAssign *asgn = assigns_[index];
		if (asgn->device_type == DEV_JOYSTICK) {
			if (btn) *btn = asgn->button_id;
			if (joybtn) *joybtn = (KJoystick::Button)asgn->key;
			if (joyindex) *joyindex = asgn->device_index;
			return true;
		}
	}
	return false;
}
bool InputSystem::getMouseAssign(ButtonAssignId assign_id, ButtonId *btn, KMouse::Button *mkey) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		const CButtonAssign *asgn = assigns_[index];
		if (asgn->device_type == DEV_MOUSE) {
			if (btn) *btn = asgn->button_id;
			if (mkey) *mkey = (KMouse::Button)asgn->key;
			return true;
		}
	}
	return false;
}
void InputSystem::changeKeyboardAssign(ButtonAssignId assign_id, KKeyboard::Key new_key) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		CButtonAssign *asgn = assigns_[index];
		asgn->key = new_key;
		if (cb_) cb_->onInputSystemButtonAssignChanged(this, assign_id);
	}
}
void InputSystem::changeJoystickAssign(ButtonAssignId assign_id, KJoystick::Button new_key) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		CButtonAssign *asgn = assigns_[index];
		asgn->key = new_key;
		if (cb_) cb_->onInputSystemButtonAssignChanged(this, assign_id);
	}
}
void InputSystem::removeAssign(ButtonAssignId assign_id) {
	int index = getAssignIndexById(assign_id);
	if (index >= 0) {
		if (cb_) cb_->onInputSystemButtonAssignRemove(this, assign_id);
		assigns_.erase(assigns_.begin() + index);
	}
}
int InputSystem::getState(ButtonId button_id) const {
	if (! isButtonEnabled(button_id)) return 0;
	for (auto it=assigns_.cbegin(); it!=assigns_.cend(); ++it) {
		const CButtonAssign *asgn = *it;
		if (asgn->button_id != button_id) continue;
		if (! asgn->assign_enabled) continue;
		if (asgn->isAlias()) {
			int s = getState(asgn->alias_of);
			if (s != 0) {
				return s;
			}
		}
		const int val = __SnapI(asgn->_state_curr);
		if (val != 0) {
			return val;
		}
	}
	return 0;
}
float InputSystem::getStateF(ButtonId button_id) const {
	if (! isButtonEnabled(button_id)) return 0;
	for (auto it=assigns_.cbegin(); it!=assigns_.cend(); ++it) {
		const CButtonAssign *asgn = *it;
		if (asgn->button_id != button_id) continue;
		if (! asgn->assign_enabled) continue;
		if (asgn->isAlias()) {
			const float s = getStateF(asgn->alias_of);
			if (s != 0) {
				return s;
			}
		}
		const float val = __SnapF(asgn->_state_curr);
		if (val != 0) {
			return val;
		}
	}
	return 0.0f;
}
bool InputSystem::getButtonDown(ButtonId button_id) const {
	return getButtonDownEx(button_id, false);
}
bool InputSystem::getButtonDownEx(ButtonId button_id, bool dont_care_button_enable) const {
	// 仮想ボタンの enabled は _isButtonDown でチェックするため、ここでは調べない。
	// disable の最中にボタンを押し、その状態で enable にしたときに getButtonDown が反応するようにするため
	// if (! isButtonEnabled(...)) return false;
	int s = stats_[button_id];
	if ((s & MASK_DOWN) == 0) {
		s |= _isButtonDown(button_id, dont_care_button_enable) ? ON_DOWN : NOT_DOWN;
		stats_[button_id] = s;
	}
	if ((s & ON_DOWN) == 0) return false;
	return true;
}
bool InputSystem::getButtonUp(ButtonId button_id) const {
	// 仮想ボタンの enabled は _isButtonUp でチェックするため、ここでは調べない。
	// ボタンを押している最中に disable になった時に isButtonUp が反応するようにするため
	// if (! isButtonEnabled(...)) return false;
	int s = stats_[button_id];
	if ((s & MASK_UP) == 0) {
		s = _isButtonUp(button_id) ? ON_UP : NOT_UP;
		stats_[button_id] = s;
	}
	return (s & ON_UP) != 0;
}
void InputSystem::resetButtonState(ButtonId button_id) {
	for (auto it=assigns_.cbegin(); it!=assigns_.cend(); ++it) {
		CButtonAssign *asgn = *it;
		if (asgn->button_id == button_id) {
			asgn->resetState();
		}
	}
}
void InputSystem::resetAllButtonStates() {
	for (auto it=assigns_.cbegin(); it!=assigns_.cend(); ++it) {
		CButtonAssign *asgn = *it;
		asgn->resetState();
	}
}

Path InputSystem::getKeyboardName(KKeyboard::Key key) const {
	return KKeyboard::getKeyName(key);
}
Path InputSystem::getJoystickName(KJoystick::Button joy) const {
	return KJoystick::getButtonName(joy);
}
bool InputSystem::getKeyboardFromName(const Path &s, KKeyboard::Key *key) const {
	return KKeyboard::findKeyByName(s.u8(), key);
}
bool InputSystem::getJoystickFromName(const Path &s, KJoystick::Button *btn) const {
	return KJoystick::findButtonByName(s.u8(), btn);
}

int InputSystem::getPressedKeyboardKeys(KKeyboard::Key *keys, int size) const {
	int num = 0;
	for (int i=0; i<KKeyboard::KEY_ENUM_MAX; i++) {
		if (keys_curr_[i]) {
			if (keys) keys[num] = (KKeyboard::Key)i;
			num++;
			if (num >= size) {
				break;
			}
		}
	}
	return num;
}
bool InputSystem::isJoystickConnected(int joystick_index) const {
	return KJoystick::isConnected(joystick_index);
}
int InputSystem::getPressedJoyButtons(KJoystick::Button *keys, int size, int joystick_index) const {
	int num = 0;
	int count = KJoystick::getButtonCount(joystick_index);
	for (int i=0; i<count; i++) {
		if (KJoystick::isButtonDown(joystick_index, (KJoystick::Button)i)) {
			keys[num] = (KJoystick::Button)i;
			num++;
			if (num >= size) {
				break;
			}
		}
	}
	return num;
}
int InputSystem::getMouseWheelDelta() const { 
	return wheel_curr_ - wheel_last_;
}
bool InputSystem::getMouseMoveDelta(int *dx, int *dy, int trigger_delta) const {
	int xdist = (int)(mpos_curr_.x - mpos_last_.x);
	int ydist = (int)(mpos_curr_.y - mpos_last_.y);
	bool xmoved = abs(xdist) >= trigger_delta;
	bool ymoved = abs(ydist) >= trigger_delta;
	if (xmoved || ymoved) {
		if (dx) *dx = xdist;
		if (dy) *dy = ydist;
		return true;
	}
	return false;
}
Vec3 InputSystem::getMousePointInWindow() const {
	// Screen pos at client top-left
	int x0 = engine_->getStatus(Engine::ST_SCREEN_AT_CLIENT_ORIGIN_X);
	int y0 = engine_->getStatus(Engine::ST_SCREEN_AT_CLIENT_ORIGIN_Y);

	// Mouse in screen
	int mx, my;
	KMouse::getPosition(&mx, &my);

	return Vec3(mx-x0, my-y0, 0);
}
bool InputSystem::getMouseDown(KMouse::Button key) const {
	int m = ~mouse_last_ & mouse_curr_; // 前回 0 で、今回 1 になっているビットだけを取り出す
	switch (key) {
	case KMouse::LEFT:   return (m & 1) ? 1 : 0;
	case KMouse::RIGHT:  return (m & 2) ? 1 : 0;
	case KMouse::MIDDLE: return (m & 4) ? 1 : 0;
	}
	return false;
}
bool InputSystem::getMouseUp(KMouse::Button key) const {
	int m = mouse_last_ & ~mouse_curr_; // 前回 1 で、今回 0 になっているビットだけを取り出す
	switch (key) {
	case KMouse::LEFT:   return (m & 1) ? 1 : 0;
	case KMouse::RIGHT:  return (m & 2) ? 1 : 0;
	case KMouse::MIDDLE: return (m & 4) ? 1 : 0;
	}
	return false;
}
bool InputSystem::isMousePressed(KMouse::Button key) const {
	int m = mouse_last_ & mouse_curr_; // 前回 1 で、今回も 1 になっているビットだけを取り出す
	switch (key) {
	case KMouse::LEFT:   return (m & 1) ? 1 : 0;
	case KMouse::RIGHT:  return (m & 2) ? 1 : 0;
	case KMouse::MIDDLE: return (m & 4) ? 1 : 0;
	}
	return false;
}

bool InputSystem::getKeyDown(KKeyboard::Key key) const {
	return !keys_last_[key] && keys_curr_[key];
}
bool InputSystem::getKeyUp(KKeyboard::Key key) const {
	return keys_last_[key] && !keys_curr_[key];
}
bool InputSystem::isKeyPressed(KKeyboard::Key key) const {
	return keys_curr_[key];
}
bool InputSystem::getJoyButtonDown(KJoystick::Button btn, int joystick_index) const {
	if (0 <= joystick_index && joystick_index < KJoystick::MAX_CONNECTION) {
		if (joy_[joystick_index]->isButtonDown(btn)) {
			return true;
		}
	}
	return false;
}
bool InputSystem::getJoyButtonUp(KJoystick::Button btn, int joystick_index) const {
	if (0 <= joystick_index && joystick_index < KJoystick::MAX_CONNECTION) {
		if (joy_[joystick_index]->isButtonUp(btn)) {
			return true;
		}
	}
	return false;
}
bool InputSystem::isJoyButtonPressed(KJoystick::Button btn, int joystick_index) const {
	if (0 <= joystick_index && joystick_index < KJoystick::MAX_CONNECTION) {
		if (joy_[joystick_index]->isButtonDownHold(btn)) {
			return true;
		}
	}
	return false;
}
float InputSystem::isJoyButtonAnalog(KJoystick::Button key, int joystick_index) const {
	if (isJoyButtonPressed(key, joystick_index)) {
		return 1.0f;
	}
	return 0.0f;
}
/// 現在の実行環境から、キーリピートの設定を取得する
void InputSystem::getSystemKeyRepeatingInFrames(int *delay_frames, int *interval_frames) const {
	// OS 設定を取得
	PlatformExtraInfo info;
	Platform::getExtraInfo(&info);

	int fps = engine_->getStatus(Engine::ST_FPS_REQUIRED);

	// リピート遅延
	if (delay_frames) {
		if (info.keyrepeat_delay_msec > 0) {
			*delay_frames = (int)(info.keyrepeat_delay_msec / (1000.0f / fps)); // (F)
		} else {
			*delay_frames = InputSystem::DEFAULT_KEYREPEAT_DELAY_FRAMES;
		}
	}

	// リピート間隔
	if (interval_frames) {
		if (info.keyrepeat_interval_msec > 0) {
			float intervalf = info.keyrepeat_interval_msec / (1000.0f / fps); // (F)
			int intrv = (int)(intervalf + 0.5f); // フレーム単位にすると誤差が激しくなるため、切り捨てではなく四捨五入する
			*interval_frames = Num_max(intrv, 1);
		} else {
			*interval_frames = InputSystem::DEFAULT_KEYREPEAT_INTERVAL_FRAMES;
		}
	}
}
void InputSystem::getKeyRepeatingInFrames(int *delay, int *interval) const {
	if (delay) *delay = keyrepeat_delay_;
	if (interval) *interval = keyrepeat_interval_;
}
void InputSystem::setKeyRepeatingInFrames(int delay, int interval) {
	keyrepeat_delay_ = delay;
	keyrepeat_interval_ = interval;
}
int InputSystem::getButtonIndexById(ButtonId id) const {
	for (size_t i=0; i<buttons_.size(); i++) {
		if (buttons_[i].btn_id == id) {
			return i;
		}
	}
	return -1;
}
int InputSystem::getButtonIndexByName(const Path &name) const {
	for (size_t i=0; i<buttons_.size(); i++) {
		if (buttons_[i].name.compare(name) == 0) {
			return i;
		}
	}
	return -1;
}
InputSystem::_VirtualBtn * InputSystem::_newButton(ButtonId id) {
	int index = getButtonIndexById(id);
	if (index >= 0) {
		return &buttons_[index];
	} else {
		_VirtualBtn newbtn;
		newbtn.btn_id = id;
		buttons_.push_back(newbtn);
		return &buttons_.back();
	}
}
int InputSystem::getAssignIndexById(ButtonAssignId id) const {
	for (int i=0; i<(int)assigns_.size(); i++) {
		if (assigns_[i]->assign_id == id) {
			return i;
		}
	}
	return -1;
}

/// キーリピートの有無をチェックする。
/// frame にはキーを押してからの経過フレーム数を指定する。
/// キーリピートが入力を発生させるべきフレームであれば true を返す
bool InputSystem::_isKeyRepeatTrigger(int frame) const {
	if (frame < keyrepeat_delay_) return false;
	int tt = frame - keyrepeat_delay_;
	if (tt % keyrepeat_interval_ > 0) return false;
	return true;
}
int InputSystem::_getRawState(const CButtonAssign *btn, int key, bool *key_repeated) const {
	if (btn->isAlias()) {
		return 0; // エイリアスボタンの場合、別のボタンを参照しているだけなので、このボタン自身の状態は存在しない
	}
	switch (btn->device_type) {
	case DEV_KEYBOARD:
		return _getRawKBState(btn, key, key_repeated);
	case DEV_MOUSE:
		return _getRawMouseState(btn, key, key_repeated);
	case DEV_JOYSTICK:
		return _getRawJoyState(btn, key, key_repeated);
	}
	return 0;
}
int InputSystem::_getRawMouseState(const CButtonAssign *btn, int key, bool *key_repeated) const {
	// マウスカーソルが ImGui 上にあるか、ImGui のコントロールをドラッグ中の場合は
	// ゲーム側への入力とはみなさない。無視する
	if (ImGui::IsAnyWindowHovered()) return 0;
	if (ImGui::IsAnyItemHovered()) return 0;
	if (ImGui::IsAnyItemActive()) return 0;

	switch ((KMouse::Button)key) {
	case KMouse::LEFT:   return (mouse_curr_ & 1) ? 1 : 0;
	case KMouse::RIGHT:  return (mouse_curr_ & 2) ? 1 : 0;
	case KMouse::MIDDLE: return (mouse_curr_ & 4) ? 1 : 0;
	}
	return 0;
}
int InputSystem::_getRawKBState(const CButtonAssign *btn, int key, bool *key_repeated) const {
	// マウスカーソルが ImGui 上にあるか、ImGui のコントロールをドラッグ中の場合は
	// ゲーム側への入力とはみなさない。無視する
	/*
	if (ImGui::IsAnyWindowHovered()) {
		// 単なるテキストや、ウィンドウ上の何もない場所を指している
		return 0;
	}
	*/
	if (ImGui::IsAnyItemHovered()) {
		// マウスポインタに対して反応するアイテムを指している
		return 0;
	}
	if (ImGui::IsAnyItemActive()) {
		// マウスポインタとは関係なく、入力受付状態になっているアイテムがある（テキストボックスなど）
		return 0;
	}

	// 現在の無加工なキーの状態
	int state = 0;
	if (engine_->getStatus(Engine::ST_WINDOW_FOCUSED)) {
		if (KKeyboard::isKeyDown((KKeyboard::Key)key)) {
			state = 1;
		}
	}
	
	// 修飾キーの排他処理
	// 例えば S という入力を受け付けるときに、
	// S + Shift を押しても反応しないようにする
	if (state && btn->no_modifier) {
		if (KKeyboard::getModifiers() != 0) {
			state = 0;
		}
	}
	if (btn->keyrepeat_enable && key_repeated) {
		if (state) {
			// キーリピートをエミュレートする
			if (_isKeyRepeatTrigger(btn->_keypress_timer)) {
				*key_repeated = true;
			}
		}
	}
	return state;
}
int InputSystem::_getRawJoyState(const CButtonAssign *btn, int key, bool *key_repeated) const {
	int state = joy_[btn->device_index]->getButtonValue((KJoystick::Button)key, btn->strict_direction);

	if (btn->keyrepeat_enable && key_repeated) {
		if (state) {
			// キーリピートをエミュレートする
			if (_isKeyRepeatTrigger(btn->_keypress_timer)) {
				*key_repeated = true;
			}
		}
	}
	return state;
}
int InputSystem::_getSeqState(CButtonAssign *btn, bool *key_repeated) {
	if (btn->keyseq.empty()) {
		// キーシーケンスなし
		return 0;
	}

	// タイムアウトした入力をキーバッファから削除する
	while (btn->_keyseq_buf.size() > 0) {
		int delta = game_time_ - btn->_keyseq_buf[0].time; // 経過時間
		if (delta < 0/*念のため不正な時間をチェックしておく*/ || btn->keyseq_timeout < delta) {
			btn->_keyseq_buf.erase(btn->_keyseq_buf.begin());
		} else {
			break; // 最初に「まだタイムアウトしていない入力」が見つかった時点で終了する
		}
	}

	// キーシーケンスを構成するキーのうち、どれかが押されているかどうか調べる
	int curr_key = 0;
	for (size_t i=0; i<btn->keyseq.size(); i++) {
		if (_getRawState(btn, btn->keyseq[i], NULL)) {
			curr_key = btn->keyseq[i];
			break;
		}
	}

	// 前回のキー状態
	int last_key = 0;
	if (btn->_keyseq_buf.size() > 0) {
		last_key = btn->_keyseq_buf.back().key;
	}

	// 前回からキー状態が変化していたら、キーバッファに追加する
	if (curr_key != last_key) {
		// これが最初のキー入力だったら所要時間情報をクリアする
		if (btn->_keyseq_buf.empty()) {
			btn->_keyseq_time_elapsed = 0;
		}
		CButtonAssign::KEYBUF k;
		k.time = game_time_;
		k.key = curr_key;
		btn->_keyseq_buf.push_back(k);
	}

	// 今よりも前のフレームで既にシーケンスが成立していた場合、
	// キーシーケンス最後のキーを押しっぱなしにしている間は、たとえタイムアウトしていても
	// キーシーケンスが成立し続けているものとする
	if (btn->_keyseq_matched) {
		int state = _getRawState(btn, btn->keyseq.back(), key_repeated/*キーリピート状態の取得*/);
		if (state) {
			return btn->value;
		}
	}
	btn->_keyseq_matched = false;

	// キーシーケンスが、キー入力履歴の末尾の並びと一致するか調べる
	bool matched = false;
	if (btn->keyseq.size() <= btn->_keyseq_buf.size()) {
		matched = true;
		int len = (int)btn->keyseq.size();
		for (int i=0; i<len; i++) {
			int formal_key = btn->keyseq[i];
			int actual_key = btn->_keyseq_buf[btn->_keyseq_buf.size() - len + i].key;
			if (formal_key != actual_key) {
				matched = false;
			}
		}
	}
	if (matched) {
		btn->_keyseq_time_elapsed = game_time_ - btn->_keyseq_buf[0].time; // 完了時間
		btn->_keyseq_matched = true;
		return btn->value;
	} else {
		btn->_keyseq_matched = false;
		return 0;
	}
}
void InputSystem::_updateButton(CButtonAssign *btn) {
	if (! btn->assign_enabled) {
		// キー無効
		btn->_state_last = btn->_state_curr;
		btn->_state_curr = 0.0f;
		btn->_keyrepeat_occurred = false;
		btn->_keypress_timer = 0;
	
	} else if (btn->keyseq.empty()) {
		// キーシーケンスなし。普通のキー入力を処理する
		bool repeat = false;
		int state = _getRawState(btn, btn->key, &repeat);
		btn->_state_last = btn->_state_curr;
		btn->_state_curr = (float)(state * btn->value);
		btn->_keyrepeat_occurred = repeat;
		if (state) {
			btn->_keypress_timer++;
		} else {
			btn->_keypress_timer = 0;
		}

	} else {
		// キーシーケンスあり。キー入力状態をチェックする
		bool repeat = false;
		int state = _getSeqState(btn, &repeat);
		btn->_state_last = btn->_state_curr;
		btn->_state_curr = (float)(state * btn->value);
		btn->_keyrepeat_occurred = repeat;
		if (state) {
			btn->_keypress_timer++;
		} else {
			btn->_keypress_timer = 0;
		}
	}
}
int InputSystem::_getButtonCount() const {
	return (int)assigns_.size();
}
bool InputSystem::_getKeyboardAssign(ButtonId button_id, _KBButtonEnumerator *cb, int max_alias_depth) const {
	for (size_t i=0; i<assigns_.size(); i++) {
		const CButtonAssign *asgn = assigns_[i];
		if (asgn->button_id != button_id) {
			// ボタンＩＤが一致しない
			continue;
		}
		if (asgn->device_type == DEV_KEYBOARD) {
			// キーボードのアサインが見つかった
			if (cb->found((KKeyboard::Key)asgn->key)) {
				continue;
			}
			return false; // EXIT
		}
		if (max_alias_depth > 0 && asgn->isAlias()) {
			// 別のボタンのエイリアスになっている。
			// 参照先をたどる。行きついた先がキーボードのキーであれば、それも記録する
			if (_getKeyboardAssign(asgn->alias_of, cb, max_alias_depth-1)) {
				continue;
			}
			return false; // EXIT
		} 
	}
	return true;
}
bool InputSystem::_isButtonDown(ButtonId button_id, bool dont_care_button_enable) const {
	if (! dont_care_button_enable) {
		if (! isButtonEnabled(button_id)) return false;
	}
	for (auto it=assigns_.cbegin(); it!=assigns_.cend(); ++it) {
		const CButtonAssign *asgn = *it;
		if (asgn->button_id != button_id) continue;
		if (! asgn->assign_enabled) continue;
		if (asgn->isAlias()) {
			if (_isButtonDown(asgn->alias_of, dont_care_button_enable)) {
				return true;	
			}
		}
		const int last = __SnapI(asgn->_state_last);
		const int curr = __SnapI(asgn->_state_curr);
		if (last==0 && curr!=0) {
			return true; // 押された
		}
		if (asgn->_keyrepeat_occurred) {
			return true; // 押されたことにする
		}
		// 一つのボタン (button_name) が複数のキーに割り当てられている場合があることに注意
		// 残りのキーについても調べるため、for ループを継続する
	}
	return false;
}
bool InputSystem::_isButtonUp(ButtonId button_id) const {
	if (! isButtonEnabled(button_id)) return false;
	for (auto it=assigns_.cbegin(); it!=assigns_.cend(); ++it) {
		const CButtonAssign *asgn = *it;
		if (asgn->button_id != button_id) continue;
		if (! asgn->assign_enabled) continue;
		if (asgn->isAlias()) {
			if (_isButtonUp(asgn->alias_of)) {
				return true;
			}
		}
		const int last = __SnapI(asgn->_state_last);
		const int curr = __SnapI(asgn->_state_curr);
		if (last!=0 && curr==0) {
			return true; // 離された
		}
		// 一つのボタン (button_name) が複数のキーに割り当てられている場合があることに注意
		// 残りのキーについても調べるため、for ループを継続する
	}
	return false;
}

}
