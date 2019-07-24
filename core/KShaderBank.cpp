#include "KShaderBank.h"
#include "KVideo.h"
#include "KFile.h"
#include "KLog.h"
#include <algorithm>

namespace mo {

ShaderBank::ShaderBank() {
	hlsl_available_ = false;
	glsl_available_ = false;
}
ShaderBank::~ShaderBank() {
	clearAssets();
}
void ShaderBank::init() {
	int hlsl = 0;
	int glsl = 0;
	Video::getParameter(Video::HAS_HLSL, &hlsl);
	Video::getParameter(Video::HAS_GLSL, &glsl);
	hlsl_available_ = hlsl != 0;
	glsl_available_ = glsl != 0;
}
Path ShaderBank::getName(SHADERID s) const {
	Path name;
	mutex_.lock();
	for (auto it=items_.begin(); it!=items_.end(); ++it) {
		if (it->second == s) {
			name = it->first;
			break;
		}
	}
	mutex_.unlock();
	return name;
}
PathList ShaderBank::getNames() const {
	PathList names;
	{
		names.reserve(items_.size());
		for (auto it=items_.cbegin(); it!=items_.cend(); ++it) {
			names.push_back(it->first);
		}
		std::sort(names.begin(), names.end());
	}
	return names;
}
SHADERID ShaderBank::addHLSLShader(const Path &name, const char *code) {
	SHADERID sh = Video::createShaderFromHLSL(code, name);
	if (sh == NULL) {
		Log_error("E_SHADER: failed to build HLSL shader.");
		return NULL;
	}
	mutex_.lock();
	{
		Log_assert(items_.find(name) == items_.end());
		items_[name] = sh;
	}
	mutex_.unlock();
	return sh;
}
SHADERID ShaderBank::addGLSLShader(const Path &name, const char *vs_code, const char *fs_code) {
	SHADERID sh = Video::createShaderFromGLSL(vs_code, fs_code, name);
	if (sh == NULL) {
		Log_error("E_SHADER: failed to build GLSL shader.");
		return NULL;
	}
	mutex_.lock();
	{
		Log_assert(items_.find(name) == items_.end());
		items_[name] = sh;
	}
	mutex_.unlock();
	return sh;
}
void ShaderBank::delAsset(const Path &name) {
	mutex_.lock();
	{
		auto it = items_.find(name);
		if (it != items_.end()) {
			SHADERID sh = it->second;
			Video::deleteShader(sh);
			items_.erase(it);
			Log_verbosef("Del shader: %s", name.u8());
		}
	}
	mutex_.unlock();
}
void ShaderBank::clearAssets() {
	mutex_.lock();
	{
		for (auto it=items_.begin(); it!=items_.end(); ++it) {
			SHADERID sh = it->second;
			Video::deleteShader(sh);
		}
		items_.clear();
	}
	mutex_.unlock();
}
bool ShaderBank::loadAsset(const Path &asset_path, FileInput &file) {
	if (hasAsset(asset_path)) {
		// 既に同名のシェーダーが存在する
		// 古いシェーダーが勝手に削除されるとバグの温床になるので、明示的に削除しない限り上書き作成しない
		//	Log_warningf("W_SHADER_OVERWRITE: Resource named '%s' already exists. The resource data will be overwriten by new one", asset_path.u8());
		return false;
	}

	Log_debugf("Load: %s", asset_path.u8());
	SHADERID shader = NULL;
	// シェーダーを新規作成
	std::string shadercode_u8 = file.readBin();

	if (asset_path.extension().compare(".fx") == 0) {
		if (hlsl_available_) {
			shader = Video::createShaderFromHLSL(shadercode_u8.c_str(), asset_path);
		}
	}
	if (shader) {
		mutex_.lock();
		items_[asset_path] = shader;
		mutex_.unlock();
		return true;
	} else {
		Log_warningf(u8"シェーダーを作成できなかった: '%s'", asset_path.u8());
		mutex_.lock();
		items_[asset_path] = NULL;
		mutex_.unlock();
		return false;
	}
}
bool ShaderBank::hasAsset(const Path &name) const {
	mutex_.lock();
	bool ret = items_.find(name) != items_.end();
	mutex_.unlock();
	return ret;
}
SHADERID ShaderBank::findShader(const Path &asset_path, bool should_exist) {
	SHADERID ret = NULL;
	mutex_.lock();
	{
		auto it = items_.find(asset_path);
		if (it != items_.end()) {
			ret = it->second;
		}
	}
	mutex_.unlock();
	if (ret) return ret;
	if (should_exist) {
		Log_errorf("No shader named '%s'", asset_path.u8());
	}
	return NULL;
}

} // namespace
