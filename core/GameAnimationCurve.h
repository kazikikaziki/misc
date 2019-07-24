#pragma once
#include "KRef.h"
#include "KNamedValues.h"
#include "GameId.h"
#include "system.h"

namespace mo {

class KNamedValues;
class AnimationCurve;

class ICurveCallback {
public:
	virtual void on_curve_repeat() = 0;
	virtual void on_curve_done()   = 0;
	virtual void on_curve_detach() = 0;
};

struct AnimationElement {
	enum Flag {
		F_LOOP = 1,
		F_REMOVE_ENTITY_AT_END = 2,
	};
	typedef int Flags;
	AnimationElement() {
		curve = NULL;
		flags = 0;
		cb = NULL;
		frame = 0.0f;
	}
	const AnimationCurve *curve;
	float frame;
	Flags flags;
	ICurveCallback *cb;

	bool hasFlag(Flag f) const {
		return (flags & f) != 0;
	}
	void setFlag(Flag f, bool value) {
		if (value) {
			flags |= f;
		} else {
			flags &= ~f;
		}
	}
};

/// アニメーションカーブ
class AnimationCurve: public virtual KRef {
public:
	AnimationCurve();
	virtual void animate(EID e, float frame) const = 0;
	virtual int  getSegmentCount() const = 0;
	virtual int  getSegmentDuration(int index) const = 0;
	virtual int  getDuration() const = 0;
	virtual void start(EID e) const {}
	virtual void end(EID e) const {}
	virtual int getFrameByLabel(const Path &label) const { return -1; }
	virtual const KNamedValues & getUserSegmentParameters(float frame) const {
		static const KNamedValues s_empty_values;
		return s_empty_values;
	}
	virtual void guiState(float frame) const {}
	virtual void guiCurve() const {}

	int getSegmentIndexByFrame(float frame) const;
};

class SingleSegmentAnimationCurve: public AnimationCurve {
public:
	SingleSegmentAnimationCurve() {
		duration_ = 0;
	}
	virtual int getSegmentCount() const override {
		return 1;
	}
	virtual int getSegmentDuration(int index) const override {
		return (index==0) ? duration_ : 0;
	}
	virtual int getDuration() const override {
		return duration_;
	}
	void setDuration(int dur) {
		duration_ = dur;
	}
protected:
	int duration_;
};

/// スプライトアニメーション
class SpriteAnimationCurve: public AnimationCurve {
public:
	SpriteAnimationCurve();
	virtual void start(EID e) const override;
	virtual void guiState(float frame) const override;
	virtual void guiCurve() const override;
	virtual void animate(EID e, float frame) const override;
	virtual int getSegmentCount() const override;
	virtual int getSegmentDuration(int index) const override;
	virtual const KNamedValues & getUserSegmentParameters(float frame) const override;
	virtual int getFrameByLabel(const Path &label) const override;
	virtual int getDuration() const override;

	struct SEG {
		SEG() {
			duration = 0;
		}

		// このセグメントにつけられた名前。シークする時などに使う。
		// 名前をつける必要がない場合は、空文字列のままでよい
		Path name;

		// このセグメントの再生時間
		int duration;

		// スプライトアニメ用パラメータ
		PathList layer_sprites;
		PathList layer_labels;
		PathList layer_commands;

		// ユーザー定義データ
		KNamedValues user_parameters;
	};

	/// ページを追加する
	void addPage(const SEG &seg);

	/// このアニメに含まれる最大のレイヤー数を返す
	int getMaxLayerCount() const;

	/// このアニメのページ数を返す
	int getPageCount() const;

	/// ページの開始フレーム番号を返す
	int getFrameByPage(int page) const;

	/// フレーム位置に対応したページ番号を返す
	int getPageByFrame(int frame, bool allow_over_frame) const;

	/// ページに含まれるスプライト名を配列で返す（レイヤーが複数枚ある場合はスプライトも複数存在するため）
	int getSpritesInPage(int page, int max_layers, Path *layer_sprite_names) const;

	int getLayerCount(int page) const;
private:
	const SEG * getSegment(int index) const;
	void guiSegment(const SEG &seg, const float *color_rgba, int framenumber) const;
	void addSegment(const SEG &seg);
	std::vector<SEG> segments_;
	int time_length_;
};

} // namespace
