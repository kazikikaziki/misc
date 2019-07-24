#include "KXmlFile.h"
//
#include "KConv.h"
#include "KFile.h"
#include "KNamedValues.h"

#if 1
#	include "KLog.h"
#	define XMLASSERT(x)        Log_assert(x)
#	define XMLERROR(fmt, ...)  Log_errorf(fmt, ##__VA_ARGS__)
#else
#	include <assert.h>
#	include <stdio.h>
#	define XMLASSERT(x)        assert(x)
#	define XMLERROR(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#endif

namespace mo {

static bool TiXml_parse(tinyxml2::XMLDocument &ti_doc_, const char *xml_u8, const Path &debugname, bool mute) {
	// TinyXML は SJIS に対応していない。
	// あらかじめUTF8に変換しておく。また、UTF8-BOM にも対応していないことに注意
	// 末尾が改行文字で終わるようにしておかないとパースエラーになる
	//
	// http://www.grinninglizard.com/tinyxmldocs/
	// For example, Japanese systems traditionally use SHIFT-JIS encoding.
	// Text encoded as SHIFT-JIS can not be read by TinyXML. 
	// A good text editor can import SHIFT-JIS and then save as UTF-8.

	tinyxml2::XMLError xerr = ti_doc_.Parse(KConv::skipBOM(xml_u8));

	if (xerr == tinyxml2::XML_SUCCESS) {
		return true;
	}

	if (!mute) {
		const char *err_name = ti_doc_.ErrorName();
		const char *err_msg = ti_doc_.ErrorStr();
		if (ti_doc_.ErrorLineNum() > 0) {
			// 行番号があるとき
			XMLERROR(u8"XMLドキュメント '%s' の %d 行目で読み取りに失敗しました。\n%s\n%s\n",
				debugname.u8(), ti_doc_.ErrorLineNum(), err_name, err_msg);
		} else {
			// 行番号がないとき（空文字列をパースしようとしたときなど）
			XMLERROR(u8"XMLドキュメント '%s' の読み取りに失敗しました。\n%s\n%s\n",
				debugname.u8(), err_name, err_msg);
		}
	}
	return false;
}


#define HAS_CHAR(str, chr) (strchr(str, chr) != NULL)
bool KXml::stringShouldEscape(const char *s) {
	XMLASSERT(s);
	return HAS_CHAR(s, '<') || HAS_CHAR(s, '>') || HAS_CHAR(s, '"') || HAS_CHAR(s, '\'') || HAS_CHAR(s, '\n');
}

tinyxml2::XMLElement * KXml::parentElement(tinyxml2::XMLElement *elm) {
	XMLASSERT(elm);
	tinyxml2::XMLNode *node = elm->Parent();
	return node ? node->ToElement() : NULL;
}

bool KXml::queryAttributeFloat(const tinyxml2::XMLElement *elm, const char *name, float *val) {
	XMLASSERT(elm);
	return elm->QueryFloatAttribute(name, val) == tinyxml2::XML_SUCCESS;
}

/// XML ノードのタグ文字列が tag と一致するなら true を返す
/// XML の仕様に従い、この関数は大小文字を区別する
/// 引数に NULL が指定された場合は false を返す
bool KXml::hasTag(const tinyxml2::XMLElement *elm, const char *tag) {
	XMLASSERT(elm);
	if (tag == NULL) return false;
	const char *s = elm->Value(); // タグはascii文字のみのハズ。無変換で比較する
	return strcmp(s, tag) == 0;
}
int KXml::getAttributeTable(const tinyxml2::XMLElement *elm, KNamedValues *values) {
	XMLASSERT(elm);
	XMLASSERT(values);
	int cnt = 0;
	for (auto it=elm->FirstAttribute(); it!=NULL; it=it->Next()) {
		values->setString(it->Name(), it->Value());
		cnt++;
	}
	return cnt;
}
std::string KXml::saveElementAsDocumentUtf8(const tinyxml2::XMLElement *e) {
	XMLASSERT(e);

	tinyxml2::XMLDocument outDoc; // 出力用のドキュメント

	// XML宣言を追加
	tinyxml2::XMLDeclaration *outDecl = outDoc.NewDeclaration();
	outDoc.LinkEndChild(outDecl);

	// ノードを再帰的にコピーし、全く同じものを追加する
	tinyxml2::XMLNode * outElm = e->DeepClone(&outDoc);
	outDoc.LinkEndChild(outElm);

	tinyxml2::XMLPrinter printer;
	outDoc.Accept(&printer);
	return printer.CStr();
}

bool KXml::saveElementAsDocument(const tinyxml2::XMLElement *elm, const Path &filename) {
	XMLASSERT(elm);
	XMLASSERT(!filename.empty());

	tinyxml2::XMLDocument outDoc; // 出力用のドキュメント

	// XML宣言を追加
	tinyxml2::XMLDeclaration *outDecl = outDoc.NewDeclaration();
	outDoc.LinkEndChild(outDecl);

	// ノードを再帰的にコピーし、全く同じものを追加する
	tinyxml2::XMLNode * outElm = elm->DeepClone(&outDoc);
	outDoc.LinkEndChild(outElm);

	// ファイルに出力
	// SaveFile は fopen を呼んでいるだけなので、現在のロケールにおけるマルチバイト文字列を渡す必要がある
	std::string filename_mb = filename.toAnsiString(Path::NATIVE_DELIM, "");
	tinyxml2::XMLError err = outDoc.SaveFile(filename_mb.c_str());
	return err == tinyxml2::XML_SUCCESS;
}
tinyxml2::XMLDocument * KXml::createXmlDocument(const char *xml_u8, const Path &name, bool mute) {
	XMLASSERT(xml_u8);
	tinyxml2::XMLDocument *xdoc = new tinyxml2::XMLDocument();
	if (TiXml_parse(*xdoc, xml_u8, name, mute)) {
		return xdoc;
	}
	KXml::deleteXml(xdoc);
	return NULL;
}
tinyxml2::XMLDocument * KXml::createXmlDocumentFromFileName(const Path &filename, bool mute) {
	tinyxml2::XMLDocument *xdoc = NULL;
	FileInput file;
	if (file.open(filename)) {
		std::string text_u8 = KConv::toUtf8(file.readBin());
		if (!text_u8.empty()) {
			tinyxml2::XMLDocument *tmp = new tinyxml2::XMLDocument();
			if (TiXml_parse(*tmp, text_u8.c_str(), filename, mute)) {
				xdoc = tmp;
			} else {
				KXml::deleteXml(tmp);
			}
		}
	}
	return xdoc;
}
void KXml::deleteXml(tinyxml2::XMLDocument *doc) {
	if (doc) delete doc;
}


} // namespace
