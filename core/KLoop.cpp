#include "KLoop.h"
#include "KLog.h"

namespace mo {


KLoop::KLoop() {
	init(NULL);
}
void KLoop::init(KLoopCallback *cb) {
	Log_trace;
	cb_ = cb;
	last_clock_ = 0;
	fps_required_ = 60;
	fps_update_ = 0;
	fps_render_ = 0;
	app_clock_ = -1; // インクリメントして 0 になるように
	game_clock_ = -1; // インクリメントして 0 になるように
	num_update_ = 0;
	num_render_ = 0;
	num_skips_ = 0;
	max_skip_frames_ = 0;
	max_skip_msec_ = 0;
	slow_motion_timer_ = 0;
	slow_motion_interval_ = 2;
	exit_required_ = false;
	game_update_ = false;
	paused_ = false;
	step_once_ = false;
	nowait_ = false;
	fpstime_next_ = 0; // reset
	fpstime_timeout_ = 500;
}
void KLoop::setFps(int fps) {
	fps_required_ = fps;
	fpstime_next_ = 0; // reset
}
int KLoop::getFps(int *fps_update, int *fps_render) {
	if (fps_update) *fps_update = fps_update_;
	if (fps_render) *fps_render = fps_render_;
	return fps_required_;
}
int KLoop::getAppFrames() {
	return app_clock_;
}
int KLoop::getGameFrames() {
	return game_clock_;
}
float KLoop::getAppTimeSeconds() {
	return (float)getAppFrames() / fps_required_;
}
float KLoop::getGameTimeSeconds() {
	return (float)getGameFrames() / fps_required_;
}
void KLoop::setFrameskips(int max_skip_frames, int max_skip_msec) {
	max_skip_frames_ = max_skip_frames;
	max_skip_msec_ = max_skip_msec;
}
void KLoop::getFrameskips(int *max_skip_frames, int *max_skip_msec) const {
	if (max_skip_frames) *max_skip_frames = max_skip_frames_;
	if (max_skip_msec) *max_skip_msec = max_skip_msec_;
}
void KLoop::setSlowMotion(int interval, int duration) {
	// スローモーション。スロー状態のフレームカウンタで duration フレーム経過するまでスローにする
	if (duration > 0) {
		slow_motion_timer_ = duration * slow_motion_interval_;
	}
	// スローモーションの更新間隔。
	// 通常のゲームフレーム単位で、interval フレームに１回だけ更新するよう設定する
	if (interval >= 2) {
		slow_motion_interval_ = interval;
	}
}
void KLoop::quit() {
	exit_required_ = true;
}
void KLoop::playStep() {
	if (isPaused()) {
		paused_ = false; // 実行させるためにポーズフラグを解除
		step_once_ = true;
	} else {
		pause();
	}
}
void KLoop::pause() {
	step_once_ = false;
	paused_ = true;
}
void KLoop::play() {
	step_once_ = false;
	paused_ = false;
}
bool KLoop::isPaused() {
	return paused_;
}
bool KLoop::beginFrame() {
	Log_assert(cb_);
	return cb_->on_loop_top();
}
int KLoop::getWaitMsec() {
	const uint32_t delta = 1000 / fps_required_;
	const uint32_t t = cb_->on_loop_get_msec();
	if (t + fpstime_timeout_ < fpstime_next_) {
		// 現在時刻に最大待ち時間を足しても、次の実行予定時間に届かない（実行予定時刻が遠すぎる）
		fpstime_next_ = t + delta;
		return 0;
	}
	if (fpstime_next_ + delta < t) {
		// 実行予定時刻をすでに過ぎている（実行が遅れている）
		fpstime_next_ = t + delta;
		return 0;
	}
	if (fpstime_next_ <= t) {
		// 実行予定時刻になった
		fpstime_next_ += delta;
		return 0;
	}
	return fpstime_next_ - t;
}
void KLoop::endFrame() {
	uint32_t time = cb_->on_loop_get_msec();
	if (time >= last_clock_ + 1000) {
		last_clock_ = time;
		fps_update_ = num_update_;
		fps_render_ = num_render_;
		num_update_ = 0;
		num_render_ = 0;
	}
	if (exit_required_) {
		return;
	}
	if (! nowait_) {
		while (getWaitMsec() > 0) {
			cb_->on_loop_wait_msec(1);
		}
	}
}
void KLoop::run() {
	Log_assert(cb_);
	while (beginFrame()) {
		stepFrame();
		endFrame();
	}
	cb_->on_loop_exit();
}
bool KLoop::shouldRender() {
	Log_assert(cb_);
	int msec_formal = 1000 * num_update_ / fps_required_;
	int msec_actual = (int)(cb_->on_loop_get_msec() - last_clock_);
	int max_skip_msec = msec_formal * 10;

	if (msec_actual <= msec_formal) {
		// 前回の描画からの経過時間が規定時間内に収まっている。描画する
		num_skips_ = 0;

	} else if (max_skip_msec <= msec_actual) {
		// 前回の描画から時間が経ちすぎている。スキップせずに描画する
		num_skips_ = 0;

	} else if (max_skip_frames_ <= num_skips_) {
		// 最大連続スキップ回数を超えた。描画する
		num_skips_ = 0;

	} else {
		// 描画スキップする
		num_skips_++;
	}
	if (num_skips_ > 0) {
		return false;
	}
	num_render_++;
	num_skips_ = 0;
	return true;
}
void KLoop::stepFrame() {
	Log_assert(cb_);
	if (!cb_->on_loop_can_update()) return;
	// アプリケーションのフレームカウンタを更新
	// デバッグ目的などでゲーム進行が停止している間でも常にカウントし続ける
	app_clock_++;
	cb_->on_loop_frame_start();
	bool stepnext = !isPaused();
	if (stepnext) {
		if (slow_motion_timer_ > 0) {
			// スローモーション有効な場合は slow_motion_interval_ 回に１回だけ更新処理する
			stepnext = (slow_motion_timer_ % slow_motion_interval_ == 0);
			slow_motion_timer_--;
		}
	}
	if (stepnext) {
		// ゲーム進行用のフレームカウンタを更新
		game_clock_++;

		// ステップ実行した場合
		if (step_once_) {
			step_once_ = false; // ステップ実行解除
			paused_ = true; // そのままポーズ状態へ
		}

		cb_->on_loop_update();

		num_update_++;
	}
	if (shouldRender()) {
		cb_->on_loop_render();
	}
	cb_->on_loop_frame_end();
}


} // namespace
