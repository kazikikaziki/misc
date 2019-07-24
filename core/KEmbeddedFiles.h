#pragma once
#include "KFile.h"

namespace mo {

/// 実行ファイルなどに埋め込まれているファイル（例：リソースデータ、バンドルデータ）をロードする
/// 
/// リソースとしてファイルを埋め込む方法 (Visual Studio)
/// <ol>
/// <li> "プロジェクト名.rc" ファイルを作成し、プロジェクトに追加する
/// <li> データ名、データタイプ、ファイル名の順番で埋め込みたいデータを記述する。
///     @par
///     [例1]<br>
///     アイコンファイル app.ico を APPICON という名前で追加する時は
///     @code
///     MYAPPICON ICON "app.ico"
///     @endcode
///     と書く。このようにして埋め込んだデータは KEmbeddedFiles::getFile("myappicon.icon" ) で取得できる
///
///     @par
///     [例2]<br>
///     ファイル gamedata.txt を GAMEDATA という名前で、ユーザー定義のデータタイプ TXT として追加するなら
///     @code
///     GAMEDATA TXT "gamedata.txt"
///     @endcode
///     と書く。このようにして埋め込んだデータは KEmbeddedFiles::getFile("gamedata.txt" ) で取得できる
///     このとき、元のファイル名と同じ名前で取得できるところがミソである。ディレクトリ構造には対応できないので注意すること
///
/// <li> ビルドすると、勝手に実行ファイルにデータが埋め込まれる
/// <li> KEmbeddedFiles::getFile("データ名.データタイプ") の形式でファイル名を指定するとストリームを取得できる
/// </ol>
/// 注意:
///     #ifdef ～などのプリプロセッサを使いたい場合、リソース用のプリプロセッサ定義は C 言語用途は別なので注意。
///     リソースコンパイラ用のプリプロセッサ定義は [プロパティ-->構成-->リソース-->プリプロセッサ] で指定する。
///
class KEmbeddedFiles {
public:
	static bool hasFile(const Path &name);
	static FileInput getFile(const Path &name);
};

} // namespace
