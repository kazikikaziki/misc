#pragma once
#include <string>

namespace mo {

class KZlib {
public:
	/// zlib ヘッダをつけて圧縮・展開する
	/// level: 0=無圧縮 1=速度優先 ... 9=サイズ優先
	/// maxoutsize: 展開用に確保するメモリサイズ。少なくとも展開後のデータが入るだけのサイズを指定すること。
	///             展開後のデータサイズについては zlib ヘッダに保存されないため、利用側が自分で管理する必要がある
	static std::string compress_zlib(const void *data, int size, int level);
	static std::string uncompress_zlib(const void *data, int size, int maxoutsize);

	/// gzip ヘッダをつけて圧縮・展開する
	static std::string compress_gzip(const void *data, int size, int level);
	static std::string uncompress_gzip(const void *data, int size, int maxoutsize);

	/// ヘッダ無しで圧縮・展開する
	static std::string compress_raw(const void *data, int size, int level);
	static std::string uncompress_raw(const void *data, int size, int maxoutsize);
};

}
