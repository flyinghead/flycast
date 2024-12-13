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
#include "stdclass.h"
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

static int post(const std::string& url, const char *headers, const u8 *payload, u32 payloadSize, std::vector<u8>& reply)
{
	char scheme[16], host[256], path[256];
	URL_COMPONENTS components{};
	components.dwStructSize = sizeof(components);
	components.lpszScheme = scheme;
	components.dwSchemeLength = sizeof(scheme) / sizeof(scheme[0]);
	components.lpszHostName = host;
	components.dwHostNameLength = sizeof(host) / sizeof(host[0]);
	components.lpszUrlPath = path;
	components.dwUrlPathLength = sizeof(path) / sizeof(path[0]);

	if (!InternetCrackUrlA(url.c_str(), url.length(), 0, &components))
		return 500;

	bool https = !strcmp(scheme, "https");

	int rc = 500;
	HINTERNET ic = InternetConnect(hInet, host, components.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (ic == NULL)
		return rc;

	HINTERNET hreq = HttpOpenRequest(ic, "POST", path, NULL, NULL, NULL, https ? INTERNET_FLAG_SECURE : 0, 0);
	if (hreq == NULL) {
		InternetCloseHandle(ic);
		return rc;
	}
	if (payloadSize > 0)
	{
		char clen[128];
		snprintf(clen, sizeof(clen), "Content-Length: %d\r\n", payloadSize);
		HttpAddRequestHeaders(hreq, clen, -1L, HTTP_ADDREQ_FLAG_ADD_IF_NEW);
	}
	if (!HttpSendRequest(hreq, headers, -1, (void *)payload, payloadSize))
		WARN_LOG(NETWORK, "HttpSendRequest Error %d", GetLastError());
	else
	{
		DWORD status;
		DWORD size = sizeof(status);
		DWORD index = 0;
		if (!HttpQueryInfo(hreq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &size, &index))
			WARN_LOG(NETWORK, "HttpQueryInfo Error %d", GetLastError());
		else
		{
			rc = status;
			reply.clear();
			u8 buffer[4096];
			DWORD bytesRead = sizeof(buffer);
			while (true)
			{
				if (!InternetReadFile(hreq, buffer, sizeof(buffer), &bytesRead))
				{
					WARN_LOG(NETWORK, "InternetReadFile failed: %lx", GetLastError());
					InternetCloseHandle(hreq);
					rc = 500;
					break;
				}
				if (bytesRead == 0)
					break;
				reply.insert(reply.end(), buffer, buffer + bytesRead);
			}
		}
	}

	InternetCloseHandle(hreq);
	InternetCloseHandle(ic);

	return rc;
}

int post(const std::string& url, const char *payload, const char *contentType, std::vector<u8>& reply)
{
	char buf[512];
	if (contentType != nullptr) {
		sprintf(buf, "Content-Type: %s", contentType);
		contentType = buf;
	}
	return post(url, contentType, (const u8 *)payload, strlen(payload), reply);
}

int post(const std::string& url, const std::vector<PostField>& fields)
{
	static const std::string boundary("----flycast-boundary-8304529454");

	std::string content;
	for (const PostField& field : fields)
	{
		content += "--" + boundary + "\r\n";
		content += "Content-Disposition: form-data; name=\"" + field.name + '"';
		if (!field.contentType.empty())
		{
			size_t pos = get_last_slash_pos(field.value);
			std::string filename;
			if (pos == std::string::npos)
				filename = field.value;
			else
				filename = field.value.substr(pos + 1);
			content += "; filename=\"" + filename + '"';
		}
		content += "\r\n";
		if (!field.contentType.empty())
			content += "Content-Type: " + field.contentType + "\r\n";
		content += "\r\n";

		if (field.contentType.empty())
		{
			content += field.value;
		}
		else
		{
			FILE *f = nowide::fopen(field.value.c_str(), "rb");
			if (f == nullptr) {
				WARN_LOG(NETWORK, "Can't open mime file %s", field.value.c_str());
				return 500;
			}
			fseek(f, 0, SEEK_END);
			size_t size = ftell(f);
			fseek(f, 0, SEEK_SET);
			std::vector<char> data;
			data.resize(size);
			size_t read = fread(data.data(), 1, size, f);
			if (read != size)
			{
				fclose(f);
				WARN_LOG(NETWORK, "Truncated read on mime file %s: %d -> %d", field.value.c_str(), (int)size, (int)read);
				return 500;
			}
			fclose(f);
			content += std::string(data.data(), size);
		}
		content += "\r\n";
	}
	content += "--" + boundary + "--\r\n";

	std::vector<u8> reply;
	std::string header("Content-Type: multipart/form-data; boundary=" + boundary);
	return post(url, header.c_str(), (const u8 *)&content[0], content.length(), reply);
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
#ifdef __vita__
#include <vitasdk.h>
void *net_mem;
#endif
#include <curl/curl.h>

namespace http {

void init()
{
#ifdef __vita__
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	int ret = sceNetShowNetstat();
	SceNetInitParam initparam;
	if (ret == SCE_NET_ERROR_ENOTINIT) {
		initparam.memory = net_mem = malloc(141 * 1024);
		initparam.size = 141 * 1024;
		initparam.flags = 0;
		sceNetInit(&initparam);
	}
#else
	curl_global_init(CURL_GLOBAL_ALL);
#endif
}

static size_t receiveData(void *buffer, size_t size, size_t nmemb, std::vector<u8> *recvBuffer)
{
	recvBuffer->insert(recvBuffer->end(), (u8 *)buffer, (u8 *)buffer + size * nmemb);
	return nmemb * size;
}

static CURL *makeCurlEasy(const std::string& url)
{
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Flycast/1.0");
	curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

#ifdef __vita__
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
#endif
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	return curl;
}

int get(const std::string& url, std::vector<u8>& content, std::string& contentType)
{
	CURL *curl = makeCurlEasy(url);

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

int post(const std::string& url, const char *payload, const char *contentType, std::vector<u8>& reply)
{
#ifndef __vita__
	CURL *curl = makeCurlEasy(url);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
	curl_slist *headers = nullptr;
	if (contentType != nullptr)
	{
		headers = curl_slist_append(headers, ("Content-Type: " + std::string(contentType)).c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	std::vector<u8> recvBuffer;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &recvBuffer);
	CURLcode res = curl_easy_perform(curl);

	long httpCode = 500;
	if (res == CURLE_OK)
	{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		reply = recvBuffer;
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return (int)httpCode;
#else
	return 500;
#endif
}

int post(const std::string& url, const std::vector<PostField>& fields)
{
#ifndef __vita__
	CURL *curl = makeCurlEasy(url);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	curl_mime *mime = curl_mime_init(curl);
	for (const auto& field : fields)
	{
		curl_mimepart *part = curl_mime_addpart(mime);
		curl_mime_name(part, field.name.c_str());
		if (field.contentType.empty()) {
			curl_mime_data(part, field.value.c_str(), CURL_ZERO_TERMINATED);
		}
		else {
			curl_mime_filedata(part, field.value.c_str());
			curl_mime_type(part, field.contentType.c_str());
		}
	}

	curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

	CURLcode res = curl_easy_perform(curl);

	long httpCode = 500;
	if (res == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup(curl);

	return (int)httpCode;
#else
	return 500;
#endif
}

void term()
{
#ifdef __vita__
	sceNetTerm();
	free(net_mem);
#else
	curl_global_cleanup();
#endif
}

}
#endif	// !_WIN32
#endif	// !defined(__ANDROID__) && !defined(__APPLE__)
