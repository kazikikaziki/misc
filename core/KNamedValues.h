#pragma once
#include "KXmlFile.h"
#include "KPath.h"
#include <unordered_map>

namespace mo {

/// 「名前＝値」の形式のデータを順序無しで扱う。
/// 順序を考慮する場合は IOrderedParameters を使う。
/// また、行と列から成る二次元的なテーブルを扱いたい場合は ITable を使う
/// なお、あまり長い文字列を格納することは想定していないため、String ではなく Path を文字列として使用する
class KNamedValues {
public:
	typedef std::unordered_map<Path, Path>::const_iterator const_iterator;
	typedef std::unordered_map<Path, Path>::iterator iterator;

	const_iterator cbegin() const;
	const_iterator cend() const;
	iterator begin();
	iterator end();
	size_t size() const;
	bool empty() const;
	void clear();
	bool contains(const Path &key) const;
	void remove(const Path &key);
	const Path & getString(const Path &key) const;
	const Path & getString(const Path &key, const Path &def) const;
	bool queryBool(const Path &key, bool *val) const;
	bool queryFloat(const Path &key, float *val) const;
	bool queryInteger(const Path &key, int *val) const;
	bool queryString(const Path &key, Path *val) const;
	bool  getBool(const Path &key, bool def=false) const;
	int   getInteger(const Path &key, int def=0) const;
	float getFloat(const Path &key, float def=0.0f) const;
	void setString(const Path &key, const Path &val);
	void setBool(const Path &key, bool val);
	void setInteger(const Path &key, int val);
	void setFloat(const Path &key, float val);
	bool loadFromFile(const Path &filename);
	bool loadFromString(const char *xml_u8, const Path &filename);
	bool loadFromXml(const tinyxml2::XMLElement *top, bool packed_in_attr=false);
	void saveToFile(const Path &filename, bool pack_in_attr=false) const;

	/// テキストに書き出す (UTF8)
	std::string saveToString(bool pack_in_attr=false) const;

	/// キー名の一覧を list に追加する
	/// sort -- アルファベット順でソートするなら true
	/// 追加した要素数を返す。この値は size() で得られる値と同じである
	int getKeys(PathList &list, bool sort) const;

	/// 既存の内容を破棄し、テーブル内容をコピーする。values が NULL だった場合は clear() と同じ動作になる
	void assign(const KNamedValues *values);

	/// テーブル内容を追加する。values が NULL だった場合は何もしない
	void append(const KNamedValues *values);
private:
	std::unordered_map<Path, Path> items_;
};

} // namespace

