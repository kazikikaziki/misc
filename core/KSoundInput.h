#pragma once
#include <inttypes.h>
#include <memory>

namespace mo {

class FileInput;

/// サウンドファイルを読み取り、生の PCM データを取り出す
class SoundInput {
public:
	static const int BITS_PER_SAMPLE = 16; // 16ビット固定
	static const int BYTES_PER_SAMPLE = 2; // 16ビット固定

	SoundInput();

	bool open(FileInput &file);

	/// 総サンプル数を返す
	/// 総サンプル数は getSamplesPerSecond() * getChannelCount() * 再生秒数 で計算される
	int getSampleCount() const;

	/// 総再生時間を秒単位で返す
	float getTotalSeconds() const;

	/// チャンネル数を返す (1=モノラル 2=ステレオ）
	int getChannelCount() const;

	/// 1チャンネル1秒当たりのサンプル数を返す
	int getSamplesPerSecond() const;

	/// サンプル単位でシークする
	/// 成功したら 0 を返す
	/// @note チャンネル数によるブロックアラインメントを考慮すること。
	/// 例えば2チャンネルサウンドならばサンプルが 左右左右左右左右... と並んでいる。
	/// この場合は左右1組（2サンプル）を1つの単位にしてシークさせる必要があるため、サンプル数には2の倍数を指定する
	int seekSamples(int sample_offset);

	/// 秒単位の時刻を指定してシークする
	/// 成功したら 0 を返す
	int seekSeconds(float sec);

	/// 現在の読み取り位置をサンプル単位で返す
	/// @note サンプルの数え方に注意。詳細は seekSamples() を参照
	int tellSamples();

	/// 現在の読み取り位置を秒単位で返す
	float tellSeconds();

	/// サンプル単位でデータを読み取り、samples に書き込む。実際に読み取って書き込んだサンプル数を返す
	/// @param samples     samples 読み取ったデータの格納先
	/// @param max_count   読み取るサンプル数の最大値
	///
	/// @note サンプルの数え方に注意。詳細は seekSamples() を参照
	int readSamples(int16_t *samples, int max_count);

	/// サンプル単位でデータを読み取り、samples に書き込む。実際に読み取って書き込んだサンプル数を返す
	/// readSamples とは異なり、書き込み先のバッファとループポイントを考慮する。
	/// 
	/// @param samples     読み取ったデータの格納先
	/// @param max_count   読み取るサンプル数の最大値
	/// @param loop_start  サンプル単位でのループ開始位置
	/// @param loop_end    サンプル単位でのループ終端位置。0 を指定した場合は曲の末尾がループ終端になる
	///
	/// @note サンプルの数え方に注意。詳細は seekSamples() を参照
	int readSamplesLoop(int16_t *samples, int max_count, int loop_start, int loop_end);

	/// Inner Implement Class
	class Impl;

private:
	std::shared_ptr<Impl> impl_;
};


} // namespace
