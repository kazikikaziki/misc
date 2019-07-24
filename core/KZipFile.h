#pragma once
#include <string>
#include <memory> // shared_ptr

namespace mo {

class Path;
class FileInput;
class FileOutput;
class KZipFileWriterImpl; // Internal class
class KZipFileReaderImpl; // Internal class


class KZipFileWriter {
public:
	KZipFileWriter();
	KZipFileWriter(const Path &filename);
	KZipFileWriter(FileOutput &file);
	~KZipFileWriter();

	bool open(const Path &filename);
	bool open(FileOutput &file);
	void close();
	bool isOpen() const;

	/// ZIPファイル全体に対するコメントを指定する
	void setZipComment(const char *comment_u8);

	/// 圧縮レベルを指定する
	///
	/// @param level
	/// - -1 : 既定の圧縮レベル
	/// -  0 : 無圧縮
	/// -  1 : 最高速
	/// -  ...
	/// -  9 : 最大圧縮
	///
	/// @note この設定はファイルごとに変えることができる。
	///       圧縮率を変更したい場合はファイル追加の直前で setCompressLevel を呼ぶ
	void setCompressLevel(int level);

	/// 暗号化パスワードを指定する。
	/// 空文字列を指定した場合は、暗号化しない
	///
	/// @note この設定はファイルごとに変えることができる。
	///       パスワードを変更したい場合はファイル追加の直前で setPassword を呼ぶ。
	///       ただし、展開アプリがファイル個別のパスワードに対応していない場合があるので注意
	void setPassword(const char *password);

	/// ファイルを追加する
	///
	/// @param filename 
	///      追加するファイル名。
	///      ここからフォルダ部分を取り除いた部分がアーカイブ内のファイル名になる
	///      ※フォルダ名部分を取り除いてしまうので、サブフォルダ内にアーカイブすることはできない。
	///      サブフォルダ内に入れたい場合は addEntryFromFileName(const Path &, const Path &) を使う
	/// @param timestamp_cma
	///      タイムスタンプ。
	///      3 個の time_t から成る配列で、作成 (Creation), 変更 (Modification), 最終アクセス (Access) の順番で格納する。
	///      不要な場合は NULL でも良い
	bool addEntryFromFileName(const Path &filename, const time_t *timestamp_cma=NULL);

	/// ファイルを追加する
	///
	/// @param entry_name アーカイブ内でのファイル名。フォルダ名を含む場合、アーカイブ内に同名のフォルダが作成される
	/// @param filename  追加するファイル名
	/// @param timestamp_cma タイムスタンプ。@see addEntryFromFileName
	bool addEntryFromFileName(const Path &entry_name, const Path &filename, const time_t *timestamp_cma=NULL);

	/// ファイルを追加する
	///
	/// @param entry_name アーカイブ内でのファイル名。フォルダ名を含む場合、アーカイブ内に同名のフォルダが作成される
	/// @param file 追加するファイル
	/// @param timestamp_cma タイムスタンプ。@see addEntryFromFileName
	bool addEntryFromFile(const Path &entry_name, FileInput &file, const time_t *timestamp_cma=NULL);

	/// バイナリデータを指定してファイルを追加する
	/// @param entry_name アーカイブ内でのファイル名。フォルダ名を含む場合、アーカイブ内に同名のフォルダが作成される
	/// @param data データ
	/// @param size データのバイト数。0を指定した場合は data がヌル終端文字であると仮定して長さを自動カウントする
	/// @param timestamp_cma タイムスタンプ。@see addEntryFromFileName
	bool addEntryFromMemory(const Path &entry_name, const void *data, size_t size, const time_t *timestamp_cma=NULL);

private:
	std::shared_ptr<KZipFileWriterImpl> impl_;
};


class KZipFileReader {
public:
	KZipFileReader();
	KZipFileReader(const char *filename);
	KZipFileReader(FileInput &file);
	~KZipFileReader();

	bool open(const char *filename);
	bool open(FileInput &file);
	void close();
	bool isOpen() const;

	/// アーカイブ内のファイルを探し、そのインデックス番号を返す。
	/// 見つからなければ -1 を返す。
	/// @param entry_name  検索ファイル名
	/// @param ignore_case 大小文字を無視して探す
	/// @param ignore_path パス部分を無視して探す
	int findEntry(const Path &entry_name, bool ignore_case, bool ignore_path) const;

	/// アーカイブ内のファイル数
	int getEntryCount() const;

	/// アーカイブ内のファイル名（フォルダ名も含む）
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	Path getEntryName(int index) const;

	/// エントリー名（無エンコード）
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	std::string getEntryNameBin(int index) const;

	/// 展開後のデータサイズ
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	size_t getEntryDataSize(int index) const;

	/// 暗号化されているかどうか
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	bool isEntryCrypt(int index) const;

	/// ファイル名やコメントがUTF8で記録されているかどうか
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	bool isEntryUtf8(int index) const;

	/// 拡張データの個数
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	int getEntryExtraCount(int index) const;

	/// コメント文字列（無変換）を返す
	/// コメント文字列が UTF8 かどうかは isEntryUtf8() で判定する
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	std::string getEntryCommentBin(int index) const;

	/// 拡張データを識別子で検索する
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	/// @param signature 16ビットの識別子。例えばファイルのタイムスタンプは 0x000A にあり、ZIP64の情報は 0x0001 にある。
	int findEntryExtraBySignature(int index, uint16_t signature) const;

	/// 拡張データ（バイナリ形式）
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	/// @param extra 拡張データ番号（拡張データの個数は getExtraCount で得られる）
	/// @param out_sign  拡張データの16ビット識別番号を得る。不要なら NULL を指定する
	/// @param out_bin   拡張データを得る。不要なら NULL を指定する
	bool getEntryExtra(int index, int extra, uint16_t *out_sign, std::string *out_bin) const;

	/// アーカイブ内のファイルを展開し、そのバイナリデータを std::string で返す
	/// 暗号化されていた場合は password に指定された文字列で復号を試みる
	/// 暗号化されているかどうかは isCrypt で調べる
	/// @param index エントリー番号。0 以上 getEntryCount() 未満
	std::string getEntryData(int index, const char *password=NULL);

	/// find と unzip を同時に行う。
	/// @see find
	/// @see unzip
	std::string getEntryDataByName(const Path &entry_name, bool ignore_case, bool ignore_path, const char *password=NULL);

	/// ZIPファイルに対するコメント文字列
	std::string getZipComment() const;

private:
	std::shared_ptr<KZipFileReaderImpl> impl_;
};


bool isZipFileSign(FileInput &file);


void Test_zip();



} // namespace
