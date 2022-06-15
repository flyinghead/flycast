/*
	Copyright 2022 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "build.h"
#if !defined(__ANDROID__) && !defined(__APPLE__)
#include "http_client.h"

#ifdef _WIN32
#ifndef TARGET_UWP
#include <windows.h>
#include <wininet.h>

namespace http {

static HINTERNET hInet;

void init()
{
	if (hInet == NULL)
		hInet = InternetOpen("Flycast/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
}

int get(const std::string& url, std::vector<u8>& content, std::string& contentType)
{
	HINTERNET hUrl = InternetOpenUrl(hInet, url.c_str(), NULL, 0, INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_AUTH | INTERNET_FLAG_NO_UI, 0);
	if (hUrl == NULL)
	{
		WARN_LOG(NETWORK, "Open URL failed: %lx", GetLastError());
		return 500;
	}

	u8 buffer[4096];
	DWORD bytesRead = sizeof(buffer);
	if (HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_TYPE, buffer, &bytesRead, 0))
		contentType = (const char *)buffer;

	content.clear();
	while (true)
	{
		if (!InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead))
		{
			WARN_LOG(NETWORK, "InternetReadFile failed: %lx", GetLastError());
			InternetCloseHandle(hUrl);
			return 500;
		}
		if (bytesRead == 0)
			break;
		content.insert(content.end(), buffer, buffer + bytesRead);
	}
	InternetCloseHandle(hUrl);

	return 200;
}

void term()
{
	if (hInet != NULL)
	{
		InternetCloseHandle(hInet);
		hInet = NULL;
	}
}

}
#endif	// !TARGET_UWP

#else
#include <curl/curl.h>

namespace http {

void init()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

static size_t receiveData(void *buffer, size_t size, size_t nmemb, std::vector<u8> *recvBuffer)
{
	recvBuffer->insert(recvBuffer->end(), (u8 *)buffer, (u8 *)buffer + size * nmemb);
	return nmemb * size;
}

int get(const std::string& url, std::vector<u8>& content, std::string& contentType)
{
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Flycast/1.0");
	curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	std::vector<u8> recvBuffer;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &recvBuffer);
	CURLcode res = curl_easy_perform(curl);
	long httpCode = 500;
	if (res == CURLE_OK)
	{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		char *ct = nullptr;
		curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
		if (ct != nullptr)
			contentType = ct;
		else
			contentType.clear();
		content = recvBuffer;
	}
	curl_easy_cleanup(curl);

	return (int)httpCode;
}

void term()
{
	curl_global_cleanup();
}

}
#endif	// !_WIN32
#endif	// !defined(__ANDROID__) && !defined(__APPLE__)
