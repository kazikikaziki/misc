#include "KExcelFile.h"
//
#include "KConv.h"
#include "KFile.h"
#include "KXmlFile.h"
#include "KZipFile.h"
#include "KStd.h"
#include <unordered_map>

#if 1
#include "KLog.h"
#define EXASSERT(x)        Log_assert(x)
#define EXERROR(fmt, ...)  Log_errorf(fmt, ##__VA_ARGS__)
#define EXPRINT(fmt, ...)  Log_verbosef(fmt, ##__VA_ARGS__)
#else
#include <assert.h>
#define EXASSERT(x)        assert(x)
#define EXERROR(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#define EXPRINT(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#endif

namespace mo {

inline int _max(int a, int b) {
	return (a >= b) ? a : b;
}
inline int _min(int a, int b) {
	return (a <= b) ? a : b;
}

static tinyxml2::XMLDocument * xml_loadFromArchive(KZipFileReader &zip, const Path &zipname, const Path &entry_name) {
	if (!zip.isOpen()) {
		EXERROR("E_INVALID_ARGUMENT");
		return NULL;
	}

	// XLSX の XML は常に UTF-8 で書いてある。それを信用する
	std::string xml_u8 = zip.getEntryDataByName(entry_name, false, false);
	if (xml_u8.empty()) {
		EXERROR("E_FILE: Failed to open file '%s' from archive '%s'", entry_name.u8(), zipname.u8());
		return NULL;
	}

	Path doc_name = zipname.join(entry_name);
	tinyxml2::XMLDocument *doc = KXml::createXmlDocument(xml_u8.c_str(), doc_name);
	if (doc == NULL) {
		EXERROR("E_XML: Failed to read xml document: '%s' from archive '%s'", entry_name.u8(), zipname.u8());
	}
	return doc;
}



#define EXCEL_ALPHABET_NUM 26
#define EXCEL_COL_LIMIT    (EXCEL_ALPHABET_NUM * EXCEL_ALPHABET_NUM)
#define EXCEL_ROW_LIMIT    1024


Path KExcel::encodeCellName(int col, int row) {
	Path s;
	encodeCellName(col, row, &s);
	return s;
}
bool KExcel::encodeCellName(int col, int row, Path *name) {
	if (col < 0) return false;
	if (row < 0) return false;
	if (col < EXCEL_ALPHABET_NUM) {
		char c = (char)('A' + col);
		char s[256];
		sprintf(s, "%c%d", c, 1+row);
		if (name) *name = Path(s);
		return true;
	}
	if (col < EXCEL_ALPHABET_NUM*EXCEL_ALPHABET_NUM) {
		char c1 = (char)('A' + (col / EXCEL_ALPHABET_NUM));
		char c2 = (char)('A' + (col % EXCEL_ALPHABET_NUM));
		char s[256];
		EXASSERT(isalpha(c1));
		EXASSERT(isalpha(c2));
		sprintf(s, "%c%c%d", c1, c2, 1+row);
		if (name) *name = Path(s);
		return true;
	}
	return false;
}

/// "A2" や "AM244" などのセル番号を int の組にデコードする
bool KExcel::decodeCellName(const char *s, int *col, int *row) {
	if (s==NULL || s[0]=='\0') return false;
	int c = -1;
	int r = -1;

	if (isalpha(s[0]) && isdigit(s[1])) {
		// セル番号が [A-Z][0-9].* にマッチしている。
		// 例えば "A1" や "Z42" など。
		c = toupper(s[0]) - 'A';
		EXASSERT(0 <= c && c < EXCEL_ALPHABET_NUM);
		r = strtol(s + 1, NULL, 0);
		r--; // １起算 --> 0起算

	} else if (isalpha(s[0]) && isalpha(s[1]) && isdigit(s[2])) {
		// セル番号が [A-Z][A-Z][0-9].* にマッチしている。
		// 例えば "AA42" や "KZ1217" など
		int idx1 = toupper(s[0]) - 'A';
		int idx2 = toupper(s[1]) - 'A';
		EXASSERT(0 <= idx1 && idx1 < EXCEL_ALPHABET_NUM);
		EXASSERT(0 <= idx2 && idx2 < EXCEL_ALPHABET_NUM);
		c = idx1 * EXCEL_ALPHABET_NUM + idx2;
		r = strtol(s + 2, NULL, 0);
		r--; // １起算 --> 0起算
	}
	if (c >= 0 && r >= 0) {
		// ここで、セル範囲として 0xFFFFF が入る可能性に注意。(LibreOffice Calc で現象を確認）
		// 0xFFFFF 以上の値が入っていたら範囲取得失敗とみなす
		if (r >= 0xFFFFF) {
			return false;
		}
		EXASSERT(0 <= c && c < EXCEL_COL_LIMIT);
		EXASSERT(0 <= r && r < EXCEL_ROW_LIMIT);
		if (col) *col = c;
		if (row) *row = r;
		return true;
	}

	return false;
}

bool KExcel::exportXml(const KExcelFile &excel, FileOutput &output, bool comment) {
	class CB: public IScanCellsCallback {
	public:
		std::string &dest_;
		int last_row_;
		int last_col_;
		
		CB(std::string &s): dest_(s) {
			last_row_ = -1;
			last_col_ = -1;
		}
		virtual void onCell(int col, int row, const char *s) override {
			if (s==NULL || s[0]=='\0') return;
			EXASSERT(last_row_ <= row); // 行番号は必ず前回と等しいか、大きくなる
			if (last_row_ != row) {
				if (last_row_ >= 0) { // 行タグを閉じる
					dest_ += "</row>\n";
				}
				if (last_row_ < 0 || last_row_ + 1 < row) {
					// 行番号が飛んでいる場合のみ列番号を付加する
					dest_ += K_sprintf("<row r='%d'>", row);
				} else {
					// インクリメントで済む場合は行番号を省略
					dest_ += "<row>";
				}
				last_row_ = row;
				last_col_ = -1;
			}
			if (last_col_ < 0 || last_col_ + 1 < col) {
				// 列番号が飛んでいる場合のみ列番号を付加する
				dest_ += K_sprintf("<c i='%d'>", col);
			} else {
				// インクリメントで済む場合は列番号を省略
				dest_ += "<c>";
			}
			if (KXml::stringShouldEscape(s)) { // xml禁止文字が含まれているなら CDATA 使う
				dest_ += K_sprintf("<![CDATA[%s]]>", s);
			} else {
				dest_ += s;
			}
			dest_ += "</c>";
			last_col_ = col;
		}
	};
	if (excel.empty()) return false;
	if (output.empty()) return false;
	
	std::string s;
	s += "<?xml version='1.0' encoding='utf-8'>\n";
	if (comment) {
		s += "<!-- ENCODING WITH UTF8 -->\n";
		s += "<!-- Element [row] corresponds each row of sheet. It has attribute [r] for zero-based index of the row. [r] may be omitted if [row] is just next of the last [row] -->\n";
		s += "<!-- Element [c] corresponds each cell of row. It has attribute [i] for zero-based index of the col. [i] may be omitted if [c] is just next of the last [c] -->\n";
	}
	s += K_sprintf("<excel numsheets='%d'>\n", excel.getSheetCount());
	for (int iSheet=0; iSheet<excel.getSheetCount(); iSheet++) {
		int col=0, row=0, nCol=0, nRow=0;
		Path sheet_name = excel.getSheetName(iSheet);
		excel.getSheetDimension(iSheet, &col, &row, &nCol, &nRow);
		s += K_sprintf("<sheet name='%s' left='%d' top='%d' cols='%d' rows='%d'>\n", sheet_name.u8(), col, row, nCol, nRow);
		{
			CB cb(s);
			excel.scanCells(iSheet, &cb);
			if (cb.last_row_ >= 0) {
				s += "</row>\n"; // 最終行を閉じる
			} else {
				// セルを一つも出力していないので <row> を閉じる必要もない
			}
		}
		s += "</sheet>";
		if (comment) {
			s += K_sprintf("<!-- %s -->", sheet_name.u8());
		}
		s += "\n";
	}
	s += "</excel>\n";
	output.write(s.c_str()); // UTF8
	return true;
}

void KExcel::exportXml(const Path &input_xlsx_filename, const Path &output_xml_filename, bool comment) {
	KExcelFile excel;
	excel.loadFromFileName(input_xlsx_filename);
	FileOutput out;
	if (!excel.empty() && out.open(output_xml_filename)) {
		exportXml(excel, out, comment);
	}
}

void escape_string(std::string &s) {
	K_strrep(s, "\\", "\\\\");
	K_strrep(s, "\"", "\\\"");
	K_strrep(s, "\n", "\\n");
	K_strrep(s, "\r", "");
	if (s.find(',') != std::string::npos) {
		s.insert(0, "\"");
		s.append("\"");
	}
}

bool KExcel::exportText(const KExcelFile &excel, FileOutput &output) {
	if (excel.empty()) return false;
	if (output.empty()) return false;
	std::string s;
	for (int iSheet=0; iSheet<excel.getSheetCount(); iSheet++) {
		int col=0, row=0, nCol=0, nRow=0;
		Path sheet_name = excel.getSheetName(iSheet);
		excel.getSheetDimension(iSheet, &col, &row, &nCol, &nRow);
		s += "\n";
		s += "============================================================================\n";
		s += std::string(sheet_name.u8()) + "\n";
		s += "============================================================================\n";
		int blank_lines = 0;
		for (int r=0; r<nRow; r++) {
			bool has_cell = false;
			for (int c=0; c<nCol; c++) {
				const char *str = excel.getDataString(iSheet, c, r);
				if (str && str[0]) {
					if (blank_lines >= 1) {
						s += "\n";
						blank_lines = 0;
					}
					if (has_cell) s += ", ";
					std::string ss = str;
					escape_string(ss);
					s += ss;
					has_cell = true;
				}
			}
			if (has_cell) {
				s += "\n";
			} else {
				blank_lines++;
			}
		}
	}
	output.write(s.c_str());
	return true;
}

void KExcel::exportText(const Path &input_xlsx_filename, const Path &output_filename) {
	KExcelFile excel;
	if (excel.loadFromFileName(input_xlsx_filename)) {
		FileOutput out;
		if (out.open(output_filename)) {
			exportText(excel, out);
		}
	}
}


class KExcelFile::Impl {
	static const int ROW_LIMIT = 10000;

	enum Type {
		TP_NUMBER,
		TP_STRINGID,
		TP_LITERAL,
		TP_OTHER,
	};
	Path filename_;
	tinyxml2::XMLDocument *workbook_doc_;
	std::vector<tinyxml2::XMLDocument *> worksheets_;
	std::unordered_map<int, std::string> strings_;
	typedef std::unordered_map<int, tinyxml2::XMLElement *> Int_Elms;
	typedef std::unordered_map<tinyxml2::XMLElement *, Int_Elms> Sheet_RowElms;
	mutable Sheet_RowElms row_elements_;
public:
	Impl() {
		workbook_doc_ = NULL;
	}
	~Impl() {
		clear();
	}
	void clear() {
		for (size_t i=0; i<worksheets_.size(); i++) {
			KXml::deleteXml(worksheets_[i]);
		}
		worksheets_.clear();
		strings_.clear();
		row_elements_.clear();
		KXml::deleteXml(workbook_doc_);
		workbook_doc_ = NULL;
		filename_.clear();
	}
	bool empty() const {
		return worksheets_.empty();
	}
	const Path & getFileName() const {
		return filename_;
	}
	int getSheetCount() const {
		return (int)worksheets_.size();
	}
	Path getSheetName(int sheet) const {
		tinyxml2::XMLElement *root_elm = workbook_doc_->FirstChildElement();
		EXASSERT(root_elm);

		tinyxml2::XMLElement *sheets_xml = root_elm->FirstChildElement("sheets");
		EXASSERT(sheets_xml);

		int idx = 0;
		for (tinyxml2::XMLElement *it=sheets_xml->FirstChildElement("sheet"); it!=NULL; it=it->NextSiblingElement("sheet")) {
			if (idx == sheet) {
				const char *s = it->Attribute("name");
				#ifdef _DEBUG
				{
					// <sheet> の sheetId には１起算のシート番号が入っていて、
					// その番号は <sheets> 内での <sheet> の並び順と同じであると仮定している。
					// 一応整合性を確認しておく
					int id = it->IntAttribute("sheetId", -1);
					EXASSERT(id == 1 + idx);
				}
				#endif
				return Path(s);
			}
			idx++;
		}
		return "";
	}
	int getSheetByName(const Path &name) const {
		tinyxml2::XMLElement *root_elm = workbook_doc_->FirstChildElement();
		EXASSERT(root_elm);

		tinyxml2::XMLElement *sheets_xml = root_elm->FirstChildElement("sheets");
		EXASSERT(sheets_xml);

		int idx = 0;
		for (tinyxml2::XMLElement *it=sheets_xml->FirstChildElement("sheet"); it!=NULL; it=it->NextSiblingElement("sheet")) {
			const char *name_u8 = it->Attribute("name");
			if (name_u8 && name.compare_str(name_u8) == 0) {
				#ifdef _DEBUG
				{
					// <sheet> の sheetId には１起算のシート番号が入っていて、
					// その番号は <sheets> 内での <sheet> の並び順と同じであると仮定している。
					// 一応整合性を確認しておく
					int id = it->IntAttribute("sheetId", -1);
					EXASSERT(id == 1 + idx);
				}
				#endif
				return idx;
			}
			idx++;
		}
		return -1;
	}
	bool getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const {
		if (sheet < 0) return false;
		if (sheet >= (int)worksheets_.size()) return false;

		tinyxml2::XMLDocument *xdoc = worksheets_[sheet];
		EXASSERT(xdoc);

		tinyxml2::XMLElement *xroot = xdoc->FirstChildElement();
		EXASSERT(xroot);

		tinyxml2::XMLElement *xdim = xroot->FirstChildElement("dimension");
		EXASSERT(xdim);

		// セルの定義範囲を表す文字列を取得する
		// この文字列は "A1:E199" のようにコロンで左上端セルと右下端セル番号が書いてある
		const char *attr = xdim->Attribute("ref");

		// コロンを区切りにして分割
		char tmp[256];
		strcpy(tmp, attr ? attr : "");
		
		// コロンの位置を見つける。
		char *colon = strchr(tmp, ':');
		if (colon) {
			// コロンの位置で文字列を分割する。コロンをヌル文字に置換する
			*colon = '\0';

			// minpos にはコロン左側の文字列を入れる
			// maxpos にはコロン右側の文字列を入れる
			const char *minpos = tmp;
			const char *maxpos = colon + 1;

			// セル番号から行列インデックスを得る
			int L=0, R=0, T=0, B=0;
			if (_getCellIndex(minpos, &L, &T) && _getCellIndex(maxpos, &R, &B)) {
				if (col) *col = L;
				if (row) *row = T;
				if (colcount) *colcount = R - L + 1;
				if (rowcount) *rowcount = B - T + 1;
				return true;
			}
		}

		// インデックスを得られなかった場合は自力で取得する
		//
		// 条件は分からないが、Libre Office Calc でシートを保存したとき、
		// 実際の範囲は "A1:M60" であるにもかかわらず "A1:AMJ60" といった記述になることがあったため。
		// 念のために自力での取得コードも書いておく。
		tinyxml2::XMLElement *sheet_xml = xroot->FirstChildElement("sheetData");
		if (sheet_xml) {
			CScanDim dim;
			_scanCells(sheet_xml, &dim);
			if (col) *col = dim.col0_;
			if (row) *row = dim.row0_;
			if (colcount) *colcount = dim.col1_ - dim.col0_ + 1;
			if (rowcount) *rowcount = dim.row1_ - dim.row0_ + 1;
			return true;
		}

		return false;
	}
	const char * getDataString(int sheet, int col, int row) const {
		if (sheet < 0 || col < 0 || row < 0) return NULL;
		tinyxml2::XMLElement *s = _getSheetData(sheet);
		tinyxml2::XMLElement *r = _getRow(s, row);
		tinyxml2::XMLElement *c = _getCell(r, col);
		return _getDataString(c);
	}
	bool getCellByText(int sheet, const char *s, int *col, int *row) const {
		if (sheet < 0) return false;
		if (s==NULL || s[0]=='\0') return false;

		// シートを選択
		tinyxml2::XMLElement *sheet_xml = _getSheetData(sheet);
		if (sheet_xml == NULL) return false;

		// 同じ文字列を持つセルを探す
		tinyxml2::XMLElement *c_xml = _getCellByText(sheet_xml, s);
		if (c_xml == NULL) return false;

		EXASSERT(col);
		EXASSERT(row);

		// 見つかったセルの行列番号を得る
		int icol = -1;
		int irow = -1;
		const char *r = c_xml->Attribute("r");
		_getCellIndex(r, &icol, &irow);

		if (icol >= 0 && irow >= 0) {
			*col = icol;
			*row = irow;
			return true;
		}
		return false;
	}
	void scanCells(int sheet, IScanCellsCallback *cb) const {
		if (sheet < 0) return;
		if (sheet >= (int)worksheets_.size()) return;

		tinyxml2::XMLDocument *doc = worksheets_[sheet];
		EXASSERT(doc);

		tinyxml2::XMLElement *root_elm = doc->FirstChildElement();
		EXASSERT(root_elm);

		tinyxml2::XMLElement *sheet_xml = root_elm->FirstChildElement("sheetData");
		if (sheet_xml) {
			_scanCells(sheet_xml, cb);
		}
	}
	bool loadFromFile(FileInput &file, const Path &xlsx_name) {
		row_elements_.clear();

		if (file.empty()) {
			EXERROR("E_INVALID_ARGUMENT");
			return false;
		}

		// XMLS ファイルを ZIP として開く
		KZipFileReader zip(file);
		if (!zip.isOpen()) {
			EXERROR("E_FAILED_TO_OPEN_EXCEL_ARCHIVE: %s", xlsx_name.u8());
			return false;
		}

		// 文字列テーブルを取得
		tinyxml2::XMLDocument *strings_doc = xml_loadFromArchive(zip, xlsx_name, "xl/sharedStrings.xml");
		if (strings_doc) {
			int id = 0;
			tinyxml2::XMLElement *string_elm = strings_doc->FirstChildElement();
			for (auto si_xml=string_elm->FirstChildElement("si"); si_xml!=NULL; si_xml=si_xml->NextSiblingElement("si"), id++) {
				// パターンA
				// <si>
				//   <t>テキスト</t>
				// </si>
				if (si_xml->FirstChildElement("t")) {
					const char *s = si_xml->FirstChildElement("t")->GetText();
					strings_[id] = s;
					continue;
				}

				// パターンB（テキストの途中でスタイル変更がある場合にこうなる？）
				// <si>
				//   <rPr>スタイル情報いろいろ</rPr>
				//   <r><t>テキスト1</t></r>
				//   <r><t>テキスト2</t></r>
				//   ....
				// </si>
				if (si_xml->FirstChildElement("r")) {
					std::string s;
					for (tinyxml2::XMLElement *r_xml=si_xml->FirstChildElement("r"); r_xml!=NULL; r_xml=r_xml->NextSiblingElement("r")) {
						tinyxml2::XMLElement *t_xml = r_xml->FirstChildElement("t");
						if (t_xml) {
							s += t_xml->GetText();
						}
					}
					strings_[id] = s;
					continue;
				}
			}
			KXml::deleteXml(strings_doc);
		}

		// ワークシート情報を取得
		workbook_doc_ = xml_loadFromArchive(zip, xlsx_name, "xl/workbook.xml");

		// ワークシートを取得
		for (int i=1; ; i++) {
			Path s = Path::fromFormat("xl/worksheets/sheet%d.xml", i);
			if (zip.findEntry(s, false, false) < 0) break;
			tinyxml2::XMLDocument *sheet_doc = xml_loadFromArchive(zip, xlsx_name, s);
			if (sheet_doc == NULL) {
				EXERROR("E_XLSX: Failed to load document: '%s' in xlsx file.", s.u8());
				break;
			}
			worksheets_.push_back(sheet_doc);
		}

		// 必要な情報がそろっていることを確認
		if (workbook_doc_ == NULL || worksheets_.empty()) {
			EXERROR("E_FAILED_TO_READ_EXCEL_ARCHIVE: %s", xlsx_name.u8());
			clear();
			return false;
		}

		filename_ = xlsx_name;
		return true;
	}

private:
	const char * _getDataString(tinyxml2::XMLElement *cell_xml) const {
		Type tp;
		const char *val = _getRawData(cell_xml, &tp);
		if (val == NULL) {
			return "";
		}
		if (tp == TP_STRINGID) {
			// val は文字列ID (整数) を表している。
			// 対応する文字列を文字列テーブルから探す
			int sid = K_strtoi(val);
			EXASSERT(sid >= 0);
			auto it = strings_.find(sid);
			if (it != strings_.end()) {
				return it->second.c_str();
			}
			return "";
		}
		if (tp == TP_LITERAL) {
			// val は文字列そのものを表している
			return val;
		}
		// それ以外のデータの場合は val の文字列をそのまま返す
		return val;
	}

	tinyxml2::XMLElement * _getCellByText(tinyxml2::XMLElement *sheet_xml, const char *s) const {
		if (sheet_xml == NULL) return NULL;
		if (s==NULL || s[0]=='\0') return NULL;
		for (tinyxml2::XMLElement *rit=sheet_xml->FirstChildElement("row"); rit!=NULL; rit=rit->NextSiblingElement("row")) {
			for (tinyxml2::XMLElement *cit=rit->FirstChildElement("c"); cit!=NULL; cit=cit->NextSiblingElement("c")) {
				const char *str = _getDataString(cit);
				if (strcmp(str, s) == 0) {
					return cit;
				}
			}
		}
		return NULL;
	}

	// "A1" や "AB12" などのセル番号から、行列インデックスを取得する
	bool _getCellIndex(const char *ss, int *col, int *row) const {
		return KExcel::decodeCellName(ss, col, row);
	}

	tinyxml2::XMLElement * _getSheetData(int sheet) const {
		if (sheet < 0) return NULL;
		if (sheet >= (int)worksheets_.size()) return NULL;

		tinyxml2::XMLDocument *doc = worksheets_[sheet];
		EXASSERT(doc);

		tinyxml2::XMLElement *root_elm = doc->FirstChildElement();
		EXASSERT(root_elm);

		return root_elm->FirstChildElement("sheetData");
	}
	void _scanCells(tinyxml2::XMLElement *sheet_xml, IScanCellsCallback *cb) const {
		if (sheet_xml == NULL) return;
		for (tinyxml2::XMLElement *rit=sheet_xml->FirstChildElement("row"); rit!=NULL; rit=rit->NextSiblingElement("row")) {
		//	int row = rit->IntAttribute("r", -1);
			for (tinyxml2::XMLElement *cit=rit->FirstChildElement("c"); cit!=NULL; cit=cit->NextSiblingElement("c")) {
				const char *pos = cit->Attribute("r");
				int cidx = -1;
				int ridx = -1;
				if (_getCellIndex(pos, &cidx, &ridx)) {
					EXASSERT(cidx >= 0 && ridx >= 0);
					const char *val = _getDataString(cit);
					cb->onCell(cidx, ridx, val);
				}
			}
		}
	}
	tinyxml2::XMLElement * _getRow(tinyxml2::XMLElement *sheet_xml, int row) const {
		if (sheet_xml == NULL) return NULL;
		if (row < 0) return NULL;
		// キャッシュからから探す
		if (! row_elements_.empty()) {
			// シートを得る
			Sheet_RowElms::const_iterator sheet_it = row_elements_.find(sheet_xml);
			if (sheet_it != row_elements_.end()) {
				// 行を得る
				Int_Elms::const_iterator row_it = sheet_it->second.find(row);
				if (row_it != sheet_it->second.end()) {
					return row_it->second;
				}
				// 指定行が存在しない
				return NULL;
			}
			// シートがまだキャッシュ化されていない。作成する
		}

		// キャッシュから見つからないなら、キャッシュを作りつつ目的のデータを探す
		tinyxml2::XMLElement *ret = NULL;
		for (tinyxml2::XMLElement *it=sheet_xml->FirstChildElement("row"); it!=NULL; it=it->NextSiblingElement("row")) {
			int val = it->IntAttribute("r");
			if (val >= 1) {
				int r = val - 1;
				row_elements_[sheet_xml][r] = it;
				if (r == row) {
					ret = it;
				}
			}
		}
		return ret;
	}
	tinyxml2::XMLElement * _getCell(tinyxml2::XMLElement *row_xml, int col) const {
		const int NUM_ALPHABET = 26;
		if (row_xml == NULL) return NULL;
		if (col < 0) return NULL;
		for (tinyxml2::XMLElement *it=row_xml->FirstChildElement("c"); it!=NULL; it=it->NextSiblingElement("c")) {
			const char *s = it->Attribute("r");
			int col_idx = -1;
			_getCellIndex(s, &col_idx, NULL);
			if (col_idx == col) {
				return it;
			}
		}
		return NULL;
	}
	const char * _getRawData(tinyxml2::XMLElement *cell_xml, Type *type) const {
		if (cell_xml == NULL) return NULL;
		EXASSERT(type);
		// cell_xml の入力例:
		// <c r="B1" s="0" t="n">
		// 	<v>12</v>
		// </c>

		// データ型
		const char *t = cell_xml->Attribute("t");
		if (t == NULL) return NULL;

		if (strcmp(t, "n") == 0) {
			*type = TP_NUMBER; // <v> には数値が指定されている
		} else if (strcmp(t, "s") == 0) {
			*type = TP_STRINGID; // <v> には文字列ＩＤが指定されている
		} else if (strcmp(t, "str") == 0) {
			*type = TP_LITERAL; // <v> には文字列が直接指定されている
		} else { 
			*type = TP_OTHER; // それ以外の値が入っている
		}
		// データ文字列
		tinyxml2::XMLElement *v_elm = cell_xml->FirstChildElement("v");
		if (v_elm == NULL) return NULL;
		return v_elm->GetText();
	}

	class CScanDim: public IScanCellsCallback {
	public:
		int col0_, col1_, row0_, row1_;

		CScanDim() {
			col0_ = -1;
			col1_ = -1;
			row0_ = -1;
			row1_ = -1;
		}
		virtual void onCell(int col, int row, const char *s) override {
			if (col0_ < 0) {
				col0_ = col;
				col1_ = col;
				row0_ = row;
				row1_ = row;
			} else {
				col0_ = _min(col0_, col);
				col1_ = _max(col1_, col);
				row0_ = _min(row0_, row);
				row1_ = _max(row1_, row);
			}
		}
	};
};

KExcelFile::KExcelFile() {
	impl_ = std::make_shared<Impl>();
}
bool KExcelFile::empty() const {
	return impl_->empty();
}
const Path & KExcelFile::getFileName() const {
	return impl_->getFileName();
}
bool KExcelFile::loadFromFile(FileInput &file, const Path &xlsx_name) {
	return impl_->loadFromFile(file, xlsx_name);
}
bool KExcelFile::loadFromFileName(const Path &name) {
	FileInput file;
	if (file.open(name)) {
		return impl_->loadFromFile(file, name);
	}
	impl_->clear();
	return false;
}
bool KExcelFile::loadFromMemory(const void *bin, size_t size, const Path &name) {
	FileInput file;
	if (file.open(bin, size)) {
		return impl_->loadFromFile(file, name);
	}
	impl_->clear();
	return false;
}
int KExcelFile::getSheetCount() const {
	return impl_->getSheetCount();
}
int KExcelFile::getSheetByName(const Path &name) const {
	return impl_->getSheetByName(name);
}
Path KExcelFile::getSheetName(int sheet) const {
	return impl_->getSheetName(sheet);
}
bool KExcelFile::getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const {
	return impl_->getSheetDimension(sheet, col, row, colcount, rowcount);
}
const char * KExcelFile::getDataString(int sheet, int col, int row) const {
	return impl_->getDataString(sheet, col, row);
}
bool KExcelFile::getCellByText(int sheet, const char *s, int *col, int *row) const {
	return impl_->getCellByText(sheet, s, col, row);
}
void KExcelFile::scanCells(int sheet, IScanCellsCallback *cb) const {
	impl_->scanCells(sheet, cb);
}





class KTable::Impl {
	KExcelFile excel_;
	PathList colnames_; // 列の名前
	int sheet_;   // 選択中のテーブルを含んでいるシートのインデックス
	int left_;    // テーブルの左端の列インデックス
	int top_;     // テーブルの開始マークのある行インデックス
	int bottom_;  // テーブルの終端マークのある行インデックス
public:
	Impl() {
		sheet_ = -1;
		left_ = 0;
		top_ = 0;
		bottom_ = 0;
	}
	~Impl() {
		_unload();
	}
	bool empty() const {
		return excel_.empty();
	}
	bool _loadFromExcelFile(const KExcelFile &file) {
		excel_ = file;
		return !file.empty();
	}
	void _unload() {
		excel_ = KExcelFile(); // claer
		sheet_ = -1;
		left_ = 0;
		top_ = 0;
		bottom_ = 0;
	}
	bool _selectTable(const Path &sheet_name, const Path &top_cell_text, const Path &bottom_cell_text) {
		sheet_ = -1;
		left_ = 0;
		top_ = 0;
		bottom_ = 0;

		if (excel_.empty()) {
			EXERROR("E_EXCEL: Null data");
			return false;
		}

		int sheet = 0;
		int col0 = 0;
		int col1 = 0;
		int row0 = 0;
		int row1 = 0;

		// シートを探す
		sheet = excel_.getSheetByName(sheet_name);
		if (sheet < 0) {
			EXERROR(u8"E_EXCEL: シート '%s' が見つかりません", sheet_name.u8());
			return false;
		}

		// 開始セルを探す
		if (! excel_.getCellByText(sheet, top_cell_text.u8(), &col0, &row0)) {
			EXERROR(u8"E_EXCEL_MISSING_TABLE_BEGIN: シート '%s' にはテーブル開始セル '%s' がありません", 
				sheet_name.u8(), top_cell_text.u8());
			return false;
		}
	//	Log_verbosef("TOP CELL '%s' FOUND AT %s", top_cell_text.u8(), KExcel::encodeCellName(col0, row0).u8());

		// セルの定義範囲
		int dim_row_top = 0;
		int dim_row_cnt = 0;
		if (! excel_.getSheetDimension(sheet, NULL, &dim_row_top, NULL, &dim_row_cnt)) {
			EXERROR(u8"E_EXCEL_MISSING_SHEET_DIMENSION: シート '%s' のサイズが定義されていません", sheet_name.u8());
			return false;
		}

		// 終了セルを探す
		// 終了セルは開始セルと同じ列で、開始セルよりも下の行にあるはず
		for (int i=row0+1; i<dim_row_top+dim_row_cnt; i++) {
			const char *s = excel_.getDataString(sheet, col0, i);
			if (strcmp(bottom_cell_text.u8(), s) == 0) {
				row1 = i;
				break;
			}
		}
		if (row1 == 0) {
			Path cell = KExcel::encodeCellName(col0, row0);
			EXERROR(u8"E_EXCEL_MISSING_TABLE_END: シート '%s' のセル '%s' に対応する終端セル '%s' が見つかりません",
				sheet_name.u8(), top_cell_text.u8(), bottom_cell_text.u8());
			return false;
		}
		EXPRINT("BOTTOM CELL '%s' FOUND AT %s", bottom_cell_text.u8(), KExcel::encodeCellName(col0, row0).u8());

		// 開始セルの右隣からは、カラム名の定義が続く
		PathList cols;
		{
			int c = col0 + 1;
			while (1) {
				Path cellstr = excel_.getDataString(sheet, c, row0);
				if (cellstr.empty()) break;
				cols.push_back(cellstr);
				EXPRINT("ID CELL '%s' FOUND AT %s", cellstr.u8(), KExcel::encodeCellName(col0, row0).u8());
				c++;
			}
			if (cols.empty()) {
				EXERROR(u8"E_EXCEL_MISSING_COLUMN_HEADER: シート '%s' のテーブル '%s' にはカラムヘッダがありません", 
					sheet_name.u8(), top_cell_text.u8());
			}
			col1 = c;
		}

		// テーブル読み取りに成功した
		colnames_ = cols;
		sheet_  = sheet;
		top_    = row0;
		bottom_ = row1;
		left_   = col0;
		return true;
	}
	Path getColumnName(int col) const {
		if (excel_.empty()) {
			EXERROR("E_EXCEL: No table loaded");
			return Path::Empty;
		}
		if (sheet_ < 0) {
			EXERROR("E_EXCEL: No table selected");
			return Path::Empty;
		}
		if (col < 0 || (int)colnames_.size() <= col) {
			return Path::Empty;
		}
		return colnames_[col];
	}
	int getDataColIndexByName(const Path &column_name) const {
		if (excel_.empty()) {
			EXERROR("E_EXCEL: No table loaded");
			return -1;
		}
		if (sheet_ < 0) {
			EXERROR("E_EXCEL: No table selected");
			return -1;
		}
		for (size_t i=0; i<colnames_.size(); i++) {
			if (colnames_[i].compare(column_name) == 0) {
				return (int)i;
			}
		}
		return -1;
	}
	int getDataColCount() const {
		if (excel_.empty()) {
			EXERROR("E_EXCEL: No table loaded");
			return 0;
		}
		if (sheet_ < 0) {
			EXERROR("E_EXCEL: No table selected");
			return 0;
		}
		return (int)colnames_.size();
	}
	int getDataRowCount() const {
		if (excel_.empty()) {
			EXERROR("E_EXCEL: No table loaded");
			return 0;
		}
		if (sheet_ < 0) {
			EXERROR("E_EXCEL: No table selected");
			return 0;
		}
		// 開始行と終了行の間にある行数
		int rows = bottom_ - top_ - 1;
		EXASSERT(rows > 0);

		return rows;
	}
	const char * getRowMarker(int data_row) const {
		if (excel_.empty()) {
			EXERROR("E_EXCEL: No table loaded");
			return NULL;
		}
		if (sheet_ < 0) {
			EXERROR("E_EXCEL: No table selected");
			return NULL;
		}
		if (data_row < 0) return NULL;

		int col = left_; // 行マーカーは一番左の列にあるものとする。（一番左のデータ列の、さらに一つ左側）
		int row = top_ + 1 + data_row;
		return excel_.getDataString(sheet_, col, row);
	}
	const char * getDataString(int data_col, int data_row) const {
		if (excel_.empty()) {
			EXERROR("E_EXCEL: No table loaded");
			return NULL;
		}
		if (sheet_ < 0) {
			EXERROR("E_EXCEL: No table selected");
			return NULL;
		}
		if (data_col < 0) return NULL;
		if (data_row < 0) return NULL;

		// 一番左の列 (left_) は開始・終端キーワードを置くためにある。
		// ほかに文字が入っていてもコメント扱いで無視される。
		// 実際のデータはその右隣の列から開始する
		if (data_col >= left_ + 1 + (int)colnames_.size()) return NULL;
		int col = left_ + 1 + data_col;

		// 一番上の行（top_) は開始キーワードとカラム名を書くためにある。
		// 実際のデータ行はその一つ下から始まる
		if (data_row >= bottom_) return NULL;
		int row = top_ + 1 + data_row;

		return excel_.getDataString(sheet_, col, row);
	}
	int getDataInt(int data_col, int data_row, int def) const {
		const char *s = getDataString(data_col, data_row);
		// 実数形式で記述されている値を整数として取り出す可能性
		float f = 0.0f;
		if (K_strtof_try(s, &f)) {
			return (int)f;
		}
		// 8桁の16進数を取り出す可能性。この場合は符号なしで取り出しておかないといけない
		unsigned int u = 0;
		if (K_strtoui_try(s, &u)) {
			return (int)u;
		}
		// 普通の整数として取り出す
		int i = 0;
		if (K_strtoi_try(s, &i)) {
			return i;
		}
		return def;
	}
	float getDataFloat(int data_col, int data_row, float def) const {
		const char *s = getDataString(data_col, data_row);
		float f = def;
		K_strtof_try(s, &f);
		return f;
	}
	bool getDataSource(int data_col, int data_row, int *col_in_file, int *row_in_file) const {
		if (data_col < 0) return false;
		if (data_row < 0) return false;
		if (col_in_file) *col_in_file = left_ + 1 + data_col;
		if (row_in_file) *row_in_file = top_  + 1 + data_row;
		return true;
	}
};




KTable::KTable() {
	impl_ = NULL;
}
bool KTable::empty() const {
	return impl_->empty();
}
bool KTable::loadFromExcelFile(const KExcelFile &excel, const Path &sheetname, const Path &top_cell_text, const Path &btm_cell_text) {
	auto impl = std::make_shared<Impl>();
	if (impl->_loadFromExcelFile(excel)) {
		Path top = (!top_cell_text.empty()) ? top_cell_text : "@BEGIN";
		Path btm = (!btm_cell_text.empty()) ? btm_cell_text : "@END";
		if (impl->_selectTable(sheetname, top, btm)) {
			impl_ = impl;
			return true;
		}
	}
	impl_ = NULL;
	return false;
}
bool KTable::loadFromFile(FileInput &xmls, const Path &filename, const Path &sheetname, const Path &top_cell_text, const Path &btm_cell_text) {
	if (!xmls.empty()) {
		KExcelFile excel;
		if (excel.loadFromFile(xmls, filename)) {
			if (loadFromExcelFile(excel, sheetname, top_cell_text, btm_cell_text)) {
				return true;
			}
		}
	}
	impl_ = NULL;
	return false;
}
bool KTable::loadFromExcelMemory(const void *xlsx_bin, size_t xlsx_size, const Path &filename, const Path &sheetname, const Path &top_cell_text, const Path &btm_cell_text) {
	if (xlsx_bin && xlsx_size > 0) {
		KExcelFile excel;
		if (excel.loadFromMemory(xlsx_bin, xlsx_size, filename)) {
			if (loadFromExcelFile(excel, sheetname, top_cell_text, btm_cell_text)) {
				return true;
			}
		}
	}
	impl_ = NULL;
	return false;
}

int KTable::getDataColIndexByName(const Path &column_name) const {
	if (impl_) {
		return impl_->getDataColIndexByName(column_name);
	}
	return 0;
}
Path KTable::getColumnName(int col) const {
	if (impl_) {
		return impl_->getColumnName(col);
	}
	return Path::Empty;
}
int KTable::getDataColCount() const {
	if (impl_) {
		return impl_->getDataColCount();
	}
	return 0;
}
int KTable::getDataRowCount() const {
	if (impl_) {
		return impl_->getDataRowCount();
	}
	return 0;
}
const char * KTable::getRowMarker(int row) const {
	if (impl_) {
		return impl_->getRowMarker(row);
	}
	return NULL;
}
const char * KTable::getDataString(int col, int row) const {
	if (impl_) {
		return impl_->getDataString(col, row);
	}
	return NULL;
}
int KTable::getDataInt(int col, int row, int def) const {
	if (impl_) {
		return impl_->getDataInt(col, row, def);
	}
	return def;
}
float KTable::getDataFloat(int col, int row, float def) const {
	if (impl_) {
		return impl_->getDataFloat(col, row, def);
	}
	return def;
}
bool KTable::getDataSource(int col, int row, int *col_in_file, int *row_in_file) const {
	if (impl_ && impl_->getDataSource(col, row, col_in_file, row_in_file)) {
		return true;
	}
	return false;
}


} // namespace

