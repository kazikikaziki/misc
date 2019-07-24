#include "GameUserData.h"
#include "KFile.h"
#include "KImgui.h"
#include "KNum.h"
#include "KStd.h"
#include "KZlib.h"
#include "KLog.h"
#include "Engine.h"
#include <algorithm>

namespace mo {


UserDataSystem::UserDataSystem() {
}
void UserDataSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
}
void UserDataSystem::_clear(const PathList &keys) {
	for (auto it=keys.begin(); it!=keys.end(); ++it) {
		const Path &k = *it;
		auto kit = tags_.find(k);
		if (kit != tags_.end()) {
			tags_.erase(kit);
		}
		values_.remove(k);
	}
}
void UserDataSystem::clearValues() {
	values_.clear();
	tags_.clear();
}
void UserDataSystem::clearValuesByTag(int tag) {
	PathList keys;
	for (auto it=tags_.begin(); it!=tags_.end(); ++it) {
		if (it->second == tag) {
			keys.push_back(it->first);
		}
	}
	_clear(keys);
}
void UserDataSystem::clearValuesByPrefix(const char *prefix) {
	PathList keys;
	for (auto it=values_.begin(); it!=values_.end(); ++it) {
		if (it->first.startsWithString(prefix)) {
			keys.push_back(it->first);
		}
	}
	_clear(keys);
}
int UserDataSystem::getKeys(PathList *keys) const {
	if (keys) {
		for (auto it=values_.cbegin(); it!=values_.cend(); ++it) {
			keys->push_back(it->first);
		}
	}
	return values_.size();
}
bool UserDataSystem::hasKey(const Path &key) const {
	return values_.queryString(key, NULL);
}
bool UserDataSystem::getBool(const Path &key, bool def) const {
	return getInt(key, def ? 1 : 0) != 0;
}
void UserDataSystem::setBool(const Path &key, bool val, int tag) {
	setInt(key, val ? 1 : 0, tag);
}
int UserDataSystem::getInt(const Path &key, int def) const {
	const char *s = getStr(key, "").u8();
	int ret = def;
	K_strtoi_try(s, &ret);
	return ret;
}
void UserDataSystem::setInt(const Path &key, int val, int tag) {
	Path pval = Path::fromFormat("%d", val);
	setStr(key, pval, tag);
}
Path UserDataSystem::getStr(const Path &key, const Path &def) const {
	return values_.getString(key, def);
}
void UserDataSystem::setStr(const Path &key, const Path &val, int tag) {
	values_.setString(key, val);
	tags_[key] = tag;
}
bool UserDataSystem::saveToFileEx(const KNamedValues *nv, const Path &filename, const char *password) {
	bool ret = false;
	FileOutput file;
	if (file.open(filename)) {
		std::string u8 = nv->saveToString();
		if (password && password[0]) {
			K_strxor(&u8[0], u8.size(), password); // encode
		}
		if (! u8.empty()) {
			file.write(u8);
		}
		ret = true;
	}
	return ret;
}
bool UserDataSystem::saveToFile(const Path &filename, const char *password) {
	return saveToFileEx(&values_, filename, password);
}
bool UserDataSystem::loadFromFileEx(KNamedValues *nv, const Path &filename, const char *password) const {
	return peekFile(filename, password, nv);
}
bool UserDataSystem::loadFromFile(const Path &filename, const char *password) {
	return peekFile(filename, password, &values_);
}
bool UserDataSystem::loadFromFileCompress(const Path &filename) {
	return loadFromFileCompress(&values_, filename);
}
bool UserDataSystem::saveToFileCompress(const Path &filename) {
	return saveToFileCompress(&values_, filename);
}
bool UserDataSystem::saveToFileCompress(const KNamedValues *nv, const Path &filename) {
	if (nv == NULL) return false;
	std::string u8 = nv->saveToString();
	if (u8.empty()) return false;

	std::string zbin = KZlib::compress_raw(u8.data(), u8.size(), 1);
	if (zbin.empty()) return false;

	FileOutput file;
	if (!file.open(filename)) return false;

	file.writeUint16((uint16_t)u8.size());
	file.writeUint16((uint16_t)zbin.size());
	file.write(zbin);
	return true;
}
bool UserDataSystem::loadFromFileCompress(KNamedValues *nv, const Path &filename) const {
	FileInput file;
	if (!file.open(filename)) return false;

	uint16_t uzsize = file.readUint16();
	uint16_t zsize  = file.readUint16();
	std::string zbin = file.readBin(zsize);
	std::string text_u8 = KZlib::uncompress_raw(zbin.data(), zbin.size(), uzsize);
	text_u8.push_back(0); // 終端文字を追加
	if (nv) {
		return nv->loadFromString(text_u8.c_str(), filename);
	} else {
		return true;
	}
}
bool UserDataSystem::loadFromNamedValues(const KNamedValues *nv) {
	if (nv) {
		values_.assign(nv);
		return true;
	}
	return false;
}
bool UserDataSystem::peekFile(const Path &filename, const char *password, KNamedValues *nv) const {
	bool ret = false;
	FileInput file;
	if (file.open(filename)) {
		std::string u8 = file.readBin();
		if (password && password[0]) {
			K_strxor(&u8[0], u8.size(), password); // decode
		}
		if (nv) nv->loadFromString(u8.c_str(), filename);
		ret = true;
	}
	return ret;
}
void UserDataSystem::onInspectorGui() {
	int ww = (int)ImGui::GetContentRegionAvail().x;
	int w = Num_min(64, ww/3);
	int id = 0;
	ImGui::Indent();
	PathList keys;
	for (auto it=values_.begin(); it!=values_.end(); it++) {
		keys.push_back(it->first);
	}
	std::sort(keys.begin(), keys.end());

	for (auto it=keys.begin(); it!=keys.end(); it++) {
		const Path &key = *it;
		const Path &val = values_.getString(key);
		char val_u8[256];
		K_strcpy(val_u8, sizeof(val_u8), val.u8());
		ImGui::PushID(ImGui_ID(id));
		ImGui::PushItemWidth((float)w);
		if (ImGui::InputText("", val_u8, sizeof(val_u8))) {
			values_.setString(key, Path::fromUtf8(val_u8));
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::Text("%s", key.u8());
		ImGui::PopID();
		id++;
	}
	ImGui::Unindent();
}


} // namespace
