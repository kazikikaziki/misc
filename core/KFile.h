#pragma once
#include <string>
#include <memory> // std::shared_ptr

namespace mo {

class Path;

class FileOutputCallback {
public:
	virtual ~FileOutputCallback() {}

	/// ファイルにバイト列を書き込む。実際に書き込んだサイズを返す
	virtual int on_file_write(const void *buf, int bytes) = 0;

	/// 現在の書き込み位置をバイト単位で返す
	virtual int on_file_tell() const = 0;

	/// 書き込み位置を設定する。
	/// pos にはどんな値を指定してもエラーにならないが、実際に設定される読み取り位置は必ず
	/// 0 以上 getSize() 以下になる（未満ではない事に注意）。
	/// 実際に移動した後の読み取り位置を返す
	virtual void on_file_seek(int pos) = 0;
};


class FileInputCallback {
public:
	virtual ~FileInputCallback() {}

	/// ファイルからバイト列を読み取り、読み取り位置を進める。
	/// ファイル終端を超えた場合、読み取り位置はファイル終端で停止し、それを超えることはない。
	/// 実際に読み取ったバイト数を返す
	/// なお、buf に NULL を指定した場合は読み取り位置だけを移動する
	virtual int on_file_read(void *buf, int len) = 0;

	/// ファイルサイズをバイト単位で返す
	virtual int on_file_size() const = 0;
	
	/// 現在の読み取り位置をバイト単位で返す。
	/// 戻り値は 0 以上 on_file_size() 以下にある（未満ではない）。
	/// on_file_size() に等しかった場合は EOF を意味する
	virtual int on_file_tell() const = 0;

	/// 読み取り位置を設定する。
	/// pos にはどんな値を指定してもエラーにならないが、実際に設定される読み取り位置は必ず
	/// 0 以上 on_file_size() 以下になる（未満ではない）。
	/// on_file_size() に等しい場合は EOF を意味する
	/// 実際に移動した後の読み取り位置を返す
	virtual int on_file_seek(int pos) = 0;
};





class FileOutput {
public:
	FileOutput();
	FileOutput(const FileOutput &other);
	FileOutput(const Path &filename, bool append=false);
	~FileOutput();
	FileOutput & operator = (const FileOutput &file);
	bool open(FileOutputCallback *cb);
	bool open(std::string &output);
	bool open(const Path &filename, bool append=false);
	void close();
	bool isOpen() const;
	bool empty() const;
	int tell() const;
	void write(const void *data, int size);
	void write(const std::string &bin);
	void writeUint8(uint8_t value);
	void writeUint16(uint16_t value);
	void writeUint32(uint32_t value);

private:
	std::shared_ptr<FileOutputCallback> cb_;
};

class FileInput {
public:
	FileInput();
	FileInput(const FileInput &other);
	FileInput(const Path &filename);
	~FileInput();

	bool open(const void *data, int size, bool copy=true);
	bool open(FileInputCallback *cb);
	bool open(const Path &filename);
	FileInput & operator = (const FileInput &file);

	/// ファイルが開かれていて読み取り可能な状態ならば true を返す。
	/// @note ファイルサイズが 0 の場合でも true を返す
	bool isOpen() const;

	/// ファイルが読み込まれていないなら true を返す。
	/// このメソッドは !isOpen() と同じ
	bool empty() const;

	/// ファイルを閉じる
	void close();

	/// シーク可能な範囲をバイト単位で返す
	int size() const;

	/// 現在の読み取り位置をバイト単位で返す
	int tell() const;

	/// 読み取り位置をファイルの先頭から pos バイト目に移動する
	void seek(int pos);

	/// 長さ size バイトを読んで data にセットする。
	/// 実際に読み取ったバイト数を返す
	int read(void *data, int size);

	/// 長さ size バイトの列を読んで file_bin_t 形式で返す。
	/// 実際に読み取ったバイト数は file_bin_t::size() で得られる
	std::string readBin(int size);

	/// 終端まですべて読んで file_bin_t 形式で返す。
	/// 実際に読み取ったバイト数は file_bin_t::size() で得られる
	std::string readBin();

	/// 1バイトのデータを uint8 として読む。
	/// 既に eof に達していた場合は 0 を返す
	uint8_t readUint8();

	/// 2バイトのデータを uint16 として読む。
	/// 2バイト未満しか読み取れなかった場合は 0 を返す
	uint16_t readUint16();

	/// 4バイトのデータを uint32 として読む。
	/// 4バイト未満しか読み取れなかった場合は 0 を返す
	uint32_t readUint32();

private:
	std::shared_ptr<FileInputCallback> cb_;
	bool eof_;
};


void Test_file();


} // namespace
