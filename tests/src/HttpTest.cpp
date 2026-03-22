#include "gtest/gtest.h"
#include "oslib/http_client.h"
#include "json.hpp"
#include <vector>
#include <curl/curl.h>

using namespace nlohmann;

class HttpTest : public ::testing::Test
{
public:
	static void SetUpTestSuite()
	{
		http::init();
		std::vector<u8> reply;
		int rc = http::post("https://www.postb.in/api/bin", "", nullptr, reply);
		if (rc != 201)
			throw std::runtime_error("http bin creation failed");
		std::string s((const char *)&reply[0], reply.size());
		json v = json::parse(s);
		binId = v.at("binId").get<std::string>();
		testUrl = "https://www.postb.in/" + binId;
		//printf("api/bin created: %s\n", binId.c_str());
	}

	static size_t nullCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
		return size * nmemb;
	}

	static void TearDownTestSuite()
	{
		if (!binId.empty())
		{
			CURL *curl = curl_easy_init();
			std::string url = "https://www.postb.in/api/bin/" + binId;
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullCallback);

			CURLcode res = curl_easy_perform(curl);

			long httpCode = 500;
			if (res == CURLE_OK)
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
			curl_easy_cleanup(curl);
			//printf("api/bin deleted: status %ld\n", httpCode);
		}
	}

	static json getLastRequest()
	{
		std::string url = "https://www.postb.in/api/bin/" + binId + "/req/shift";
		std::vector<u8> content;
		std::string contentType;
		http::get(url, content, contentType);
		std::string s((const char *)&content[0], content.size());
		//printf(">>>: %s\n", s.c_str());
		return json::parse(s);
	}

protected:
	 static std::string binId;
	 static std::string testUrl;
};
std::string HttpTest::binId;
std::string HttpTest::testUrl;

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

TEST_F(HttpTest, get)
{
	std::vector<u8> content;
	std::string contentType;
	int rc = http::get(testUrl, content, contentType);
	ASSERT_EQ(200, rc);
	ASSERT_FALSE(content.empty());
	ASSERT_EQ("text/plain", contentType.substr(0, 10));
	std::string reqId((const char *)&content[0], content.size());

	json req = getLastRequest();
	ASSERT_EQ("GET", req.at("method"));
	std::string ua = req.at("headers").at("user-agent").get<std::string>();
	ASSERT_EQ("Flycast/", ua.substr(0, 8));
	ASSERT_EQ(req.at("reqId"), reqId);
}

TEST_F(HttpTest, getHeaders)
{
	std::vector<u8> content;
	const http::Headers reqHeaders = { { "Test-Header", "test-value" } };
	http::Headers respHeaders;
	int rc = http::get(testUrl, content, &reqHeaders, &respHeaders);
	ASSERT_EQ(200, rc);
	ASSERT_FALSE(content.empty());
	auto it = std::find_if(respHeaders.begin(), respHeaders.end(), [](std::pair<std::string, std::string>& v) { return v.first == "content-type"; });
	ASSERT_NE(respHeaders.end(), it);
	ASSERT_EQ("text/plain", it->second.substr(0, 10));
	std::string reqId((const char *)&content[0], content.size());

	json req = getLastRequest();
	ASSERT_EQ("GET", req.at("method"));
	std::string ua = req.at("headers").at("user-agent").get<std::string>();
	ASSERT_EQ("Flycast/", ua.substr(0, 8));
	ASSERT_EQ(req.at("reqId"), reqId);
	std::string v = req.at("headers").at("test-header").get<std::string>();
	ASSERT_EQ("test-value", v);
}

TEST_F(HttpTest, postFields)
{
	std::vector<http::PostField> fields;
	fields.emplace_back("field1", "value1");
	fields.emplace_back("field2", "value2");
	fields.emplace_back("fileField", FLYCAST_TEST_FILES "/test_cues/d/cs.cue", "text/plain");
	int rc = http::post(testUrl, fields);
	ASSERT_EQ(200, rc);

	json req = getLastRequest();
	ASSERT_EQ("POST", req.at("method"));
	std::string ua = req.at("headers").at("user-agent").get<std::string>();
	ASSERT_EQ("Flycast/", ua.substr(0, 8));
	std::string contentType = req.at("headers").at("content-type").get<std::string>();
	ASSERT_EQ("multipart/form-data; boundary=", contentType.substr(0, 30));
	// postb.in doesn't seem to save the request body so we can't check the form fields? :(
}

TEST_F(HttpTest, postContent)
{
	std::vector<u8> content;
	int rc = http::post(testUrl, "This is the payload", "text/plain; charset=x-sjis", content);
	ASSERT_EQ(200, rc);
	ASSERT_FALSE(content.empty());
	std::string reqId((const char *)&content[0], content.size());

	json req = getLastRequest();
	ASSERT_EQ("POST", req.at("method"));
	std::string ua = req.at("headers").at("user-agent").get<std::string>();
	ASSERT_EQ("Flycast/", ua.substr(0, 8));
	std::string contentType = req.at("headers").at("content-type").get<std::string>();
	ASSERT_EQ("text/plain; charset=x-sjis", contentType);
	// can't check the payload for the same reason
}
