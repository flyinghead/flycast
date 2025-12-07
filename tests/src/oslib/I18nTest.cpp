#include "gtest/gtest.h"
#include "oslib/i18n.h"
#include <string>

namespace i18n
{
void parseLocale(const std::string& locale, std::string& language, std::string& country, std::string& variant);
}
using namespace i18n;

class I18nTest : public ::testing::Test {
};

TEST_F(I18nTest, parseLocale)
{
	std::string language, country, variant;

	// Unix
	parseLocale("fr", language, country, variant);
	ASSERT_EQ("fr", language);
	ASSERT_EQ("", country);
	ASSERT_EQ("", variant);

	parseLocale("fr_FR", language, country, variant);
	ASSERT_EQ("fr", language);
	ASSERT_EQ("FR", country);
	ASSERT_EQ("", variant);

	parseLocale("fr_FR.UTF-8", language, country, variant);
	ASSERT_EQ("fr", language);
	ASSERT_EQ("FR", country);
	ASSERT_EQ("", variant);

	parseLocale("fr_FR.UTF-8@Argot", language, country, variant);
	ASSERT_EQ("fr", language);
	ASSERT_EQ("FR", country);
	ASSERT_EQ("Argot", variant);

	parseLocale("ca_ES@valencia", language, country, variant);
	ASSERT_EQ("ca", language);
	ASSERT_EQ("ES", country);
	ASSERT_EQ("valencia", variant);

	// Other OSes
	// fr-FR az-Latn-AZ yi
	// macOS: zh-Hans-CH
	parseLocale("fr-FR", language, country, variant);
	ASSERT_EQ("fr", language);
	ASSERT_EQ("FR", country);
	ASSERT_EQ("", variant);

	parseLocale("az-Latn-AZ", language, country, variant);
	ASSERT_EQ("az", language);
	ASSERT_EQ("AZ", country);
	ASSERT_EQ("Latn", variant);

	parseLocale("yi", language, country, variant);
	ASSERT_EQ("yi", language);
	ASSERT_EQ("", country);
	ASSERT_EQ("", variant);

	parseLocale("zh-Hant-TW", language, country, variant);
	ASSERT_EQ("zh", language);
	ASSERT_EQ("TW", country);
	ASSERT_EQ("Hant", variant);
}
