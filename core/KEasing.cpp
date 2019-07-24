#define _USE_MATH_DEFINES // M_PI
#include "KEasing.h"
#include <math.h>

namespace mo {

static float __clampf(float a) {
	return (a < 0.0f) ? 0.0f : ((a < 1.0f) ? a : 1.0f);
}


const float KEasing::BACK_10  = 1.70158f; // 10%行き過ぎてから戻る
const float KEasing::BACK_20  = 2.59238f; // 20%行き過ぎてから戻る
const float KEasing::BACK_30  = 3.39405f; // 30%行き過ぎてから戻る
const float KEasing::BACK_40  = 4.15574f; // 40%行き過ぎてから戻る
const float KEasing::BACK_50  = 4.89485f; // 50%行き過ぎてから戻る
const float KEasing::BACK_60  = 5.61962f; // 60%行き過ぎてから戻る
const float KEasing::BACK_70  = 6.33456f; // 70%行き過ぎてから戻る
const float KEasing::BACK_80  = 7.04243f; // 80%行き過ぎてから戻る
const float KEasing::BACK_90  = 7.74502f; // 90%行き過ぎてから戻る
const float KEasing::BACK_100 = 8.44353f; //100%行き過ぎてから戻る





float KEasing::hermite(float t, float v0, float v1, float slope0, float slope1) {
	t = __clampf(t);
	float a =  2 * (v0 - v1) + slope0 + slope1;
	float b = -3 * (v0 - v1) - 2 * slope0 + slope1;
	float c = slope0;
	float d = v0;
	float t2 = t * t;
	float t3 = t * t2;
	return a * t3  + b * t2 + c * t + d; 
}
float KEasing::linear(float t, float a, float b) {
	t = __clampf(t);
	return a + (b - a) * t;
}
float KEasing::insine(float t, float a, float b) {
	t = __clampf(t);
	return b - (b - a) * cosf(t * (float)M_PI / 2);
}
float KEasing::outsine(float t, float a, float b) {
	t = __clampf(t);
	return a + (b - a) * sinf(t * (float)M_PI / 2);
}
float KEasing::inoutsine(float t, float a, float b) {
	t = __clampf(t);
	return a + (b - a) / 2 * (1 - cosf(t * (float)M_PI));
}
float KEasing::inquad(float t, float a, float b) {
	t = __clampf(t);
	return a + (b - a) * t * t;
}
float KEasing::outquad(float t, float a, float b) {
	t = __clampf(t);
	float T = 1.0f - t;
	return a + (b - a) * (1.0f - T * T);
}
float KEasing::inoutquad(float t, float a, float b) {
	return (t < 0.5f) ?
		inquad(t*2, a, (a+b)/2) :
		outquad((t-0.5f)*2, (a+b)/2, b);
}
float KEasing::outexp(float t, float a, float b) {
	return a + (b - a) * (1.0f - powf(2.0f, -10.0f * t));
}
float KEasing::inexp(float t, float a, float b) {
	return a + (b - a) * powf(2.0f, 10.0f * (t - 1.0f));
}
float KEasing::inoutexp(float t, float a, float b) {
	return (t < 0.5f) ?
		inexp(t*2, a, (a+b)/2) :
		outexp((t-0.5f)*2, (a+b)/2, b);
}
float KEasing::incubic(float t, float a, float b) {
	t = __clampf(t);
	return a + (b - a) * t * t * t;
}
float KEasing::outcubic(float t, float a, float b) {
	t = __clampf(t);
	float T = 1.0f - t;
	return a + (b - a) * (1.0f - T * T * T);
}
float KEasing::inoutcubic(float t, float a, float b) {
	return (t < 0.5f) ?
		incubic(t*2, a, (a+b)/2) :
		outcubic((t-0.5f)*2, (a+b)/2, b);
}
float KEasing::inquart(float t, float a, float b) {
	t = __clampf(t);
	return a + (b - a) * t * t * t * t;
}
float KEasing::outquart(float t, float a, float b) {
	t = __clampf(t);
	float T = 1.0f - t;
	return a - (b - a) * (1.0f - T * T * T * T);
}
float KEasing::inoutquart(float t, float a, float b) {
	return (t < 0.5f) ?
		inquart(t*2, a, (a+b)/2) :
		outquart((t-0.5f)*2, (a+b)/2, b);
}
float KEasing::inquint(float t, float a, float b) {
	t = __clampf(t);
	return a + (b - a) * t * t * t * t * t;
}
float KEasing::outquint(float t, float a, float b) {
	t = __clampf(t);
	float T = 1.0f - t;
	return a - (b - a) * (1.0f - T * T * T * T * T);
}
float KEasing::inoutquint(float t, float a, float b) {
	return (t < 0.5f) ?
		inquart(t*2, a, (a+b)/2) :
		outquart((t-0.5f)*2, (a+b)/2, b);
}
float KEasing::inback(float t, float a, float b, float s) {
	// https://github.com/kikito/tween.lua/blob/master/tween.lua
	t = __clampf(t);
	return (b - a) * t * t * ((s + 1) * t - s) + a;
}
float KEasing::outback(float t, float a, float b, float s) {
	float T = t - 1;
	return (b - a) * (T * T * ((s + 1) * T + s) + 1) + a;
}
float KEasing::inoutback(float t, float a, float b, float s) {
	float S = s * 1.525f;
	return (t < 0.5f) ? 
		inback(t * 2, a, (a + b) / 2, S) : 
		outback(t * 2 - 1, (a + b) / 2, b, S);
}
float KEasing::incirc(float t, float a, float b) {
	t = __clampf(t);
	return a - (b - a) * (sqrtf(1.0f - t * t) - 1.0f);
}
float KEasing::outcirc(float t, float a, float b) {
	t = __clampf(t);
	float T = t - 1.0f;
	return a + (b - a) * sqrtf(1.0f - T * T);
}
float KEasing::inoutcirc(float t, float a, float b) {
	return (t < 0.5f) ?
		incirc(t * 2, a, (a + b) / 2) :
		outcirc(t * 2 - 1, (a + b) / 2, b);
}
float KEasing::wave(float t, float a, float b) {
	float k = 0.5f + 0.5f * -cosf(2 * (float)M_PI * t);
	return linear(k, a, b);
}



}