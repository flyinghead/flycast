/* ====================================================================
 * Copyright (c) 2012-2013 Lounge Katt Entertainment.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by Lounge Katt for use
 *    by authorized development projects. (http://loungekatt.com/)"
 *
 * 4. The names "Lounge Katt", "TwistedUmbrella", and "StarKissed"  
 *    must not be used to endorse or promote products derived from this 
 *    software without prior written permission. For written permission,
 *    please contact admin@loungekatt.com.
 *
 * 5. Products derived from this software may not be called "Lounge Katt"
 *    nor may "Lounge Katt" appear in their names without prior written
 *    permission of Lounge Katt Entertainment.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Lounge Katt for use
 *    by authorized development projects. (http://loungekatt.com/)"
 *
 * THIS SOFTWARE IS PROVIDED BY Lounge Katt ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * The license and distribution terms for any publicly available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution license
 * [including the GNU Public License.] Content not subject to these terms is
 * subject to to the terms and conditions of the Apache License, Version 2.0.
 */

package com.reicast.emulator.debug;

import java.io.IOException;
import java.net.MalformedURLException;
import java.util.ArrayList;
import java.util.List;

import org.apache.http.client.HttpClient;
import org.apache.http.client.ResponseHandler;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.BasicResponseHandler;
import org.apache.http.impl.client.DefaultHttpClient;
import org.json.JSONArray;
import org.json.JSONObject;

import android.annotation.SuppressLint;
import android.os.AsyncTask;
import android.os.Build;
import android.os.StrictMode;
import android.util.Log;

public class RequestArchive extends AsyncTask<String, String, List<String[]>> {

	@SuppressLint("NewApi")
	protected void onPreExecute() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
			StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder()
					.permitAll().build();
			StrictMode.setThreadPolicy(policy);
		}
	}

	@Override
	protected List<String[]> doInBackground(String... urls) {
		try {
			List<String[]> messages = new ArrayList<String[]>();
			HttpClient client = new DefaultHttpClient();
			HttpPost post = new HttpPost(urls[0]);
			ResponseHandler<String> responseHandler = new BasicResponseHandler();
			String response = client.execute(post, responseHandler);
			if (response.contains("{") && response.contains("}")) {
				JSONObject archive = new JSONObject(response);
				JSONArray items = archive.getJSONArray("archive");
				for (int i = 0; i < items.length(); i++) {
					JSONObject log = items.getJSONObject(i);
					String id = log.getString("identifier");
					String msg = log.getString("message");
					String date = log.getString("created_at");
					messages.add(new String[] { id, msg, date });
				}
				return messages;
			}
			return null;

		} catch (MalformedURLException e) {
			Log.d("reicast-debug", "MalformedURLException: " + e);
		} catch (IOException e) {
			Log.d("reicast-debug", "IOException: " + e);
		} catch (Exception e) {
			Log.d("reicast-debug", "Exception: " + e);
		}
		return null;
	}

	@Override
	protected void onPostExecute(List<String[]> jsonArray) {
		super.onPostExecute(jsonArray);
	}
}