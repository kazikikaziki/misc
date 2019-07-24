#include "Engine.h"
//
#include "GameGizmo.h"
#include "GameRender.h"
#include "GameScene.h"
//
#include "inspector.h"
#include "VideoBank.h"
#include "Screen.h"
//
#include "KClock.h"
#include "KLog.h"
#include "KPlatform.h"
#include "KNum.h"
#include "KSound.h"
#include "KImgui.h"
#include "KVideo.h"

// stb (https://github.com/nothings/stb)
//#define STB_TRUETYPE_IMPLEMENTATION // すでに mo_core.cpp で取り込み済み
#include "stb_truetype.h"

#ifdef _WIN32
#include <windows.h>
#endif




namespace mo {

#ifdef _WIN32
static TIMECAPS s_timecaps;

static void Timer_begin() {
	timeGetDevCaps(&s_timecaps, sizeof(TIMECAPS));
	timeBeginPeriod(s_timecaps.wPeriodMin);
}
static void Timer_end() {
	timeEndPeriod(s_timecaps.wPeriodMin);
}
static void Timer_wait(int msec) {
	Sleep(msec);
}

#else
static void Timer_begin() {
}
static void Timer_end() {
}
static void Timer_wait(int msec) {
	struct timespec ts;
	ts.tv_sec  = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000 * 1000;

	// nanosleep の戻り値が -1 だった場合は errno を確認する。
	// errno が EINTR だった場合は nanosleep が中断されたことになるので、
	// 時間を更新して nanosleep を継続する
	// ※nanosleep の第二引数には、nanosleep が中断された場合の残り時間が入る
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
	}
}
#endif








Engine *g_engine = NULL;



#pragma region Engine
Engine::Engine() {
	g_engine = this;
	zeroClear();
}
void Engine::zeroClear() {
	video_bank_ = NULL;
	filesystem_ = NULL;
	screen_ = NULL;
	inspector_ = NULL;
	window_ = NULL;
	gizmo_ = NULL;
	new_eid_ = 0;
	abort_timer_ = 0;
	sleep_until_ = 0;
	using_own_window_ = false;
	using_own_screen_ = false;
	using_own_video_ = false;
	using_own_sound_ = false;
	fullscreen_ = false;
	mark_tofullscreen_ = false;
	mark_towindowed_ = false;
	mark_resize_ = false;
	init_called_ = false;
}

bool Engine::init(const EngineDef &params) {
	Log_trace;
	zeroClear();
	init_called_ = true;
	window_ = params.window;
	screen_ = params.screen;
	filesystem_ = params.filesystem;
	gizmo_ = params.gizmo;
	inspector_ = params.inspector;
	loop_.init(this/*KLoopCallback*/);
	loop_.setFps(params.fps);

	int w = params.resolution_x;
	int h = params.resolution_y;

	// ウィンドウが未指定なら自前のウィンドウを作成
	if (window_ == NULL) {
		using_own_window_ = true;
		window_ = new Window();
		window_->create(w, h, "");
	}

	// スクリーンが未指定なら自前のスクリーンを作成
	if (screen_ == NULL) {
		using_own_screen_ = true;
		screen_ = new Screen();
		screen_->init(this, w, h);
	}

	// ビデオデバイスを初期化
	if (!Video::isInitialized()) {
		Log_assert(window_);
		using_own_video_ = true;
		Video::init(window_->getHandle());
	}

	// サウンドデバイスを初期化
	if (!Sound::isInitialized()) {
		Sound::init(window_->getHandle());
		using_own_sound_ = true;
	}

	// インスペクターを初期化
	if (inspector_) {
		inspector_->init(this);
	}

	// ビデオリソースの管理を開始
	video_bank_ = new VideoBank();

	window_->addCallback(this);

	if (screen_ == NULL) {
		Log_errorf("E_ENGINE: NO SCREEN");
		return false;
	}
	screen_->start();
	screen_->createCoreResources();
	return true;
}
Engine::~Engine() {
	shutdown();
}
void Engine::shutdown() {
	// GUIの管理を終了
	ImGuiVideo_Shutdown();

	// ビデオリソースの管理を終了
	if (video_bank_) {
		delete video_bank_;
		video_bank_ = NULL;
	}

	// ビデオデバイス終了
	if (using_own_video_) {
		Video::shutdown();
		using_own_video_ = false;
	}

	// サウンド終了
	if (using_own_sound_) {
		Sound::shutdown();
		using_own_sound_ = false;
	}

	// スクリーン終了
	if (screen_) {
		screen_->shutdown();
		if (using_own_screen_) {
			delete screen_;
		}
		screen_ = NULL;
		using_own_screen_ = false;
	}

	// ウィンドウ終了
	if (window_) {
		window_->closeWindow();
		if (using_own_window_) {
			delete window_;
		}
		window_ = NULL;
		using_own_window_ = false;
	}
	loop_.shutdown();

	// メンバーをゼロで初期化
	zeroClear();

	if (g_engine == this) {
		g_engine = NULL;
	}
}
/// フルスクリーンモードへの切り替え要求が出ているなら切り替えを実行する。
/// 切り替えを実行した場合は true を返す。
/// 切り替える必要がないか、失敗した場合は false を返す
bool Engine::processFullscreenReq() {
	if (! mark_tofullscreen_) return false;
	mark_tofullscreen_ = false;
	mark_towindowed_ = false;

	int w = screen_->getGameResolutionW();
	int h = screen_->getGameResolutionH();

	// フルスクリーン化する。
	// デバイスのフルスクリーン化よりも先にウィンドウのフルスクリーン化を行う(WIN32)
	window_->fullscreenWindow();
	if (! Video::resetDevice(w, h, 1)) {
		// 失敗
		// ウィンドウモード用のウィンドウスタイルに戻す
		window_->restoreWindow();
		window_->setClientSize(w, h);
		Log_info("Failed to switch to fullscreen mode.");
		return false;
	}
	// ビデオモードを切り替えた。
	Log_info("Switching to fullscreen -- OK");
	fullscreen_ = true;
	{
		EngineEvent e;
		e.type = EngineEvent::VIDEO_FULLSCREEN;
		sendEngineEvent(e);
	}
	return true;
}

/// ウィンドウモードへの切り替え要求が出ているなら切り替えを実行する。
/// 切り替えを実行した場合は true を返す。
/// 切り替える必要がないか、失敗した場合は false を返す
bool Engine::processWindowedReq() {
	if (! mark_towindowed_) return false;
	mark_tofullscreen_ = false;
	mark_towindowed_ = false;

	int w = screen_->getGameResolutionW();
	int h = screen_->getGameResolutionH();

	if (! Video::resetDevice(w, h, 0)) {
		// 失敗
		Log_info("Failed to switch to window mode.");
		return false;
	}
	// ビデオモードを切り替えた。
	// ウィンドウモード用のウィンドウスタイルを適用する。
	// ウィンドウモード用のスタイル適用は、必ずフルスクリーンを解除してから行う
	window_->restoreWindow();
	window_->setClientSize(w, h);
	fullscreen_ = false;
	{
		EngineEvent e;
		e.type = EngineEvent::VIDEO_WINDOWED;
		sendEngineEvent(e);
	}
	return true;
}
void Engine::onWindowEvent(WindowEvent *e) {
	if (e->type == WindowEvent::WINDOW_SIZE) {
		mark_resize_ = true;
	}
}
/// 描画解像度の変更要求が出ているなら、変更を実行する
/// （ウィンドウのサイズは既に変更されているものとする）
/// 変更を実行した場合は true を返す。実行する必要がないか、失敗した場合は false を返す
bool Engine::processResizeReq() {
	if (! mark_resize_) return false;
	int cw, ch;
	window_->getClientSize(&cw, &ch);
	Video::resetDevice(cw, ch, -1);
	mark_resize_ = false;
	return true;
}
void Engine::processReq() {
	// フルスクリーンモードへの切り替え要求が出ているなら切り替えを実行する
	processFullscreenReq();

	// ウィンドウモードへの切り替え要求が出ているなら切り替えを実行する
	processWindowedReq();

	// 描画解像度が変化しているなら、グラフィックデバイスの設定を更新する
	processResizeReq();
}
void Engine::setWindowTitle(const char *title_u8) {
	window_->setTitle(title_u8);
}
void Engine::moveWindow(int w, int h) {
	// ウィンドウモードでのみ移動可能
	if (fullscreen_) return;
	if (window_ == NULL) return;
	if (window_->getAttribute(Window::FULLFILL)) return;
	if (window_->isMaximized()) return;
	if (window_->isIconified()) return;
	if (!window_->isWindowFocused()) return;
	window_->setWindowPosition(w, h);
}
void Engine::resizeWindow(int w, int h) {
	if (fullscreen_) {
		mark_tofullscreen_ = false;
		mark_towindowed_ = true;
	}
	processReq(); // 今すぐに切り替える
	window_->setAttribute(Window::HAS_BORDER, 1);
	window_->setAttribute(Window::HAS_TITLE, 1);
	window_->setAttribute(Window::FULLFILL, 0);
	window_->setClientSize(w, h);
}
void Engine::maximizeWindow() {
	// 通常の最大化
	if (fullscreen_) {
		mark_tofullscreen_ = false;
		mark_towindowed_ = true;
	}
	window_->setAttribute(Window::HAS_BORDER, 1);
	window_->setAttribute(Window::HAS_TITLE, 1);
	window_->setAttribute(Window::FULLFILL, 0);
	window_->maximizeWindow();
}
void Engine::restoreWindow() {
	if (fullscreen_) {
		mark_tofullscreen_ = false;
		mark_towindowed_ = true;
	}
	window_->setAttribute(Window::HAS_BORDER, 1);
	window_->setAttribute(Window::HAS_TITLE, 1);
	window_->setAttribute(Window::FULLFILL, 0);
	window_->restoreWindow();
}
void Engine::setFullscreen() {
	if (! fullscreen_) {
		mark_tofullscreen_ = true;
		mark_towindowed_ = false;
	}
}
void Engine::setWindowedFullscreen() {
	if (fullscreen_) {
		mark_tofullscreen_ = false;
		mark_towindowed_ = true;
	}
	processReq(); // 今すぐに切り替える
	window_->setAttribute(Window::HAS_BORDER, 0);
	window_->setAttribute(Window::HAS_TITLE, 0);
	window_->setAttribute(Window::FULLFILL, 1);
	window_->maximizeWindow();
}

/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）の座標を、
/// ウィンドウクライアント座標系（左上原点、y軸下向き）で表す。
/// ただし z 座標は加工しない
Vec3 Engine::identityToWindowClientPoint(const Vec3 &p) const {
	Log_assert(screen_);
	RectF vp = screen_->getGameViewportRect();
	Vec3 out;
	out.x = Num_lerp(vp.xmin, vp.xmax, Num_normalize(p.x, -1.0f, 1.0f));
	out.y = Num_lerp(vp.ymin, vp.ymax, Num_normalize(p.y, 1.0f, -1.0f)); // Ｙ軸反転。符号に注意
	out.z = p.z;
	return out;
}

/// ウィンドウクライアント座標系（左上原点、y軸下向き）の座標を、
/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）で表す。
/// ただし z 座標は加工しない
Vec3 Engine::windowClientToIdentityPoint(const Vec3 &p) const {
	Log_assert(screen_);
	RectF vp = screen_->getGameViewportRect();
	if (vp.getSizeX() == 0) return Vec3();
	if (vp.getSizeY() == 0) return Vec3();
	Vec3 out;
	out.x = Num_lerp(-1.0f, 1.0f, Num_normalize(p.x, vp.xmin, vp.xmax));
	out.y = Num_lerp(1.0f, -1.0f, Num_normalize(p.y, vp.ymin, vp.ymax)); // Ｙ軸反転。符号に注意
	out.z = p.z;
	return out;
}


int Engine::getStatus(Status s) {
	int val = 0;
	switch (s) {
	case ST_FRAME_COUNT_APP: // アプリケーションレベルでの実行フレーム数
		return loop_.getAppFrames();

	case ST_FRAME_COUNT_GAME: // ゲームレベルでの実行フレーム数
		return loop_.getGameFrames();

	case ST_FPS_UPDATE: // 更新関数のFPS値
		loop_.getFps(&val, NULL);
		return val;

	case ST_FPS_RENDER: // 描画関数のFPS値
		loop_.getFps(NULL, &val);
		return val;

	case ST_FPS_REQUIRED:
		return loop_.getFps(NULL, NULL);

	case ST_MAX_SKIP_FRAMES:
		loop_.getFrameskips(&val, NULL);
		return val;

	case ST_WINDOW_FOCUSED:
		return window_->isWindowFocused() ? 1 : 0;

	case ST_WINDOW_POSITION_X:
		window_->getWindowPosition(&val, NULL);
		return val;

	case ST_WINDOW_POSITION_Y:
		window_->getWindowPosition(NULL, &val);
		return val;

	case ST_WINDOW_CLIENT_SIZE_W:
		window_->getClientSize(&val, NULL);
		return val;

	case ST_WINDOW_CLIENT_SIZE_H:
		window_->getClientSize(NULL, &val);
		return val;

	case ST_WINDOW_IS_MAXIMIZED:
		return window_->isMaximized() ? 1 : 0;

	case ST_WINDOW_IS_ICONIFIED:
		return window_->isIconified() ? 1 : 0;

	case ST_VIDEO_IS_FULLSCREEN:
		// スクリーンが fullscreen ならば本物のフルスクリーンになっている
		return fullscreen_;

	case ST_VIDEO_IS_WINDOWED_FULLSCREEN:
		// ウィンドウモード（ボーダレスフルスクリーンは、ウィンドウモードでウィンドウを最大化しているだけなので）
		// かつ境界線が無ければ、ボーダレスフルスクリーン
		if (!fullscreen_) {
			if (!window_->getAttribute(Window::HAS_BORDER)) {
				return true;
			}
		}
		return false;

	case ST_VIDEO_IS_WINDOWED:
		// ウィンドウモードかつボーダレスでなければ、通常ウィンドウモード
		if (!fullscreen_) {
			if (window_->getAttribute(Window::HAS_BORDER)) {
				return true;
			}
		}
		return false;

	case ST_SCREEN_AT_CLIENT_ORIGIN_X:
		{
			POINT p = {0, 0};
			::ClientToScreen((HWND)window_->getHandle(), &p);
			return (int)p.x;
		}

	case ST_SCREEN_AT_CLIENT_ORIGIN_Y:
		{
			POINT p = {0, 0};
			::ClientToScreen((HWND)window_->getHandle(), &p);
			return (int)p.y;
		}

	case ST_SCREEN_W:
		// 現在のモニタのX解像度
		Platform::getScreenSizeByWindow(window_->getHandle(), &val, NULL);
		return val;

	case ST_SCREEN_H:
		// 現在のモニタのY解像度
		Platform::getScreenSizeByWindow(window_->getHandle(), NULL, &val);
		return val;
	}
	return 0;
}


void Engine::run() {
	Log_assert(init_called_);
	Timer_begin();
	loop_.run();
	Timer_end();
}
bool Engine::isPaused() {
	return loop_.isPaused();
}
void Engine::play() {
	loop_.play();
	EngineEvent e;
	e.type = EngineEvent::LOOP_PLAY;
	sendEngineEvent(e);
}
void Engine::playStep() {
	// ステップ実行は、既にポーズされている状態でこそ意味がある。
	// 非ポーズ状態からのステップ実行は、単なるポーズ開始と同じ
	if (!loop_.isPaused()) {
		pause();
		return;
	}

	// ステップ実行
	loop_.playStep();
	EngineEvent e;
	e.type = EngineEvent::LOOP_PLAYPAUSE;
	sendEngineEvent(e);
}
void Engine::pause() {
	loop_.pause();
	EngineEvent e;
	e.type = EngineEvent::LOOP_PAUSE;
	sendEngineEvent(e);
}
void Engine::quit() {
	loop_.quit();
}
void Engine::setFps(int fps, int skip) {
	if (fps >= 0) {
		loop_.setFps(fps);
	}
	if (skip >= 0) {
		loop_.setFrameskips(skip, skip * 100);
	}
}


EID Engine::newId() {
	new_eid_++;
	EID e = (EID)new_eid_;
	entities_.insert(e);
	return e;
}
void Engine::invalidate(EID e) {
	if (e) invalid_.insert(e);
}
void Engine::removeInvalidated() {
	if (invalid_.empty()) return;
	// システムからエンティティを削除
	for (auto it=invalid_.begin(); it!=invalid_.end(); ++it) {
		EID e = *it;
		detachFromAll(e);
		entities_.erase(e);
	}
	// 削除処理終わり
	invalid_.clear();
}
void Engine::removeAll() {
	invalid_.insert(entities_.begin(), entities_.end());
	removeInvalidated();
}
System * Engine::getSystem(int index) {
	if (0 <= index && index < (int)systems_.size()) {
		return systems_[index];
	}
	return NULL;
}
int Engine::getSystemCount() const {
	return (int)systems_.size();
}
void Engine::addSystem(System *s) {
	if (s) {
		systems_.push_back(s);
		unstarted_.push_back(s);
	}
}
void Engine::addScreenCallback(IScreenCallback *cb) {
	Log_assert(init_called_);
	Log_assert(screen_);
	screen_->addCallback(cb);
}
void Engine::addWindowCallback(IWindowCallback *cb) {
	Log_assert(init_called_);
	Log_assert(window_);
	window_->addCallback(cb);
}
void Engine::addEngineCallback(IEngineCallback *cb) {
	cblist_.push_back(cb);
}
void Engine::addVideoCallback(IVideoCallback *cb) {
	Video::addCallback(cb);
}
void Engine::removeScreenCallback(IScreenCallback *cb) {
	Log_assert(init_called_);
	Log_assert(screen_);
	screen_->removeCallback(cb);
}
void Engine::removeWindowCallback(IWindowCallback *cb) {
	Log_assert(init_called_);
	Log_assert(window_);
	window_->removeCallback(cb);
}
void Engine::removeEngineCallback(IEngineCallback *cb) {
	auto it = std::find(cblist_.begin(), cblist_.end(), cb);
	if (it != cblist_.end()) {
		cblist_.erase(it);
	}
}
void Engine::removeVideoCallback(IVideoCallback *cb) {
	Video::removeCallback(cb);
}

void Engine::detachFromAll(EID e) {
	// すべてのシステムからエンティティをデタッチする
	// 念のために、システム追加順とは逆の順番で detach を呼び出す
	for (auto it=systems_.rbegin(); it!=systems_.rend(); ++it) {
		System *s = *it;
		s->onDetach(e);
	}
}
void Engine::callEnd() {
	// onEnd は追加した順番と逆順で呼び出す
	for (auto it=systems_.rbegin(); it!=systems_.rend(); ++it) {
		System *s = *it;
		s->onEnd();
	}
}
void Engine::sendEngineEvent(const EngineEvent &e) {
	for (auto it=cblist_.rbegin(); it!=cblist_.rend(); ++it) {
		IEngineCallback *cb = *it;
		cb->onEngineEvent(e);
	}
}

void Engine::addTimeConsumed(void *s, int msec) {
	consumed_[s] += msec;
}
int Engine::getTimeConsumed(void *s) const {
	auto it = consumed_.find(s);
	if (it != consumed_.end()) {
		return it->second;
	}
	return 0;
}
void Engine::clearTimeConsumed() {
	consumed_.clear();
}
KFileLoader * Engine::getFileSystem() {
	return filesystem_;
}
Screen * Engine::getScreen() {
	return screen_;
}
Inspector * Engine::getInspector() {
	return inspector_;
}
Window * Engine::getWindow() {
	return window_;
}
VideoBank * Engine::videobank() {
	return video_bank_;
}

void Engine::on_loop_frame_start() {
	
	// 入力状態
	KJoystick::update();

	// システムのスタートアップ処理
	if (unstarted_.size() > 0) {
		for (auto it=unstarted_.begin(); it!=unstarted_.end(); ++it) {
			System *s = *it;
			s->onStart();
		}
		unstarted_.clear();
	}

	// シーン設定と切り替えなど
	SceneSystem *scene_sys = findSystem<SceneSystem>();
	if (scene_sys) {
		scene_sys->processReloading();
		scene_sys->processSwitching();
	}

	// gizmoは他のシステムよりも先に最初にアップデートしておく。
	// そうしないと、持続時間が 1 フレームの gizmo を他のシステムから登録したときに
	// gizmo 登録後の最初の描画よりも前にアップデートされ、1フレーム経過したと判断して削除されてしまう
	if (gizmo_) {
		gizmo_->newFrame();
	}

	// onAppFrameUpdate
	KClock cl;
	for (auto it=systems_.begin(); it!=systems_.end(); ++it) {
		cl.reset();
		System *s = *it;
		s->onAppFrameUpdate();
		addTimeConsumed(s, cl.getTimeMsec());
	}
}
void Engine::on_loop_update() {
	KClock cl;

	// onFrameStart
	for (auto it=systems_.begin(); it!=systems_.end(); ++it) {
		cl.reset();
		System *s = *it;
		s->onFrameStart();
		addTimeConsumed(s, cl.getTimeMsec());
	}

	// onFrameUpdate
	for (auto it=systems_.begin(); it!=systems_.end(); ++it) {
		cl.reset();
		System *s = *it;
		s->onFrameUpdate();
		addTimeConsumed(s, cl.getTimeMsec());
	}

	// onFrameLateUpdate
	for (auto it=systems_.begin(); it!=systems_.end(); ++it) {
		cl.reset();
		System *s = *it;
		s->onFrameLateUpdate();
		addTimeConsumed(s, cl.getTimeMsec());
	}
}
void Engine::on_loop_frame_end() {
	KClock cl;
	// onFrameEnd は追加した順番と逆順で呼び出す
	for (auto it=systems_.rbegin(); it!=systems_.rend(); ++it) {
		cl.reset();
		System *s = *it;
		s->onFrameEnd();
		addTimeConsumed(s, cl.getTimeMsec());
	}
}
bool Engine::on_loop_top() {
	return window_ && window_->processEvents();
}
void Engine::on_loop_exit() {
	window_->closeWindow();
}
void Engine::on_loop_render() {
	Log_assert(screen_);

	// ウィンドウが最小化されているなら描画処理しない
	if (window_->isIconified()) {
		sleep_until_ = KClock::getSystemTimeMsec() + 500; // しばらく一時停止
		return;
	}

	// 画面モードの切り替え要求が出ているなら処理する
	processReq();

	// onRenderUpdate
	{
		KClock cl;
		for (auto it=systems_.begin(); it!=systems_.end(); ++it) {
			cl.reset();
			System *s = *it;
			s->onAppRenderUpdate();
			addTimeConsumed(s, cl.getTimeMsec());
		}
	}

	if (screen_->render()) {
		sleep_until_ = 0;
	} else {
		// 描画に失敗した。ゲーム進行を停止しておく
		sleep_until_ = KClock::getSystemTimeMsec() + 500;
	}
}
bool Engine::on_loop_can_update() {
	if (sleep_until_ > 0) {
		// 描画トラブルにより、一定時間更新を停止している。
		// 解除時刻になるまで何もしない
		if (KClock::getSystemTimeMsec() < sleep_until_) {
			return false;
		}
		// 解除時刻になった。更新を再開する
		sleep_until_ = 0;
	}

	// 最小化されているときは描画しない
	if (window_->isIconified()) {
		return false;
	}
	// デバイスロスト状態になっている。しばらくの間更新しない
	int graphics_non_avaiable = 0;
	if (Video::isInitialized()) {
		Video::getParameter(Video::DEVICELOST, &graphics_non_avaiable);
	}
	if (!graphics_non_avaiable) {
		abort_timer_ = -1;
		return true;
	}

	// グラフィックが無効になっている。復帰を試みるが、ゲーム内容の更新はしない
	on_loop_render(); // <-- Reset device を呼ぶために、あえて描画処理を呼んでおく
#if 1
	// 自分が最前面にあるにも関わらずデバイスロストが 15 秒間続いた場合は強制終了する
	if (window_->isWindowFocused()) {
		if (abort_timer_ == -1) {
			int fps = loop_.getFps(NULL, NULL);
			abort_timer_ = 15 * 1000 / fps; // 15秒のフレーム数
		}
		else if (abort_timer_ > 0) {
			abort_timer_--;
		}
		else if (abort_timer_ == 0) {
			// タイトルバーと境界線を付けて、ユーザーがウィンドウを移動したりリサイズできるようにしておく
			window_->setAttribute(Window::HAS_BORDER, true);
			window_->setAttribute(Window::HAS_TITLE, true);
			window_->setAttribute(Window::RESIZABLE, true);
			Platform::dialog(u8"デバイスロストから回復できませんでした。ゲームエンジンを終了します");
			loop_.quit();
		}
	}
#endif
	return false;
}
uint32_t Engine::on_loop_get_msec() {
	return KClock::getSystemTimeMsec();
}
void Engine::on_loop_wait_msec(uint32_t msec) {
	Timer_wait((int)msec);
}
#pragma endregion // Engine


#pragma region Window_state
#define MIN_INTERSECTION_W  64
#define MIN_INTERSECTION_H  32
#define USERINI_KEY_POSX   "Window_Pos_X"
#define USERINI_KEY_POSY   "Window_Pos_Y"
#define USERINI_KEY_SIZEW  "Window_Size_W"
#define USERINI_KEY_SIZEH  "Window_Size_H"
#define USERINI_KEY_ISMAX  "Window_IsMaximized"

void Window_saveState(Window &wnd, KNamedValues &nv) {
	bool maximized = wnd.isMaximized();

	// 最大化やフルスクリーンになっているウィンドウの位置とサイズを取得しても意味がないので、
	// ウィンドウを通常状態に戻しておく
	wnd.restoreWindow();

	// この時点で、通常ウィンドウに戻っているはず。
	// この時の位置とサイズを記録する
	int x, y, w, h;
	wnd.getWindowPosition(&x, &y);
	wnd.getClientSize(&w, &h);

	// 現在のウィンドウ状態を保存する
	nv.setInteger(USERINI_KEY_ISMAX, maximized);
	nv.setInteger(USERINI_KEY_POSX, x);
	nv.setInteger(USERINI_KEY_POSY, y);
	nv.setInteger(USERINI_KEY_SIZEW, w);
	nv.setInteger(USERINI_KEY_SIZEH, h);
}
bool Window_loadState(Window &wnd, const KNamedValues &nv) {
	// 前回終了時のウィンドウ状態を復元。
	// 不正な値だったり、画面外にウィンドウが行ってしまっていた場合は自動的にリセットされる
	// ウィンドウ位置とサイズを復元する
	// マルチディスプレイ環境では、プライマリモニタの左側や上側にセカンダリモニタを配置した場合にスクリーン座標が負の値になる事に注意
	int x = nv.getInteger(USERINI_KEY_POSX, 0); // 通常状態（非最大化＆非最小化）でのウィンドウ座標
	int y = nv.getInteger(USERINI_KEY_POSY, 0);
	int w = nv.getInteger(USERINI_KEY_SIZEW, 0); // 通常状態（非最大化＆非最小化）でのウィンドウサイズ（クライアント領域ではない）
	int h = nv.getInteger(USERINI_KEY_SIZEH, 0);
	
#ifdef _WIN32
	// 作成可能な最小ウィンドウサイズでチェック
	{
		int min_w = GetSystemMetrics(SM_CXMINIMIZED);
		int min_h = GetSystemMetrics(SM_CYMINIMIZED);
		if (w < min_w) return false;
		if (h < min_h) return false;
	}

	// 作成可能な最大ウィンドウサイズでチェック
	{
		int max_w = GetSystemMetrics(SM_CXMAXIMIZED);
		int max_h = GetSystemMetrics(SM_CYMAXIMIZED);
		if (w > max_w) return false;
		if (h > max_h) return false;
	}

	// 画面の外に出ていたらダメ
	{
		// 画面とウィンドウが重なっている部分のサイズを得る
		int intersection_w = 0;
		int intersection_h = 0;
		RECT rect = {x, y, x+w, y+h};
		HMONITOR monitor = ::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		::GetMonitorInfoA(monitor, &info);
		RECT intersection;
		if (::IntersectRect(&intersection, &rect, &info.rcMonitor)) {
			intersection_w = intersection.right - intersection.left;
			intersection_h = intersection.bottom - intersection.top;
		}

		// 重なりが小さすぎる場合は、ウィンドウの大部分が画面外にあるということ
		if (intersection_w < MIN_INTERSECTION_W) return false;
		if (intersection_h < MIN_INTERSECTION_H) return false;
	}
#endif // _WIN32
	// OK
	if (nv.getBool(USERINI_KEY_ISMAX, false)) {
		if (0) {
			// 最大化状態から開始する
			wnd.maximizeWindow();
		} else {
			// デフォルト状態から開始する
			return false;
		}
	} else {
		wnd.setWindowPosition(x, y);
		wnd.setClientSize(w, h);
	}
	return true;
}
#pragma endregion // Window_state


} // namespace
