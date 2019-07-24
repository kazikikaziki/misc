#include "GameSnapshot.h"
//
#include "Engine.h"
#include "Screen.h"
#include "VideoBank.h"
#include "KFile.h"
#include "KImgui.h"
#include "KLog.h"
#include "KMenu.h"
#include "KPlatform.h"
#include "KStd.h"
#include "KVideo.h"

namespace mo {

SnapshotSystem::SnapshotSystem() {
	engine_ = NULL;
	with_time_  = false;
	with_frame_ = false;
	basename_ = "__snapshot";
	do_shot_ = false;
}
void SnapshotSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine_ = engine;
	menu_ = engine->getWindow()->getMenu();
	with_time_  = true;
	with_frame_ = true;
	do_shot_ = false;
	if (menu_) {
		menu_->addSeparator();
		menu_->addItem(L"スナップショット\tPrtScr");
	}
}
int SnapshotSystem::getFrameNumber() const {
	Log_assert(engine_);
	return engine_->getStatus(Engine::ST_FRAME_COUNT_GAME);
}
void SnapshotSystem::onStart() {
	engine_->addWindowCallback(this);
}
void SnapshotSystem::onEnd() {
	engine_->removeWindowCallback(this);
}
void SnapshotSystem::onWindowEvent(WindowEvent *e) {
	switch (e->type) {
	case WindowEvent::KEY_DOWN:
		if (e->key_ev.key == KKeyboard::SNAPSHOT) {
			do_shot_ = true;
		}
		break;
	}
}
void SnapshotSystem::onAppFrameUpdate() {
	if (do_shot_) {
		capture(L"");
		captureNow(capture_filename_);
		do_shot_ = false;
	}
}
void SnapshotSystem::onInspectorGui() {
#ifndef NO_IMGUI
	if (with_time_ || with_frame_) {
		// ファイル名にフレーム番号または時刻を含んでいる。
		// 時間によってファイル名が変化するため GUI を表示しているあいだは常に更新する
		next_output_name_.clear(); // consumed this name
	}
	ImGui::Text("Next output file: ");
	ImGui::Text("%s", _getNextName().u8());
	if (ImGui::Checkbox("Include local time", &with_time_)) {
		next_output_name_.clear(); // consumed this name
	}
	if (ImGui::Checkbox("Include frame number", &with_frame_)) {
		next_output_name_.clear(); // consumed this name
	}
	if (ImGui::Button("Snap shot!")) {
		capture(L"");
	}
	if (Platform::fileExists(last_saved_filename_)) {
		std::string s = K_sprintf("Open: %s", last_saved_filename_.u8());
		if (ImGui::Button(s.c_str())) {
			Platform::openFileInSystem(last_saved_filename_);
		}
	}
#endif // !NO_IMGUI
}
void SnapshotSystem::capture(const Path &filename) {
	if (filename.empty()) {
		capture_filename_ = _getNextName();
		next_output_name_.clear(); // consumed this name
	} else {
		capture_filename_ = filename;
	}
}
void SnapshotSystem::captureNow(const Path &_filename) {
	Path filename = _filename;
	if (filename.empty()) {
		filename = _getNextName();
		next_output_name_.clear(); // consumed this name
	}

	if (!texture_name_.empty()) {
		// キャプチャするテクスチャが指定されている
		TEXID tex = engine_->videobank()->getTexture(texture_name_);
		KImage image = Video::exportTextureImage(tex);
		if (!image.empty()) {
			std::string png = image.saveToMemory();
			FileOutput(filename).write(png);
			Log_infof("Texture image saved %s", filename.getFullPath().u8());
			last_saved_filename_ = filename;
		}
	} else {
		// キャプチャ対象が未指定
		// 既定の画面を書き出す
		Screen *screen = engine_->getScreen();
		screen->postExportScreenTexture(filename); // 次回更新時に保存される。本当に保存されるか確定できないのでログはまだ出さない
	}
}
void SnapshotSystem::setCaptureTargetRenderTexture(const Path &texname) {
	texture_name_ = texname;
}
const Path & SnapshotSystem::_getNextName() const {
	if (next_output_name_.empty()) {
		Path name = basename_;

		// 時刻情報を付加する
		if (with_time_) {
			std::string mb = KLocalTime::now().format("(%y%m%d-%H%M%S)");
			Path time = Path::fromUtf8(mb.c_str());
			name = name.joinString(time);
		}

		// フレーム番号を付加する
		if (with_frame_) {
			Path frame = Path::fromFormat("[04%d]", getFrameNumber());
			name = name.joinString(frame);
		}

		// ファイル名を作成
		next_output_name_ = name.joinString(".png");

		// 重複を確認する。重複していればインデックス番号を付加する
		int num = 1;
		while (Platform::fileExists(next_output_name_)) {
			next_output_name_ = Path::fromFormat("%s_%d.png", name.u8(), num);
			num++;
		}
	}
	return next_output_name_;
}

} // namespace