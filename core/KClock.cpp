#include "KClock.h"
#include "KStd.h"

namespace mo {


#pragma region KClock
uint64_t KClock::getSystemTimeNano64() {
	return K_gettime_nano();
}
uint64_t KClock::getSystemTimeMsec64() {
	uint64_t nano = getSystemTimeNano64();
	return nano / 1000000;
}
int KClock::getSystemTimeMsec() {
	return (int)getSystemTimeMsec64();
}
float KClock::getSystemTimeMsecF() {
	uint64_t nano = getSystemTimeNano64();
	double msec = nano / 1000000.0;
	return (float)msec;
}
int KClock::getSystemTimeSec() {
	uint64_t nano = getSystemTimeNano64();
	return (int)nano / 1000000000;
}
float KClock::getSystemTimeSecF() {
	return getSystemTimeMsecF() / 1000.0f;
}


KClock::KClock() {
	start_ = getSystemTimeNano64();
}
void KClock::reset() {
	start_ = getSystemTimeNano64();
}
uint64_t KClock::getTimeNano64() const {
	const uint64_t t = getSystemTimeNano64();
	return t - start_;
}
uint64_t KClock::getTimeMsec64() const {
	uint64_t t = getTimeNano64();
	return t / 1000000;
}
int KClock::getTimeNano() const {
	uint64_t t = getTimeNano64();
	return (int)(t & 0x7FFFFFFF);
}
int KClock::getTimeMsec() const {
	uint64_t t = getTimeMsec64();
	return (int)(t & 0x7FFFFFFF);
}
float KClock::getTimeMsecF() const {
	const double MS = 1000.0 * 1000.0;
	uint64_t ns = getTimeNano64();
	return (float)((double)ns / MS);
}
float KClock::getTimeSecondsF() const {
	const double NS = 1000.0 * 1000.0 * 1000.0;
	uint64_t ns = getTimeNano64();
	return (float)((double)ns / NS);
}

#pragma endregion // KClock





} // namespace
