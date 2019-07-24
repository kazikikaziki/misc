#pragma once
#include <vector>
#include <unordered_map>
#include "KPath.h"

namespace mo {

class KOrderedParameters {
public:
	KOrderedParameters();
	void clear();
	int size() const;
	bool contains(const Path &key) const;
	void add(const Path &key, const Path &val);
	void add(const Path &key, int val);
	void add(const Path &key, float val);
	void add(const KOrderedParameters &other);
	bool queryString(const Path &key, Path *val) const;
	bool queryInteger(const Path &key, int *val) const;
	bool queryFloat(const Path &key, float *val) const;
	bool getParameter(int index, Path *key, Path *val) const;
private:
	struct Item {
		Path key;
		Path val;
	};
	void _add(const Item &item);
	const Item * _get(const Path &key) const;
	const Item * _at(int index) const;
	std::vector<Item> items_;
	std::unordered_map<Path, int> key_index_map_; // Key => Table index
};

} // namespace
