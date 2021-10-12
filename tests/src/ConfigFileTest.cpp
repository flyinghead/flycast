#include "gtest/gtest.h"
#include "types.h"
#include "cfg/ini.h"

class ConfigFileTest : public ::testing::Test {
};

TEST_F(ConfigFileTest, TestLoadSave)
{
	using namespace emucfg;
	ConfigFile file;
	file.set("", "prop1", "value1");
	file.set_int("", "prop2", 2);
	file.set_bool("", "prop3", true);
	ASSERT_EQ("value1", file.get("", "prop1", ""));
	ASSERT_EQ(2, file.get_int("", "prop2", 0));
	ASSERT_TRUE(file.get_bool("", "prop3", false));

	FILE *fp = fopen("test.cfg", "w");
	file.save(fp);
	fclose(fp);
	fp = fopen("test.cfg", "r");
	char buf[1024];
	int l = fread(buf, 1, sizeof(buf) - 1, fp);
	buf[l] = '\0';
	fclose(fp);
	ASSERT_EQ("prop1 = value1\nprop2 = 2\nprop3 = yes\n", std::string(buf));

	fp = fopen("test.cfg", "r");
	file = {};
	file.parse(fp);
	fclose(fp);
	ASSERT_EQ("value1", file.get("", "prop1", ""));
	ASSERT_EQ(2, file.get_int("", "prop2", 0));
	ASSERT_TRUE(file.get_bool("", "prop3", false));
}

TEST_F(ConfigFileTest, TestQuotes)
{
	using namespace emucfg;
	FILE *fp = fopen("test.cfg", "w");
	fprintf(fp, "propWithQuotes=\"value with quotes\"\n");
	fprintf(fp, "propWithQuotes2=\"42\"\n");
	fprintf(fp, "propWithQuotes3=\"true\"\n");
	fclose(fp);
	fp = fopen("test.cfg", "r");
	ConfigFile file;
	file.parse(fp);
	fclose(fp);
	ASSERT_EQ("value with quotes", file.get("", "propWithQuotes", ""));
	ASSERT_EQ(42, file.get_int("", "propWithQuotes2", 0));
	ASSERT_TRUE(file.get_bool("", "propWithQuotes3", false));
}

TEST_F(ConfigFileTest, TestTrim)
{
	using namespace emucfg;
	FILE *fp = fopen("test.cfg", "w");
	fprintf(fp, "   prop   =    \"value 1 \"     \n\n\n");
	fprintf(fp, " prop2 = 42     \n");
	fprintf(fp, " prop3 = yes   \r\n\n");
	fclose(fp);
	fp = fopen("test.cfg", "r");
	ConfigFile file;
	file.parse(fp);
	fclose(fp);
	ASSERT_EQ("value 1 ", file.get("", "prop", ""));
	ASSERT_EQ(42, file.get_int("", "prop2", 0));
	ASSERT_TRUE(file.get_bool("", "prop3", false));
}

TEST_F(ConfigFileTest, TestLoadSaveSection)
{
	using namespace emucfg;
	ConfigFile file;
	file.set("sect1", "prop1", "value1");
	file.set_int("sect2", "prop2", 2);
	file.set_bool("sect2", "prop3", true);
	ASSERT_EQ("value1", file.get("sect1", "prop1", ""));
	ASSERT_EQ(2, file.get_int("sect2", "prop2", 0));
	ASSERT_TRUE(file.get_bool("sect2", "prop3", false));

	FILE *fp = fopen("test.cfg", "w");
	file.save(fp);
	fclose(fp);
	fp = fopen("test.cfg", "r");
	char buf[1024];
	int l = fread(buf, 1, sizeof(buf) - 1, fp);
	buf[l] = '\0';
	fclose(fp);
	ASSERT_EQ("[sect1]\nprop1 = value1\n\n[sect2]\nprop2 = 2\nprop3 = yes\n\n", std::string(buf));

	fp = fopen("test.cfg", "r");
	file = {};
	file.parse(fp);
	fclose(fp);
	ASSERT_EQ("value1", file.get("sect1", "prop1", ""));
	ASSERT_EQ(2, file.get_int("sect2", "prop2", 0));
	ASSERT_TRUE(file.get_bool("sect2", "prop3", false));
}

