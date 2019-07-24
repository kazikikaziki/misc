#include "KFile.h"
#include "KLog.h"
#include "KPath.h"
#include "KPacFile.h"
#include "KZlib.h"
#include <stdlib.h> // rand

#define PAC_COMPRESS_LEVEL 1
#define PAC_MAX_LABEL_LEN  (uint8_t)128 // 128以外にすると昔の pac データが読めなくなるので注意


namespace mo {


class KPacFileWriterImpl {
	FileOutput file_;
public:
	KPacFileWriterImpl() {
	}
	~KPacFileWriterImpl() {
		close();
	}
	bool open(const Path &filename) {
		return file_.open(filename);
	}
	bool open(FileOutput &file) {
		file_ = file;
		return file_.isOpen();
	}
	void close() {
		file_.close();
	}
	bool isOpen() const {
		return file_.isOpen();
	}
	bool addEntryFromFileName(const Path &entry_name, const Path &filename) {
		FileInput file;
		if (file.open(filename)) {
			std::string bin = file.readBin();
			return addEntryFromMemory(entry_name, bin.data(), bin.size());
		}
		return false;
	}
	bool addEntryFromMemory(const Path &entry_name, const void *data, size_t size) {
		
		// エントリー名を書き込む。固定長で、XORスクランブルをかけておく
		{
			const char *u8 = entry_name.u8();
			if (strlen(u8) >= PAC_MAX_LABEL_LEN) {
				Log_errorf(u8"ラベル名 '%s' が長すぎます", u8);
				return false;
			}
			char label[PAC_MAX_LABEL_LEN];
			memset(label, 0, PAC_MAX_LABEL_LEN);
			strcpy(label, entry_name.u8());
			// '\0' 以降の文字は完全に無視される部分なのでダミーの乱数を入れておく
			for (int i=strlen(u8)+1; i<PAC_MAX_LABEL_LEN; i++) {
				label[i] = rand() & 0xFF;
			}
			for (uint8_t i=0; i<PAC_MAX_LABEL_LEN; i++) {
				label[i] = label[i] ^ i;
			}
			file_.write(label, PAC_MAX_LABEL_LEN);
		}
		if (data == NULL || size <= 0) {
			// NULLデータ
			// Data size in file
			file_.writeUint32(0); // Hash
			file_.writeUint32(0); // 元データサイズ
			file_.writeUint32(0); // pacファイル内でのデータサイズ
			file_.writeUint32(0); // Flags

		} else {
			// 圧縮データ
			std::string zbuf = KZlib::compress_zlib(data, size, PAC_COMPRESS_LEVEL);
			file_.writeUint32(0); // Hash
			file_.writeUint32(size); // 元データサイズ
			file_.writeUint32(zbuf.size()); // pacファイル内でのデータサイズ
			file_.writeUint32(0); // Flags
			file_.write(zbuf.data(), zbuf.size());
		}
		return true;
	}
};

class KPacFileReaderImpl {
	mutable FileInput file_;
public:
	KPacFileReaderImpl() {
	}
	~KPacFileReaderImpl() {
		close();
	}
	bool open(const Path &filename) {
		return file_.open(filename);
	}
	bool open(FileInput &file) {
		file_ = file;
		return file_.isOpen();
	}
	void close() {
		file_.close();
	}
	bool isOpen() const {
		return file_.isOpen();
	}
	void seekFirst() {
		file_.seek(0);
	}
	bool seekNext(Path *name) {
		if (file_.tell() >= file_.size()) {
			return false;
		}
		uint32_t len = 0;
		file_.read(NULL, PAC_MAX_LABEL_LEN); // Name
		file_.read(NULL, 4); // Hash
		file_.read(NULL, 4); // Data size
		file_.read(&len, 4); // Data size in pac file
		file_.read(NULL, 4); // Flags
		file_.read(NULL, len); // Data
		return true;
	}
	bool readFile(Path *name, std::string *data) {
		if (file_.tell() >= file_.size()) {
			return false;
		}
		if (name) {
			char s[PAC_MAX_LABEL_LEN];
			file_.read(s, PAC_MAX_LABEL_LEN);
			for (uint8_t i=0; i<PAC_MAX_LABEL_LEN; i++) {
				s[i] = s[i] ^ i;
			}
			*name = Path::fromUtf8(s);
		} else {
			file_.read(NULL, PAC_MAX_LABEL_LEN);
		}

		// Hash (NOT USE)
		uint32_t hash = file_.readUint32();

		// Data size (NOT USE)
		uint32_t datasize_orig = file_.readUint32();

		// Data size in pac file
		uint32_t datasize_inpac = file_.readUint32();

		// Flags (NOT USE)
		uint32_t flags = file_.readUint32();

		// Read data
		if (data) {
			if (datasize_orig > 0) {
				std::string zdata = file_.readBin(datasize_inpac);
				*data = KZlib::uncompress_zlib(zdata.data(), zdata.size(), datasize_orig);
				if (data->size() != datasize_orig) {
					Log_error("E_PAC_DATA_SIZE_NOT_MATCHED");
					data->clear();
					return false;
				}
			} else {
				*data = std::string();
			}
		} else {
			file_.read(NULL, datasize_inpac);
		}
		return true;
	}
	int getCount() {
		int num = 0;
		seekFirst();
		while (seekNext(NULL)) {
			num++;
		}
		return num;
	}
	int getIndexByName(const Path &entry_name, bool ignore_case, bool ignore_path) {
		seekFirst();
		int idx = 0;
		Path name;
		while (readFile(&name, NULL)) {
			if (name.compare(entry_name, ignore_case, ignore_path) == 0) {
				return idx;
			}
#ifdef _DEBUG
			if (name.compare(entry_name, true, false) == 0) { // ignore case で一致した
				if (name.compare(entry_name, false, false) != 0) { // case sensitive で不一致になった
					Log_warningf(
						u8"W_PAC_CASE_NAME: PACファイル内をファイル名 '%s' で検索中に、"
						u8"大小文字だけが異なるファイル '%s' を発見しました。"
						u8"これは意図した動作ですか？予期せぬ不具合の原因になるため、ファイル名の変更を強く推奨します",
						entry_name.u8(), name.u8()
					);
				}
			}
#endif
			idx++;
		}
		return -1;
	}
	Path getName(int index) {
		seekFirst();
		for (int i=1; i<index; i++) {
			if (!seekNext(NULL)) {
				return Path::Empty;
			}
		}
		Path name;
		if (readFile(&name, NULL)) {
			return name;
		}
		return Path::Empty;
	}
	std::string getData(int index) {
		seekFirst();
		for (int i=0; i<index; i++) {
			if (!seekNext(NULL)) {
				return false;
			}
		}
		std::string data;
		if (readFile(NULL, &data)) {
			return data;
		}
		return std::string();
	}
};



#pragma region KPacFileWriter
KPacFileWriter::KPacFileWriter() {
	impl_ = std::make_shared<KPacFileWriterImpl>();
}
KPacFileWriter::KPacFileWriter(const Path &filename) {
	impl_ = std::make_shared<KPacFileWriterImpl>();
	impl_->open(filename);
}
KPacFileWriter::KPacFileWriter(FileOutput &file) {
	impl_ = std::make_shared<KPacFileWriterImpl>();
	impl_->open(file);
}
bool KPacFileWriter::open(const Path &filename) {
	return impl_->open(filename);
}
bool KPacFileWriter::open(FileOutput &file) {
	return impl_->open(file);
}
void KPacFileWriter::close() {
	impl_->close();
}
bool KPacFileWriter::isOpen() const {
	return impl_->isOpen();
}
bool KPacFileWriter::addEntryFromFileName(const Path &entry_name, const Path &filename) {
	return impl_->addEntryFromFileName(entry_name, filename);
}
bool KPacFileWriter::addEntryFromMemory(const Path &entry_name, const void *data, size_t size) {
	return impl_->addEntryFromMemory(entry_name, data, size);
}
#pragma endregion // KPacFileWriter


#pragma region KPacFileReader
KPacFileReader::KPacFileReader() {
	impl_ = std::make_shared<KPacFileReaderImpl>();
}
KPacFileReader::KPacFileReader(const Path &filename) {
	impl_ = std::make_shared<KPacFileReaderImpl>();
	impl_->open(filename);
}
KPacFileReader::KPacFileReader(FileInput &file) {
	impl_ = std::make_shared<KPacFileReaderImpl>();
	impl_->open(file);
}
bool KPacFileReader::open(const Path &filename) {
	return impl_->open(filename);
}
bool KPacFileReader::open(FileInput &file) {
	return impl_->open(file);
}
void KPacFileReader::close() {
	impl_->close();
}
bool KPacFileReader::isOpen() const {
	return impl_->isOpen();
}
int KPacFileReader::getCount() const {
	return impl_->getCount();
}
int KPacFileReader::getIndexByName(const Path &entry_name, bool ignore_case, bool ignore_path) const {
	return impl_->getIndexByName(entry_name, ignore_case, ignore_path);
}
Path KPacFileReader::getName(int index) const {
	return impl_->getName(index);
}
std::string KPacFileReader::getData(int index) const {
	return impl_->getData(index);
}

#pragma endregion // KPacFileReader


} // namesace
