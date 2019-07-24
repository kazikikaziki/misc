#pragma once
#include "GameId.h"
#include "KLoop.h"
#include "KVec3.h"
#include "KWindow.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

/// ゲームエンジンの名前空間
namespace mo {

class IBuildCallback;
class IScreenCallback;
class IVideoCallback;
class Gizmo;
class Inspector;
class Screen;
class KFileLoader;
class Window;
class VideoBank;
class System;
class KNamedValues;
class Engine;

struct EngineDef {
	EngineDef() {
		window = NULL;
		screen = NULL;
		filesystem = NULL;
		gizmo = NULL;
		inspector = NULL;
		fps = 60;
		resolution_x = 640;
		resolution_y = 480;
	}

	Window *window;
	Screen *screen;
	KFileLoader *filesystem;
	Gizmo *gizmo;
	Inspector *inspector;
	int fps;
	int resolution_x;
	int resolution_y;
};

struct EngineEvent {
	EngineEvent() {
		type = NONE;
		engine = NULL;
	}
	enum Type {
		NONE,
		LOOP_PLAY,
		LOOP_PLAYPAUSE,
		LOOP_PAUSE,
		VIDEO_FULLSCREEN,
		VIDEO_WINDOWED,
	};
	Type type;
	Engine *engine;
};

class IEngineCallback {
public:
	virtual void onEngineEvent(const EngineEvent &e) = 0;
};

class Engine: public KLoopCallback, public IWindowCallback {
public:
	Engine();
	virtual ~Engine();
	bool init(const EngineDef &params);
	void shutdown();
	KFileLoader * getFileSystem();
	Screen * getScreen();
	Inspector * getInspector();
	Window * getWindow();
	VideoBank * videobank();
	Gizmo * getGizmo() { return gizmo_; }

	/////////////////////////////////////////////

	/// 新しい EID を生成する
	EID newId();

	/// 無効化された EID を削除する
	void removeInvalidated();

	/// すべてのエンティティを削除する
	void removeAll();

	/// エンティティを無効化する
	void invalidate(EID e);

	/////////////////////////////////////////////

	/// システムを追加する
	/// ※システムは一度追加したら削除することはできない
	void addSystem(System *s);

	/// コールバックを登録する
	void addScreenCallback(IScreenCallback *cb);
	void addWindowCallback(IWindowCallback *cb);
	void addEngineCallback(IEngineCallback *cb);
	void addVideoCallback(IVideoCallback *cb);

	/// コールバックを解除する
	void removeScreenCallback(IScreenCallback *cb);
	void removeWindowCallback(IWindowCallback *cb);
	void removeEngineCallback(IEngineCallback *cb);
	void removeVideoCallback(IVideoCallback *cb);

	/// システムの数を返す
	int getSystemCount() const;

	/// インデックスを指定してシステムを取得する
	System * getSystem(int index);

	/// T または T と互換性のあるシステムを返す。
	/// T は System の直接または派生クラスでないといけない。
	/// T に System を指定すると全システムが該当してしまう事に注意
	template <class T> T * findSystem();

	/////////////////////////////////////////////

	/// ゲームループを実行する。
	/// 実行終了するまで処理は戻ってこない
	void run();

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

	/// 更新サイクルなどを指定する
	/// fps: 1秒あたりの更新＆描画回数: -1 を指定すると既存の FPS 値をそのまま使う（FPS変更しない）
	/// skip: フレームスキップを許可する場合、1秒間での最大スキップ可能フレーム数。
	///  0 を指定するとスキップしない。1以上の値 n を指定すると、1秒間に最大で n 回の描画を省略する。
	/// -1 を指定すると、既存のSKIP値を変更せずに使う
	/// 例えば FPS を変更せずに SKIP だけ 0 にしたいなら、 setFps(-1, 0) のようにする
	void setFps(int fps, int skip);

	/////////////////////////////////////////////

	/// ウィンドウのタイトル文字列を指定する
	void setWindowTitle(const char *title_u8);

	/// ウィンドウのサイズを変更する
	void resizeWindow(int w, int h);

	/// ウィンドウ位置を設定する
	void moveWindow(int x, int y);

	/// ウィンドウを最大化する
	void maximizeWindow();

	/// ウィンドウ化する
	/// ウィンドウが最大化または最小化の場合に効果がある。
	/// フルスクリーンモードの場合は、自動的に解除される
	void restoreWindow();

	/// フルスクリーンにする
	/// フルスクリーンを解除する時は restoreWindow を使う
	void setFullscreen();

	/// 疑似フルスクリーンにする。
	/// ウィンドウを境界線なし最大化＆最前面状態にする。
	/// （ビデオモードは Windowed のままで、ウィンドウだけフルスクリーンにする）
	/// フルスクリーンを解除する時は restoreWindow を使う
	void setWindowedFullscreen();


	/////////////////////////////////////////////
	/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）の座標を、
	/// ウィンドウクライアント座標系（左上原点、y軸下向き）で表す。
	/// ただし z 座標は加工しない
	Vec3 identityToWindowClientPoint(const Vec3 &p) const;

	/// ウィンドウクライアント座標系（左上原点、y軸下向き）の座標を、
	/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）で表す。
	/// ただし z 座標は加工しない
	Vec3 windowClientToIdentityPoint(const Vec3 &p) const;

	/////////////////////////////////////////////

	enum Status {
		ST_NONE,
		ST_FRAME_COUNT_APP,  // アプリケーションレベルでの実行フレーム数
		ST_FRAME_COUNT_GAME, // ゲームレベルでの実行フレーム数
		ST_FPS_UPDATE, // 更新関数のFPS値
		ST_FPS_RENDER, // 描画関数のFPS値
		ST_FPS_REQUIRED, // 要求されたFPS値
		
		// 指定されたFPSで処理できなかった場合に、1秒間でスキップできる描画フレームの最大数。
		// 0 ならスキップなし
		// 1以上の値 n を指定すると、1秒あたり最大 n フレームまでスキップする
		ST_MAX_SKIP_FRAMES,

		ST_WINDOW_POSITION_X, // ウィンドウX座標
		ST_WINDOW_POSITION_Y, // ウィンドウY座標
		ST_WINDOW_CLIENT_SIZE_W, // ウィンドウのクライアント幅
		ST_WINDOW_CLIENT_SIZE_H, // ウィンドウのクライアント高さ
		ST_WINDOW_IS_MAXIMIZED,  // ウィンドウが最大化されている
		ST_WINDOW_IS_ICONIFIED,  // ウィンドウが最小化されている
		ST_WINDOW_FOCUSED, // ウィンドウが入力フォーカスを持っているかどうか

		ST_VIDEO_IS_FULLSCREEN, // フルスクリーンモード
		ST_VIDEO_IS_WINDOWED_FULLSCREEN, // 疑似フルスクリーン（ウィンドウを境界線なし最大化かつ最前面にしている）
		ST_VIDEO_IS_WINDOWED,  // ウィンドウモード

		ST_SCREEN_AT_CLIENT_ORIGIN_X, // クライアント座標の原点に対応するスクリーン座標
		ST_SCREEN_AT_CLIENT_ORIGIN_Y,

		ST_SCREEN_W, // 現在のモニタ（ゲームウィンドウを含んでいるモニタ）の解像度
		ST_SCREEN_H,
	};

	int getStatus(Status s);

	/////////////////////////////////////////////

	/// システムの onEnd() を呼ぶ
	/// （onEnd は addSystem した順番と逆順で呼ばれる）
	void callEnd();
	void sendEngineEvent(const EngineEvent &e);

	void clearTimeConsumed();
	void addTimeConsumed(void *s, int msec);
	int getTimeConsumed(void *s) const;


	// IWindowCallback
	virtual void onWindowEvent(WindowEvent *e) override;

	// KLoopCallback
	virtual bool on_loop_top() override;
	virtual void on_loop_frame_start() override;
	virtual void on_loop_render() override;
	virtual void on_loop_update() override;
	virtual void on_loop_frame_end() override;
	virtual void on_loop_exit() override;
	virtual bool on_loop_can_update() override;
	virtual uint32_t on_loop_get_msec() override;
	virtual void on_loop_wait_msec(uint32_t msec) override;

	IBuildCallback *build_callback_;

private:
	void zeroClear();
	void detachFromAll(EID e);

	/// モード切替要求を今すぐ処理する
	void processReq();
	bool processFullscreenReq();
	bool processWindowedReq();
	bool processResizeReq();

	std::vector<System *> systems_; // バインドされているシステム
	std::vector<System *> unstarted_; // まだ onStart を呼んでいないシステム
	std::unordered_map<void *, int> consumed_;
	std::unordered_set<EID> invalid_;
	std::unordered_set<EID> entities_;
	std::vector<IEngineCallback *> cblist_;
	KLoop loop_;
	
	VideoBank *video_bank_;
	KFileLoader *filesystem_;
	Screen *screen_;
	Inspector *inspector_;
	Window *window_;
	Gizmo *gizmo_;
	int new_eid_;
	int abort_timer_;
	int sleep_until_;
	bool using_own_video_;  // VIDEOを終了させる義務がある
	bool using_own_window_; // ウィンドウを終了させる義務がある
	bool using_own_screen_; // スクリーンを終了させる義務がある
	bool using_own_sound_; // サウンドデバイスを終了させる義務がある
	bool fullscreen_;
	bool mark_tofullscreen_;
	bool mark_towindowed_;
	bool mark_resize_;
	bool init_called_;
};

// テンプレートの実装部
template <class T> T * Engine::findSystem() {
	// 指定されたままの型で探す
	for (auto it=systems_.begin(); it!=systems_.end(); ++it) {
		System *s = *it;
		if (typeid(*s) == typeid(T)) {
			return (T*)s;
		}
	}
	// みつからない。
	// 指定された型が上位型である可能性がある
	// ひとつひとつ dynamic_cast でのキャストを試みる
	for (auto it=systems_.begin(); it!=systems_.end(); ++it) {
		System *s = *it;
		T *t = dynamic_cast<T*>(s);
		if (t) {
			// ここで、もしも T が System だった場合、systems_ の全てのオブジェクトが該当してしまうことに注意
			// （当然ながら、systems_ に入っているオブジェクトはすべて System を継承している）
			// ここでは最初に見つかったものだけを返す
			return t;
		}
	}
	return NULL;
}



void Window_saveState(Window &wnd, KNamedValues &nv);
bool Window_loadState(Window &wnd, const KNamedValues &nv);


extern Engine *g_engine;



} // namespace







