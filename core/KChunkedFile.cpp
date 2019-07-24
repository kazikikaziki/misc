#include "KChunkedFile.h"

#if 1
#	include "KLog.h"
#	define CFERROR(msg)   Log_error(msg)
#	define CFASSERT(x)    Log_assert(x)
#else
#	include <assert.h>
#	define CFERROR(msg)   assert(0 && msg)
#	define CFASSERT(x)    assert(x)
#endif

#define SIGN_SIZE sizeof(chunk_id_t)
#define SIZE_SIZE sizeof(chunk_size_t)

namespace mo {

#pragma region KChunkedFileReader
	KChunkedFileReader::KChunkedFileReader() {
	ptr_ = NULL;
	end_ = NULL;
	chunk_data_ = NULL;
	chunk_id_ = 0;
	chunk_size_ = 0;
	nest_ = std::stack<const uint8_t *>(); // clear
}
void KChunkedFileReader::init(const void *buf, size_t len) {
	ptr_ = (uint8_t *)buf;
	end_ = (uint8_t *)buf + len;
	chunk_id_ = 0;
	chunk_data_ = NULL;
	chunk_size_ = 0;
	nest_ = std::stack<const uint8_t *>(); // clear
}
bool KChunkedFileReader::eof() const {
	return end_ <= ptr_;
}
bool KChunkedFileReader::checkChunkId(chunk_id_t id) const {
	chunk_id_t s;
	if (getChunkHeader(&s, NULL)) {
		if (s == id) {
			return true;
		}
	}
	return false;
}
void KChunkedFileReader::openChunk(chunk_id_t id) {
	// 子チャンクの先頭に移動する
	chunk_size_t size = 0;
	getChunkHeader(NULL, &size);
	readChunkHeader(id, 0);

	// この時点で ptr_ は子チャンク先頭を指している。
	// そこからさらに size 進めた部分が子チャンク終端になる
	nest_.push(ptr_ + size);
}
void KChunkedFileReader::closeChunk() {
	// 入れ子の終端に一致しているか確認する
	CFASSERT(!nest_.empty());
	CFASSERT(nest_.top() == ptr_);
	nest_.pop();
}
bool KChunkedFileReader::getChunkHeader(chunk_id_t *out_id, chunk_size_t *out_size) const {
	if (ptr_+SIGN_SIZE+SIZE_SIZE < end_) {
		if (out_id) {
			memcpy(out_id, ptr_, SIGN_SIZE);
		}
		if (out_size) {
			memcpy(out_size, ptr_+SIGN_SIZE, SIZE_SIZE);
		}
		return true;
	}
	return false;
}
void KChunkedFileReader::readChunkHeader(chunk_id_t id, chunk_size_t size) {
	CFASSERT(ptr_);

	memcpy(&chunk_id_, ptr_, SIGN_SIZE);
	ptr_ += SIGN_SIZE;

	memcpy(&chunk_size_, ptr_, SIZE_SIZE);
	ptr_ += SIZE_SIZE;

	CFASSERT(id==0 || id==chunk_id_);
	CFASSERT(size==0 || size==chunk_size_);
	chunk_data_ = ptr_;
}
void KChunkedFileReader::readChunk(chunk_id_t id, chunk_size_t size, void *data) {
	readChunkHeader(id, size);

	CFASSERT(chunk_data_);
	if (chunk_size_ > 0) {
		if (data) memcpy(data, chunk_data_, chunk_size_);
	}
	ptr_ = chunk_data_ + chunk_size_;
}

void KChunkedFileReader::readChunk(chunk_id_t id, chunk_size_t size, std::string *data) {
	readChunkHeader(id, size);

	CFASSERT(chunk_data_);
	if (data) {
		data->resize(chunk_size_);
		if (chunk_size_ > 0) {
			memcpy(&(*data)[0], chunk_data_, chunk_size_);
		}
	}
	ptr_ = chunk_data_ + chunk_size_;
}
std::string KChunkedFileReader::readChunk(chunk_id_t id) {
	std::string val;
	readChunk(id, 0, &val);
	return val;
}
uint8_t KChunkedFileReader::readChunk1(chunk_id_t id) {
	uint8_t val = 0;
	readChunk(id, 1, &val);
	return val;
}
uint16_t KChunkedFileReader::readChunk2(chunk_id_t id) {
	uint16_t val = 0;
	readChunk(id, 2, &val);
	return val;
}
uint32_t KChunkedFileReader::readChunk4(chunk_id_t id) {
	uint32_t val = 0;
	readChunk(id, 4, &val);
	return val;
}

void KChunkedFileReader::readChunk(chunk_id_t id, std::string *data) {
	readChunkHeader(id, 0);

	CFASSERT(chunk_data_);
	if (data) {
		data->resize(chunk_size_);
		if (chunk_size_ > 0) {
			memcpy(&(*data)[0], chunk_data_, chunk_size_);
		}
	}
	ptr_ = chunk_data_ + chunk_size_;
}
#pragma endregion // KChunkedFileReader





#pragma region KChunkedFileWriter
KChunkedFileWriter::KChunkedFileWriter() {
	pos_ = 0;
}
void KChunkedFileWriter::clear() {
	pos_ = 0;
	buf_.clear();
	while (! stack_.empty()) stack_.pop();
}
const void * KChunkedFileWriter::data() const {
	return buf_.data();
}
size_t KChunkedFileWriter::size() const {
	return buf_.size();
}
void KChunkedFileWriter::openChunk(chunk_id_t id) {
	// チャンクを開く。チャンクヘッダだけ書き込んでおく
	// ただし、チャンクデータサイズは子チャンクを追加してからでないと確定できないので、
	// いま書き込むのは識別子のみ
	buf_.resize(pos_ + SIGN_SIZE + SIZE_SIZE);

	memcpy(&buf_[pos_], &id, SIGN_SIZE);
	pos_ += SIGN_SIZE;

	stack_.push(pos_);
	pos_ += SIZE_SIZE; // データサイズは未確定（チャンクが閉じたときにはじめて確定する）なので、まだ書き込まない
}
void KChunkedFileWriter::closeChunk() {
	// 現在のチャンクのヘッダー位置（サイズ書き込み位置）
	chunk_size_t sizepos = stack_.top();

	// 現在のチャンクのデータ開始位置
	chunk_size_t datapos = sizepos + SIZE_SIZE;

	// 現在のチャンクのデータサイズ
	chunk_size_t size = pos_ - datapos;

	// 現在のチャンクヘッダ位置まで戻り、確定したチャンクデータサイズを書き込む
	memcpy(&buf_[sizepos], &size, SIZE_SIZE);
	stack_.pop();
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, chunk_size_t size, const void *data) {
	// チャンクを書き込むのに必要なサイズだけ拡張する
	// 新たに必要になるサイズ = チャンクヘッダ＋データ
	buf_.resize(pos_ + (SIGN_SIZE + SIZE_SIZE) + size);

	memcpy(&buf_[pos_], &id, SIGN_SIZE);
	pos_ += SIGN_SIZE;

	memcpy(&buf_[pos_], &size, 4);
	pos_ += SIZE_SIZE;

	if (size > 0) {
		if (data) {
			memcpy(&buf_[pos_], data, size);
		} else {
			memset(&buf_[pos_], 0, size);
		}
		pos_ += size;
	}
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, const std::string &data) {
	if (data.empty()) {
		// サイズゼロのデータを持つチャンク
		writeChunkN(id, 0, NULL);
	} else {
		// std::string::size() の戻り値 size_t は 32ビットとは限らないので注意。
		// 例えば MacOS だと size_t は 64bit になる
		writeChunkN(id, (chunk_size_t)data.size(), &data[0]);
	}
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, const char *data) {
	// strlen の戻り値 size_t は 32ビットとは限らないので注意。
	// 例えば MacOS だと size_t は 64bit になる
	writeChunkN(id, (chunk_size_t)strlen(data), data);
}
void KChunkedFileWriter::writeChunk1(chunk_id_t id, uint8_t data) {
	writeChunkN(id, 1, &data);
}
void KChunkedFileWriter::writeChunk2(chunk_id_t id, uint16_t data) {
	writeChunkN(id, 2, &data);
}
void KChunkedFileWriter::writeChunk4(chunk_id_t id, uint32_t data) {
	writeChunkN(id, 4, &data);
}
#pragma endregion // KChunkedFileWriter



void Test_chunk() {

	// データ作成
	KChunkedFileWriter w;
	w.writeChunk2(0x1000, 0x2019);
	w.writeChunk4(0x1001, 0xDeadBeef);
	w.writeChunkN(0x1002, "HELLO WOROLD!");
	w.openChunk(0x1003);
	w.writeChunk1(0x2000, 'a');
	w.writeChunk1(0x2001, 'b');
	w.writeChunk1(0x2002, 'c');
	w.closeChunk();

	// 書き込んだバイナリデータを得る
	std::string bin((const char *)w.data(), w.size());

	// 読み取る
	KChunkedFileReader r;
	r.init(bin.data(), bin.size());
	CFASSERT(r.readChunk2(0x1000) == 0x2019);
	CFASSERT(r.readChunk4(0x1001) == 0xDeadBeef);
	CFASSERT(r.readChunk(0x1002) == "HELLO WOROLD!");

	r.openChunk(0); // 入れ子チャンクに入る(識別子気にしない)
	CFASSERT(r.readChunk1(0) == 'a');
	CFASSERT(r.readChunk1(0) == 'b');
	CFASSERT(r.readChunk1(0) == 'c');
	r.closeChunk();
}



} // namespace

