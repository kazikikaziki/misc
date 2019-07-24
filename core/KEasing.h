#pragma once

namespace mo {


/// ある値から別の値への遷移関数をまとめたクラス。
/// 各関数の遷移グラフは https://easings.net/ を参照せよ
class KEasing {
public:

	/// inback, outback, inoutback の係数 s の値。
	///
	/// イージングのマジックナンバーを自分で求めてみた
	/// http://void.heteml.jp/blog/archives/2014/05/easing_magicnumber.html
	static const float BACK_10 ;// 1.70158f; // 10%行き過ぎてから戻る
	static const float BACK_20 ;// 2.59238f; // 20%行き過ぎてから戻る
	static const float BACK_30 ;// 3.39405f; // 30%行き過ぎてから戻る
	static const float BACK_40 ;// 4.15574f; // 40%行き過ぎてから戻る
	static const float BACK_50 ;// 4.89485f; // 50%行き過ぎてから戻る
	static const float BACK_60 ;// 5.61962f; // 60%行き過ぎてから戻る
	static const float BACK_70 ;// 6.33456f; // 70%行き過ぎてから戻る
	static const float BACK_80 ;// 7.04243f; // 80%行き過ぎてから戻る
	static const float BACK_90 ;// 7.74502f; // 90%行き過ぎてから戻る
	static const float BACK_100;// 8.44353f; //100%行き過ぎてから戻る

	/// 線形補間。lerp(a, b, t) と同じ
	static float linear(float t, float a, float b);

	static float insine(float t, float a, float b);
	static float outsine(float t, float a, float b);
	static float inoutsine(float t, float a, float b);
	static float inquad(float t, float a, float b);
	static float outquad(float t, float a, float b);
	static float inoutquad(float t, float a, float b);
	static float outexp(float t, float a, float b);
	static float inexp(float t, float a, float b);
	static float inoutexp(float t, float a, float b);
	static float incubic(float t, float a, float b);
	static float outcubic(float t, float a, float b);
	static float inoutcubic(float t, float a, float b);
	static float inquart(float t, float a, float b);
	static float outquart(float t, float a, float b);
	static float inoutquart(float t, float a, float b);
	static float inquint(float t, float a, float b);
	static float outquint(float t, float a, float b);
	static float inoutquint(float t, float a, float b);
	static float inback(float t, float a, float b, float s=BACK_10);
	static float outback(float t, float a, float b, float s=BACK_10);
	static float inoutback(float t, float a, float b, float s=BACK_10);
	static float incirc(float t, float a, float b);
	static float outcirc(float t, float a, float b);
	static float inoutcirc(float t, float a, float b);

	/// 値 v0 から v1 へのエルミート補間
	/// @param t 補間係数
	/// @param v0 t=0での値
	/// @param v1 t=1での値
	/// @param slope0 t=0における補完曲線の傾き
	/// @param slope1 t=1における補完曲線の傾き
	static float hermite(float t, float v0, float v1, float slope0, float slope1);

	/// サインカーブ。
	///
	/// t=0.0 のとき a になり、t=0.5 で b に達し、t=1.0 で再び a になる
	/// @param t 補間係数
	/// @param a t=0.0 および 1.0 での値
	/// @param b t=0.5 での値
	static float wave(float t, float a, float b);
};

}