#include "gtest/gtest.h"
#include "oslib/http_client.h"

class HttpTest : public ::testing::Test
{
};

TEST_F(HttpTest, test_urlencode)
{
	EXPECT_EQ("", http::urlEncode(""));
	EXPECT_EQ("Hello", http::urlEncode("Hello"));
	EXPECT_EQ("Hello%20World", http::urlEncode("Hello World"));
	EXPECT_EQ("1.2-57_alpha~", http::urlEncode("1.2-57_alpha~"));
	EXPECT_EQ("%3Chtml%20value%3D%221%22%3E", http::urlEncode("<html value=\"1\">"));

	EXPECT_EQ("%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F", http::urlEncode("!\"#$%&'()*+,-./"));
	EXPECT_EQ("0123456789", http::urlEncode("0123456789"));
	EXPECT_EQ("%3A%3B%3C%3D%3E%3F%40", http::urlEncode(":;<=>?@"));
	EXPECT_EQ("ABCDEFGHIJKLMNOPQRSTUVWXYZ", http::urlEncode("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
	EXPECT_EQ("%5B%5C%5D%5E_%60", http::urlEncode("[\\]^_`"));
	EXPECT_EQ("abcdefghijklmnopqrstuvwxyz", http::urlEncode("abcdefghijklmnopqrstuvwxyz"));
	EXPECT_EQ("%7B%7C%7D~%7F", http::urlEncode("{|}~\x7f"));
}

TEST_F(HttpTest, test_urldecode)
{
	EXPECT_EQ("", http::urlDecode(""));
	EXPECT_EQ("0123456789", http::urlDecode("0123456789"));
	EXPECT_EQ("ABCDEFGHIJKLMNOPQRSTUVWXYZ", http::urlDecode("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
	EXPECT_EQ("abcdefghijklmnopqrstuvwxyz", http::urlDecode("abcdefghijklmnopqrstuvwxyz"));
	EXPECT_EQ("!\"#$%&'()*+,-./", http::urlDecode("%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F"));
	for (char c = ' '; c > 0 && c <= '\x7f'; c++)
	{
		char cs[4];
		sprintf(cs, "%%%02X", c);
		EXPECT_EQ(std::string(1, c), http::urlDecode(cs));
	}
	EXPECT_EQ("Hello World", http::urlDecode("Hello+World"));

	// not encoded
	EXPECT_EQ("!\"#$&'()*,-./", http::urlDecode("!\"#$&'()*,-./"));
	EXPECT_EQ(":;<=>?@[\\]^_`{|}~\x7f", http::urlDecode(":;<=>?@[\\]^_`{|}~\x7f"));
}
