#include "KFile.h"
#include "KPath.h"

#if 1
#include "KLog.h"
#include "KStd.h"
#define FILEASSERT(expr)    Log_assert(expr)
#define FILELOG(fmt, ...)   Log_verbosef(fmt, ##__VA_ARGS__)
#define FILEOPEN_U8(f, m)   K_fopen_u8(f, m)
#else
#include <assert.h>
#define FILEASSERT(expr)    assert(expr)
#define FILELOG(fmt, ...)   printf(fmt, ##__VA_ARGS__), putchar('\n')
#define FILEOPEN_U8(f, m)   fopen(f, m)
#endif



namespace mo {

namespace {

class CNativeOutput: public FileOutputCallback {
	Path name_;
	FILE *cb_;
	bool auto_close_;
public:
	CNativeOutput(FILE *fp, const Path &name, bool auto_close) {
		cb_ = fp;
		name_ = name;
		auto_close_ = auto_close;
	}
	virtual ~CNativeOutput() override {
		if (auto_close_) fclose(cb_);
	}
	virtual int on_file_tell() const override {
		return (int)ftell(cb_);
	}
	virtual void on_file_seek(int pos) override {
		fseek(cb_, pos, SEEK_SET);
	}
	virtual int on_file_write(const void *buf, int bytes) override {
		int ret = bytes;
		if (buf) {
			ret = (int)fwrite(buf, 1, bytes, cb_);
		} else {
			fseek(cb_, bytes, SEEK_CUR);
		}
		fflush(cb_);
		return ret;
	}
};

class CNativeInput: public FileInputCallback {
	FILE *cb_;
	int size_;
	bool auto_close_;
public:
	CNativeInput(FILE *fp, bool auto_close) {
		FILEASSERT(fp);
		cb_ = fp;
		auto_close_ = auto_close;
		int init = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		int head = ftell(fp);
		fseek(fp, 0, SEEK_END);
		int tail = ftell(fp);
		fseek(fp, init, SEEK_SET);
		FILEASSERT(tail >= head);
		size_ = tail - head;
	}
	virtual ~CNativeInput() override {
		if (auto_close_) fclose(cb_);
	}
	virtual int on_file_size() const override {
		return size_;
	}
	virtual int on_file_read(void *buf, int bytes) override {
		if (buf) {
			return (int)fread(buf, 1, bytes, cb_);
		} else {
			fseek(cb_, bytes, SEEK_CUR);
			return bytes;
		}
	}
	virtual int on_file_seek(int bytes) override {
		fseek(cb_, bytes, SEEK_SET);
		return ftell(cb_);
	}
	virtual int on_file_tell() const override {
		return (int)ftell(cb_);
	}
};

class CStaticMemoryInput: public FileInputCallback {
	uint8_t *buf_;
	int size_;
	int pos_;
public:
	CStaticMemoryInput(const void *data, int size) {
		// data == NULL はダメ
		// size == 0 は OK
		FILEASSERT(data);
		pos_ = 0;
		buf_ = (uint8_t *)data;
		size_ = size;
	}
	virtual int on_file_size() const override {
		return size_;
	}
	virtual int on_file_read(void *buf, int bytes) override {
		int len = bytes;
		if (size_ < pos_ + len) {
			len = size_ - pos_;
		}
		if (len > 0) {
			if (buf) memcpy(buf, buf_ + pos_, len);
			pos_ += len;
			return len;
		}
		return 0;
	}
	virtual int on_file_seek(int new_pos) override {
		if (new_pos < 0) {
			pos_ = 0;
		} else if (new_pos >= size_) {
			pos_ = size_;
		} else {
			pos_ = new_pos;
		}
		return pos_;
	}
	virtual int on_file_tell() const override {
		FILEASSERT(0 <= pos_ && pos_ <= size_);
		return pos_;
	}
};

class CMemoryInput: public FileInputCallback {
	CStaticMemoryInput *static_reader_;
	std::string buf_;
public:
	CMemoryInput(const void *data, int size) {
		static_reader_ = NULL;
		if (size > 0) {
			buf_.resize(size);
			buf_.assign((const char*)data, size);
			static_reader_ = new CStaticMemoryInput(buf_.data(), size);
		} else {
			static_reader_ = new CStaticMemoryInput(NULL, 0);
		}
	}
	virtual ~CMemoryInput() {
		delete static_reader_;
	}
	virtual int on_file_size() const override {
		return static_reader_->on_file_size();
	}
	virtual int on_file_read(void *buf, int bytes) override {
		return static_reader_->on_file_read(buf, bytes);
	}
	virtual int on_file_seek(int new_pos) override {
		return static_reader_->on_file_seek(new_pos);
	}
	virtual int on_file_tell() const override {
		return static_reader_->on_file_tell();
	}
};

class CMemoryOutput: public FileOutputCallback {
public:
	std::string *buf_;
	int start_;
	int pos_;

	explicit CMemoryOutput(std::string *buf) {
		FILEASSERT(buf);
		buf_ = buf;
		start_ = buf_->size();
		pos_ = 0;
	}
	virtual int on_file_write(const void *buf, int bytes) override {
		FILEASSERT(bytes >= 0);
		size_t cur = start_ + pos_;
		if (cur + bytes > buf_->size()) {
			buf_->resize(cur + bytes);
		}
		memcpy((void *)(buf_->c_str() + cur), buf, bytes);
		pos_ += bytes;
		return bytes;
	}
	virtual int on_file_tell() const override {
		return pos_;
	}
	virtual void on_file_seek(int pos) override {
		if (pos >= 0) {
			pos_ = pos;
		} else {
			pos_ = 0;
		}
	}
};


} // private

#pragma region FileOutput 
FileOutput::FileOutput() {
	cb_ = NULL;
}
FileOutput::FileOutput(const FileOutput &other) {
	cb_ = other.cb_;
}
FileOutput::FileOutput(const Path &filename, bool append) {
	cb_ = NULL;
	open(Path(filename));
}
FileOutput::~FileOutput() {
	close();
}
bool FileOutput::open(FileOutputCallback *cb) {
	close();
	if (cb) {
		cb_ = std::shared_ptr<FileOutputCallback>(cb);
		return true;
	}
	return false;
}
bool FileOutput::open(std::string &output) {
	close();
	cb_ = std::make_shared<CMemoryOutput>(&output);
	return true;
}
bool FileOutput::open(const Path &filename, bool append) {
	close();
	FILE *fp = FILEOPEN_U8(filename.u8(), append ? "ab" : "wb");
	if (fp) {
		cb_ = std::make_shared<CNativeOutput>(fp, filename, true);
		FILELOG("FileOutput::open %s", filename.u8());
		return true;
	}
	FILELOG("FileOutput::open %s [FAIELD!!]", filename.u8());
	return false;
}
void FileOutput::close() {
	cb_ = NULL;
}
bool FileOutput::isOpen() const {
	return !empty();
}
bool FileOutput::empty() const {
	return cb_ == NULL;
}
FileOutput & FileOutput::operator = (const FileOutput &other) {
	cb_ = other.cb_;
	return *this;
}
int FileOutput::tell() const {
	return cb_ ? cb_->on_file_tell() : 0;
}
void FileOutput::write(const void *data, int size) {
	if (cb_) cb_->on_file_write(data, size);
}
void FileOutput::write(const std::string &bin) {
	write(bin.data(), bin.size());
}
void FileOutput::writeUint8(uint8_t value) {
	write(&value, sizeof(value));
}
void FileOutput::writeUint16(uint16_t value) {
	write(&value, sizeof(value));
}
void FileOutput::writeUint32(uint32_t value) {
	write(&value, sizeof(value));
}
#pragma endregion // FileOutput 


#pragma region FileInput 
FileInput::FileInput() {
	cb_ = NULL;
	eof_ = false;
}
FileInput::FileInput(const FileInput &other) {
	cb_ = other.cb_;
	eof_ = other.eof_;
}
FileInput::FileInput(const Path &filename) {
	cb_ = NULL;
	eof_ = false;
	open(filename);
}
FileInput::~FileInput() {
	close();
}
bool FileInput::open(FileInputCallback *cb) {
	close();
	//
	if (cb) {
		cb_ = std::shared_ptr<FileInputCallback>(cb);
		eof_ = false;
		return true;
	}
	return false;
}
bool FileInput::open(const void *data, int size, bool copy) {
	close();
	//
	if (data && size > 0) {
		if (copy) {
			cb_ = std::make_shared<CMemoryInput>(data, size);
		} else {
			cb_ = std::make_shared<CStaticMemoryInput>(data, size);
		}
		eof_ = false;
		return true;
	}
	return false;
}
bool FileInput::open(const Path &filename) {
	close();
	//
	FILE *fp = FILEOPEN_U8(filename.u8(), "rb");
	if (fp) {
		cb_ = std::make_shared<CNativeInput>(fp, true);
		eof_ = feof(fp);
		FILELOG("FileInput::open %s", filename.u8());
		return true;
	}
	FILELOG("FileInput::open %s [FAIELD!!]", filename.u8());
	return false;
}
FileInput & FileInput::operator = (const FileInput &other) {
	cb_ = other.cb_;
	eof_ = other.eof_;
	return *this;
}
bool FileInput::empty() const {
	return cb_ == NULL;
}
bool FileInput::isOpen() const {
	return !empty();
}
void FileInput::close() {
	cb_ = NULL;
	eof_ = true;
}
int FileInput::size() const {
	return cb_ ? cb_->on_file_size() : 0;
}
int FileInput::tell() const {
	return cb_ ? cb_->on_file_tell() : 0;
}
void FileInput::seek(int pos) {
	if (cb_) {
		cb_->on_file_seek(pos);
	}
}
int FileInput::read(void *data, int size) {
	int rsize = 0;
	if (cb_) {
		rsize = cb_->on_file_read(data, size);
		eof_ = rsize < size;
	}
	return rsize;
}
uint8_t FileInput::readUint8() {
	const int size = 1;
	uint8_t val = 0;
	if (read(&val, size) == size) {
		return val;
	}
	return 0;
}
uint16_t FileInput::readUint16() {
	const int size = 2;
	uint16_t val = 0;
	if (read(&val, size) == size) {
		return val;
	}
	return 0;
}
uint32_t FileInput::readUint32() {
	const int size = 4;
	uint32_t val = 0;
	if (read(&val, size) == size) {
		return val;
	}
	return 0;
}
std::string FileInput::readBin(int size) {
	std::string bin(size, '\0');
	int sz = read((void*)bin.data(), bin.size());
	bin.resize(sz);
	return bin;
}
std::string FileInput::readBin() {
	std::string bin;
	if (cb_) {
		int restsize = size() - tell();
		if (restsize > 0) {
			bin.resize(restsize);
			int readsize = read(&bin[0], bin.size());
			if (readsize < (int)bin.size()) {
				bin.resize(readsize);
			}
		}
	}
	return bin;
}
#pragma endregion // FileInput




void Test_file() {
	FileInput file;
	FILEASSERT(file.empty());
	FILEASSERT(!file.isOpen());
	
	FileOutput o;
	o.open("~test.bin");
	FILEASSERT(o.isOpen());
	o.writeUint32(0xAABBCCDD);

	FileOutput o2 = o;
	FILEASSERT(o2.isOpen());
	o2.close();

	FileInput i;
	i.open("~test.bin");
	FILEASSERT(i.tell() == 0);
	FILEASSERT(i.readUint32() == 0xAABBCCDD);
	FILEASSERT(i.tell() == 4);
	i.seek(0);
	FILEASSERT(i.tell() == 0);
	FILEASSERT(i.readUint32() == 0xAABBCCDD);
	FILEASSERT(i.tell() == 4);

	i.seek(0);
	i.seek(i.tell() + 2);
	FILEASSERT(i.readUint8() == 0xBB);
	FILEASSERT(i.readUint8() == 0xAA);
}


} // namespace
