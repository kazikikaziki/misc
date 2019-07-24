#pragma once
#include "tinyxml/tinyxml2.h"
#include <string>

namespace mo {

class KNamedValues;
class Path;

/// TinyXML2 のためのヘルパー関数群
///
/// TinyXML-2
/// http://leethomason.github.io/tinyxml2/
///
class KXml {
public:
	static bool hasTag(const tinyxml2::XMLElement *elm, const char *tag);
	static int  getAttributeTable(const tinyxml2::XMLElement *elm, KNamedValues *values);
	static bool saveElementAsDocument(const tinyxml2::XMLElement *elm, const Path &filename);
	static std::string saveElementAsDocumentUtf8(const tinyxml2::XMLElement *e);
	static bool queryAttributeFloat(const tinyxml2::XMLElement *elm, const char *name, float *val);
	static tinyxml2::XMLElement * parentElement(tinyxml2::XMLElement *elm);
	static bool stringShouldEscape(const char *s);

	/// XML文字列からXMLオブジェクトを作成する。成功すれば非NULLを返す。
	/// ファイルが見つからない、ロード中にエラーが発生したなどの場合は NULL を返す。
	/// 得られたオブジェクトは deleteXml() で解放する
	static tinyxml2::XMLDocument * createXmlDocument(const char *xml_u8, const Path &name, bool mute=false);

	/// ファイルからXMLオブジェクトを作成する。成功すれば非NULLを返す。
	/// ファイルが見つからない、ロード中にエラーが発生したなどの場合は NULL を返す。
	/// 得られたオブジェクトは deleteXml() で解放する
	static tinyxml2::XMLDocument * createXmlDocumentFromFileName(const Path &filename, bool mute=false);

	/// XMLオブジェクトを解放する
	static void deleteXml(tinyxml2::XMLDocument *doc);

}; // KXml

} // namespace

