#include "KFile.h"
#include "KFileTime.h"
#include "KPath.h"
#include "KZipFile.h"
#include "KZlib.h"
#include <vector>
#include <stdlib.h> // rand


#if 1
#include "KLog.h"
#define ZIPASSERT(x) Log_assert(x)
#define ZIPERROR(x)  Log_error(x)
#else
#include <assert.h>
#define ZIPASSERT(x) assert(x)
#define ZIPERROR(x)  printf("%s\n", x)
#endif


// ファイル名などの情報を UTF8 で書き込む場合には 1 を指定する。
// 実行環境のロケールにしたがってマルチバイト文字列（日本語環境ならSJIS）で書き込むなら 0 を指定する
#define SAVE_AS_UTF8 1

namespace mo {



// ZIP書庫ファイルフォーマット
// http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html

// 圧縮zipを作ってみる
// http://www.tnksoft.com/reading/zipfile/


// ローカルファイルヘッダ
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
#pragma pack (push, 1)
struct zip_local_file_header_t {
	uint32_t signature;
	uint16_t version_needed_to_extract;
	uint16_t general_purpose_bit_flag;
	uint16_t compression_method;
	uint16_t last_mod_file_time;
	uint16_t last_mod_file_date;
	uint32_t data_crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t file_name_length;
	uint16_t extra_field_length;
};
#pragma pack (pop)



// 中央ディレクトリヘッダ
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
#pragma pack (push, 1)
struct zip_central_directory_header_t {
	uint32_t signature;
	uint16_t version_made_by;
	uint16_t version_needed_to_extract;
	uint16_t general_purpose_bit_flag;
	uint16_t compression_method;
	uint16_t last_mod_file_time;
	uint16_t last_mod_file_date;
	uint32_t data_crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t file_name_length;
	uint16_t extra_field_length;
	uint16_t file_comment_length;
	uint16_t disk_number_start;
	uint16_t internal_file_attributes;
	uint32_t external_file_attributes;
	uint32_t relative_offset_of_local_header;
};
#pragma pack (pop)



// 終端レコード
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
#pragma pack (push, 1)
struct zip_end_of_central_directory_record_t {
	uint32_t signature;
	uint16_t disknum;
	uint16_t startdisknum;
	uint16_t diskdirentry;
	uint16_t direntry;
	uint32_t dirsize;
	uint32_t startpos;
	uint16_t comment_length;
};
#pragma pack (pop)


#if 0
// データデスクリプタ
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
//
//
// ところで、このプログラムでは、これは使わない。
//
// データデスクリプタ―の有無は ZIP_OPT_DATADESC フラグで調べられるが、
// でスクリプターが存在する場合はローカルファイルヘッダにファイルサイズが載っていないため
// ファイル末尾の終端ヘッダから辿らないといけない。
// 終端ヘッダには中央ディレクトリヘッダのオフセットが載っており、それを元にしてチュウオウディレクトリヘッダまで辿ることができる。
// ところで、チュウオウディレクトリヘッダにもデータサイズが載っている。
// 故に、データデスクリプタ―を参照してファイルサイズを得ることはない。
#pragma pack (push, 1)
struct zip_data_descriptor_t {
	uint32_t data_crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
};
#pragma pack (pop)
#endif



#define ZIP_CRYPT_HEADER_SIZE           12         // 暗号ヘッダのバイト数
#define ZIP_VERSION_UNCOMPRESS          10         // ver1.0 無圧縮
#define ZIP_VERSION_DEFLATE             20         // ver2.0 Deflate 圧縮
#define ZIP_COMPRESS_METHOD_UNCOMPRESS  0          // 圧縮方法: 無圧縮
#define ZIP_COMPRESS_METHOD_DEFLATE     8          // 圧縮方法: Deflate
#define ZIP_OPT_ENCRYPTED               0x0001     // bit  0 -- 暗号化されている
#define ZIP_OPT_DATADESC                0x0008     // bit  3 -- Data Descriptor (PK0708) が存在する
#define ZIP_OPT_UTF8                    0x0800     // bit 11 -- ファイル名やコメントが UTF-8文字
#define ZIP_SIGN_PK0102                 0x02014B50 // 中央ディレクトリヘッダ
#define ZIP_SIGN_PK0304                 0x04034B50 // ローカルファイルヘッダ
#define ZIP_SIGN_PK0506                 0x06054B50 // 終端レコード
#define ZIP_SIGN_PK0708                 0x08074B50 // データデスクリプタ http://www.tnksoft.com/reading/zipfile/pk0708.php
#define ZIPEX_MAX_NAME                  256        // ZIP格納するエントリー名の最大数（自主的に科した制限で、ZIPの仕様とは無関係）
#define ZIPEX_MAX_EXTRA                 16         // ZIPに格納する拡張データの最大数（自主的に科した制限で、ZIPの仕様とは無関係）


// time_t 形式の時刻（GMT) をローカル時刻 struct tm に変換する
static void _GetLocalTime(struct tm *out_tm, time_t gmt) {
#ifdef _WIN32
	localtime_s(out_tm, &gmt);
#else
	localtime_r(&gmt, out_tm);
#endif
}

// file の現在位置からの2バイトが value と等しいか調べる。
// この関数は読み取りヘッダを移動しない
static bool _FileMatchUint16(FileInput &file, uint16_t value) {
	int pos = file.tell();
	uint16_t data = file.readUint16();
	file.seek(pos);
	return data == value;
}

// file の現在位置からの4バイトが value と等しいか調べる。
// この関数は読み取りヘッダを移動しない
static bool _FileMatchUint32(FileInput &file, uint32_t value) {
	int pos = file.tell();
	uint32_t data = file.readUint32();
	file.seek(pos);
	return data == value;
}



// Zip で使われている CRC32 の計算を行う。
//（crc32 ではなく、crc32b であることに注意）
// https://stackoverflow.com/questions/15861058/what-is-the-difference-between-crc32-and-crc32b
//
class KZipCrc32 {
public:
	static uint32_t compute(const void *buf, uint32_t size) {
		uint32_t a = (uint32_t)(-1); // 0 のビット反転からスタート
		for (uint32_t i=0; i<size; i++){
			uint8_t b = ((const uint8_t *)buf)[i];
			a = computeByte(a, b);
		}
		return ~a; // 最後にビット反転するのを忘れないように（初期値が 0x00000000 ではなく、その反転 0xFFFFFFFF で始まっているため）
	}

	// 1ByteデータのCRC32を計算する
	//
	// crc には前回の計算結果を指定する。
	// 初回計算時や、データにつながりがないときは (uint32_t)(-1) を指定する
	static uint32_t computeByte(uint32_t crc, uint8_t val) {
#if 0
		// テーブルを自動生成する
		static uint32_t s_table[256] = {-1}; // 未初期化のフラグとして最初の要素に -1 を入れておく
		if (s_table[0] != 0) {
			for (uint32_t i=0; i<256; i++) {
				uint32_t c = i;
				for (int j=0; j<8; j++) {
					c = (c & 1) ? (c >> 1) ^ 0xedb88320 : (c >> 1);
				}
				s_table[i] = c;
			}
		}
#else
		// 生成済みテーブルを使う
		static const uint32_t s_table[256] = {
			0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
			0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
			0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
			0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
			0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
			0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
			0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
			0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
			0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
			0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
			0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
			0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
			0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
			0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
			0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
			0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
		};
#endif
		return (crc >> 8) ^ s_table[(crc ^ val) & 0xFF];
	}
};

class KZipCrypt {
public:
	// 圧縮済みデータを暗号化する
	// data: バイナリデータ
	// size: バイナリデータのバイト数
	// password: パスワード。他ツールでの復元を考慮しないなら、ASCII 文字でなくても良い。
	// crypt_header: ZIPファイルに書き込むための暗号化ヘッダへのポインタ。長さ ZIP_CRYPT_HEADER_SIZE バイトの領域を指定する
	// crc32_hi_8bit: 圧縮前のデータに対するCRC32値の上位8ビット
	static void encode(void *data, int size, const char *password, uint8_t *crypt_header, uint8_t crc32_hi_8bit) {
		KZipCrypt z;
		z.encodeInit(password, crypt_header, crc32_hi_8bit);
		z.encodeData(data, size);
	}

	// 暗号化された圧縮済みデータを復号する
	// data: バイナリデータ
	// size: バイナリデータのバイト数
	// password: パスワード。他ツールでの復元を考慮しないなら、ASCII 文字でなくても良い。
	// crypt_header: 暗号化ヘッダ
	static void decode(void *data, int size, const char *password, const uint8_t *crypt_header) {
		KZipCrypt z;
		z.decodeInit(password, crypt_header);
		z.decodeData(data, size);
	}
public:
	KZipCrypt() {
		keys_[0] = 0;
		keys_[1] = 0;
		keys_[2] = 0;
	}
	void encodeInit(const char *password, uint8_t *crypt_header, uint8_t crc32_hi_8bit) {
		ZIPASSERT(password);
		ZIPASSERT(crypt_header);
		initKeys();
		for (const char *p=password; *p!='\0'; p++) {
			update(*p);
		}
		for (int i=0; i<ZIP_CRYPT_HEADER_SIZE; ++i) {
			if (i == ZIP_CRYPT_HEADER_SIZE - 1){
				crypt_header[i] = crc32_hi_8bit;
			} else {
				crypt_header[i] = ::rand() % 0xFF;
			}
			crypt_header[i] = encode_byte(crypt_header[i]);
		}
	}
	void decodeInit(const char *password, const uint8_t *crypt_header) {
		ZIPASSERT(password);
		ZIPASSERT(crypt_header);
		initKeys();
		for (const char *p=password; *p!='\0'; p++) {
			update(*p);
		}
		for (int i=0; i<ZIP_CRYPT_HEADER_SIZE; ++i) {
			decode_byte(crypt_header[i]);
		}
	}
	void encodeData(void *data, int size) {
		uint8_t *p = (uint8_t *)data;
		for (int i=0; i<size; ++i) {
			p[i] = encode_byte(p[i]);
		}
	}
	void decodeData(void *data, int size) {
		uint8_t *p = (uint8_t *)data;
		for (int i=0; i<size; ++i) {
			p[i] = decode_byte(p[i]);
		}
	}
private:
	void initKeys() {
		keys_[0] = 0x12345678;
		keys_[1] = 0x23456789;
		keys_[2] = 0x34567890;
	}
	void update(uint8_t val) {
		keys_[0] = KZipCrc32::computeByte(keys_[0], val);
		keys_[1] += (keys_[0] & 0xFF);
		keys_[1] = keys_[1] * 134775813 + 1;
		keys_[2] = KZipCrc32::computeByte(keys_[2], keys_[1] >> 24);
	}
	uint8_t next() {
		uint16_t tmp = (uint16_t)((keys_[2] & 0xFFFF) | 2);
		return ((tmp * (tmp ^ 1)) >> 8) & 0xFF;
	}
	uint8_t encode_byte(uint8_t val) {
		uint8_t t = next();
		update(val);
		return t ^ val;
	}
	uint8_t decode_byte(uint8_t val) {
		val ^= next();
		update(val);
		return val;
	}
	uint32_t keys_[3];
};

struct KZipExtra {
	uint16_t sign;   // 識別子
	uint16_t size;   // データバイト数
	uint32_t offset; // データの位置（ZIPファイル先頭からのオフセット）

	KZipExtra() {
		sign = 0;
		size = 0;
		offset = 0;
	}
};

struct KZipEntry {
	zip_local_file_header_t lo_hdr;        // ローカルファイルヘッダ
	zip_central_directory_header_t cd_hdr; // 中央ディレクトリヘッダ
	uint32_t lo_hdr_offset;            // ローカルファイルヘッダの位置（ZIPファイル先頭からのオフセット）
	uint32_t dat_offset;               // 圧縮データの位置（ZIPファイル先頭からのオフセット）

	// ファイル名。
	// 絶対パスやNULLは指定できない。"../" や "./" などの上に登るようなパスも指定できない。
	// この namebin の値がそのままバイナリとしてZIPファイル内に保存される。
	// 文字コードが何になっているかは環境依存だが、
	// ローカルファイルヘッダまたは中央ディレクトリヘッダの general_purpose_bit_flag が ZIP_OPT_UTF8 を
	// 含んでいれば UTF8 であり、そうでなければ環境依存のマルチバイト文字列になっている。
	char namebin[ZIPEX_MAX_NAME];

	time_t mtime;                      // コンテンツの最終更新日時
	time_t atime;                      // コンテンツの最終アクセス日時
	time_t ctime;                      // コンテンツの作成日時
	uint32_t num_extras;               // 拡張データ数
	KZipExtra extras[ZIPEX_MAX_EXTRA]; // 拡張データ
	uint32_t comment_offset;           // コメントがあるなら、その位置（ZIPファイル先頭からのオフセット）。なければ 0

	KZipEntry() {
		memset(&lo_hdr, 0, sizeof(lo_hdr));
		memset(&cd_hdr, 0, sizeof(cd_hdr));
		lo_hdr_offset = 0;
		dat_offset = 0;
		namebin[0] = 0;
		atime = 0;
		mtime = 0;
		ctime = 0;
		num_extras = 0;
		memset(extras, 0, sizeof(extras));
		comment_offset = 0;
	}
};

// 現在位置に中央ディレクトリヘッダ及び圧縮データを追加する
// namebin -- ファイル名（文字コード未定義）
// data -- 未圧縮データ
// times -- time_t[3] の配列で、ファイルのタイムスタンプを creation, modification, access の順番で格納したもの。タイムスタンプを含めない場合は NULL
// file_attr -- ファイル属性。特に気にしない場合は 0
// password -- パスワード。設定しない場合は NULL または空文字列
// level -- 圧縮レベル。0 で最速、9 で最大圧縮。-1でデフォルト値を使う
struct KZipWriteParams {
	std::string namebin; // ファイル名。詳細は KZipEntry の namebin を参照
	std::string data; // 書き込むデータ
	time_t atime; // 最終アクセス日時
	time_t mtime; // 最終更新日時
	time_t ctime; // 作成日時
	uint32_t file_attr; // ファイル属性。特に気にしない場合は 0
	std::string password; // パスワード。設定しない場合は空文字列
	int level; // 圧縮レベル。0 で最速、9 で最大圧縮。-1でデフォルト値を使う
	bool is_name_utf8; // namebin が UT8 形式かどうか

	zip_local_file_header_t output_lo_hdr; // ローカルファイルヘッダ（zip_write_content の実行結果として設定される）
	zip_central_directory_header_t output_cd_hdr; // 中央ディレクトリヘッダ（zip_write_content の実行結果として設定される）

	KZipWriteParams() {
		atime = 0;
		mtime = 0;
		ctime = 0;
		file_attr = 0;
		level = 0;
		is_name_utf8 = false;
		memset(&output_lo_hdr, 0, sizeof(output_lo_hdr));
		memset(&output_cd_hdr, 0, sizeof(output_cd_hdr));
	}
};


// ZIP ファイル書き込み用の低レベルAPI
class KZipW {
public:
	// データを圧縮する
	// compress_level     圧縮レベルを 0 から 9 までの整数で指定する。9 は無圧縮で 9 は最大圧縮、-1 だと標準設定
	// write_zlib_header  zlib のヘッダを出力するかどうか。通常は true でよいが、ZIPファイルの圧縮に利用する場合はfalseにする必要がある
	// return 圧縮後のデータサイズ。関数が失敗した場合は0を返す
	static size_t compress(FileOutput &writer, FileInput &reader, int compress_level, bool write_zlib_header) {
		size_t insize = reader.size();
		std::string inbuf(insize, 0);
		reader.read(&inbuf[0], insize);
		std::string outbuf = KZlib::compress_raw(inbuf.data(), inbuf.size(), compress_level);
		writer.write(outbuf.data(), outbuf.size());
		return outbuf.size();
	}

	// time_tによる時刻表現からZIPのヘッダで使用する時刻表現に変換する
	// zdate 更新月日
	// ztime 更新時刻
	// mtime time_t 形式での更新日時
	static void encodeFileTime(uint16_t *zdate, uint16_t *ztime, time_t time) {
		ZIPASSERT(zdate);
		ZIPASSERT(ztime);
		if (time == 0) {
			*ztime = 0;
			*zdate = 0;
		} else {
			struct tm tm;
			_GetLocalTime(&tm, time); // time_t --> struct tm
			// %1111100000000000 = 0..23 (hour)
			// %0000011111100000 = 0..59 (min)
			// %0000000000011111 = 0..29 (2 seconds)
			*ztime = (tm.tm_hour << 11) |
			         (tm.tm_min  <<  5) |
			         (tm.tm_sec  >>  1);
			// %1111111000000000 = 0..XX (year from 1980)
			// %0000000111100000 = 1..12 (month)
			// %0000000000011111 = 1..31 (day)
			*zdate = ((1900 + tm.tm_year - 1980) << 9) |
			         ((1    + tm.tm_mon        ) << 5) |
			         tm.tm_mday;
		}
	}

	// コンテンツを書き込む。
	// 書き込みに成功した場合は params->output_lo_hdr と params->output_cd_hdr にヘッダ情報をセットして true を返す
	static bool writeContent(FileOutput &file, KZipWriteParams *params) {
		if (file.empty()) return false;
		if (params == NULL) return false;
		if (params->namebin.empty()) return false;
		if (params->namebin[0] == '.') return false;
		if (params->namebin[0] == '/') return false;

		// 元データの CRC32 を計算
		uint32_t data_crc32 = KZipCrc32::compute(params->data.data(), params->data.size());

		// 圧縮
		std::string encoded_data;
		if (params->level == 0) {
			encoded_data.resize(params->data.size()); // 無圧縮
			memcpy(&encoded_data[0], params->data.data(), params->data.size());
		} else {
			// あらかじめ圧縮後のサイズを知りたい場合は ::compressBound(local_file_hdr.uncompressed_size) を使う
			FileOutput w;
			w.open(encoded_data);

			FileInput r;
			r.open(params->data.data(), params->data.size(), false);

			compress(w, r, params->level!=0, false);
		}

		// 暗号化
		uint8_t crypt_header[ZIP_CRYPT_HEADER_SIZE];
		if (!params->password.empty()) {
			uint8_t crc32_hi_8bit = (data_crc32 >> 24) & 0xFF;
			KZipCrypt::encode(&encoded_data[0], encoded_data.size(), params->password.c_str(), crypt_header, crc32_hi_8bit);
		}

		// ローカルファイルヘッダを作成
		zip_local_file_header_t local_file_hdr;
		local_file_hdr.signature = ZIP_SIGN_PK0304;
		local_file_hdr.version_needed_to_extract = (params->level!=0) ? ZIP_VERSION_DEFLATE : ZIP_VERSION_UNCOMPRESS;
		local_file_hdr.general_purpose_bit_flag = 0;
		if (!params->password.empty()) {
			local_file_hdr.general_purpose_bit_flag |= ZIP_OPT_ENCRYPTED;
		}
		if (params->is_name_utf8) {
			local_file_hdr.general_purpose_bit_flag |= ZIP_OPT_UTF8;
		}
		local_file_hdr.compression_method = (params->level!=0) ? ZIP_COMPRESS_METHOD_DEFLATE : ZIP_COMPRESS_METHOD_UNCOMPRESS;
		encodeFileTime(&local_file_hdr.last_mod_file_date, &local_file_hdr.last_mod_file_time, params->mtime);
		local_file_hdr.data_crc32 = data_crc32;
		local_file_hdr.uncompressed_size = params->data.size();
		if (!params->password.empty()) {
			local_file_hdr.compressed_size = encoded_data.size() + ZIP_CRYPT_HEADER_SIZE; // 暗号化している場合、暗号化ヘッダも圧縮済みファイルサイズに含む
		} else {
			local_file_hdr.compressed_size = encoded_data.size();
		}
		local_file_hdr.file_name_length = (uint16_t)params->namebin.size();
		local_file_hdr.extra_field_length = 0;
		params->output_lo_hdr = local_file_hdr;

		// あとでローカルファイルヘッダの書き込み位置が必要になる
		int header_pos = file.tell();

		// データの書き込み
		file.write(&local_file_hdr, sizeof(local_file_hdr)); // ローカルファイルヘッダ
		file.write(params->namebin.c_str(), local_file_hdr.file_name_length); // ファイル名
		if (!params->password.empty()) {
			file.write(crypt_header, ZIP_CRYPT_HEADER_SIZE);
			file.write(&encoded_data[0], encoded_data.size());
		} else {
			file.write(&encoded_data[0], encoded_data.size());
		}
		// 中央ディレクトリ
		// http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html#rainbow
		zip_central_directory_header_t central_dir_hdr;
		central_dir_hdr.signature                 = ZIP_SIGN_PK0102;
		central_dir_hdr.version_made_by           = 10;
		central_dir_hdr.version_needed_to_extract = local_file_hdr.version_needed_to_extract;
		central_dir_hdr.general_purpose_bit_flag  = local_file_hdr.general_purpose_bit_flag;
		central_dir_hdr.compression_method        = local_file_hdr.compression_method;
		central_dir_hdr.last_mod_file_time        = local_file_hdr.last_mod_file_time;
		central_dir_hdr.last_mod_file_date        = local_file_hdr.last_mod_file_date;
		central_dir_hdr.data_crc32                = local_file_hdr.data_crc32;
		central_dir_hdr.compressed_size           = local_file_hdr.compressed_size;
		central_dir_hdr.uncompressed_size         = local_file_hdr.uncompressed_size;
		central_dir_hdr.file_name_length          = local_file_hdr.file_name_length;
		central_dir_hdr.extra_field_length        = local_file_hdr.extra_field_length;
		central_dir_hdr.file_comment_length       = 0;
		central_dir_hdr.disk_number_start         = 0;
		central_dir_hdr.internal_file_attributes  = 0;
		central_dir_hdr.external_file_attributes  = params->file_attr;
		central_dir_hdr.relative_offset_of_local_header = header_pos;
		params->output_cd_hdr = central_dir_hdr;
	
		return true;
	}

	// 中央ディレクトリヘッダを書き込む
	// params は既に zip_write_content によって必要な値がセットされていないといけない
	static bool writeCentralDirectoryHeader(FileOutput &file, KZipWriteParams *params) {
		ZIPASSERT(params);
		ZIPASSERT(!params->namebin.empty());
		ZIPASSERT(params->output_cd_hdr.file_name_length == params->namebin.size());
		file.write(&params->output_cd_hdr, sizeof(zip_central_directory_header_t));
		file.write(params->namebin.c_str(), params->output_cd_hdr.file_name_length);
		return true;
	}

	// 終端レコードを書き込む
	// cd_hdr_offset 中央ディレクトリヘッダの位置（すでに zip_write_central_directory_header によってファイルに書き込まれているものとする）
	// contents      コンテンツ配列（すでに zip_write_content によってファイルに書き込まれているものとする）
	// num_contents  コンテンツ数
	// comment       ZIPファイル全体に対するコメント文字列または NULL
	static bool writeEndOfCentralDirectoryHeaderAndComment(FileOutput &file, size_t cd_hdr_offset, const KZipWriteParams *contents, int num_contents, const char *comment) {
		zip_end_of_central_directory_record_t hdr;
		hdr.signature    = ZIP_SIGN_PK0506;
		hdr.disknum      = 0;
		hdr.startdisknum = 0;
		hdr.diskdirentry = (uint16_t)num_contents;
		hdr.direntry     = (uint16_t)num_contents;
		hdr.dirsize      = 0;
		// オフセットを計算
		for (int i=0; i<num_contents; ++i) {
			const zip_central_directory_header_t *cd_hdr = &contents[i].output_cd_hdr;
			hdr.dirsize += sizeof(zip_central_directory_header_t);
			hdr.dirsize += cd_hdr->file_name_length;
			hdr.dirsize += cd_hdr->extra_field_length;
			hdr.dirsize += cd_hdr->file_comment_length;
		}
		hdr.startpos = cd_hdr_offset; // 中央ディレクトリの開始バイト位置
		hdr.comment_length = comment ? (uint16_t)strlen(comment) : 0; // このヘッダに続くzipコメントのサイズ
		file.write(&hdr, sizeof(hdr));

		// ZIPファイルに対するコメント
		if (comment) {
			file.write(comment, hdr.comment_length);
		}
		return true;
	}
};


// ZIP ファイル読み取り用の低レベルAPI
class KZipR {
public:
	// 圧縮データを展開する
	static size_t uncompress(FileOutput &writer, FileInput &reader, int maxoutsize) {
		size_t insize = reader.size();
		std::string inbuf(insize, 0);
		reader.read(&inbuf[0], insize);
		std::string outbuf = KZlib::uncompress_raw(inbuf.data(), inbuf.size(), maxoutsize);
		writer.write(outbuf.data(), outbuf.size());
		return outbuf.size();
	}

	// フラグの有無を調べる
	// zip_opt:
	//         ZIP_OPT_ENCRYPTED
	//         ZIP_OPT_DATADESC
	//         ZIP_OPT_UTF8
	static bool hasOpt(const KZipEntry *entry, uint32_t zip_opt) {
		ZIPASSERT(entry);
		return (entry->cd_hdr.general_purpose_bit_flag & zip_opt) != 0;
	}

	// 展開後のサイズ
	static size_t getUncompressedSize(const KZipEntry *entry) {
		ZIPASSERT(entry);
		// サイズ情報を見る時は必ず中央ディレクトリヘッダを見るようにする。

		// ファイルサイズは他にもローカルファイルヘッダとデータデスクリプタに記録されているが、
		// そのどちらにあるか、あるいは両方に記録されているのかは ZIP_OPT_DATADESC フラグに依存する。
		//
		// ZIP_OPT_DATADESC フラグがあるならファイルデータ末尾にデータデスクリプタが存在し、そこにファイルサイズが書いてある。
		// その場合、ローカルファイルヘッダの方にはデータサイズ 0 と記載される。
		// ZIP_OPT_DATADESC フラグが無い場合、ファイルデータ末尾にデータデスクリプタは存在せず、ローカルファイルヘッダ側にデータサイズが記録されている
		return entry->cd_hdr.uncompressed_size;
	}

	// コメントを得る
	// 文字コードなどは一切考慮しない。ZIPに格納されているそのままのバイナリ列を返す
	static bool getComment(FileInput &file, const KZipEntry *entry, std::string *bin) {
		ZIPASSERT(entry);
		if (entry->comment_offset > 0) {
			if (bin) {
				file.seek(entry->comment_offset);
				*bin = file.readBin(entry->cd_hdr.file_comment_length);
			}
			return true;
		}
		return false;
	}

	// 拡張情報を得る
	static bool getExtra(FileInput &file, const KZipExtra *extra, std::string *bin) {
		ZIPASSERT(extra);
		if (extra->offset > 0) {
			if (bin) {
				file.seek(extra->offset);
				*bin = file.readBin(extra->size);
			}
			return true;
		}
		return false;
	}

	// コンテンツデータを復元する
	static bool unzipContent(FileInput &file, const KZipEntry *entry, const char *password, std::string *output) {
		ZIPASSERT(entry);
		ZIPASSERT(output);

		// 圧縮データ部分に移動
		file.seek(entry->dat_offset);

		// サイズ情報を見る時は必ず中央ディレクトリヘッダを見るようにする。

		// ファイルサイズは他にもローカルファイルヘッダとデータデスクリプタに記録されているが、
		// そのどちらにあるか、あるいは両方に記録されているのかは ZIP_OPT_DATADESC フラグに依存する。
		//
		// ZIP_OPT_DATADESC フラグがあるならファイルデータ末尾にデータデスクリプタが存在し、
		// そこにファイルサイズが書いてある。その場合、ローカルファイルヘッダの方にはデータサイズ 0 と記載される。
		//
		// ZIP_OPT_DATADESC フラグが無い場合、ファイルデータ末尾にデータデスクリプタは存在せず、
		// ローカルファイルヘッダ側にデータサイズが記録されている

	//	zip_local_file_header_t hdr = entry->lo_hdr;
		zip_central_directory_header_t hdr = entry->cd_hdr;

		if (hdr.compressed_size == 0) {
			ZIPERROR("E_ZIP: Invalid data size");
			return false;
		}

		std::string compressed_data(hdr.compressed_size, '\0');
		file.read(&compressed_data[0], hdr.compressed_size);

		void *data_ptr = NULL;
		int data_len = 0;
		if (hdr.general_purpose_bit_flag & ZIP_OPT_ENCRYPTED) {
			// 暗号化を解除
			const uint8_t *crypt_header = (const uint8_t *)&compressed_data[0];
			data_ptr = &compressed_data[ZIP_CRYPT_HEADER_SIZE];
			data_len = hdr.compressed_size - ZIP_CRYPT_HEADER_SIZE;
			KZipCrypt::decode(data_ptr, data_len, password, crypt_header);
		} else {
			// 暗号化なし
			data_ptr = &compressed_data[0];
			data_len = hdr.compressed_size;
		}

		if (hdr.compression_method) {
			// 圧縮を解除
			FileOutput w;
			w.open(*output);

			FileInput r;
			r.open(data_ptr, data_len);
		
			KZipR::uncompress(w, r, hdr.uncompressed_size);
			ZIPASSERT(output->size() == hdr.uncompressed_size);
			return true;
		
		} else {
			// 無圧縮
			output->resize(hdr.uncompressed_size);
			memcpy(&(*output)[0], data_ptr, hdr.uncompressed_size);
			return true;
		}
	}

	// ZIPのヘッダで使用されている時刻形式を time_t に変換する
	// zdate: 更新月日
	// ztime: 更新時刻
	static time_t decodeFileTime(uint16_t zdate, uint16_t ztime) {
		struct tm time;
		// http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html
		// last mod file time タイムスタンプ(時刻)
		// ビット割り当て
		// %1111100000000000 = 0..23 (hour)
		// %0000011111100000 = 0..59 (min)
		// %0000000000011111 = 0..29 (2 seconds)
		time.tm_hour = (ztime >> 11) & 0x1F;
		time.tm_min  = (ztime >>  5) & 0x3F;
		time.tm_sec  = (ztime & 0x1F) * 2;
		// last mod file date タイムスタンプ(日付)
		// ビット割り当て
		// %1111111000000000 = 0..XX (year from 1980)
		// %0000000111100000 = 1..12 (month)
		// %0000000000011111 = 1..31 (day)
		time.tm_year = ( zdate >> 9) + 1980 - 1900; // tm_year の値は 1900 からの年数
		time.tm_mon  = ((zdate >> 5) & 0xF) - 1;
		time.tm_mday = ( zdate & 0x1F);
		//
		time_t ret = ::mktime(&time); // struct tm --> time_t
		return ret;
	}

	// ZIPファイル全体に対するコメントを得る
	// comment_bin コメント文字列。文字コード変換などは何も行わず、ZIPに記録されているバイト列をそのままセットする。
	// UTF8 かどうかは KZipR::hasOpt に ZIP_OPT_UTF8 を指定して判定する
	static bool getZipComment(FileInput &file, std::string *comment_bin) {

		// End of central directory record までシークする
		if (! seekToEndOfCentralDirectoryRecord(file)) {
			return false;
		}

		// End of central directory record を得る
		if (! _FileMatchUint32(file, ZIP_SIGN_PK0506)) {
			return false;
		}

		zip_end_of_central_directory_record_t hdr;
		file.read(&hdr, sizeof(zip_end_of_central_directory_record_t));

		// コメントの存在を確認
		if (hdr.comment_length == 0) {
			return false;
		}

		// コメント文字列を取得
		if (comment_bin) {
			*comment_bin = file.readBin(hdr.comment_length);
		}
		return true;
	}

	// 拡張フィールドに入っているタイムスタンプを読み取る
	// ZIPの仕様では最終更新日時のみが記録されており、作成日時、アクセス日時が知りたい場合は拡張フィールドを見る必要がある。
	// タイムスタンプは識別子0x000Aの拡張フィールドに記録されている
	static bool readNtfsExtraField(FileInput &file, time_t *ctime, time_t *mtime, time_t *atime) {
		ZIPASSERT(ctime);
		ZIPASSERT(mtime);
		ZIPASSERT(atime);
		if (_FileMatchUint16(file, 0x000A)) {
			// NTFS extra field for file attributes
			// http://archimedespalimpsest.net/Documents/External/ZIPFileFormatSpecification_6.3.2.txt
			uint16_t sign;
			uint16_t size;
			file.read(&sign, 2); // 2: [NTFS extra field sign]
			file.read(&size, 2); // 2: [NTFS extra field size]
			int ntfs_end = file.tell() + size;
			{
				// ここから NTFS extra field の中
				// 4: [reseved]
				// 2: [attr tag]
				// 2: [attr len]
				// n: [attr data]
				// ... これ以降、必要なだけ [attr tag], [attr len], [attr data] を繰り返す
				//
				file.read(NULL, 4); // [reserved]
				while (file.tell() < ntfs_end) {
					uint16_t tag;
					uint16_t len;
					file.read(&tag, 2);
					file.read(&len, 2);
					// [attr data]
					if (tag == 0x0001) {
						// NTFS 属性の 0x0001 はファイルのタイムスタンプ属性を表す
						// 8: [last modification time]
						// 8: [last access time]
						// 8: [creation time]
						uint64_t ntfs_mtime;
						uint64_t ntfs_atime;
						uint64_t ntfs_ctime;
						file.read(&ntfs_mtime, 8);
						file.read(&ntfs_atime, 8);
						file.read(&ntfs_ctime, 8);
						*mtime = FileTime::to_time_t(ntfs_mtime);
						*atime = FileTime::to_time_t(ntfs_atime);
						*ctime = FileTime::to_time_t(ntfs_ctime);
					} else {
						file.read(NULL, len);
					}
				}
			}
			ZIPASSERT(file.tell() == ntfs_end); // この時点でぴったり NTFS extra field の末尾を指しているはず
			file.seek(ntfs_end);
			return true;
		}
		return false;
	}

	// 現在の読み取り位置が中央ディレクトリヘッダを指していると仮定し、中央ディレクトリヘッダとコンテンツ情報を読み取る
	static bool readCenteralDirectoryHeaderAndContent(FileInput &file, KZipEntry *entry) {
		ZIPASSERT(entry);

		if (! _FileMatchUint32(file, ZIP_SIGN_PK0102)) {
			return false;
		}

		memset(entry, 0, sizeof(KZipEntry));

		// 中央ディレクトリヘッダ
		file.read(&entry->cd_hdr, sizeof(zip_central_directory_header_t));

		zip_central_directory_header_t *cd = &entry->cd_hdr;

		// ローカルファイルヘッダの位置
		//
		// ※ローカルファイルヘッダは ZIP ファイルの先頭にあり、
		//   本来なら真っ先に読み取っているはずだが、
		//   一部のアーカイバが作成した ZIP ファイルでは、
		//   ローカルファイルヘッダに記述されているはずのデータサイズが 0 に設定されていて、
		//   正しく逐次読み出しすることができない。
		//   そこでローカルファイルヘッダは信用せず、
		//   中央ディレクトリヘッダにあるローカルファイルヘッダ情報を使うようにする
		//	entry->lo_hdr_offset = 0; <-- 先頭のローカルファイルヘッダは使わない
		entry->lo_hdr_offset = cd->relative_offset_of_local_header;

		// ついでにローカルファイルヘッダ自体も取得しておく
		{
			int p = file.tell();
			file.seek(entry->lo_hdr_offset);
			file.read(&entry->lo_hdr, sizeof(zip_local_file_header_t));
			file.seek(p);
		}

		// Data Descriptor の処理
		if (entry->lo_hdr.general_purpose_bit_flag & ZIP_OPT_DATADESC) {
			// Data Descriptor が存在する。
			// この場合、Local file header の compressed_size, uncompressed_size は 0 になっている。
			// （ランダムシークできないシステムのために、データを書き込んだ後に Data Descriptor という形でファイルサイズ情報を追加している）
			// ローカルファイルヘッダのサイズ情報を補完しておく
			entry->lo_hdr.compressed_size = cd->compressed_size;
			entry->lo_hdr.uncompressed_size = cd->uncompressed_size;
		}

		// 圧縮データの先頭位置（ファイル先頭からのオフセット）
		entry->dat_offset = entry->lo_hdr_offset + sizeof(zip_local_file_header_t) + cd->file_name_length + cd->extra_field_length;

		// タイムスタンプ
		// ZIPには各コンテンツの最終更新日時だけが入っている。
		// 作成日時、アクセス日時も取得したい場合は拡張データ (識別子 0x000A) を調べる必要がある
		entry->ctime = 0;
		entry->mtime = KZipR::decodeFileTime(cd->last_mod_file_date, cd->last_mod_file_time);
		entry->atime = 0;
		
		// ファイル名
		if (cd->file_name_length > 0) {
			if (cd->file_name_length < ZIPEX_MAX_NAME) {
				file.read(entry->namebin, cd->file_name_length);
				entry->namebin[cd->file_name_length] = '\0';
			} else {
				ZIPERROR("E_ZIP: Too long file name");
				return false;
			}
		}

		// 拡張データ
		entry->num_extras = 0;
		if (cd->extra_field_length > 0) {
			int extra_end = file.tell() + cd->extra_field_length;
			while (file.tell() < extra_end) {
				KZipExtra extra;
				file.read(&extra.sign, 2);
				file.read(&extra.size, 2);
				extra.offset = file.tell();
				file.read(NULL, extra.size); // SKIP

				// 拡張データがファイルのタイムスタンプを表している場合、その時刻を取得する
				if (extra.sign == 0x000A) {
					readNtfsExtraField(file, &entry->ctime, &entry->mtime, &entry->atime);
				}

				if (entry->num_extras < ZIPEX_MAX_EXTRA) {
					entry->extras[entry->num_extras] = extra;
					entry->num_extras++;
				} else {
					ZIPERROR("E_ZIP: Too many extra fields");
					return false;
				}
			}
			ZIPASSERT(file.tell() == extra_end);
		}

		// コメント
		entry->comment_offset = 0;
		if (cd->file_comment_length > 0) {
			entry->comment_offset = file.tell();
			file.read(NULL, cd->file_comment_length); // SKIP
		}

		return true;
	}

	// 終端レコードの先頭に移動する
	static bool seekToEndOfCentralDirectoryRecord(FileInput &file) {

		zip_end_of_central_directory_record_t eocd;

		// ファイル末尾からシークする
		int offset = (int)(file.size() - sizeof(eocd));

		// 識別子を確認
		while (offset > 0) {
			file.seek(offset);
			if (file.readUint32() == ZIP_SIGN_PK0506) {
				// 識別子が一致したら、終端レコード全体を読む。
				// このときコメントのサイズとファイルサイズの関係に矛盾が無ければ
				// 正しい終端レコードを見つけられたものとする
				file.seek(offset);
				file.read(&eocd, sizeof(eocd));
				if (offset + sizeof(eocd) + eocd.comment_length == file.size()) {
					// 辻褄が合う。OK
					// レコード先頭に戻しておく
					file.seek(offset);
					return true;
				}
			}
			offset--;
		}
		ZIPERROR("E_ZIP: Failed to find an End of Centeral Directory Record");
		return false;
	}

	// 中央ディレクトリヘッダの先頭に移動する
	static bool seekToCentralDirectoryHeader(FileInput &file) {

		// 終端レコードまでシーク
		if (! seekToEndOfCentralDirectoryRecord(file)) {
			ZIPERROR("E_ZIP: Failed to find a Centeral Directory Record");
			return false;
		}

		// 終端レコードを読む
		zip_end_of_central_directory_record_t hdr;
		file.read(&hdr, sizeof(zip_end_of_central_directory_record_t));

		// 終端レコードには中央ディレクトリヘッダの位置が記録されているので、その値を使ってシークする
		file.seek(0);
		file.seek(hdr.startpos);

		// 中央ディレクトリヘッダの識別子を確認
		if (!_FileMatchUint32(file, ZIP_SIGN_PK0102)) {
			ZIPERROR("E_ZIP: Failed to find a Centeral Directory Record");
			return false;
		}
		return true;
	}

};


class KZipFileWriterImpl {
	std::vector<KZipWriteParams> entries_;
	std::string password_;
	std::string comment_;
	FileOutput file_;
	int central_directory_header_offset_;
	int compress_level_;
	bool save_as_utf8_;
public:
	KZipFileWriterImpl() {
		save_as_utf8_ = SAVE_AS_UTF8;
		clear();
	}
	~KZipFileWriterImpl() {
		finalize();
	}
	void clear() {
		entries_.clear();
		password_.clear();
		file_ = FileOutput();
		central_directory_header_offset_ = 0;
		compress_level_ = -1;
	}
	void open(FileOutput &file) {
		clear();
		file_ = file;
	}
	bool isOpen() const {
		return file_.isOpen();
	}
	void finalize() {
		if (file_.isOpen()) {
			addCentralDirectories();
			addEndOfCentralDirectoryRecord(comment_.c_str());
		}
	}
	void setCompressLevel(int level) {
		if (level < -1) {
			compress_level_ = -1;
		} else if (level > 9) {
			compress_level_ = 9;
		} else {
			compress_level_ = level;
		}
	}
	void setZipComment(const char *comment_u8) {
		ZIPASSERT(save_as_utf8_);
		comment_ = comment_u8;
	}
	void setPassword(const char *password) {
		password_ = password;
	}
	// アーカイブにファイルを追加する。
	// @param password 暗号化パスワード。暗号化しない場合は NULL または "" を指定する
	// @param times    タイムスタンプ。3要素から成る times_t 配列を指定する。creation, modification, access の順番で格納する。タイムスタンプ不要ならば NULL 出もよい
	// @param attr     ファイル属性。デフォルトは 0
	bool addEntry(const Path &entry_name, FileInput &file, const time_t *times=NULL, uint32_t file_attr=0) {
		if (file.empty()) {
			ZIPERROR("E_ZIP: Invliad file");
			return false;
		}
		if (entry_name.empty()) {
			// 名前必須
			ZIPERROR("E_ZIP: Invliad name");
			return false;
		}

		// ZIPファイル内でのファイル名を用意
		Path name;
		if (entry_name.isRelative()) {
			// 相対パスが ".." などを含まないようにする
			name = entry_name;

		} else {
			// 絶対パス形式で指定されている場合はファイル名だけを取り出す
			name = entry_name.filename();
		}
		if (name.empty()) {
			return false;
		}
		// 元データを用意
		std::string original_data;
		{
			int size = file.size();
			if (size > 0) {
				original_data.resize(size);
				file.read(&original_data[0], size);
			}
		}

		KZipWriteParams params;
		if (save_as_utf8_) {
			params.namebin = name.u8();
			params.is_name_utf8 = true;
		} else {
			params.namebin = name.toAnsiString('/', ""); // 現在のロケールにしたがってMBCS化する
			params.is_name_utf8 = false;
		}
		params.data = original_data;
		params.file_attr = 0;
		if (times) {
			params.ctime = times[0];
			params.mtime = times[1];
			params.atime = times[2];
		} else {
			params.ctime = 0;
			params.mtime = 0;
			params.atime = 0;
		} 
		params.password = password_.c_str();
		params.level = compress_level_;
		KZipW::writeContent(file_, &params);

		entries_.push_back(params);
		return true;
	}
	// Central directory header を追加
	void addCentralDirectories() {
		central_directory_header_offset_ = file_.tell(); // 中央ディレクトリの開始位置を記録しておく
		for (size_t i=0; i<entries_.size(); ++i) {
			KZipW::writeCentralDirectoryHeader(file_, &entries_[i]);
		}
	}
	// End of central directory record を追加
	void addEndOfCentralDirectoryRecord(const char *comment) {
		KZipW::writeEndOfCentralDirectoryHeaderAndComment(
			file_, 
			central_directory_header_offset_, 
			&entries_[0], 
			(int)entries_.size(), 
			comment
		);
	}
};


class KZipFileReaderImpl {
	std::vector<KZipEntry> entries_;
	mutable FileInput file_;
public:
	KZipFileReaderImpl() {
		clear();
	}
	~KZipFileReaderImpl() {
	}
	void clear() {
		entries_.clear();
		file_ = FileInput();
	}
	void open(FileInput &file) {
		clear();
		file_ = file;
		
		// 中央ディレクトリヘッダまでシーク
		KZipR::seekToCentralDirectoryHeader(file_);

		// 中央ディレクトリヘッダ及びコンテンツ情報を読み取る
		while (_FileMatchUint32(file_, ZIP_SIGN_PK0102)) {
			KZipEntry entry;
			KZipR::readCenteralDirectoryHeaderAndContent(file_, &entry);
			entries_.push_back(entry);
		}
	}
	bool isOpen() const {
		return file_.isOpen();
	}
	int count() const {
		return (int)entries_.size();
	}
	const KZipEntry * getEntry(int index) const {
		if (0 <= index && index < (int)entries_.size()) {
			return &entries_[index];
		}
		return NULL;
	}
	bool getComment(int index, std::string *bin) const {
		const KZipEntry *entry = getEntry(index);
		if (entry) {
			return KZipR::getComment(file_, entry, bin);
		}
		return false;
	}
	bool getExtra(int index, int extra_index, uint16_t *out_sign, std::string *out_bin) const {
		const KZipEntry *entry = getEntry(index);
		if (entry) {
			if (0 <= extra_index && extra_index < (int)entry->num_extras) {
				const KZipExtra *extra = &entry->extras[extra_index];
				if (out_sign) {
					*out_sign = extra->sign;
				}
				if (out_bin) {
					KZipR::getExtra(file_, extra, out_bin);
				}
			}
			return true;
		}
		return false;
	}
	std::string getZipComment() const {
		std::string s;
		KZipR::getZipComment(file_, &s);
		return s;
	}

	// エントリー名を検索する
	// entry_name  比較用のエントリー名
	// ignore_case 大小文字を無視して検索
	// ignore_path フォルダ名部分を無視して検索
	int indexOf(const Path &entry_name, bool ignore_case, bool ignore_path) const {
		for (size_t i=0; i<entries_.size(); i++) {
			Path s;
			if (KZipR::hasOpt(&entries_[i], ZIP_OPT_UTF8)) {
				s = Path::fromUtf8(entries_[i].namebin);
			} else {
				s = Path::fromAnsi(entries_[i].namebin, "");
			}
			if (s.compare(entry_name, ignore_case, ignore_path) == 0) {
				return (int)i;
			}
		}
		return -1;
	}
	std::string unzip(int index, const char *password) const {
		std::string bin;
		const KZipEntry *entry = &entries_[index];
		KZipR::unzipContent(file_, entry, password, &bin);
		return bin;
	}
};



#pragma region KZipFileWriter
KZipFileWriter::KZipFileWriter() {
	impl_ = std::make_shared<KZipFileWriterImpl>();
}
KZipFileWriter::KZipFileWriter(const Path &filename) {
	impl_ = std::make_shared<KZipFileWriterImpl>();
	open(filename);
}
KZipFileWriter::KZipFileWriter(FileOutput &file) {
	impl_ = std::make_shared<KZipFileWriterImpl>();
	open(file);
}
KZipFileWriter::~KZipFileWriter() {
}
bool KZipFileWriter::open(const Path &filename) {
	impl_->clear();
	FileOutput file;
	if (file.open(filename)) {
		impl_->open(file);
		return true;
	}
	return false;
}
bool KZipFileWriter::open(FileOutput &file) {
	if (file.isOpen()) {
		impl_->open(file);
		return true;
	}
	return false;
}
void KZipFileWriter::close() {
	impl_->finalize();
	impl_->clear();
}
bool KZipFileWriter::isOpen() const {
	return impl_->isOpen();
}
void KZipFileWriter::setZipComment(const char *comment_u8) {
	impl_->setZipComment(comment_u8);
}
void KZipFileWriter::setCompressLevel(int level) {
	impl_->setCompressLevel(level);
}
void KZipFileWriter::setPassword(const char *password) {
	impl_->setPassword(password);
}
bool KZipFileWriter::addEntryFromFileName(const Path &filename, const time_t *timestamp_cma) {
	Path entry_name = filename.filename(); // フォルダ名部分を取り除く
	return addEntryFromFileName(entry_name, filename, timestamp_cma);
}
bool KZipFileWriter::addEntryFromFileName(const Path &entry_name, const Path &filename, const time_t *timestamp_cma) {
	FileInput file;
	if (entry_name.empty()) {
		return false;
	}
	if (!entry_name.isRelative()) {
		return false;
	}
	if (!file.open(filename)) {
		return false;
	}

	// タイムスタンプ
	time_t times[] = {0, 0, 0}; // creation, modification, access
	if (timestamp_cma) {
		memcpy(times, timestamp_cma, sizeof(times));
	} else {
		FileTime::getTimeStamp(filename.u8(), times);
	}

	// ファイル属性
	uint32_t attr = 0;

	impl_->addEntry(entry_name, file, times, attr);
	return true;
}
bool KZipFileWriter::addEntryFromFile(const Path &entry_name, FileInput &file, const time_t *timestamp_cma) {
	if (entry_name.empty()) {
		return false;
	}
	if (!entry_name.isRelative()) {
		return false;
	}
	if (!file.isOpen()) {
		return false;
	}
	impl_->addEntry(entry_name, file, timestamp_cma, 0);
	return true;
}
bool KZipFileWriter::addEntryFromMemory(const Path &entry_name, const void *data, size_t size, const time_t *timestamp_cma) {
	if (entry_name.empty()) {
		return false;
	}
	if (!entry_name.isRelative()) {
		return false;
	}
	if (size == 0) {
		size = strlen((const char *)data);
	}
	FileInput file;
	file.open(data, size, false);
	impl_->addEntry(entry_name, file, timestamp_cma, 0);
	return true;
}
#pragma endregion // KZipFileWriter



#pragma region KZipFileReader
KZipFileReader::KZipFileReader() {
	impl_ = std::make_shared<KZipFileReaderImpl>();
}
KZipFileReader::KZipFileReader(const char *filename) {
	impl_ = std::make_shared<KZipFileReaderImpl>();
	open(filename);
}
KZipFileReader::KZipFileReader(FileInput &file) {
	impl_ = std::make_shared<KZipFileReaderImpl>();
	open(file);
}
KZipFileReader::~KZipFileReader() {
}
bool KZipFileReader::open(const char *filename) {
	ZIPASSERT(impl_ != nullptr);
	impl_->clear();
	FileInput file;
	if (file.open(filename)) {
		impl_->open(file);
		return true;
	}
	return false;
}
bool KZipFileReader::open(FileInput &file) {
	ZIPASSERT(impl_ != nullptr);
	if (file.isOpen()) {
		impl_->open(file);
		return true;
	}
	return false;
}
void KZipFileReader::close() {
	ZIPASSERT(impl_ != nullptr);
	impl_->clear();
}
bool KZipFileReader::isOpen() const {
	ZIPASSERT(impl_ != nullptr);
	return impl_->isOpen();
}
int KZipFileReader::getEntryCount() const {
	ZIPASSERT(impl_ != nullptr);
	return impl_->count();
}
std::string KZipFileReader::getZipComment() const {
	ZIPASSERT(impl_ != nullptr);
	return impl_->getZipComment();
}
int KZipFileReader::findEntry(const Path &entry_name, bool ignore_case, bool ignore_path) const {
	ZIPASSERT(impl_ != nullptr);
	return impl_->indexOf(entry_name, ignore_case, ignore_path);
}
size_t KZipFileReader::getEntryDataSize(int index) const {
	ZIPASSERT(impl_ != nullptr);
	const KZipEntry *entry = impl_->getEntry(index);
	if (entry) {
		return KZipR::getUncompressedSize(entry);
	} else {
		return 0;
	}
}
bool KZipFileReader::isEntryCrypt(int index) const {
	ZIPASSERT(impl_ != nullptr);
	const KZipEntry *entry = impl_->getEntry(index);
	if (entry) {
		return KZipR::hasOpt(entry, ZIP_OPT_ENCRYPTED);
	} else {
		return false;
	}
}
bool KZipFileReader::isEntryUtf8(int index) const {
	ZIPASSERT(impl_ != nullptr);
	const KZipEntry *entry = impl_->getEntry(index);
	if (entry) {
		return KZipR::hasOpt(entry, ZIP_OPT_UTF8);
	} else {
		return false;
	}
}
Path KZipFileReader::getEntryName(int index) const {
	ZIPASSERT(impl_ != nullptr);
	Path ret;
	if (0 <= index && index < impl_->count()) {
		const KZipEntry *entry = impl_->getEntry(index);
		ZIPASSERT(entry);
		if (KZipR::hasOpt(entry, ZIP_OPT_UTF8)) {
			ret = Path::fromUtf8(entry->namebin);
		} else {
			ret = Path::fromAnsi(entry->namebin, "");
		}
	}
	return ret;
}
std::string KZipFileReader::getEntryNameBin(int index) const {
	ZIPASSERT(impl_ != nullptr);
	if (0 <= index && index < impl_->count()) {
		const KZipEntry *entry = impl_->getEntry(index);
		ZIPASSERT(entry);
		return entry->namebin;
	}
	return std::string();
}
std::string KZipFileReader::getEntryCommentBin(int index) const {
	ZIPASSERT(impl_ != nullptr);
	std::string comment;
	impl_->getComment(index, &comment);
	return comment;
}
int KZipFileReader::findEntryExtraBySignature(int index, uint16_t signature) const {
	ZIPASSERT(impl_ != nullptr);
	const KZipEntry *entry = impl_->getEntry(index);
	if (entry) {
		for (uint32_t i=0; i<entry->num_extras; i++) {
			if (entry->extras[i].sign == signature) {
				return (int)i;
			}
		}
	}
	return -1;
}
int KZipFileReader::getEntryExtraCount(int index) const {
	ZIPASSERT(impl_ != nullptr);
	const KZipEntry *entry = impl_->getEntry(index);
	if (entry) {
		return entry->num_extras;
	} else {
		return 0;
	}
}
bool KZipFileReader::getEntryExtra(int index, int extra, uint16_t *out_sign, std::string *out_bin) const {
	ZIPASSERT(impl_ != nullptr);
	const KZipEntry *entry = impl_->getEntry(index);
	if (entry) {
		return impl_->getExtra(index, extra, out_sign, out_bin);
	} else {
		return false;
	}
}
std::string KZipFileReader::getEntryData(int index, const char *password) {
	ZIPASSERT(impl_ != nullptr);
	if (0 <= index && index < impl_->count()) {
		return impl_->unzip(index, password);
	}
	return std::string(); // Empty
}
std::string KZipFileReader::getEntryDataByName(const Path &entry_name, bool ignore_case, bool ignore_path, const char *password) {
	int idx = findEntry(entry_name, ignore_case, ignore_path);
	if (idx >= 0) {
		return getEntryData(idx, password);
	}
	return std::string(); // Empty
}
#pragma endregion // KZipFileReader




bool isZipFileSign(FileInput &file) {
	return _FileMatchUint32(file, ZIP_SIGN_PK0304);
}



void Test_zip() {
	// 通常書き込み
	{
		KZipFileWriter zip("~zip_test.zip");
		zip.addEntryFromMemory("a.txt", "AAA\n", 0);
		zip.addEntryFromMemory("b.txt", "BBB\n", 0);
		zip.addEntryFromMemory("c.txt", "CCC\n", 0);
		zip.addEntryFromMemory(u8"日本語ファイル名.txt", u8"日本語テキスト\n", 0);
		zip.addEntryFromMemory(u8"日本語ソ連表\\ファイル.txt", u8"SJISでのダメ文字を含む日本語ファイル名を付けてみる\n", 0);
		zip.addEntryFromMemory("sub/d.txt", "DDD\n", 0);
		zip.setZipComment("COMMENT");
		zip.close();
	}

	// パスワード付きで保存
	{
		const char *password = "pass";
		KZipFileWriter zip("~zip_test_pass.zip");
		zip.setPassword(password);
		zip.addEntryFromMemory("a.txt", "AAA\n", 0);
		zip.addEntryFromMemory("b.txt", "BBB\n", 0);
		zip.addEntryFromMemory("c.txt", "CCC\n", 0);
		zip.close();
	}
	
	// 個別のパスワード付きで保存
	{
		KZipFileWriter zip("~zip_test_pass2.zip");

		zip.setPassword("passa");
		zip.addEntryFromMemory("a.txt", "AAA\n", 0);

		zip.setPassword("passb");
		zip.addEntryFromMemory("b.txt", "BBB\n", 0);

		zip.setPassword("passc");
		zip.addEntryFromMemory("c.txt", "CCC\n", 0);
		zip.close();
	}

	// 復元
	{
		KZipFileReader zip("~zip_test.zip");
		ZIPASSERT(zip.getEntryDataByName("a.txt", false, false).compare("AAA\n") == 0);
		ZIPASSERT(zip.getEntryDataByName("b.txt", false, false).compare("BBB\n") == 0);
		ZIPASSERT(zip.getEntryDataByName("c.txt", false, false).compare("CCC\n") == 0);
		ZIPASSERT(zip.getEntryDataByName("sub/d.txt", false, false).compare("DDD\n") == 0); // フォルダ名まで含めて検索
		ZIPASSERT(zip.getEntryDataByName("d.txt", false, true).compare("DDD\n") == 0); // フォルダ部分を無視して検索
		ZIPASSERT(zip.getZipComment().compare("COMMENT") == 0);
	}

	// 復元（パスワード付き）
	{
		KZipFileReader zip("~zip_test_pass.zip");
		ZIPASSERT(zip.getEntryDataByName("a.txt", false, false, "pass").compare("AAA\n") == 0);
		ZIPASSERT(zip.getEntryDataByName("b.txt", false, false, "pass").compare("BBB\n") == 0);
		ZIPASSERT(zip.getEntryDataByName("c.txt", false, false, "pass").compare("CCC\n") == 0);
	}

	// 復元（個別のパスワード）
	{
		KZipFileReader zip("~zip_test_pass2.zip");
		ZIPASSERT(zip.getEntryDataByName("a.txt", false, false, "passa").compare("AAA\n") == 0);
		ZIPASSERT(zip.getEntryDataByName("b.txt", false, false, "passb").compare("BBB\n") == 0);
		ZIPASSERT(zip.getEntryDataByName("c.txt", false, false, "passc").compare("CCC\n") == 0);
	}
}



} // namespace
