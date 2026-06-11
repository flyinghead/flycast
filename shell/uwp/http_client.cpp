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
#include "oslib/http_client.h"
#include "stdclass.h"
#include <windows.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <mutex>

namespace http {

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Web::Http::Headers;

static HttpClient httpClient{ nullptr };
static std::mutex httpMutex;

void init()
{
	std::lock_guard<std::mutex> lock(httpMutex);
	if (httpClient)
		return;

	nowide::wstackstring wagent;
	wagent.convert(getUserAgent().c_str());

	httpClient = HttpClient();
	httpClient.DefaultRequestHeaders().UserAgent().ParseAdd(wagent.get());
}

void term() {
	std::lock_guard<std::mutex> lock(httpMutex);
	httpClient = nullptr;
}

static void getHeaders(IIterator<IKeyValuePair<hstring, hstring>> it, Headers& headers)
{
	while (it.HasCurrent())
	{
		hstring key = it.Current().Key();
		hstring value = it.Current().Value();
		nowide::stackstring nwkey;
		nowide::stackstring nwvalue;
		if (nwkey.convert(key.c_str()) && nwvalue.convert(value.c_str()))
		{
			std::string strkey(nwkey.get());
			string_tolower(strkey);
			headers.emplace_back(strkey, nwvalue.get());
		}
		it.MoveNext();
	}
}

int get(const std::string& url, std::vector<u8>& content, const Headers *reqHeaders, Headers *respHeaders)
{
	HttpClient client{ nullptr };
	{
		std::lock_guard<std::mutex> lock(httpMutex);
		client = httpClient;
	}
	if (!client)
		return 500;

	nowide::wstackstring wurl;
	if (!wurl.convert(url.c_str()))
		return 500;
	try
	{
		Uri uri(wurl.get());
		HttpRequestMessage request(HttpMethod::Get(), uri);

		if (reqHeaders != nullptr)
		{
			for (const auto& [ name, value ] : *reqHeaders)
			{
				nowide::wstackstring wname;
				nowide::wstackstring wvalue;
				if (wname.convert(name.c_str()) && wvalue.convert(value.c_str()))
					request.Headers().Insert(wname.get(), wvalue.get());
			}
		}

		auto op = client.SendRequestAsync(request);
		auto status = op.wait_for(std::chrono::seconds(30));
		if (status == AsyncStatus::Started)
		{
			op.Cancel();
			return 408;
		}
		if (status != AsyncStatus::Completed)
			return 500;

		HttpResponseMessage resp = op.GetResults();

		if (resp.IsSuccessStatusCode())
		{
			IHttpContent httpContent = resp.Content();

			if (respHeaders != nullptr) {
				getHeaders(resp.Headers().First(), *respHeaders);
				getHeaders(httpContent.Headers().First(), *respHeaders);
			}

			auto readOp = httpContent.ReadAsBufferAsync();
			status = readOp.wait_for(std::chrono::seconds(30));
			if (status == AsyncStatus::Started)
			{
				readOp.Cancel();
				return 408;
			}
			if (status != AsyncStatus::Completed)
				return 500;

			IBuffer buffer = readOp.GetResults();

			content.resize(buffer.Length());
			if (buffer.Length() > 0)
			{
				DataReader reader = DataReader::FromBuffer(buffer);
				reader.ReadBytes(content);
			}
		}
		return (int)resp.StatusCode();
	}
	catch (hresult_error const& e)
	{
		WARN_LOG(COMMON, "http::get error %ls", e.message().c_str());
		return 500;
	}
}

int post(const std::string& url, const char *payload, const char *contentType, std::vector<u8>& reply)
{
	HttpClient client{ nullptr };
	{
		std::lock_guard<std::mutex> lock(httpMutex);
		client = httpClient;
	}
	if (!client)
		return 500;

	nowide::wstackstring wurl;
	if (!wurl.convert(url.c_str()))
		return 500;
	nowide::wstackstring wpayload;
	if (!wpayload.convert(payload))
		return 500;
	nowide::wstackstring wcontentType;
	if (contentType != nullptr && !wcontentType.convert(contentType))
		return 500;
	try
	{
		Uri uri(wurl.get());
		HttpStringContent contentStr(wpayload.get());
		contentStr.Headers().ContentLength(strlen(payload));
		if (contentType != nullptr)
			contentStr.Headers().ContentType(HttpMediaTypeHeaderValue(wcontentType.get()));

		auto op = client.PostAsync(uri, contentStr);
		auto status = op.wait_for(std::chrono::seconds(30));
		if (status == AsyncStatus::Started)
		{
			op.Cancel();
			return 408;
		}
		if (status != AsyncStatus::Completed)
			return 500;

		HttpResponseMessage resp = op.GetResults();

		if (resp.IsSuccessStatusCode())
		{
			IHttpContent httpContent = resp.Content();
			auto readOp = httpContent.ReadAsBufferAsync();
			status = readOp.wait_for(std::chrono::seconds(30));
			if (status == AsyncStatus::Started)
			{
				readOp.Cancel();
				return 408;
			}
			if (status != AsyncStatus::Completed)
				return 500;

			IBuffer buffer = readOp.GetResults();
			reply.resize(buffer.Length());
			if (buffer.Length() > 0)
			{
				DataReader reader = DataReader::FromBuffer(buffer);
				reader.ReadBytes(reply);
			}
		}
		return (int)resp.StatusCode();
	}
	catch (hresult_error const& e)
	{
		WARN_LOG(COMMON, "http::post error %ls", e.message().c_str());
		return 500;
	}
}

int post(const std::string & url, const std::vector<PostField>&fields) {
	// not implemented (used by sentry minidump upload)
	return 500;
}

} // namespace http
