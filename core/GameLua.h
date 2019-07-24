#pragma once
#include "system.h"
#include "KLua.h"
#include "KPath.h"
#include <unordered_map>
#include <mutex>

namespace mo {

class Engine;
class Path;
class FileInput;

class LuaSystem: public System {
public:
	LuaSystem();
	virtual ~LuaSystem();
	virtual void onEnd() override;
	virtual void onInspectorGui() override;

	void init(Engine *engine);
	lua_State * addEmptyScript(const Path &asset_path);
	lua_State * findScript(const Path &asset_path) const;
	lua_State * queryScript(const Path &asset_path);
	lua_State * makeThread(const Path &asset_path);
	bool hasAsset(const Path &asset_path) const;
	void delAsset(const Path &asset_path);
	void clearAssets();
	bool loadAsset(const Path &asset_path, FileInput &file);

private:
	bool _load(const Path &asset_path, FileInput &file);
	std::unordered_map<Path, lua_State *> items_;
	mutable std::recursive_mutex mutex_;
	mutable PathList script_names_cache_;
};


}
