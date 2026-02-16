#include "gtest/gtest.h"
#include "cfg/ini.h"
#include "types.h"
#include <locale>
using namespace config;

class IniFileTest : public ::testing::Test {
};

TEST_F(IniFileTest, load)
{
	IniFile ini;
	ini.load(" [section1] \n"
			"\n"
			"\n"
			"   ; comment\n"
			"entry1=\n"
			" entry2 = value2\n"
			"\tentry3\t=  value3 \n");
	ASSERT_TRUE(ini.hasSection("section1"));
	ASSERT_TRUE(ini.hasEntry("section1", "entry1"));
	ASSERT_FALSE(ini.hasSection("section2"));
	ASSERT_FALSE(ini.hasEntry("section1", "entry4"));

	ASSERT_EQ("", ini.getString("section1", "entry1"));
	ASSERT_EQ("value2", ini.getString("section1", "entry2"));
	ASSERT_EQ(" value3 ", ini.getString("section1", "entry3"));
}

TEST_F(IniFileTest, save)
{
	IniFile ini;
	ini.set("section1", "entry1", "value1");
	ini.set("section1", "entry2", " value2 ");
	ini.set("section1", "entry3", " ");
	ini.set("section1", "entry4", "");
	ini.set("section2", "entry1", true);
	ini.set("section2", "entry2", false);
	ini.set("section2", "entry3", 42);
	ini.set("section2", "entry4", 42ll);
	ini.set("section2", "entry5", 3.141565f);

	std::string data;
	ini.save(data);
	ASSERT_EQ("[section1]\n"
			"entry1 = value1\n"
			"entry2 =  value2 \n"
			"entry3 =  \nentry4 = \n"
			"\n"
			"[section2]\n"
			"entry1 = yes\n"
			"entry2 = no\n"
			"entry3 = 42\n"
			"entry4 = 42\n"
			"entry5 = 3.141565\n"
			"\n", data);
}

TEST_F(IniFileTest, getSet)
{
	std::locale::global(std::locale("fr_FR.UTF-8"));
	IniFile ini;
	ini.set("s", "e", 1);
	ASSERT_EQ(1, ini.getInt("s", "e"));
	ASSERT_EQ(1, ini.getInt64("s", "e"));
	ASSERT_TRUE(ini.getBool("s", "e"));
	ASSERT_EQ(1.f, ini.getFloat("s", "e"));
	ASSERT_EQ("1", ini.getString("s", "e"));

	ini.deleteEntry("s", "e");
	ASSERT_EQ("nada", ini.getString("s", "e", "nada"));

	ini.set("s", "e", true);
	ASSERT_TRUE(ini.getBool("s", "e"));
	ASSERT_EQ("yes", ini.getString("s", "e"));

	ini.set("s", "e", 8_GB);
	ASSERT_EQ(8_GB, ini.getInt64("s", "e"));

	ini.set("s", "e", 1.732f);
	ASSERT_EQ(1.732f, ini.getFloat("s", "e"));
	ASSERT_EQ("1.732", ini.getString("s", "e"));

	ini.set("s", "e", " \t+1.414 \t");
	ASSERT_EQ(1.414f, ini.getFloat("s", "e"));

	ini.set("s", "e", "  0XFFFF ");
	ASSERT_EQ(0xffff, ini.getInt("s", "e"));
	ASSERT_EQ(0xffff, ini.getInt64("s", "e"));

	ini.set("s", "e", "-123456");
	ASSERT_EQ(-123456, ini.getInt("s", "e"));
	ASSERT_EQ(-123456, ini.getInt64("s", "e"));

	ini.set("s", "e", "+420");
	ASSERT_EQ(420, ini.getInt("s", "e"));
	ASSERT_EQ(420, ini.getInt64("s", "e"));

	ini.set("s", "e", " \t ");
	ASSERT_EQ(0, ini.getInt("s", "e"));
	ASSERT_EQ(0, ini.getInt64("s", "e"));
	ASSERT_EQ(0.f, ini.getFloat("s", "e"));
	ASSERT_FALSE(ini.getBool("s", "e"));

	ini.set("s", "e", "");
	ASSERT_EQ(0, ini.getInt("s", "e"));
	ASSERT_EQ(0, ini.getInt64("s", "e"));
	ASSERT_EQ(0.f, ini.getFloat("s", "e"));
	ASSERT_FALSE(ini.getBool("s", "e"));

	ini.set("s", "e", "1 000");
	ASSERT_EQ(1, ini.getInt("s", "e"));
	ASSERT_EQ(1, ini.getInt64("s", "e"));
}

TEST_F(IniFileTest, transient)
{
	IniFile ini;
	ini.set("s", "e", 42);
	ASSERT_FALSE(ini.isTransient("s", "e"));
	ini.set("s", "e", 43, true);
	ASSERT_TRUE(ini.isTransient("s", "e"));
	// this is pretty much a nop
	ini.set("s", "e", 42);
	// transient values hide non-transient ones
	ASSERT_TRUE(ini.isTransient("s", "e"));
	ASSERT_EQ(43, ini.getInt("s", "e"));
}

TEST_F(IniFileTest, errors)
{
	IniFile ini;
	// Empty section name is valid (implicit or explicit)
	// Empty entry name isn't
	ini.load("entry1 = value1\n [] \nthis = that\n[section]\n \t  =value\nthis2 = that2");
	ASSERT_TRUE(ini.hasEntry("", "entry1"));
	ASSERT_TRUE(ini.hasSection(""));
	ASSERT_TRUE(ini.hasEntry("", "this"));
	ASSERT_TRUE(ini.hasSection("section"));
	ASSERT_FALSE(ini.hasEntry("section", ""));
	ASSERT_FALSE(ini.hasEntry("section", "\t"));
}

TEST_F(IniFileTest, quotes)
{
	std::string content = "propWithQuotes=\"value with quotes\"\n"
						"propWithQuotes2=\"42\"\n"
						"propWithQuotes3=\"true\"\n";
	IniFile file;
	file.load(content);
	ASSERT_EQ("value with quotes", file.getString("", "propWithQuotes", ""));
	ASSERT_EQ(42, file.getInt("", "propWithQuotes2", 0));
	ASSERT_TRUE(file.getBool("", "propWithQuotes3", false));
}

TEST_F(IniFileTest, loadSaveFile)
{
	IniFile file;
	file.set("", "root", 42);
	file.set("s1", "e1", "e1");
	file.set("s1", "e2", true);
	file.set("s1", "e3", 3.14f);
	FILE *f = fopen("test.cfg", "wb");
	file.save(f);
	fclose(f);
	file = {};
	f = fopen("test.cfg", "rb");
	file.load(f);
	fclose(f);
	ASSERT_EQ(42, file.getInt("", "root"));
	ASSERT_EQ("e1", file.get("s1", "e1"));
	ASSERT_TRUE(file.getBool("s1", "e2"));
	ASSERT_EQ(3.14f, file.getFloat("s1", "e3"));
}

TEST_F(IniFileTest, cEscapeSequence)
{
	IniFile ini;
	std::string content = "[sec\\ntion]\n"
			"\\tentry=value\\r\n"
			"entry\\f=\\\\value\n";
	ini.load(content, true);
	ASSERT_EQ("value\r", ini.get("sec\ntion", "\tentry"));
	ASSERT_EQ("\\value", ini.get("sec\ntion", "entry\f"));
}
