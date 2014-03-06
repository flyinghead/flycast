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
 *    "This product includes software developed by Lounge Katt" unless
 *    otherwise displayed by public repository entries.
 *
 * 4. The names "Lounge Katt", "TwistedUmbrella", and "LiveLog"  
 *    must not be used to endorse or promote products derived from this 
 *    software without prior written permission. For written permission,
 *    please contact admin@loungekatt.com.
 *
 * 5. Products derived from this software may not be called "LiveLog"
 *    nor may "LiveLog" appear in their names without prior written
 *    permission of Lounge Katt Entertainment.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Lounge Katt" unless
 *    otherwise displayed by tagged repository entries.
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

package com.reicast.emulator;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.app.Activity;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.util.Linkify;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

@SuppressWarnings("deprecation")
public class AboutFragment extends Fragment {

	String buildId = "";

	private Activity parentActivity;

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.about_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		parentActivity = getActivity();

		try {
			InputStream file = getResources().getAssets().open("build");
			if (file != null) {
				BufferedReader reader = new BufferedReader(
						new InputStreamReader(file));
				buildId = reader.readLine();
				file.close();
			}
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}

		try {
			String versionName = parentActivity.getPackageManager()
					.getPackageInfo(parentActivity.getPackageName(), 0).versionName;
			int versionCode = parentActivity.getPackageManager()
					.getPackageInfo(parentActivity.getPackageName(), 0).versionCode;
			TextView version = (TextView) getView().findViewById(
					R.id.revision_text);
			String revision = getString(R.string.revision_text,
					versionName, String.valueOf(versionCode));
			if (!buildId.isEmpty()) {
				revision = getString(R.string.revision_text,
						versionName, buildId.substring(0,7));
			}
			version.setText(revision);
		} catch (NameNotFoundException e) {
			e.printStackTrace();
		}
		
		TextView website = (TextView) getView().findViewById(R.id.site_text);
		Linkify.addLinks(website, Linkify.ALL);
	}
}
