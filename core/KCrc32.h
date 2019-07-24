#pragma once
#include <inttypes.h>

namespace mo {

/// crc32b を求める (crc32 とは異なる）
class KCrc32 {
public:
	static const uint32_t INIT = (uint32_t)(-1);
	static uint32_t compute(const void *buf, uint32_t size, uint32_t prev_crc=INIT);
	static uint32_t compute(uint32_t prev_crc, uint8_t value);
};

}
