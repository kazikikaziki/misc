#pragma once
#include <string>
#include <vector>
#include "KFile.h"

namespace mo {

class Path;

/// KFileLoader に登録するためのコールバッククラス
class KFileLoaderCallback {
public:
	/// ファイルが存在するかどうか問い合わせられたときに呼ばれる。
	/// 効率を気にしないなら、一番簡単な実装は以下の通り
	/// @code
	/// virtual bool on_fileloader_hasfile(const Path &name) {
	///   return on_fileloader_getfile(name).isOpen();
	/// }
	/// @endcode
	virtual bool on_fileloader_hasfile(const Path &name) = 0;

	/// ファイルを取得しようとしたときに呼ばれる。
	/// ロードできない場合は空の FileInput を返す
	virtual FileInput on_fileloader_getfile(const Path &name) = 0;
};

class KFileLoader {
public:
	KFileLoader();
	~KFileLoader();
	void clear();

	/// ファイルをロードしようとしたときなどに呼ばれるコールバックを登録する
	/// @note 必ず new したコールバックオブジェクトを渡すこと。
	/// ここで渡したポインタに対してはデストラクタまたは clear() で delete が呼ばれる
	void addCallback(KFileLoaderCallback *cb);

	/// 通常フォルダを検索対象に追加する
	bool addFolder(const Path &dir);

	/// パックファイルを検索対象に追加する
	/// @see KPacFileReader
	bool addPacFile(const Path &filename);

	/// アプリケーションに埋め込まれたファイル（リソースファイル）を検索対象に追加する
	/// @see KEmbeddedFiles
	bool addEmbeddedFiles();

	/// アプリケーションに埋め込まれたパックファイルを検索対象に追加する
	/// @see KPacFileReader
	/// @see KEmbeddedFiles
	bool addEmbeddedPacFileLoader(const Path &filename);

	/// 指定した名前のファイルがあれば、有効な FileInput を返す。
	/// 見つからない場合は空っぽの FileInput を返す
	FileInput getInputFile(const Path &filename);
	
	/// 指定されたファイルが存在すれば、その内容をすべてバイナリモードで読み取る
	/// 戻り値は文字列型だが、ファイル内容のバイナリを無加工で保持している
	std::string loadBinary(const Path &filename);

	bool exists(const Path &filename);

private:
	std::vector<KFileLoaderCallback *> archives_;
};

}
