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
#include "rend/boxart/http_client.h"
#include "stdclass.h"

namespace http {

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;

static HttpClient^ httpClient;

void init() {
	httpClient = ref new HttpClient();
	httpClient->DefaultRequestHeaders->UserAgent->ParseAdd(L"Flycast/1.0");
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
		Uri^ uri = ref new Uri(ref new String(wurl.get()));
		IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^ op = httpClient->GetAsync(uri);
		cResetEvent asyncEvent;
		op->Completed = ref new AsyncOperationWithProgressCompletedHandler<HttpResponseMessage^, HttpProgress>(
				[&asyncEvent](IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^, AsyncStatus) {
					asyncEvent.Set();
		        });
		asyncEvent.Wait();
		HttpResponseMessage^ resp = op->GetResults();

		if (resp->IsSuccessStatusCode)
		{
			IHttpContent^ httpContent = resp->Content;
			contentType.clear();
			HttpMediaTypeHeaderValue^ contentTypeHeader = httpContent->Headers->ContentType;
			if (contentTypeHeader != nullptr && contentTypeHeader->MediaType != nullptr)
			{
				String^ mediaType = contentTypeHeader->MediaType;
				nowide::stackstring nwstring;
				nwstring.convert(mediaType->Data());
				contentType = nwstring.get();
			}
			IAsyncOperationWithProgress<IBuffer^, uint64_t>^ readOp = httpContent->ReadAsBufferAsync();
			asyncEvent.Reset();
			readOp->Completed = ref new AsyncOperationWithProgressCompletedHandler<IBuffer^, uint64_t>(
				[&asyncEvent](IAsyncOperationWithProgress<IBuffer^, uint64_t>^, AsyncStatus) {
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

int post(const std::string& url, const std::vector<PostField>& fields) {
	// not implemented
	return 500;
}

}
