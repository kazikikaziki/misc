#include "KFileLoader.h"
//
#include "KEmbeddedFiles.h"
#include "KFile.h"
#include "KLog.h"
#include "KPacFile.h"
#include "KPath.h"
#include "KPlatform.h"
#include <unordered_map>

namespace mo {


class CPacFile: public KFileLoaderCallback {
public:
	KPacFileReader reader_;
	CPacFile() {}
	virtual ~CPacFile() {}; // DO NOT REMOVE IT!

	virtual bool on_fileloader_hasfile(const Path &name) override {
		return reader_.getIndexByName(name, false, false) >= 0;
	}
	virtual FileInput on_fileloader_getfile(const Path &name) override {
		FileInput file;
		int index = reader_.getIndexByName(name, false, false);
		if (index >= 0) {
			std::string bin = reader_.getData(index);
			file.open(bin.data(), bin.size());
		}
		return file;
	}
};

class CResourceFiles: public KFileLoaderCallback {
public:
	virtual bool on_fileloader_hasfile(const Path &name) override {
		return KEmbeddedFiles::hasFile(name);
	}
	virtual FileInput on_fileloader_getfile(const Path &name) override {
		return KEmbeddedFiles::getFile(name);
	}
};

class CFolder: public KFileLoaderCallback {
	Path dir_;
public:
	CFolder(const Path &dir) {
		dir_ = dir;
		if (!Platform::directoryExists(dir)) {
			Log_warningf("CFolder: Directory not exists: '%s'", dir.u8());
		}
	}
	virtual bool on_fileloader_hasfile(const Path &name) override {
		Path realname = dir_.join(name);
		if (! Platform::fileExists(realname)) {
			return true;
		}
		if (Platform::directoryExists(realname)) {
			return true;
		}
		return false;
	}
	virtual FileInput on_fileloader_getfile(const Path &name) override {
		Path realname = dir_.join(name);
		FileInput file;
		if (file.open(realname)) {
			Log_verbosef("%s: %s ==> OK", __FUNCTION__, realname.u8());
		}
		return file;
	}
};




#pragma region KFileLoader
KFileLoader::KFileLoader() {
}
KFileLoader::~KFileLoader() {
	clear();
}
void KFileLoader::clear() {
	for (size_t i=0; i<archives_.size(); i++) {
		delete archives_[i];
	}
	archives_.clear();
}
void KFileLoader::addCallback(KFileLoaderCallback *cb) {
	if (cb) archives_.push_back(cb);
}
bool KFileLoader::addFolder(const Path &dir) {
	KFileLoaderCallback *cb = new CFolder(dir);
	addCallback(cb);
	Log_debugf(u8"検索パスにディレクトリ %s を追加しました", dir.u8());
	return true;
}
bool KFileLoader::addPacFile(const Path &name) {
	FileInput file;
	if (file.open(name)) {
		CPacFile *cb = new CPacFile();
		if (cb->reader_.open(file)) {
			addCallback(cb);
			Log_debugf(u8"検索パスにファイル %s を追加しました", name.u8());
			return true;
		}
		delete cb;
	}
	Log_warningf("E_FILE_FAIL: Failed to open a pac file: '%s'", name.u8());
	return false;
}
bool KFileLoader::addEmbeddedFiles() {
	CResourceFiles *cb = new CResourceFiles();
	addCallback(cb);
	Log_debug(u8"検索パスに埋め込みリソースを追加しました");
	return true;
}
bool KFileLoader::addEmbeddedPacFileLoader(const Path &pacname) {
	// リソースとして埋め込まれた pac ファイルを得る
	FileInput file = KEmbeddedFiles::getFile(pacname);
	if (file.empty()) {
		Log_warningf("Failed to open '%s'", pacname.u8());
		return false;
	}

	// pac ファイルローダーを作成
	CPacFile *cb = new CPacFile();
	if (!cb->reader_.open(file)) {
		Log_warningf("Invalid pac format: '%s'", pacname.u8());
		delete cb;
		return false;
	}

	addCallback(cb);
	Log_debugf(u8"検索パスに埋め込み pac ファイルを追加しました: %s", pacname.u8());
	return true;
}
FileInput KFileLoader::getInputFile(const Path &name) {
	if (archives_.empty()) {
		// ローダーが一つも設定されていない。
		// 一番基本的な方法で開く
		return FileInput(name);
	}
	for (size_t i=0; i<archives_.size(); i++) {
		KFileLoaderCallback *ar = archives_[i];
		FileInput f = ar->on_fileloader_getfile(name);
		if (f.isOpen()) {
			return f;
		}
	}
	return FileInput();
}
bool KFileLoader::exists(const Path &name) {
	return getInputFile(name).isOpen();
}
std::string KFileLoader::loadBinary(const Path &filename) {
	std::string bin;
	FileInput file = getInputFile(filename);
	if (file.isOpen()) {
		bin = file.readBin();
	} else {
		Log_errorf(u8"E_FILE_NOT_FOUND: ファイルが見つかりません: %s", filename.u8());
	}
	return bin;
}
#pragma endregion // KFileLoader





} // namespace
