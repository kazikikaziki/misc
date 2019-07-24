#pragma once
#include "KPath.h"

namespace mo {


class KDirScan {
public:
	struct Item {
		Path name;
		bool is_directory;
	};
	KDirScan();
	void clear();
	void scanFlat(const Path &dir);
	void scanRecursive(const Path &dir);
	int size() const;
	const Item & items(int index) const;

private:
	void scanFlat2(const Path &dir, const Path &prefix);
	void scanRecursive2(const Path &dir, const Path &prefix);
	std::vector<Item> items_;
	int depth_;
};


}
