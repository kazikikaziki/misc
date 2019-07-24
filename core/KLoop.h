#pragma once
#include <inttypes.h>


namespace mo {

class KLoopCallback {
public:
	/// フレーム先頭で毎回呼ばれる
	/// ゲームループを継続すべきかどうかを返す
	/// false を返した場合、アプリケーションが終了する
	virtual bool on_loop_top() { return true; }

	/// 更新してもよいかどうかの問い合わせが発生したときに呼ばれる
	/// false を返した場合、そのフレームは何もせずにスキップし、描画も行われない
	virtual bool on_loop_can_update() { return true; }

	/// フレーム先頭で呼ばれる
	virtual void on_loop_frame_start() {}

	/// 更新が必要な時に呼ばれる
	virtual void on_loop_update() {}

	/// 描画が必要な時に呼ばれる
	virtual void on_loop_render() {}

	/// フレーム終端で呼ばれる
	virtual void on_loop_frame_end() {}

	/// ゲームループ終了時に呼ばれる
	virtual void on_loop_exit() {}

	///
	virtual void on_loop_wait_msec(uint32_t msec) {}

	///
	virtual uint32_t on_loop_get_msec() { return 0; }

};

class KLoop {
public:
	KLoop();
	void init(KLoopCallback *cb);
	void shutdown() {}

	/// 描画すべきかどうか判定する
	/// FPS の設定と前回描画からの経過時間を調べ、
	/// 今が描画すべきタイミングであれば true を返す
	bool shouldRender();

	/// アプリケーションレベルでの実行フレーム番号を返す
	int getAppFrames();

	/// ゲームレベルでの実行フレーム数を返す
	int getGameFrames();

	/// アプリケーションの実行時間（秒）を返す
	float getAppTimeSeconds();

	/// ゲームの実行時間（秒）を返す
	float getGameTimeSeconds();

	/// フレームスキップ設定
	/// max_skip_frames -- 最大スキップフレーム数。このフレーム数スキップが続いた場合は、１度描画してスキップ状態をリセットする
	/// max_skip_msec -- 最大スキップ時間（ミリ秒）この時間以上経過していたら、１度描画してスキップ状態をリセットする
	void setFrameskips(int max_skip_frames, int max_skip_msec);
	void getFrameskips(int *max_skip_frames, int *max_skip_msec) const;

	/// FPSを設定する
	void setFps(int fps);
	
	/// FPS値を取得する（setFps で設定されたもの）
	/// 引数を指定すると、追加情報が得られる
	/// fps_update -- 更新関数のFPS値
	/// fps_rebder -- 描画関数のFPS値
	int getFps(int *fps_update, int *fps_render);

	/// 強制スローモーションを設定する
	/// interval -- 更新間隔。通常のゲームフレーム単位で、interval フレームに１回だけ更新するよう設定する
	/// duration -- 継続フレーム数。スロー状態のフレームカウンタで duration フレーム経過するまでスローにする
	void setSlowMotion(int interval, int duration);

	/// ポーズ中かどうか
	bool isPaused();

	/// ポーズ解除
	void play();

	/// ループを１フレームだけ進める
	void playStep();

	/// ポーズする（更新は停止するが、描画は継続する）
	void pause();

	/// ループを終了する
	void quit();

	/// ループ先頭での処理を行う。
	/// ループを継続してよいなら true を返す
	bool beginFrame();

	/// ループ本体の処理を行う
	void stepFrame();

	/// ループ終端の処理を行う
	void endFrame();

	/// ループ開始
	void run();

	/// 次のフレームを開始するまでに待機するべきミリ秒数を返す
	int getWaitMsec();

private:
	uint32_t last_clock_;
	KLoopCallback *cb_;
	int run_stat_;
	int app_clock_;
	int game_clock_;
	int fps_update_;
	int fps_render_;
	int fps_required_;
	int fpstime_timeout_;
	int fpstime_next_;
	int num_update_;
	int num_render_;
	int slow_motion_timer_;
	int slow_motion_interval_;
	int num_skips_; // 現在の連続描画スキップ数。
	int max_skip_frames_; // 最大の連続描画スキップ数。この回数に達した場合は、必ず一度描画する
	int max_skip_msec_; // 最大の連続描画スキップ時間（ミリ秒）。直前の描画からこの時間が経過していた場合は、必ず一度描画する
	bool paused_;
	bool step_once_;
	bool exit_required_;
	bool game_update_;
	bool nowait_;
};

} // namespace
