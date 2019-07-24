#pragma once
#include "system.h"
#include "KPath.h"
#include "KWindow.h" // IWindowCallback

namespace mo {

class Engine;
class KMenu;

class SnapshotSystem: public System, public IWindowCallback {
public:
	SnapshotSystem();
	void init(Engine *engine);

	/// このフレームの最後に画面をキャプチャ―する。
	/// 「画面」とは、setCaptureRenderTexture で指定された
	/// レンダーテクスチャの内容である。
	void capture(const Path &filename=Path::Empty);

	/// 現在の画面を直ちにキャプチャーする
	/// 「画面」とは、setCaptureRenderTexture で指定された
	/// レンダーテクスチャの内容である
	void captureNow(const Path &filename=Path::Empty);

	/// 画面として使用するレンダーテクスチャを指定する
	/// 空文字列を指定した場合は、既定のスクリーン画像をキャプチャーする
	void setCaptureTargetRenderTexture(const Path &texname);

	// IWindowCallback
	virtual void onWindowEvent(WindowEvent *e) override;

	virtual void onStart() override;
	virtual void onEnd() override;
	virtual void onAppFrameUpdate() override;
	virtual void onInspectorGui() override;

	Path basename_;
	bool with_time_;
	bool with_frame_;

private:
	int getFrameNumber() const;
	const Path & _getNextName() const;

	mutable Path next_output_name_;
	Path texture_name_;
	Path capture_filename_;
	Path last_saved_filename_;
	Engine *engine_;
	KMenu *menu_;
	bool do_shot_;
};


}