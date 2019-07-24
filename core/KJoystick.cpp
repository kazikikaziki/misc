#include "KJoystick.h"
#include "KLog.h"
#include <string>
#include <vector>
#include <math.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef _WIN32
#	define K_stricmp _stricmp
#else
#	define K_stricmp strcasecmp
#endif

#define JOYSTICK_THRESHOLD 0.2f

namespace mo {

namespace {

#ifdef _WIN32
class Win32Joy {
	JOYCAPSW caps_;
	JOYINFOEX info_;
	UINT id_;
	DWORD last_check_;
	float threshold_;
	bool connected_;
public:
	Win32Joy() {
		// ここではログとか呼ばない事。static に確保されるので
		// ログ機構のコンストラクタよりも前に実行される可能性がある
		ZeroMemory(&caps_, sizeof(caps_));
		ZeroMemory(&info_, sizeof(info_));
		last_check_ = 0;
		connected_ = false;
		threshold_ = JOYSTICK_THRESHOLD;
	}
	void init(int id) {
		ZeroMemory(&caps_, sizeof(caps_));
		ZeroMemory(&info_, sizeof(info_));
		threshold_ = JOYSTICK_THRESHOLD;
		id_ = JOYSTICKID1 + id;
		if (joyGetDevCapsW(id_, &caps_, sizeof(caps_)) == 0) {
			connected_ = true;
		}
		Log_infof("GamePad[%d]: %s", id_, connected_ ? "Connected" : "N/A");
	}
	bool isConnected() const {
		return connected_;
	}
	void poll() {
		// Joystickが接続されていないと分かっているとき、なるべく Joystick の API を呼ばないようにする。
		// ゲーム起動中にスリープモードに入り、そこからレジュームしたときに API内部でメモリ違反が起きることがある。
		// スリープ中にゲームパッドを外してレジュームした場合もヤバいかもしれない。
		// この現象を再現するには、ゲーム起動中にノートＰＣをスリープさせればよい

		// 接続されているかどうかの確認には joyGetDevCapsW を使わないようにする。
		// 一度接続された状態になって joyGetDevCapsW が成功すると、たとえその後
		// パッドを外しても joyGetDevCapsW は成功し続ける（最初に接続成功したときの結果を返し続ける）
		// そのため、接続状態かどうかは joyGetPosEx がエラーになるかどうかで判断する

		if (connected_) {
			info_.dwSize = sizeof(info_);
			info_.dwFlags = JOY_RETURNALL;
			if (joyGetPosEx(id_, &info_) != 0) {
				// 取得に失敗した。パッドが切断されたと判断する
				Log_infof("GamePad[%d]: Disconnected", id_);
				connected_ = false;
			}
		} else {
			// パッドが非接続状態。一定時間ごとに接続を確認する
			DWORD time = GetTickCount();
			if (last_check_ + 2000 <= time) { // 前回の確認から一定時間経過した？
				last_check_ = time;
				// 接続されているかどうかを joyGetPosEx の戻り値で確認する。
				// なお、joyGetDevCapsW は使わないこと。
				// この関数は一度でも成功すると以後キャッシュされた値を返し続け、失敗しなくなる
				info_.dwSize = sizeof(info_);
				info_.dwFlags = JOY_RETURNALL;
				if (joyGetPosEx(id_, &info_) == 0) {
					// 接続できた。
					// caps を取得する
					joyGetDevCapsW(id_, &caps_, sizeof(caps_));
					connected_ = true;
					Log_infof("GamePad[%d]: Connected", id_);
				}
			}
		}
	}

	/// 接続中のジョイスティックについている軸の数
	int numAxes() const {
		return caps_.wNumAxes;
	}

	/// ハットスイッチ(POV)を持っているかどうか
	bool hasPov() const {
		return (caps_.wCaps & JOYCAPS_HASPOV) != 0;
	}

	/// 接続中のジョイスティックについているボタンの数
	int numButtons() const {
		return caps_.wNumButtons;
	}

	/// 軸の入力値を得る。値は -1.0 以上 1.0 以下に正規化されている。
	/// ニュートラル状態がきっかり 0.0 になるとは限らないので注意すること
	/// axis 軸番号。0 以上 numAxes() 未満
	float getAxis(int axis) const {
		if (axis < 0) return 0.0f;
		if (axis >= (int)caps_.wNumAxes) return 0.0f;
		DWORD table[] = {
			info_.dwXpos, // axis0
			info_.dwYpos, // axis1
			info_.dwZpos, // axis2
			info_.dwRpos, // axis3
			info_.dwUpos, // axis4
			info_.dwVpos  // axis5
		};
		DWORD dwAxis = table[axis];
		// dwAxis は 0x0000 .. 0xFFFF の値を取る。
		// dwXpos なら 0 が一番左に倒したとき、0xFFFF が一番右に倒したときに該当する
		// ニュートラル状態はその中間だが、きっちり真ん中の値 0x7FFF になっている保証はない。
		float value = ((float)dwAxis / 0xFFFF - 0.5f) * 2.0f;
		if (fabsf(value) < threshold_) return 0.0f;
		return value;
	}

	/// ボタンの状態を調べる
	/// ボタンが押されているなら true を返す
	/// btn ボタン番号。0 以上 numButtons() 未満
	bool getButton(int btn) const {
		if (btn < 0) return false;
		if (btn >= (int)caps_.wNumButtons) return false;
		DWORD mask = 1 << btn;
		return (info_.dwButtons & mask) != 0;
	}

	/// ハットスイッチの方向を調べる。
	/// スイッチが押されているなら、その向きを x, y, deg にセットして true を返す。
	/// ニュートラル状態の場合は何もせずに false を返す
	///
	/// x ハットスイッチの入力向きを XY 成分で表したときの X 値。-1, 0, 1 のどれかになる
	/// y ハットスイッチの入力向きを XY 成分で表したときの Y 値。-1, 0, 1 のどれかになる
	/// ハットスイッチの入力向き。上方向を 0 度として時計回りに360未満の値を取る
	bool getPov(int *x, int *y, float *deg) const {
		// ハットスイッチの有無をチェック
		if ((caps_.wCaps & JOYCAPS_HASPOV) == 0) {
			return false;
		}

		// 入力の有無をチェック
		// ハットスイッチの入力がない場合、dwPOV は JOY_POVCENTERED(=0xFFFF) なのだが、
		// JOY_POVCENTERED は WORD 値であることに注意。DWORD の場合でも 0xFFFF がセットされているとは限らない。
		// 環境によっては 0xFFFFFFFF になるとの情報もあるので、どちらでも対処できるようにしておく
		if ((WORD)info_.dwPOV == JOY_POVCENTERED) {
			return false;
		}

		// ハットスイッチの入力がある。その向きを調べる。
		if (x || y) {
			// x, y 成分での値が要求されている。
			// dwPOV は真上入力を角度 0 とし、0.01 度単位で時計回りに定義される。最大 35900。
			// ただ、ハットスイッチが無い場合は dwPOV は 0 になり、上入力と区別がつかないため
			// joyGetDevCapsW でハットスイッチの有無を事前にチェックしておくこと
			const int xmap[] = { 0, 1, 1, 1,  1, 1, 1, 0,  0,-1,-1,-1,  -1,-1,-1, 0};
			const int ymap[] = {-1,-1,-1, 0,  0, 1, 1, 1,  1, 1, 1, 0,   0,-1,-1,-1};
			int i = info_.dwPOV / (36000 / 16);
			if (x) *x = xmap[i];
			if (y) *y = ymap[i];
		}
		if (deg) {
			*deg = info_.dwPOV / 100.0f; // 時計回りに 0.01 度単位
		}
		return true;
	}
};

typedef Win32Joy Joy;
#else
class NullJoy {
public:
	NullJoy() {}
	void init(int id) {}
	bool isConnected() const { return false; }
	void poll() {}
	int numAxes() const { return 0; }
	bool hasPov() const { return false; }
	int numButtons() const { return 0; }
	float getAxis(int axis) const { return 0.0f; }
	bool getButton(int btn) const { return false; }
	bool getPov(int *x, int *y, float *deg) const { return false; }
};

typedef NullJoy Joy;
#endif

class JoyManager {
public:
	Joy list_[KJoystick::MAX_CONNECTION];
	bool pov_as_axis_;
	bool uninit_;

	JoyManager() {
		pov_as_axis_ = true;
		uninit_ = true;
	}
	Joy & get(int id) {
		if (uninit_) {
			for (int i=0; i<KJoystick::MAX_CONNECTION; i++) {
				list_[i].init(i);
			}
			uninit_ = NULL;
		}
		return list_[id];
	}
};
static JoyManager s_joy;


class JoyButtonNames {
public:
	const char * getButtonName(KJoystick::Button btn) {
		static const char *s_empty = "";
		prepare();
		if (0 <= btn && btn < (int)buttons_.size()) {
			return buttons_[btn].c_str();
		} else {
			return s_empty;
		}
	}
	const char * getAxisName(KJoystick::Axis axis) {
		static const char *s_empty = "";
		prepare();
		if (0 <= axis && axis < (int)axes_.size()) {
			return axes_[axis].c_str();
		} else {
			return s_empty;
		}
	}
	bool parseButton(const char *s, KJoystick::Button *btn) {
		prepare();
		if (s && s[0]) {
			for (size_t i=0; i<buttons_.size(); i++) {
				const char *n = buttons_[i].c_str();
				if (K_stricmp(n, s) == 0) {
					if (btn) *btn = (KJoystick::Button)i;
					return true;
				}
			}
		}
		return false;
	}
	bool parseAxis(const char *s, KJoystick::Axis *axis) {
		prepare();
		if (s && s[0]) {
			for (size_t i=0; i<axes_.size(); i++) {
				const char *n = axes_[i].c_str();
				if (K_stricmp(n, s) == 0) {
					if (axis) *axis = (KJoystick::Axis)i;
					return true;
				}
			}
		}
		return false;
	}

private:
	void prepare() {
		if (!buttons_.empty()) {
			return;
		}
		{
			auto &t = buttons_;
			t.resize(KJoystick::MAX_BUTTONS);
			t[KJoystick::B1 ] = "Button1";
			t[KJoystick::B2 ] = "Button2";
			t[KJoystick::B3 ] = "Button3";
			t[KJoystick::B4 ] = "Button4";
			t[KJoystick::B5 ] = "Button5";
			t[KJoystick::B6 ] = "Button6";
			t[KJoystick::B7 ] = "Button7";
			t[KJoystick::B8 ] = "Button8";
			t[KJoystick::B9 ] = "Button9";
			t[KJoystick::B10] = "Button10";
			t[KJoystick::B11] = "Button11";
			t[KJoystick::B12] = "Button12";
			t[KJoystick::B13] = "Button13";
			t[KJoystick::B14] = "Button14";
			t[KJoystick::B15] = "Button15";
			t[KJoystick::B16] = "Button16";
		}
		{
			auto &t = axes_;
			t.resize(KJoystick::MAX_AXIS);
			t[KJoystick::X] = "X";
			t[KJoystick::Y] = "Y";
			t[KJoystick::Z] = "Z";
			t[KJoystick::R] = "R";
			t[KJoystick::U] = "U";
			t[KJoystick::V] = "V";
			t[KJoystick::POV_X] = "POV_X";
			t[KJoystick::POV_Y] = "POV_Y";
		}
	}
	std::vector<std::string> buttons_;
	std::vector<std::string> axes_;
};
static JoyButtonNames s_Names;


} // private namespace


#pragma region KJoystick
void KJoystick::setUsePovAsAxis(bool value) {
	s_joy.pov_as_axis_ = value;
}
bool KJoystick::isConnected(int id) {
	Joy &joy = s_joy.get(id);
	return joy.isConnected();
}
bool KJoystick::hasAxis(int id, Axis axis) {
	const Joy &joy = s_joy.get(id);
	if (axis < KJoystick::POV_X) {
		// axis は通常の軸を表している
		return axis < joy.numAxes();
	} else {
		// axis はハットスイッチ(POV)を表している
		return joy.hasPov();
	}
}
int KJoystick::getButtonCount(int id) {
	const Joy &joy = s_joy.get(id);
	return joy.numButtons();
}
bool KJoystick::isButtonDownRaw(int id, KJoystick::Button btn) {
	const Joy &joy = s_joy.get(id);
	int pov = 0;

	switch (btn) {
	// 軸の入力を見る
	case X_NEGATIVE: return joy.getAxis(0) < 0;
	case X_POSITIVE: return joy.getAxis(0) > 0;
	case Y_NEGATIVE: return joy.getAxis(1) < 0;
	case Y_POSITIVE: return joy.getAxis(1) > 0;
	case Z_NEGATIVE: return joy.getAxis(2) < 0;
	case Z_POSITIVE: return joy.getAxis(2) > 0;
	case R_NEGATIVE: return joy.getAxis(3) < 0;
	case R_POSITIVE: return joy.getAxis(3) > 0;
	case U_NEGATIVE: return joy.getAxis(4) < 0;
	case U_POSITIVE: return joy.getAxis(4) > 0;
	case V_NEGATIVE: return joy.getAxis(5) < 0;
	case V_POSITIVE: return joy.getAxis(5) > 0;
	case POV_X_NEGATIVE: joy.getPov(&pov, NULL, NULL); return pov < 0;
	case POV_X_POSITIVE: joy.getPov(&pov, NULL, NULL); return pov > 0;
	case POV_Y_NEGATIVE: joy.getPov(NULL, &pov, NULL); return pov < 0;
	case POV_Y_POSITIVE: joy.getPov(NULL, &pov, NULL); return pov > 0;

	// 普通のボタンの状態を見る
	default:
		return joy.getButton(btn);
	}
}
bool KJoystick::isButtonDown(int id, KJoystick::Button btn) {
	if (s_joy.pov_as_axis_) {
		// ハットスイッチの入力もXY軸として扱う。
		// X, Y 軸についての問い合わせがあったら、PovX, PovY の状態も調べる。
		switch (btn) {
		case X_NEGATIVE: return isButtonDownRaw(id, X_NEGATIVE) || isButtonDownRaw(id, POV_X_NEGATIVE);
		case X_POSITIVE: return isButtonDownRaw(id, X_POSITIVE) || isButtonDownRaw(id, POV_X_POSITIVE);
		case Y_NEGATIVE: return isButtonDownRaw(id, Y_NEGATIVE) || isButtonDownRaw(id, POV_Y_NEGATIVE);
		case Y_POSITIVE: return isButtonDownRaw(id, Y_POSITIVE) || isButtonDownRaw(id, POV_Y_POSITIVE);
		}
	}
	// 生のボタン状態を見る
	return isButtonDownRaw(id, btn);
}
float KJoystick::getAxisRaw(int id, KJoystick::Axis axis) {
	Joy &joy = s_joy.get(id);
	int pov = 0;
	switch (axis) {
	case POV_X:
		// ハットスイッチのX位置を見る
		joy.getPov(&pov, NULL, NULL);
		return (float)pov;

	case POV_Y:
		// ハットスイッチのY位置を見る
		joy.getPov(NULL, &pov, NULL);
		return (float)pov;

	default:
		// スティック軸を見る
		return joy.getAxis(axis);
	}
}
float KJoystick::getAxis(int id, KJoystick::Axis axis) {
	if (s_joy.pov_as_axis_) {
		// ハットスイッチの入力もXY軸として扱う
		// X, Y 軸の状態について問い合わせがあったら、軸とハットスイッチのうち絶対値の大きなほうを使う
		float a, h;
		switch (axis) {
		case X:
			a = getAxisRaw(id, X);
			h = getAxisRaw(id, POV_X);
			return (fabsf(a) >= fabsf(h)) ? a : h;
		case Y:
			a = getAxisRaw(id, Y);
			h = getAxisRaw(id, POV_Y);
			return (fabsf(a) >= fabsf(h)) ? a : h;
		}
	}
	// 生のスティック状態を見る
	return getAxisRaw(id, axis);
}
void KJoystick::update() {
	for (int i=0; i<KJoystick::MAX_CONNECTION; i++) {
		Joy &joy = s_joy.get(i);
		joy.poll();
	}
}
const char * KJoystick::getAxisName(Axis axis) {
	return s_Names.getAxisName(axis);
}
const char * KJoystick::getButtonName(Button btn) {
	return s_Names.getButtonName(btn);
}
bool KJoystick::findButtonByName(const char *name, Button *btn) {
	return s_Names.parseButton(name, btn);
}


#pragma endregion // KJoystick

}
