#pragma once
#include <inttypes.h>
#include <stack>
#include <string>

namespace mo {

class IFileWriter;

typedef uint16_t chunk_id_t;
typedef uint32_t chunk_size_t;

/// チャンクを連結した形式のファイルの読み込みを行う。
/// ここで言うチャンク構造とは、次のブロックが連続しているフォーマットを言う
/// <pre>
/// +---------------------+
/// | Siganture (2 bytes) |  <--- Header block
/// | Size (4 bytes)      |
/// +---------------------+
/// | Data (n bytes)      |  <--- Data block
/// +---------------------+
///
/// 入れ子も
/// +---------------------+
/// | Siganture (2 bytes) |
/// | Size (4 bytes)      |
/// +---------------------+
/// | Data (n bytes) <-- 子チャンクの総サイズ
/// |                     |
/// |　　　+---------------------+
/// |　　　| Siganture (2 bytes) |
/// |　　　| Size (4 bytes)      |
/// |　　　+---------------------+
/// |　　　| Data (n bytes)      |
/// |　　　+---------------------+
/// |                     |
/// |　　　+---------------------+
/// |　　　| Siganture (2 bytes) |
/// |　　　| Size (4 bytes)      |
/// |　　　+---------------------+
/// |　　　| Data (n bytes)      |
/// |　　　+---------------------+
/// |                     |
/// +---------------------+
/// </pre>
class KChunkedFileReader {
public:
	KChunkedFileReader();

	/// 読み取り対象のバイナリデータをセットし、読み取り位置を初期化する
	void init(const void *buf, size_t len);

	/// 読み取り位置がファイル終端に達しているかどうか
	bool eof() const;

	/// 次のチャンク識別子が id と一致するかどうか
	bool checkChunkId(chunk_id_t id) const;

	/// 次のチャンクの識別子とサイズを得る（読み取りポインタは移動しない）
	/// EOFに達しているか、次のチャンクが存在しない場合は何もせずに false を返す
	bool getChunkHeader(chunk_id_t *out_id, chunk_size_t *out_size) const;

	/// チャンクの入れ子を開始する
	/// チャンク識別子が id と一致しない場合はエラーになる
	void openChunk(chunk_id_t id);

	/// チャンクの入れ子を終了する
	void closeChunk();

	/// 次のチャンクをロードし、データを data にコピーする。
	/// @param id        確認用のチャンク識別子。この識別子と一致しない場合はエラーになる
	/// @param data[out] チャンクデータのコピー先。NULLでも良い。読み取ったデータサイズは data->size() で得る
	void readChunk(chunk_id_t id, std::string *data);

	/// 次のチャンクをロードし、データを data にコピーする。
	/// @param id         @see readChunkHeader
	/// @param size       @see readChunkHeader
	/// @param data[out]  チャンクデータのコピー先。少なくとも size バイトが必要。
	void readChunk(chunk_id_t id, chunk_size_t size, void *data);

	/// 次のチャンクをロードし、データを data にコピーする。
	/// @param id         @see readChunkHeader
	/// @param size       @see readChunkHeader
	/// @param data[out]  チャンクデータのコピー先。NULLでも良い。読み取ったデータサイズは data->size() で得る
	void readChunk(chunk_id_t id, chunk_size_t size, std::string *data);

	std::string readChunk(chunk_id_t id);
	uint8_t readChunk1(chunk_id_t id);
	uint16_t readChunk2(chunk_id_t id);
	uint32_t readChunk4(chunk_id_t id);

private:
	/// チャンクヘッダを読み取り、データ部先頭にポインタを進める。
	/// @param id    チャンクヘッダの識別子確認用。識別子が id と一致しない場合はエラーになる。確認不要の場合は 0 にする
	/// @param size  チャンクデータのサイズ確認用。サイズが size と一致しない場合はエラーになる。確認不要の場合は 0 にする
	/// 一致しない場合はエラーになる。
	void readChunkHeader(chunk_id_t id, chunk_size_t size);

	const uint8_t *ptr_; // 現在のデータ読み取り位置
	const uint8_t *end_; // データ終端
	const uint8_t *chunk_data_; // 現在のチャンクのデータ部分先頭アドレス
	chunk_id_t chunk_id_;   // 現在のチャンク識別子
	chunk_size_t chunk_size_; // 現在のチャンクのデータ部分バイト数
	std::stack<const uint8_t *> nest_;
};

/// チャンクを連結した形式のファイルの書き込みを行う。
/// チャンクのフォーマットについては KChunkedFileReader を参照
class KChunkedFileWriter {
public:
	KChunkedFileWriter();
	void clear();

	/// 入れ子チャンクを開始する
	void openChunk(chunk_id_t id);

	/// 入れ子チャンクを閉じる
	void closeChunk();

	/// 1バイトデータを持つチャンクを追加する
	void writeChunk1(chunk_id_t id, uint8_t data);

	/// 2バイトデータを持つチャンクを追加する
	void writeChunk2(chunk_id_t id, uint16_t data);

	/// 4バイトデータを持つチャンクを追加する
	void writeChunk4(chunk_id_t id, uint32_t data);

	/// nバイトデータを持つチャンクを追加する
	void writeChunkN(chunk_id_t id, chunk_size_t size, const void *data);

	/// nバイトデータを持つチャンクを追加する
	void writeChunkN(chunk_id_t id, const std::string &data);

	/// nバイトデータを持つチャンクを追加する
	/// @param data ヌル終端文字列
	void writeChunkN(chunk_id_t id, const char *data);

	/// ファイルに書き込むべきチャンクデータを返す
	const void * data() const;

	/// ファイルに書き込むべきチャンクデータのサイズを返す
	size_t size() const;

private:
	uint32_t pos_;
	std::string buf_;
	std::stack<uint32_t> stack_;
};


void Test_chunk();


} // namespace

