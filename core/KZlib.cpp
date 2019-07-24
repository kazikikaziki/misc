#include "KZlib.h"

#ifdef MO_ZLIB
	// libz を使う
	#include <zlib.h>
#else
	// miniz を使う
	// miniz は libz と完全な互換性があるので、libz <==> miniz の切り替えには .h を切り替えるだけでよい
	// https://github.com/richgel999/miniz
	#include "miniz/miniz.h"
#endif

namespace mo {
// level: 0=無圧縮 1=速度優先 ... 9=サイズ優先
// window_bits: MAX_WBITS    = zlib形式ヘッダ、Adler32チェックサム
// window_bits: MAX_WBITS+16 = gzip形式ヘッダ、CRC32チェックサム
// window_bits: MAX_WBITS+32 = zlib/gzip自動判別（inflate のみ)
// window_bits:-MAX_WBITS    = ヘッダなし、チェックサムなし
static std::string zlib__compress(const void *data, int size, int level, int window_bits) {
	uLong maxoutsize = ::compressBound(size);
	std::string outbuf(maxoutsize, 0);
	
	z_stream zstrm;
	memset(&zstrm, 0, sizeof(zstrm));
	zstrm.next_in   = (Bytef*)data;
	zstrm.avail_in  = size;
	zstrm.next_out  = (Bytef*)&outbuf[0];
	zstrm.avail_out = outbuf.size();

	deflateInit2(
		&zstrm, 
		level,       // 圧縮レベル(0-9)
		Z_DEFLATED,  // 圧縮方法
		window_bits, // 履歴バッファのサイズ(8-15) 負の値だとヘッダ無し圧縮データになる
		8,           // 圧縮のために確保するメモリ領域 (0-9 標準8)
		Z_DEFAULT_STRATEGY // 圧縮アルゴリズムの調整
	);
	int result = ::deflate(&zstrm, Z_FINISH);

	if (result == Z_STREAM_END || result == Z_OK) {
		outbuf.resize(zstrm.total_out);
	} else {
		outbuf.clear();
	}
	deflateEnd(&zstrm);
	return outbuf;
}
static std::string zlib__uncompress(const void *data, int size, int maxoutsize, int window_bits) {
	std::string outbuf(maxoutsize, 0);
	
	z_stream zstrm;
	memset(&zstrm, 0, sizeof(zstrm));
	zstrm.next_in   = (Bytef*)data;
	zstrm.avail_in  = size;
	zstrm.next_out  = (Bytef*)outbuf.data();
	zstrm.avail_out = outbuf.size();

	inflateInit2(&zstrm, window_bits);
	int result = inflate(&zstrm, Z_FINISH);

	if (result == Z_STREAM_END || result == Z_OK) {
		outbuf.resize(zstrm.total_out);
	} else {
		outbuf.clear();
	}
	inflateEnd(&zstrm);
	return outbuf;
}

std::string KZlib::compress_zlib(const void *data, int size, int level) {
	return zlib__compress(data, size, level, MAX_WBITS);
}
std::string KZlib::compress_gzip(const void *data, int size, int level) {
	return zlib__compress(data, size, level, MAX_WBITS+16);
}
std::string KZlib::compress_raw(const void *data, int size, int level) {
	return zlib__compress(data, size, level, -MAX_WBITS);
}
std::string KZlib::uncompress_zlib(const void *data, int size, int maxoutsize) {
	return zlib__uncompress(data, size, maxoutsize, MAX_WBITS);
}
std::string KZlib::uncompress_gzip(const void *data, int size, int maxoutsize) {
	return zlib__uncompress(data, size, maxoutsize, MAX_WBITS+16);
}
std::string KZlib::uncompress_raw(const void *data, int size, int maxoutsize) {
	return zlib__uncompress(data, size, maxoutsize, -MAX_WBITS);
}

}
