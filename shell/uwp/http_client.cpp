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

namespace http {

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Web::Http::Headers;

static HttpClient httpClient{ nullptr };

void init()
{
	nowide::wstackstring wagent;
	wagent.convert(getUserAgent().c_str());

	httpClient = HttpClient();
	httpClient.DefaultRequestHeaders().UserAgent().ParseAdd(wagent.get());
}

void term() {
	httpClient = nullptr;
}

int get(const std::string& url, std::vector<u8>& content, std::string& contentType)
{
	nowide::wstackstring wurl;
	if (!wurl.convert(url.c_str()))
		return 500;
	try
	{
		Uri uri(wurl.get());
		HttpResponseMessage resp = httpClient.GetAsync(uri).get();

		if (resp.IsSuccessStatusCode())
		{
			IHttpContent httpContent = resp.Content();
			HttpMediaTypeHeaderValue contentTypeHeader = httpContent.Headers().ContentType();
			if (contentTypeHeader != nullptr)
			{
				winrt::hstring mediaType = contentTypeHeader.MediaType();
				nowide::stackstring mtype;
				mtype.convert(mediaType.c_str());
				contentType = mtype.get();
			}

			IBuffer buffer = httpContent.ReadAsBufferAsync().get();
			content.resize(buffer.Length());
			if (buffer.Length() > 0)
			{
				std::memcpy(content.data(), buffer.data(), buffer.Length());
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

		HttpResponseMessage resp = httpClient.PostAsync(uri, contentStr).get();

		if (resp.IsSuccessStatusCode())
		{
			IHttpContent httpContent = resp.Content();
			IBuffer buffer = httpContent.ReadAsBufferAsync().get();
			reply.resize(buffer.Length());
			if (buffer.Length() > 0)
			{
				std::memcpy(reply.data(), buffer.data(), buffer.Length());
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
	// not implemented
	return 500;
}

} // namespace http
