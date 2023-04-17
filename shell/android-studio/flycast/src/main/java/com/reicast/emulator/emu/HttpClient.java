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
package com.reicast.emulator.emu;

import android.util.Log;

import org.apache.hc.client5.http.classic.methods.HttpPost;
import org.apache.hc.client5.http.entity.mime.MultipartEntityBuilder;
import org.apache.hc.client5.http.impl.classic.CloseableHttpClient;
import org.apache.hc.client5.http.impl.classic.CloseableHttpResponse;
import org.apache.hc.client5.http.impl.classic.HttpClients;
import org.apache.hc.core5.http.ContentType;
import org.apache.hc.core5.http.HttpEntity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.charset.Charset;

public class HttpClient {
    private CloseableHttpClient httpClient;

    // Called from native code
    public int openUrl(String url_string, byte[][] content, String[] contentType)
    {
        try {
            URL url = new URL(url_string);
            HttpURLConnection conn = (HttpURLConnection)url.openConnection();
            conn.connect();
            if (conn.getResponseCode() >= 200 && conn.getResponseCode() < 300) {
                InputStream is = conn.getInputStream();

                byte[] buffer = new byte[1024];
                int length;

                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                while ((length = is.read(buffer)) > 0) {
                    baos.write(buffer, 0, length);
                }
                is.close();
                baos.close();
                content[0] = baos.toByteArray();
                if (contentType != null)
                    contentType[0] = conn.getContentType();
            }

            return conn.getResponseCode();
        } catch (MalformedURLException e) {
            Log.e("flycast", "Malformed URL", e);
        } catch (IOException e) {
            Log.e("flycast", "I/O error", e);
        } catch (SecurityException e) {
            Log.e("flycast", "Security error", e);
        } catch (Throwable t) {
            Log.e("flycast", "Unknown error", t);
        }
        return 500;
    }

    public int post(String urlString, String[] fieldNames, String[] fieldValues, String[] contentTypes)
    {
        try {
            if (httpClient == null)
                httpClient = HttpClients.createDefault();
            HttpPost httpPost = new HttpPost(urlString);
            MultipartEntityBuilder builder = MultipartEntityBuilder.create();
            builder.setCharset(Charset.forName("UTF-8"));
            for (int i = 0; i < fieldNames.length; i++) {
                if (contentTypes[i].isEmpty()) {
                    builder.addTextBody(fieldNames[i], fieldValues[i]);
                }
                else {
                    File file = new File(fieldValues[i]);
                    builder.addBinaryBody(fieldNames[i], file, ContentType.create(contentTypes[i]), file.getName());
                }
            }
            HttpEntity multipart = builder.build();
            httpPost.setEntity(multipart);
            CloseableHttpResponse response = httpClient.execute(httpPost);

            return response.getCode();
        } catch (MalformedURLException e) {
            Log.e("flycast", "Malformed URL", e);
        } catch (IOException e) {
            Log.e("flycast", "I/O error", e);
        } catch (SecurityException e) {
            Log.e("flycast", "Security error", e);
        } catch (Throwable t) {
            Log.e("flycast", "Unknown error", t);
        }
        return 500;
    }

    public native void nativeInit();
}