#pragma once
#include "KVideoDef.h"
#include "KPath.h"
#include <unordered_map>
#include <mutex>

namespace mo {

class FileInput;

class ShaderBank {
public:
	ShaderBank();
	virtual ~ShaderBank();
	void init();
	Path getName(SHADERID shader) const;
	PathList getNames() const;
	SHADERID addHLSLShader(const Path &name, const char *code);
	SHADERID addGLSLShader(const Path &name, const char *vs_code, const char *fs_code);
	SHADERID findShader(const Path &asset_path, bool should_exist=true);
	int getCount() const { return (int)items_.size(); }
	void delAsset(const Path &name);
	void clearAssets();
	bool loadAsset(const Path &asset_path, FileInput &file);
	bool hasAsset(const Path &name) const;
private:
	std::unordered_map<Path, SHADERID> items_;
	mutable std::recursive_mutex mutex_;
	bool hlsl_available_;
	bool glsl_available_;
};

} // namespace
