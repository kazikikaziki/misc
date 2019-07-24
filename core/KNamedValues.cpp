#include "KNamedValues.h"
#include "KConv.h"
#include "KFile.h"
#include "KLog.h"
#include "KStd.h"
#include "KXmlFile.h"
#include <algorithm>

namespace mo {

KNamedValues::const_iterator KNamedValues::cbegin() const {
	return items_.cbegin();
}
KNamedValues::const_iterator KNamedValues::cend() const {
	return items_.cend();
}
KNamedValues::iterator KNamedValues::begin() {
	return items_.begin();
}
KNamedValues::iterator KNamedValues::end() {
	return items_.end();
}
size_t KNamedValues::size() const {
	return items_.size();
}
bool KNamedValues::empty() const {
	return items_.empty();
}
void KNamedValues::clear() { 
	items_.clear();
}
bool KNamedValues::contains(const Path &key) const {
	return items_.find(key) != items_.end(); 
}
void KNamedValues::remove(const Path &key) {
	items_.erase(items_.find(key));
}
const Path & KNamedValues::getString(const Path &key) const {
	auto it = items_.find(key);
	if (it != items_.end()) {
		return it->second;
	} else {
		return Path::Empty; // 戻り値が参照であることに注意。ローカルオブジェクトを返さないように
	}
}
const Path & KNamedValues::getString(const Path &key, const Path &def) const {
	auto it = items_.find(key);
	if (it != items_.end()) {
		return it->second;
	} else {
		return def;
	}
}
bool KNamedValues::getBool(const Path &key, bool def) const {
	const Path &v = getString(key);
	if (v.compare("on" ) == 0) return true;
	if (v.compare("off") == 0) return false;
	return getInteger(key, def ? 1 : 0) != 0;
}
int KNamedValues::getInteger(const Path &key, int def) const {
	const Path &s = getString(key);
	int ret = def;
	K_strtoi_try(s.u8(), &ret);
	return ret;
}
float KNamedValues::getFloat(const Path &key, float def) const {
	const Path &s = getString(key);
	float ret = def;
	K_strtof_try(s.u8(), &ret);
	return ret;
}
void KNamedValues::setString(const Path &key, const Path &val) {
	if (key.empty()) {
		Log_error("E_INVALID_KEY: empty");
		return;
	}
	items_[key] = val;
}
void KNamedValues::setBool(const Path &key, bool val) {
	setString(key, Path(val ? "1" : "0"));
}
void KNamedValues::setInteger(const Path &key, int val) {
	setString(key, Path::fromFormat("%d", val));
}
void KNamedValues::setFloat(const Path &key, float val) {
	setString(key, Path::fromFormat("%f", val));
}
int KNamedValues::getKeys(PathList &list, bool sort) const {
	size_t start = list.size();
	for (auto it=items_.begin(); it!=items_.end(); it++) {
		list.push_back(it->first);
	}
	if (sort) {
		std::sort(list.begin() + start, list.end());
	}
	return (int)items_.size();
}
void KNamedValues::assign(const KNamedValues *values) {
	clear();
	append(values);
}
void KNamedValues::append(const KNamedValues *values) {
	if (values) {
		for (auto it=values->cbegin(); it!=values->cend(); ++it) {
			items_[it->first] = it->second;
		}
	}
}
bool KNamedValues::queryBool(const Path &key, bool *val) const {
	if (contains(key)) {
		if (val) *val = getBool(key);
		return true;
	}
	return false;
}
bool KNamedValues::queryFloat(const Path &key, float *val) const {
	if (contains(key)) {
		if (val) *val = getFloat(key);
		return true;
	}
	return false;
}
bool KNamedValues::queryInteger(const Path &key, int *val) const {
	if (contains(key)) {
		if (val) *val = getInteger(key);
		return true;
	}
	return false;
}
bool KNamedValues::queryString(const Path &key, Path *val) const {
	if (contains(key)) {
		if (val) *val = getString(key);
		return true;
	}
	return false;
}
bool KNamedValues::loadFromFile(const Path &filename) {
	bool success = false;
	FileInput file;
	if (file.open(filename)) {
		std::string xml_u8 = KConv::toUtf8(file.readBin());
		if (! xml_u8.empty()) {
			success = loadFromString(xml_u8.c_str(), filename);
		}
	}
	return success;
}
bool KNamedValues::loadFromXml(const tinyxml2::XMLElement *top, bool packed_in_attr) {
	clear();
	if (packed_in_attr) {
		// <XXX AAA="BBB" CCC="DDD" EEE="FFF" ... >
		for (auto it=top->FirstAttribute(); it!=NULL; it=it->Next()) {
			Path key = it->Name();
			Path val = it->Value();
			setString(key, val);
		}
	} else {
		// <XXX name="AAA">BBB</XXX>/>
		for (const tinyxml2::XMLElement *it=top->FirstChildElement(); it!=NULL; it=it->NextSiblingElement()) {
			Path key = it->Attribute("name");
			Path val = it->GetText();
			if (!key.empty()) {
				setString(key, val);
			}
		}
	}
	return true;
}
bool KNamedValues::loadFromString(const char *xml_u8, const Path &filename) {
	clear();
	tinyxml2::XMLDocument *xml_doc = KXml::createXmlDocument(xml_u8, filename);
	if (xml_doc == NULL) {
		Log_errorf("E_XML: Failed to load: %s", filename.u8());
		return false;
	}
	bool result = loadFromXml(xml_doc->FirstChildElement(), false);
	KXml::deleteXml(xml_doc);
	return result;
}

std::string KNamedValues::saveToString(bool pack_in_attr) const {
	std::string s;
	typedef std::pair<Path, Path> nvpair;
	std::vector<nvpair> sorted;
	sorted.reserve(items_.size());
	for (auto it=items_.begin(); it!=items_.end(); ++it) {
		sorted.push_back(nvpair(it->first, it->second));
	}
	std::sort(sorted.begin(), sorted.end());
	if (pack_in_attr) {
		s += "<KNamedValues\n";
		for (auto it=sorted.begin(); it!=sorted.end(); ++it) {
			s += K_sprintf("\t%s = '%s'\n", it->first.u8(), it->second.u8());
		}
		s += "/>\n";
	} else {
		s += "<KNamedValues>\n";
		for (auto it=sorted.begin(); it!=sorted.end(); ++it) {
			s += K_sprintf("  <Pair name='%s'>%s</Pair>\n", it->first.u8(), it->second.u8());
		}
		s += "</KNamedValues>\n";
	}
	return s;
}

void KNamedValues::saveToFile(const Path &filename, bool pack_in_attr) const {
	FileOutput file;
	if (file.open(filename)) {
		std::string xml_u8 = saveToString(pack_in_attr);
		file.write(xml_u8.c_str(), xml_u8.size());
	}
}

} // namespace
