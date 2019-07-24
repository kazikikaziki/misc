#pragma once
#ifdef _WIN32
#include <Windows.h>
#include <string>

namespace mo {
std::string DX_GetErrorString(HRESULT hr); // UTF8
}

#else
#include <string>

namespace mo {
	std::string DX_GetErrorString(long hr) { return ""; }
}

#endif



