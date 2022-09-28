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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

public class HttpClient {
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
        }
        return 500;
    }

    public native void nativeInit();
}