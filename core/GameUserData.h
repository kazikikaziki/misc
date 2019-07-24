#pragma once
#include "system.h"
#include "KNamedValues.h"


namespace mo {

class Engine;
class KNamedValues;

class UserDataSystem: public System {
public:
	UserDataSystem();

	void init(Engine *engine);
	virtual void onInspectorGui() override;

	/// すべての値を削除する
	void clearValues();

	/// 指定されたタグの値をすべて削除する
	void clearValuesByTag(int tag);

	/// 指定された文字で始まる名前の値をすべて削除する
	void clearValuesByPrefix(const char *prefix);
	
	/// ブール値を得る
	bool getBool(const Path &key, bool def=false) const;

	/// ブール値を設定する
	void setBool(const Path &key, bool val, int tag=0);

	/// 整数値を得る
	int getInt(const Path &key, int def=0) const;

	/// 整数値を設定する
	void setInt(const Path &key, int val, int tag=0);

	/// 文字列を得る
	Path getStr(const Path &key, const Path &def="") const;

	/// 文字列を設定する
	void setStr(const Path &key, const Path &val, int tag=0);

	/// キーが存在するかどうか
	bool hasKey(const Path &key) const;

	/// 全てのキーを得る
	int getKeys(PathList *keys) const;

	/// 値を保存する。password に文字列を指定した場合はファイルを暗号化する
	bool saveToFile(const Path &filename, const char *password="");
	bool saveToFileEx(const KNamedValues *nv, const Path &filename, const char *password="");
	bool saveToFileCompress(const Path &filename);
	bool saveToFileCompress(const KNamedValues *nv, const Path &filename);

	/// 値をロードする。暗号化されている場合は password を指定する必要がある
	bool loadFromFile(const Path &filename, const char *password="");
	bool loadFromFileEx(KNamedValues *nv, const Path &filename, const char *password="") const;
	bool loadFromNamedValues(const KNamedValues *nv);
	bool loadFromFileCompress(const Path &filename);
	bool loadFromFileCompress(KNamedValues *nv, const Path &filename) const;

	/// 実際にロードせずに、保存内容だけを見る
	bool peekFile(const Path &filename, const char *password, KNamedValues *nv) const;


private:
	void _clear(const PathList &keys);
	KNamedValues values_;
	std::unordered_map<Path, int> tags_;
};


} // namespace

