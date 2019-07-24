#include "GameLua.h"
#include "Engine.h"
#include "KLog.h"
#include "KFileLoader.h"
#include "KImgui.h"

namespace mo {

#pragma region LuaSystem
LuaSystem::LuaSystem() {
}
LuaSystem::~LuaSystem() {
	onEnd();
}
void LuaSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
}
void LuaSystem::onEnd() {
	clearAssets();
}
lua_State * LuaSystem::addEmptyScript(const Path &asset_path) {
	lua_State *ls = luaL_newstate();
	luaL_openlibs(ls);
	mutex_.lock();
	{
		if (items_.find(asset_path) != items_.end()) {
			Log_warningf("W_SCRIPT_OVERWRITE: Resource named '%s' already exists. The resource data will be overwriten by new one", asset_path.u8());
			delAsset(asset_path);
		}
		Log_assert(items_[asset_path] == NULL);
		items_[asset_path] = ls;
	}
	mutex_.unlock();
	return ls;
}
lua_State * LuaSystem::findScript(const Path &asset_path) const {
	lua_State *ret;
	mutex_.lock();
	{
		auto it = items_.find(asset_path);
		ret = (it!=items_.end()) ? it->second : NULL;
	}
	mutex_.unlock();
	return ret;
}
bool LuaSystem::_load(const Path &asset_path, FileInput &file) {
	lua_State *ls = addEmptyScript(asset_path);
	Log_assert(ls);
	const std::string bin = file.readBin(); // utf8 または バイトコード
	// ロードする
	if (luaL_loadbuffer(ls, bin.data(), bin.size(), asset_path.u8()) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		Log_errorf("E_LUA: %s", msg);
		delAsset(asset_path);
		return false;
	}
	if (lua_pcall(ls, 0, 0, 0) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		Log_errorf("E_LUA: %s", msg);
		delAsset(asset_path);
		return false;
	}
	return true;
}
lua_State * LuaSystem::queryScript(const Path &asset_path) {
	lua_State *ls = findScript(asset_path);
	if (ls == NULL) {
		FileInput file = g_engine->getFileSystem()->getInputFile(asset_path);
		if (file.isOpen()) {
			if (_load(asset_path, file)) {
				ls = findScript(asset_path);
			}
		}
	}
	return ls;
}
lua_State * LuaSystem::makeThread(const Path &asset_path) {
	lua_State *ls = queryScript(asset_path);
	Log_assert(ls);
	return lua_newthread(ls);
}

bool LuaSystem::hasAsset(const Path &asset_path) const {
	bool ret;
	mutex_.lock();
	{
		ret = items_.find(asset_path) != items_.end();
	}
	mutex_.unlock();
	return ret;
}
void LuaSystem::delAsset(const Path &asset_path) {
	mutex_.lock();
	{
		auto it = items_.find(asset_path);
		if (it != items_.end()) {
			lua_State *ls = it->second;
			lua_close(ls);
			items_.erase(it);
		}
	}
	mutex_.unlock();
}
void LuaSystem::clearAssets() {
	mutex_.lock();
	{
		for (auto it=items_.begin(); it!=items_.end(); ++it) {
			lua_State *ls = it->second;
			lua_close(ls);
		}
		items_.clear();
	}
	mutex_.unlock();
}
bool LuaSystem::loadAsset(const Path &asset_path, FileInput &file) {
	return _load(asset_path, file);
}
void LuaSystem::onInspectorGui() {
#ifndef NO_IMGUI
	if (ImGui::TreeNode(ImGui_ID(0), "LuaState(%d)", items_.size())) {
		if (ImGui::Button("Clear")) {
			clearAssets();
		}
		for (auto it=items_.cbegin(); it!=items_.cend(); ++it) {
			ImGui::Text("%s", it->first.u8());
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
#pragma endregion // LuaSystem


} // namespace
