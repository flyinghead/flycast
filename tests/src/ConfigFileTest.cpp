#include "gtest/gtest.h"
#include "types.h"
#include "cfg/cfg.h"
using namespace config;

class ConfigTest : public ::testing::Test {
};

TEST_F(ConfigTest, parseCommandLine)
{
	settings.content.path = "foo";
	const char *argv[10] = { "flycast" };
	parseCommandLine(1, argv);
	ASSERT_TRUE(settings.content.path.empty());

	argv[1] = "bar";
	parseCommandLine(2, argv);
	ASSERT_EQ("bar", settings.content.path);

	argv[1] = "bar.elf";
	parseCommandLine(2, argv);
	ASSERT_EQ("bar.elf", settings.content.path);
	ASSERT_TRUE(isTransient("config", "bios.UseReios"));
	ASSERT_TRUE(loadBool("config", "bios.UseReios"));

	argv[1] = "-config";
	argv[2] = "s:empty=,s:entry=value,s:space=a b c, sect : key = \"quoted text with , and '\" , sect:key2='quoted too with \" and ,'";
	argv[3] = "rom.zip";
	parseCommandLine(4, argv);
	ASSERT_EQ("rom.zip", settings.content.path);
	ASSERT_TRUE(isTransient("s", "empty"));
	ASSERT_EQ("", loadStr("s", "empty"));
	ASSERT_TRUE(isTransient("s", "entry"));
	ASSERT_EQ("value", loadStr("s", "entry"));
	ASSERT_TRUE(isTransient("s", "space"));
	ASSERT_EQ("a b c", loadStr("s", "space"));
	ASSERT_TRUE(isTransient("sect", "key"));
	ASSERT_EQ("quoted text with , and '", loadStr("sect", "key"));
	ASSERT_TRUE(isTransient("sect", "key2"));
	ASSERT_EQ("quoted too with \" and ,", loadStr("sect", "key2"));

	argv[2] = "";
	parseCommandLine(3, argv);
	argv[2] = ",";
	parseCommandLine(3, argv);
	argv[2] = ":,";
	parseCommandLine(3, argv);
	argv[2] = ",:";
	parseCommandLine(3, argv);
	argv[2] = ":=,:";
	parseCommandLine(3, argv);
	argv[2] = ":=,:=";
	parseCommandLine(3, argv);
	argv[2] = ",,";
	parseCommandLine(3, argv);

	argv[1] = "-NSDocumentRevisions";
	argv[2] = "whatever";
	parseCommandLine(3, argv);
	ASSERT_TRUE(settings.content.path.empty());
}
