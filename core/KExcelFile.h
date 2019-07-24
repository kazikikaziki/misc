#pragma once
#include "KPath.h"
namespace mo {

class FileInput;
class FileOutput;
class KExcelFile;

/// 行と列で定義されるデータテーブルを扱う。
/// 単純な「名前＝値」の形式のテーブルで良ければ KNamedValues または IOrderedParameters を使う
class KTable {
public:
	KTable();
	bool empty() const;

	/// Excel シートからテーブルオブジェクトを作成する
	/// 
	/// @param excel          Excel オブジェクト
	/// @param sheetname      シート名
	/// @param top_cell_text  テーブル範囲の左上にあるセルのテキスト（省略すると "@BEGIN"）（このテキストと一致するものをシートから探し、テーブル範囲上端とする）
	/// @param btm_cell_text  テーブル範囲の左下にあるセルのテキスト（省略すると "@END"）（このテキストと一致するものをシートから探し、テーブル範囲下端とする）
	///
	/// テーブルの行範囲は top_cell_text と btm_cell_text で挟まれた部分になる。
	/// テーブルの列範囲は top_cell_text の右隣から始まるカラムIDテキストが終わるまで（top_cell_text の右側のセルを順番に見ていったとき、空白セルがあればそこがテーブルの右端になる）
	/// 例えばデータシートを次のように記述する:
	/// @code
	///      | [A]    | [B]   | [C] | [D] |
	/// -----+--------+-------+-----+-----+--
	///    1 | @BEGIN | KEY   | VAL |     |
	/// -----+--------+-------+-----+-----+--
	///    2 |        | one   | 100 |     |
	/// -----+--------+-------+-----+-----+--
	///    3 | //     | two   | 200 |     |
	/// -----+--------+-------+-----+-----+--
	///    4 |        | three | 300 |     |
	/// -----+--------+-------+-----+-----+--
	///    5 |        | four  | 400 |     |
	/// -----+--------+-------+-----+-----+--
	///    6 |        | five  | 500 |     |
	/// -----+--------+-------+-----+-----+--
	///    7 | @END   |       |     |     |
	/// -----+--------+-------+-----+-----+--
	///    8 |        |       |     |     |
	/// @endcode
	/// この状態で createTableFromExcel(excel, sheetname, "@BEGIN", "@END") を呼び出すと、A1 から C7 の範囲を ITable に読み込むことになる
	/// このようにして取得した ITable は以下のような値を返す
	///     ITable::getDataColIndexByName("KEY") ==> 0
	///     ITable::getDataColIndexByName("VAL") ==> 1
	///     ITable::getDataString(0, 0) ==> "one"
	///     ITable::getDataString(1, 0) ==> "100"
	///     ITable::getDataString(0, 2) ==> "three"
	///     ITable::getDataString(1, 2) ==> "300"
	///     ITable::getRowMarker(0) ==> NULL
	///     ITable::getRowMarker(1) ==> "//"
	///
	bool loadFromExcelFile(const KExcelFile &file, const Path &sheet_name, const Path &top_cell_text, const Path &bottom_cell_text);

	/// テーブルを作成する。詳細は createTableFromExcel を参照
	/// @param xmls   .xlsx ファイルオブジェクト
	/// @param filename   フィアル名（エラーメッセージの表示などで使用）
	/// @param sheetname  シート名
	/// @param top_cell_text テーブル範囲の左上にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左上とする
	/// @param btm_cell_text テーブル範囲の左下（右下ではない）にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左下とする
	bool loadFromFile(FileInput &xmls, const Path &filename, const Path &sheetname, const Path &top_cell_text, const Path &btm_cell_text);

	/// テーブルを作成する。詳細は createTableFromExcel を参照
	/// @param xlsx_bin   .xlsx ファイルのバイナリデータ
	/// @param xlsx_size  .xlsx ファイルのバイナリバイト数
	/// @param filename   フィアル名（エラーメッセージの表示などで使用）
	/// @param sheetname  シート名
	/// @param top_cell_text テーブル範囲の左上にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左上とする
	/// @param btm_cell_text テーブル範囲の左下（右下ではない）にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左下とする
	bool loadFromExcelMemory(const void *xlsx_bin, size_t xlsx_size, const Path &filename, const Path &sheetname, const Path &top_cell_text, const Path &btm_cell_text);

	/// カラム名のインデックスを返す。カラム名が存在しない場合は -1 を返す
	int getDataColIndexByName(const Path &column_name) const;

	/// 指定された列に対応するカラム名を返す
	Path getColumnName(int col) const;
	
	/// データ列数
	int getDataColCount() const;

	/// データ行数
	int getDataRowCount() const;

	/// データ行にユーザー定義のマーカーが設定されているなら、それを返す。
	/// 例えば行全体がコメントアウトされている時には "#" や "//" を返すなど。
	const char * getRowMarker(int row) const;

	/// このテーブルのデータ文字列を得る(UTF8)
	/// col  データ列インデックス（ゼロ起算）
	/// row  データ行インデックス（ゼロ起算）
	/// retval: utf8文字列またはNULL
	const char * getDataString(int col, int row) const;

	/// このテーブルのデータ整数を得る
	/// col  データ列インデックス（ゼロ起算）
	/// row  データ行インデックス（ゼロ起算）
	int getDataInt(int col, int row, int def=0) const;

	/// このテーブルのデータ実数を得る
	/// col  データ列インデックス（ゼロ起算）
	/// row  データ行インデックス（ゼロ起算）
	float getDataFloat(int col, int row, float def=0.0f) const;

	/// データ列とデータ行を指定し、それが定義されている列と行を得る
	bool getDataSource(int col, int row, int *col_in_file, int *row_in_file) const;
private:
	class Impl;
	std::shared_ptr<Impl> impl_;
};


/// セル巡回用のコールバック
class IScanCellsCallback {
public:
	/// セル巡回時に呼ばれる
	/// col 列番号（Excelの画面とは異なり、0起算であることに注意）
	/// row 行番号（Excelの画面とは異なり、0起算であることに注意）
	/// s セルの文字列(UTF8)
	virtual void onCell(int col, int row, const char *s) = 0;
};


class KExcelFile {
public:
	KExcelFile();

	bool empty() const;

	/// 元のファイル名を返す
	const Path & getFileName() const;

	/// .XLSX ファイルをロードする
	bool loadFromFile(FileInput &file, const Path &xlsx_name);
	bool loadFromFileName(const Path &name);
	bool loadFromMemory(const void *bin, size_t size, const Path &name);

	/// シート数を返す
	int getSheetCount() const;

	/// シート名からシートを探し、見つかればゼロ起算のシート番号を返す。
	/// 見つからなければ -1 を返す
	int getSheetByName(const Path &name) const;

	/// シート名を得る
	Path getSheetName(int sheet) const;

	/// シート内で有効なセルの存在する範囲を得る。みつかれば true を返す
	/// sheet   : シート番号（ゼロ起算）
	/// col     : 左上の列番号（ゼロ起算）
	/// row     : 左上の行番号（ゼロ起算）
	/// colconut: 列数
	/// rowcount: 行数
	bool getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const;

	/// 指定されたセルの文字列を返す。文字列を含んでいない場合は "" を返す
	/// sheet: シート番号（ゼロ起算）
	/// col: 列番号（ゼロ起算）
	/// row: 行番号（ゼロ起算）
	/// retval: utf8文字列またはNULL
	const char * getDataString(int sheet, int col, int row) const;

	/// 文字列を完全一致で検索する。みつかったらセルの行列番号を row, col にセットして true を返す
	/// 空文字列を検索することはできない。
	/// col: 見つかったセルの列番号（ゼロ起算）
	/// row: 見つかったセルの行番号（ゼロ起算）
	bool getCellByText(int sheet, const char *s, int *col, int *row) const;

	/// 全てのセルを巡回する
	/// sheet   : シート番号（ゼロ起算）
	/// cb      : セル巡回時に呼ばれるコールバックオブジェクト
	void scanCells(int sheet, IScanCellsCallback *cb) const;

private:
	class Impl;
	std::shared_ptr<Impl> impl_;
};

class KExcel {
public:
	/// excel オブジェクトのセル文字列を XML 形式でエクスポートする
	static bool exportXml(const KExcelFile &excel, FileOutput &output, bool comment=true);

	/// .xlsx ファイルをロードし、セル文字列を XML 形式でエクスポートする
	static void exportXml(const Path &input_xlsx_filename, const Path &output_xml_filename, bool comment=true);

	/// excel オブジェクトのセル文字列をテキスト形式でエクスポートする
	static bool exportText(const KExcelFile &excel, FileOutput &output);

	/// .xlsx ファイルをロードし、セル文字列をテキスト形式でエクスポートする
	static void exportText(const Path &input_xlsx_filename, const Path &output_filename);


	///行列インデックス（0起算）から "A1" や "AB12" などのセル名を得る
	static bool encodeCellName(int col, int row, Path *name);
	static Path encodeCellName(int col, int row);

	/// "A1" や "AB12" などのセル名から、行列インデックス（0起算）を取得する
	static bool decodeCellName(const char *s, int *col, int *row);

}; // class

} // namespace
