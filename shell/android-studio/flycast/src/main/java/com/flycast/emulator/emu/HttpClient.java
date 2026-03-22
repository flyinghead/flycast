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
package com.flycast.emulator.emu;

import android.util.Log;

import org.apache.hc.client5.http.classic.methods.HttpPost;
import org.apache.hc.client5.http.config.RequestConfig;
import org.apache.hc.client5.http.entity.mime.MultipartEntityBuilder;
import org.apache.hc.client5.http.impl.classic.CloseableHttpClient;
import org.apache.hc.client5.http.impl.classic.CloseableHttpResponse;
import org.apache.hc.client5.http.impl.classic.HttpClientBuilder;
import org.apache.hc.core5.http.ContentType;
import org.apache.hc.core5.http.HttpEntity;
import org.apache.hc.core5.http.io.entity.StringEntity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.TimeUnit;

public class HttpClient {
    private CloseableHttpClient httpClient;
    private String userAgent = "Flycast/1.0";

    static { System.loadLibrary("flycast"); }

    // Called from native code
    public void init(String userAgent) {
        this.userAgent = userAgent;
    }

    public int openUrl(String url_string, byte[][] content, String[] reqHeaderNames, String[] reqHeaderValues,
                       String[][] respHeaderNames, String[][] respHeaderValues)
    {
        try {
            URL url = new URL(url_string);
            HttpURLConnection conn = (HttpURLConnection)url.openConnection();
            conn.setConnectTimeout(30 * 1000); // 30 s
            conn.setRequestProperty("User-Agent", userAgent);
            if (reqHeaderNames != null) {
                for (int i = 0; i < reqHeaderNames.length; i++)
                    conn.setRequestProperty(reqHeaderNames[i], reqHeaderValues[i]);
            }
            conn.connect();
            if (conn.getResponseCode() >= 200 && conn.getResponseCode() < 300) {
                if (respHeaderNames != null)
                {
                    List<String> keys = new ArrayList<>();
                    List<String> values = new ArrayList<>();
                    Map<String, List<String>> headers = conn.getHeaderFields();
                    for (String key : headers.keySet()) {
                        if (key == null)
                            continue;
                        String lkey = key.toLowerCase(Locale.ROOT);
                        for (String value : headers.get(key)) {
                            keys.add(lkey);
                            values.add(value);
                        }
                    }
                    respHeaderNames[0] = keys.toArray(new String[keys.size()]);
                    respHeaderValues[0] = values.toArray(new String[values.size()]);
                }
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

    private CloseableHttpClient getHttpClient()
    {
        if (httpClient == null)
        {
            RequestConfig.Builder requestBuilder = RequestConfig.custom();
            requestBuilder.setConnectTimeout(30, TimeUnit.SECONDS);
            requestBuilder.setConnectionRequestTimeout(30, TimeUnit.SECONDS);

            HttpClientBuilder builder = HttpClientBuilder.create();
            builder.setDefaultRequestConfig(requestBuilder.build());
            httpClient = builder.build();
        }
        return httpClient;
    }

    public int post(String urlString, String payload, String contentType, byte[][] reply) {
        try {
            HttpPost httpPost = new HttpPost(urlString);
            httpPost.setEntity(new StringEntity(payload, contentType != null ? ContentType.create(contentType) : ContentType.APPLICATION_FORM_URLENCODED));
            httpPost.setHeader("User-Agent", userAgent);
            CloseableHttpResponse response = getHttpClient().execute(httpPost);
            InputStream is = response.getEntity().getContent();

            byte[] buffer = new byte[1024];
            int length;

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            while ((length = is.read(buffer)) > 0) {
                baos.write(buffer, 0, length);
            }
            is.close();
            baos.close();
            reply[0] = baos.toByteArray();

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
    public int post(String urlString, String[] fieldNames, String[] fieldValues, String[] contentTypes)
    {
        try {
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
            httpPost.setHeader("User-Agent", userAgent);
            CloseableHttpResponse response = getHttpClient().execute(httpPost);

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