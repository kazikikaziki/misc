#include "GameAnimationCurve.h"
//
#include "GameSprite.h"
#include "VideoBank.h"
#include "Engine.h"
#include "KEasing.h"
#include "KFile.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNum.h"
#include "KPerlin.h"
#include "KStd.h"

namespace mo {

#pragma region AnimationCurve
AnimationCurve::AnimationCurve() {
}
int AnimationCurve::getSegmentIndexByFrame(float framef) const {
	int frame = (int)floorf(framef);
	if (frame < 0) {
		return 0;
	}
	int count = getSegmentCount();
	if (count > 0) {
		int pos = 0;
		for (int i=0; i<count; i++) {
			int dur = getSegmentDuration(i);
			if (frame < pos + dur) {
				return i;
			}
			pos += dur;
		}
		return count - 1;
	}
	return 0;
}
#pragma endregion // AnimationCurve


#pragma region SpriteAnimationCurve
SpriteAnimationCurve::SpriteAnimationCurve() {
	time_length_ = 0;
}
int SpriteAnimationCurve::getSegmentCount() const {
	return (int)segments_.size();
}
int SpriteAnimationCurve::getDuration() const {
	return time_length_;
}
int SpriteAnimationCurve::getFrameByLabel(const Path &label) const {
	int t = 0;
	for (size_t i=0; i<segments_.size(); i++) {
		const SEG &seg = segments_[i];
		if (seg.name.compare(label) == 0) {
			return t;
		}
		t += seg.duration;
	}
	return -1;
}
const KNamedValues & SpriteAnimationCurve::getUserSegmentParameters(float frame) const {
	int page = getSegmentIndexByFrame(frame);
	const SEG *seg = getSegment(page);
	Log_assert(seg);
	return seg->user_parameters;
}
const SpriteAnimationCurve::SEG * SpriteAnimationCurve::getSegment(int index) const {
	if (0 <= index && index < (int)segments_.size()) {
		return &segments_[index];
	}
	return NULL;
}
void SpriteAnimationCurve::guiState(float frame) const {
#ifndef NO_IMGUI
	int framenumber = 0;
	for (size_t i=0; i<segments_.size(); i++) {
		const SEG &seg = segments_[i];
		ImColor c = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
		if ((framenumber <= frame) && (frame < framenumber + seg.duration)) {
			c = ImColor(1.0f, 0.1f, 0.1f, 1.0f);
		}
		ImGui::PushID(ImGui_ID(i));
		guiSegment(seg, (const float *)&c, framenumber);
		ImGui::PopID();
		framenumber += seg.duration;
	}
#endif // !NO_IMGUI
}
void SpriteAnimationCurve::guiCurve() const {
#ifndef NO_IMGUI
	for (size_t i=0; i<segments_.size(); i++) {
		const SEG &seg = segments_[i];
		for (size_t k=0; k<seg.layer_sprites.size(); k++) {
			ImGui::Text("[%d] Layer(%d): %s: dur=%d", i, k, seg.layer_sprites[k].u8(), seg.duration);
		}
	}

	// アニメーションカーブをエクスポートする
	if (ImGui::Button("Export")) {
		std::string s;
		s += K_sprintf("TimeLength: %d\n", time_length_);
		s += K_sprintf("Num pages: %d\n", segments_.size());
		for (size_t i=0; i<segments_.size(); i++) {
			s += K_sprintf("Page[%d] {\n", i);
			const SEG &seg = segments_[i];
			s += K_sprintf("\tName: %s\n", seg.name.u8());
			s += K_sprintf("\tDur : %d\n", seg.duration);
			for (size_t j=0; j<seg.layer_sprites.size(); j++) {
				s += K_sprintf("\tLayer[%d]\n", j);
				s += K_sprintf("\t\tSprite : %s\n", seg.layer_sprites[j].u8());
				s += K_sprintf("\t\tLabel  : %s\n", seg.layer_labels[j].u8());
				s += K_sprintf("\t\tCommand: %s\n", seg.layer_commands[j].u8());
				s += K_sprintf("\t\tParams {\n");
				for (auto it=seg.user_parameters.cbegin(); it!=seg.user_parameters.cend(); it++) {
					s += K_sprintf("\t\t\t%s: %s\n", it->first.u8(), it->second.u8());
				}
				s += K_sprintf("\t\t} // Params\n");
				s += K_sprintf("\t} // Layer\n");
			}
			s += K_sprintf("} // Page\n");
		}
		s += '\n';
		FileOutput file;
		file.open("~sprite_curve.txt");
		file.write(s);
	}
#endif // !NO_IMGUI
}
void SpriteAnimationCurve::guiSegment(const SEG &seg, const float *color_rgba, int framenumber) const {
#ifndef NO_IMGUI
	ImColor c = *(ImColor*)color_rgba;
	ImGui::TextColored(c, "F%03d: (DUR=%03d)", framenumber, seg.duration);
	ImGui::Indent();
	for (size_t s=0; s<seg.layer_sprites.size(); s++) {
		ImGui::TextColored(c, "'%s' (Label: %s)", seg.layer_sprites[s].u8(), seg.layer_labels[s].u8());
	}
	ImGui::Unindent();
	if (! seg.user_parameters.empty()) {
		ImGui::Indent();
		if (ImGui::TreeNodeEx("User parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (auto it=seg.user_parameters.cbegin(); it!=seg.user_parameters.cend(); ++it) {
				ImGui::BulletText("%s: '%s'", it->first.u8(), it->second.u8());
			}
			ImGui::TreePop();
		}
		ImGui::Unindent();
	}
#endif // !NO_IMGUI
}
int SpriteAnimationCurve::getSegmentDuration(int index) const {
	const SEG *seg = getSegment(index);
	return seg ? seg->duration : 0;
}
void SpriteAnimationCurve::start(EID e) const {
	RenderSystem *rs = g_engine->findSystem<RenderSystem>();
	SpriteRendererCo *co = rs->getSpriteRenderer(e);
	if (co) {
		// 必要なレイヤー数を用意する
		// そうしないと、ページごとにレイヤー枚数が異なった場合に面倒なことになるほか、
		// 前回のクリップよりもレイヤー数が少なかった場合に、今回使われないレイヤーがそのまま残ってしまう
		int m = getMaxLayerCount();
		co->setLayerCount(m);
	}
}
void SpriteAnimationCurve::animate(EID e, float frame) const {
	RenderSystem *rs = g_engine->findSystem<RenderSystem>();
	SpriteRendererCo *co = rs->getSpriteRenderer(e);
	if (co == NULL) return;

	int page_index = getPageByFrame((int)frame, true);

	const SEG *seg = getSegment(page_index);
	if (seg == NULL) return;

	int numlayers = (int)seg->layer_sprites.size();
	co->setLayerCount(numlayers);
	for (int i=0; i<numlayers; i++) {
		const Path &sprite_name = seg->layer_sprites[i];
		const Path &layer_name = seg->layer_labels[i];
		co->setLayerSprite(i, sprite_name);
		co->setLayerLabel(i, layer_name);

		Sprite *sprite = co->getLayerSprite(i);
		if (sprite && sprite->blend_hint_ != Blend_Invalid) {
			co->getLayerMaterialAnimatable(i)->blend = sprite->blend_hint_;
		}
		#if 0
		// "@blend=add @blend=screen
		if (!seg->layer_commands[i].empty()) {
			if (seg->layer_commands[i].compare("blend=screen") == 0) {
				co->getLayerMaterialAnimatable(i)->blend = Blend_Screen;
			} else {
				co->getLayerMaterialAnimatable(i)->blend = Blend_Alpha;
			}
		}
		#endif
	}
}
void SpriteAnimationCurve::addSegment(const SEG &seg) {
	Log_assert(seg.duration >= 0);
	Log_assert(seg.layer_sprites.size() == seg.layer_labels.size());
	segments_.push_back(seg);
	time_length_ += seg.duration;
}
void SpriteAnimationCurve::addPage(const SEG &seg) {
	addSegment(seg);
}
int SpriteAnimationCurve::getMaxLayerCount() const {
	int bound = 0;
	for (auto it=segments_.begin(); it!=segments_.end(); ++it) {
		int num_layers = (int)it->layer_sprites.size();
		if (num_layers > bound) {
			bound = num_layers;
		}
	}
	return bound;
}
int SpriteAnimationCurve::getPageCount() const {
	return segments_.size();
}
int SpriteAnimationCurve::getFrameByPage(int page) const {
	if (page < 0) return -1; // FAIL

	int frames = 0;
	for (int i=0; i<page; i++) {
		const SEG *seg = getSegment(i);
		if (seg == NULL) return -1; // FAIL
		Log_assert(seg->duration >= 0);
		frames += seg->duration;
	}
	return frames;
}
int SpriteAnimationCurve::getPageByFrame(int frame, bool allow_over_frame) const {
	if (frame < 0) {
		return -1;
	}
	int fr = 0;
	for (size_t i=0; i<segments_.size(); i++) {
		fr += segments_[i].duration;
		if (frame < fr) {
			return i;
		}
	}

	// 総フレーム数を超えていた場合は、最後のセグメントインデックスを返す
	if (allow_over_frame) {
		return (int)segments_.size() - 1;
	}

	return -1;
}
int SpriteAnimationCurve::getSpritesInPage(int page, int max_layers, Path *layer_sprite_names) const {
	if (page < 0) return 0;
	if (page >= (int)segments_.size()) return 0;
	const SEG &seg = segments_[page];
	int n = Num_min(max_layers, (int)seg.layer_sprites.size());
	for (int i=0; i<n; i++) {
		layer_sprite_names[i] = seg.layer_sprites[i];
	}
	return n;
}
int SpriteAnimationCurve::getLayerCount(int page) const {
	if (page < 0) return 0;
	if (page >= (int)segments_.size()) return 0;
	return (int)segments_[page].layer_sprites.size();
}
#pragma endregion //SpriteAnimationCurve

} // namespace

