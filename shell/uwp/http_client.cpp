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

namespace http {

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;

static HttpClient^ httpClient;

void init()
{
	nowide::wstackstring wagent;
	wagent.convert(getUserAgent().c_str());

	httpClient = ref new HttpClient();
	httpClient->DefaultRequestHeaders->UserAgent->ParseAdd(ref new String(wagent.get()));
}

void term() {
	httpClient = nullptr;
}

static void getHeaders(IIterator<IKeyValuePair<String^, String^>^>^ it, Headers& headers)
{
	while (it->HasCurrent)
	{
		String^ key = it->Current->Key;
		String^ value = it->Current->Value;
		nowide::stackstring nwkey;
		nowide::stackstring nwvalue;
		if (nwkey.convert(key->Data()) && nwvalue.convert(value->Data()))
		{
			std::string strkey(nwkey.get());
			string_tolower(strkey);
			headers.emplace_back(strkey, nwvalue.get());
		}
		it->MoveNext();
	}
}

int get(const std::string& url, std::vector<u8>& content, const Headers *reqHeaders, Headers *respHeaders)
{
	nowide::wstackstring wurl;
	if (!wurl.convert(url.c_str()))
		return 500;
	try
	{
		Uri^ uri = ref new Uri(ref new String(wurl.get()));
		HttpRequestMessage^ request = ref new HttpRequestMessage(HttpMethod::Get, uri);

		if (reqHeaders != nullptr)
		{
			for (const auto& [ name, value ] : *reqHeaders)
			{
				nowide::wstackstring wname;
				nowide::wstackstring wvalue;
				if (wname.convert(name.c_str()) && wvalue.convert(value.c_str()))
					request->Headers->Insert(ref new String(wname.get()), ref new String(wvalue.get()));
			}
		}

		IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^ op = httpClient->SendRequestAsync(request);
		cResetEvent asyncEvent;
		op->Completed = ref new AsyncOperationWithProgressCompletedHandler<HttpResponseMessage^, HttpProgress>(
				[&asyncEvent](IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^, Windows::Foundation::AsyncStatus) {
					asyncEvent.Set();
		        });
		if (!asyncEvent.Wait(30000))
			return 408;
		HttpResponseMessage^ resp = op->GetResults();

		if (resp->IsSuccessStatusCode)
		{
			IHttpContent^ httpContent = resp->Content;

			if (respHeaders != nullptr) {
				getHeaders(resp->Headers->First(), *respHeaders);
				getHeaders(httpContent->Headers->First(), *respHeaders);
			}

			IAsyncOperationWithProgress<IBuffer^, uint64_t>^ readOp = httpContent->ReadAsBufferAsync();
			asyncEvent.Reset();
			readOp->Completed = ref new AsyncOperationWithProgressCompletedHandler<IBuffer^, uint64_t>(
				[&asyncEvent](IAsyncOperationWithProgress<IBuffer^, uint64_t>^, Windows::Foundation::AsyncStatus) {
					asyncEvent.Set();
				});
			asyncEvent.Wait();
			IBuffer^ buffer = readOp->GetResults();

			Array<u8>^ array = ref new Array<u8>(buffer->Length);
			DataReader::FromBuffer(buffer)->ReadBytes(array);
			content = std::vector<u8>(array->begin(), array->end());
		}
		return (int)resp->StatusCode;
	}
	catch (Exception^ e)
	{
		WARN_LOG(COMMON, "http::get error %.*S", e->Message->Length(), e->Message->Data());
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
		Uri^ uri = ref new Uri(ref new String(wurl.get()));
		HttpStringContent^ content = ref new HttpStringContent(ref new String(wpayload.get()));
		content->Headers->ContentLength = ref new Box<UINT64>(strlen(payload));
		if (contentType != nullptr)
			content->Headers->ContentType = ref new HttpMediaTypeHeaderValue(ref new String(wcontentType.get()));

		IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^ op = httpClient->PostAsync(uri, content);
		cResetEvent asyncEvent;
		op->Completed = ref new AsyncOperationWithProgressCompletedHandler<HttpResponseMessage^, HttpProgress>(
			[&asyncEvent](IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^, Windows::Foundation::AsyncStatus) {
				asyncEvent.Set();
			});
		if (!asyncEvent.Wait(30000))
			return 408;
		HttpResponseMessage^ resp = op->GetResults();

		if (resp->IsSuccessStatusCode)
		{
			IHttpContent^ httpContent = resp->Content;
			IAsyncOperationWithProgress<IBuffer^, uint64_t>^ readOp = httpContent->ReadAsBufferAsync();
			asyncEvent.Reset();
			readOp->Completed = ref new AsyncOperationWithProgressCompletedHandler<IBuffer^, uint64_t>(
				[&asyncEvent](IAsyncOperationWithProgress<IBuffer^, uint64_t>^, Windows::Foundation::AsyncStatus) {
					asyncEvent.Set();
				});
			asyncEvent.Wait();
			IBuffer^ buffer = readOp->GetResults();

			Array<u8>^ array = ref new Array<u8>(buffer->Length);
			DataReader::FromBuffer(buffer)->ReadBytes(array);
			reply = std::vector<u8>(array->begin(), array->end());
		}
		return (int)resp->StatusCode;
	}
	catch (Exception^ e)
	{
		WARN_LOG(COMMON, "http::post error %.*S", e->Message->Length(), e->Message->Data());
		return 500;
	}
}

int post(const std::string & url, const std::vector<PostField>&fields) {
	// not implemented (used by sentry minidump upload)
	return 500;
}

}
