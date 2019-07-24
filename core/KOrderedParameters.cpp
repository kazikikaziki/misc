#include "KStd.h"
#include "KOrderedParameters.h"

namespace mo {

KOrderedParameters::KOrderedParameters() {
}
const KOrderedParameters::Item * KOrderedParameters::_get(const Path &key) const {
	auto it = key_index_map_.find(key);
	if (it != key_index_map_.end()) {
		int index = it->second;
		return &items_[index];
	}
	return NULL;
}
const KOrderedParameters::Item * KOrderedParameters::_at(int index) const {
	K_assert(0 <= index && index < (int)items_.size());
	return &items_[index];
}
void KOrderedParameters::clear() {
	items_.clear();
}
int KOrderedParameters::size() const {
	return (int)items_.size();
}
void KOrderedParameters::_add(const Item &item) {
	key_index_map_[item.key] = (int)items_.size(); // new index
	items_.push_back(item);
}
bool KOrderedParameters::contains(const Path &key) const {
	return _get(key) != NULL;
}
void KOrderedParameters::add(const Path &key, const Path &val) {
	if (key.empty()) {
		K_assert(0);//M_error("E_INVALID_KEY: empty");
		return;
	}
	Item item;
	item.key = key;
	item.val = val;
	_add(item);
}
void KOrderedParameters::add(const Path &key, int val) {
	if (key.empty()) {
		K_assert(0);//M_error("E_INVALID_KEY: empty");
		return;
	}
	Item item;
	item.key = key;
	item.val = Path::fromFormat("%d", val);
	_add(item);
}
void KOrderedParameters::add(const Path &key, float val) {
	if (key.empty()) {
		K_assert(0);//M_error("E_INVALID_KEY: empty");
		return;
	}
	Item item;
	item.key = key;
	item.val = Path::fromFormat("%g", val);
	_add(item);
}
void KOrderedParameters::add(const KOrderedParameters &other) {
	for (int i=0; i<other.size(); i++) {
		Path key;
		Path val;
		other.getParameter(i, &key, &val);
		add(key, val);
	}
}
bool KOrderedParameters::queryString(const Path &key, Path *val) const {
	const Item *item = _get(key);
	if (item) {
		if (val) *val = item->val;
		return true;
	}
	return false;
}
bool KOrderedParameters::queryInteger(const Path &key, int *val) const {
	const Item *item = _get(key);
	if (item) {
		if (val) *val = K_strtoi(item->val.u8());
		return true;
	}
	return false;
}
bool KOrderedParameters::queryFloat(const Path &key, float *val) const {
	const Item *item = _get(key);
	if (item) {
		if (val) *val = K_strtof(item->val.u8());
		return true;
	}
	return false;
}
bool KOrderedParameters::getParameter(int index, Path *key, Path *val) const {
	if (0 <= index && index < size()) {
		const Item *item = _at(index);
		K_assert(item);
		if (key) *key = item->key;
		if (val) *val = item->val;
		return true;
	}
	return false;
}

} // namespace
