#ifdef _WIN32
#include "KDxGetErrorString.h"
#include "KConv.h"
#include <d3d9.h> // D3D_SDK_VERSION

// dxerr.h と dxerr9.h のどちらが存在するか
#define HAS_DXERR9_H   (D3D_SDK_VERSION <= 32)

#if HAS_DXERR9_H
#	include <dxerr9.h>
#	define _DXGetErrorStringW(hr)  DXGetErrorString9W(hr)
#else
#	include <dxerr.h> // dxerr.lib
#	define _DXGetErrorStringW(hr)  DXGetErrorStringW(hr)
#endif


namespace mo {

std::string DX_GetErrorString(HRESULT hr) {
	std::wstring ws = _DXGetErrorStringW(hr);
	return KConv::wideToUtf8(ws);
}

}

#endif // _WIN32
