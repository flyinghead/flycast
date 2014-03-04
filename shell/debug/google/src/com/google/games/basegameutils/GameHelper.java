/*
 * Copyright (C) 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.games.basegameutils;

import java.util.Vector;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;

import com.google.android.gms.appstate.AppStateClient;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesClient;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.common.Scopes;
import com.google.android.gms.games.GamesClient;
import com.google.android.gms.games.OnSignOutCompleteListener;
import com.google.android.gms.games.multiplayer.Invitation;
import com.google.android.gms.plus.PlusClient;

public class GameHelper implements
		GooglePlayServicesClient.ConnectionCallbacks,
		GooglePlayServicesClient.OnConnectionFailedListener,
		OnSignOutCompleteListener {

	/** Listener for sign-in success or failure events. */
	public interface GameHelperListener {
		/**
		 * Called when sign-in fails. As a result, a "Sign-In" button can be
		 * shown to the user; when that button is clicked, call
		 * 
		 * @link{GamesHelper#beginUserInitiatedSignIn . Note that not all calls
		 *                                            to this method mean an
		 *                                            error; it may be a result
		 *                                            of the fact that automatic
		 *                                            sign-in could not proceed
		 *                                            because user interaction
		 *                                            was required (consent
		 *                                            dialogs). So
		 *                                            implementations of this
		 *                                            method should NOT display
		 *                                            an error message unless a
		 *                                            call to @link{GamesHelper#
		 *                                            hasSignInError} indicates
		 *                                            that an error indeed
		 *                                            occurred.
		 */
		void onSignInFailed();

		/** Called when sign-in succeeds. */
		void onSignInSucceeded();
	}

	/**
	 * The Activity we are bound to. We need to keep a reference to the Activity
	 * because some games methods require an Activity (a Context won't do). We
	 * are careful not to leak these references: we release them on onStop().
	 */
	Activity mActivity = null;

	// OAuth scopes required for the clients. Initialized in setup().
	String mScopes[];

	// Request code we use when invoking other Activities to complete the
	// sign-in flow.
	final static int RC_RESOLVE = 9001;

	// Request code when invoking Activities whose result we don't care about.
	final static int RC_UNUSED = 9002;

	// Client objects we manage. If a given client is not enabled, it is null.
	GamesClient mGamesClient = null;
	PlusClient mPlusClient = null;
	AppStateClient mAppStateClient = null;

	// What clients we manage (OR-able values, can be combined as flags)
	public final static int CLIENT_NONE = 0x00;
	public final static int CLIENT_GAMES = 0x01;
	public final static int CLIENT_PLUS = 0x02;
	public final static int CLIENT_APPSTATE = 0x04;
	public final static int CLIENT_ALL = CLIENT_GAMES | CLIENT_PLUS
			| CLIENT_APPSTATE;

	// What clients were requested? (bit flags)
	int mRequestedClients = CLIENT_NONE;

	// What clients are currently connected? (bit flags)
	int mConnectedClients = CLIENT_NONE;

	// What client are we currently connecting?
	int mClientCurrentlyConnecting = CLIENT_NONE;

	// A progress dialog we show when we are trying to sign the user is
	ProgressDialog mProgressDialog = null;

	// Whether to automatically try to sign in on onStart().
	boolean mAutoSignIn = true;

	/*
	 * Whether user has specifically requested that the sign-in process begin.
	 * If mUserInitiatedSignIn is false, we're in the automatic sign-in attempt
	 * that we try once the Activity is started -- if true, then the user has
	 * already clicked a "Sign-In" button or something similar
	 */
	boolean mUserInitiatedSignIn = false;

	// The connection result we got from our last attempt to sign-in.
	ConnectionResult mConnectionResult = null;

	// Whether our sign-in attempt resulted in an error. In this case,
	// mConnectionResult
	// indicates what was the error we failed to resolve.
	boolean mSignInError = false;

	// Whether we launched the sign-in dialog flow and therefore are expecting
	// an
	// onActivityResult with the result of that.
	boolean mExpectingActivityResult = false;

	// Are we signed in?
	boolean mSignedIn = false;

	// Print debug logs?
	boolean mDebugLog = false;
	String mDebugTag = "BaseGameActivity";

	// Messages (can be set by the developer).
	String mSigningInMessage = "";
	String mSigningOutMessage = "";
	String mUnknownErrorMessage = "Unknown error";

	// If we got an invitation id when we connected to the games client, it's
	// here.
	// Otherwise, it's null.
	String mInvitationId;

	// Listener
	GameHelperListener mListener = null;

	/**
	 * Construct a GameHelper object, initially tied to the given Activity.
	 * After constructing this object, call @link{setup} from the onCreate()
	 * method of your Activity.
	 */
	public GameHelper(Activity activity) {
		mActivity = activity;
	}

	/** Sets the message that appears onscreen while signing in. */
	public void setSigningInMessage(String message) {
		mSigningInMessage = message;
	}

	/** Sets the message that appears onscreen while signing out. */
	public void setSigningOutMessage(String message) {
		mSigningOutMessage = message;
	}

	/**
	 * Sets the message that appears onscreen when there is an unknown error
	 * (rare!)
	 */
	public void setUnknownErrorMessage(String message) {
		mUnknownErrorMessage = message;
	}

	/**
	 * Same as calling @link{setup(GameHelperListener, int)} requesting only the
	 * CLIENT_GAMES client.
	 */
	public void setup(GameHelperListener listener) {
		setup(listener, CLIENT_GAMES);
	}

	/**
	 * Performs setup on this GameHelper object. Call this from the onCreate()
	 * method of your Activity. This will create the clients and do a few other
	 * initialization tasks. Next, call @link{#onStart} from the onStart()
	 * method of your Activity.
	 * 
	 * @param listener
	 *            The listener to be notified of sign-in events.
	 * @param clientsToUse
	 *            The clients to use. Use a combination of CLIENT_GAMES,
	 *            CLIENT_PLUS and CLIENT_APPSTATE, or CLIENT_ALL to request all
	 *            clients.
	 */
	public void setup(GameHelperListener listener, int clientsToUse) {
		mListener = listener;
		mRequestedClients = clientsToUse;

		Vector<String> scopesVector = new Vector<String>();
		if (0 != (clientsToUse & CLIENT_GAMES)) {
			scopesVector.add(Scopes.GAMES);
		}
		if (0 != (clientsToUse & CLIENT_PLUS)) {
			scopesVector.add(Scopes.PLUS_LOGIN);
		}
		if (0 != (clientsToUse & CLIENT_APPSTATE)) {
			scopesVector.add(Scopes.APP_STATE);
		}

		mScopes = new String[scopesVector.size()];
		scopesVector.copyInto(mScopes);

		if (0 != (clientsToUse & CLIENT_GAMES)) {
			debugLog("onCreate: creating GamesClient");
			mGamesClient = new GamesClient.Builder(getContext(), this, this)
					.setGravityForPopups(
							Gravity.TOP | Gravity.CENTER_HORIZONTAL)
					.setScopes(mScopes).create();
		}

		if (0 != (clientsToUse & CLIENT_PLUS)) {
			debugLog("onCreate: creating GamesPlusClient");
			mPlusClient = new PlusClient.Builder(getContext(), this, this)
					.setScopes(mScopes).build();
		}

		if (0 != (clientsToUse & CLIENT_APPSTATE)) {
			debugLog("onCreate: creating AppStateClient");
			mAppStateClient = new AppStateClient.Builder(getContext(), this,
					this).setScopes(mScopes).create();
		}
	}

	/**
	 * Returns the GamesClient object. In order to call this method, you must
	 * have called @link{setup} with a set of clients that includes
	 * CLIENT_GAMES.
	 */
	public GamesClient getGamesClient() {
		if (mGamesClient == null) {
			throw new IllegalStateException(
					"No GamesClient. Did you request it at setup?");
		}
		return mGamesClient;
	}

	/**
	 * Returns the AppStateClient object. In order to call this method, you must
	 * have called @link{#setup} with a set of clients that includes
	 * CLIENT_APPSTATE.
	 */
	public AppStateClient getAppStateClient() {
		if (mAppStateClient == null) {
			throw new IllegalStateException(
					"No AppStateClient. Did you request it at setup?");
		}
		return mAppStateClient;
	}

	/**
	 * Returns the PlusClient object. In order to call this method, you must
	 * have called @link{#setup} with a set of clients that includes
	 * CLIENT_PLUS.
	 */
	public PlusClient getPlusClient() {
		if (mPlusClient == null) {
			throw new IllegalStateException(
					"No PlusClient. Did you request it at setup?");
		}
		return mPlusClient;
	}

	/** Returns whether or not the user is signed in. */
	public boolean isSignedIn() {
		return mSignedIn;
	}

	/**
	 * Returns whether or not there was a (non-recoverable) error during the
	 * sign-in process.
	 */
	public boolean hasSignInError() {
		return mSignInError;
	}

	/**
	 * Returns the error that happened during the sign-in process, null if no
	 * error occurred.
	 */
	public ConnectionResult getSignInError() {
		return mSignInError ? mConnectionResult : null;
	}

	/** Call this method from your Activity's onStart(). */
	public void onStart(Activity act) {
		mActivity = act;

		debugLog("onStart.");
		if (mExpectingActivityResult) {
			// this Activity is starting because the UI flow we launched to
			// resolve a connection problem has just returned. In this case,
			// we should NOT automatically reconnect the client, since
			// onActivityResult will handle that.
			debugLog("onStart: won't connect because we're expecting activity result.");
		} else if (!mAutoSignIn) {
			// The user specifically signed out, so don't attempt to sign in
			// automatically. If the user wants to sign in, they will click
			// the sign-in button, at which point we will try to sign in.
			debugLog("onStart: not signing in because user specifically signed out.");
		} else {
			// Attempt to connect the clients.
			debugLog("onStart: connecting clients.");
			startConnections();
		}
	}

	/** Call this method from your Activity's onStop(). */
	public void onStop() {
		debugLog("onStop: disconnecting clients.");

		// disconnect the clients -- this is very important (prevents resource
		// leaks!)
		killConnections(CLIENT_ALL);

		// no longer signed in
		mSignedIn = false;
		mSignInError = false;

		// destroy progress dialog -- we create it again when needed
		dismissDialog();
		mProgressDialog = null;

		// let go of the Activity reference
		mActivity = null;
	}

	/** Convenience method to show an alert dialog. */
	public void showAlert(String title, String message) {
		(new AlertDialog.Builder(getContext())).setTitle(title)
				.setMessage(message)
				.setNeutralButton(android.R.string.ok, null).create().show();
	}

	/** Convenience method to show an alert dialog. */
	public void showAlert(String message) {
		(new AlertDialog.Builder(getContext())).setMessage(message)
				.setNeutralButton(android.R.string.ok, null).create().show();
	}

	/**
	 * Returns the invitation ID received through an invitation notification.
	 * This should be called from your GameHelperListener's
	 * 
	 * @link{GameHelperListener#onSignInSucceeded method, to check if there's an
	 *                                            invitation available. In that
	 *                                            case, accept the invitation.
	 * @return The id of the invitation, or null if none was received.
	 */
	public String getInvitationId() {
		return mInvitationId;
	}

	/** Enables debug logging, with the given logcat tag. */
	public void enableDebugLog(boolean enabled, String tag) {
		mDebugLog = enabled;
		mDebugTag = tag;
	}

	/**
	 * Returns the current requested scopes. This is not valid until setup() has
	 * been called.
	 * 
	 * @return the requested scopes, including the oauth2: prefix
	 */
	public String getScopes() {
		StringBuilder scopeStringBuilder = new StringBuilder();
		int clientsToUse = mRequestedClients;
		// GAMES implies PLUS_LOGIN
		if (0 != (clientsToUse & CLIENT_GAMES)) {
			addToScope(scopeStringBuilder, Scopes.GAMES);
		}
		if (0 != (clientsToUse & CLIENT_PLUS)) {
			addToScope(scopeStringBuilder, Scopes.PLUS_LOGIN);
		}
		if (0 != (clientsToUse & CLIENT_APPSTATE)) {
			addToScope(scopeStringBuilder, Scopes.APP_STATE);
		}
		return scopeStringBuilder.toString();
	}

	/** Sign out and disconnect from the APIs. */
	public void signOut() {
		mConnectionResult = null;
		mAutoSignIn = false;
		mSignedIn = false;
		mSignInError = false;

		if (mPlusClient != null && mPlusClient.isConnected()) {
			mPlusClient.clearDefaultAccount();
		}
		if (mGamesClient != null && mGamesClient.isConnected()) {
			showProgressDialog(false);
			mGamesClient.signOut(this);
		}

		// kill connects to all clients but games, which must remain
		// connected til we get onSignOutComplete()
		killConnections(CLIENT_ALL & ~CLIENT_GAMES);
	}

	/**
	 * Handle activity result. Call this method from your Activity's
	 * onActivityResult callback. If the activity result pertains to the sign-in
	 * process, processes it appropriately.
	 */
	public void onActivityResult(int requestCode, int responseCode,
			Intent intent) {
		if (requestCode == RC_RESOLVE) {
			// We're coming back from an activity that was launched to resolve a
			// connection
			// problem. For example, the sign-in UI.
			mExpectingActivityResult = false;
			debugLog("onActivityResult, req " + requestCode + " response "
					+ responseCode);
			if (responseCode == Activity.RESULT_OK) {
				// Ready to try to connect again.
				debugLog("responseCode == RESULT_OK. So connecting.");
				connectCurrentClient();
			} else {
				// Whatever the problem we were trying to solve, it was not
				// solved.
				// So give up and show an error message.
				debugLog("responseCode != RESULT_OK, so not reconnecting.");
				giveUp();
			}
		}
	}

	/**
	 * Starts a user-initiated sign-in flow. This should be called when the user
	 * clicks on a "Sign In" button. As a result, authentication/consent dialogs
	 * may show up. At the end of the process, the GameHelperListener's
	 * onSignInSucceeded() or onSignInFailed() methods will be called.
	 */
	public void beginUserInitiatedSignIn() {
		if (mSignedIn)
			return; // nothing to do

		// reset the flag to sign in automatically on onStart() -- now a
		// wanted behavior
		mAutoSignIn = true;

		// Is Google Play services available?
		int result = GooglePlayServicesUtil
				.isGooglePlayServicesAvailable(getContext());
		debugLog("isGooglePlayServicesAvailable returned " + result);
		if (result != ConnectionResult.SUCCESS) {
			// Nope.
			debugLog("Google Play services not available. Show error dialog.");
			Dialog errorDialog = getErrorDialog(result);
			errorDialog.show();
			if (mListener != null)
				mListener.onSignInFailed();
			return;
		}

		mUserInitiatedSignIn = true;
		if (mConnectionResult != null) {
			// We have a pending connection result from a previous failure, so
			// start with that.
			debugLog("beginUserInitiatedSignIn: continuing pending sign-in flow.");
			showProgressDialog(true);
			resolveConnectionResult();
		} else {
			// We don't have a pending connection result, so start anew.
			debugLog("beginUserInitiatedSignIn: starting new sign-in flow.");
			startConnections();
		}
	}

	Context getContext() {
		return mActivity;
	}

	void addToScope(StringBuilder scopeStringBuilder, String scope) {
		if (scopeStringBuilder.length() == 0) {
			scopeStringBuilder.append("oauth2:");
		} else {
			scopeStringBuilder.append(" ");
		}
		scopeStringBuilder.append(scope);
	}

	void startConnections() {
		mConnectedClients = CLIENT_NONE;
		mInvitationId = null;
		connectNextClient();
	}

	void showProgressDialog(boolean signIn) {
		String message = signIn ? mSigningInMessage : mSigningOutMessage;

		if (mProgressDialog == null) {
			if (getContext() == null)
				return;
			mProgressDialog = new ProgressDialog(getContext());
		}

		mProgressDialog.setMessage(message == null ? "" : message);
		mProgressDialog.setIndeterminate(true);
		mProgressDialog.show();
	}

	void dismissDialog() {
		if (mProgressDialog != null)
			mProgressDialog.dismiss();
		mProgressDialog = null;
	}

	void connectNextClient() {
		// do we already have all the clients we need?
		int pendingClients = mRequestedClients & ~mConnectedClients;
		if (pendingClients == 0) {
			debugLog("All clients now connected. Sign-in successful.");
			succeedSignIn();
			return;
		}

		showProgressDialog(true);

		// which client should be the next one to connect?
		if (mGamesClient != null && (0 != (pendingClients & CLIENT_GAMES))) {
			debugLog("Connecting GamesClient.");
			mClientCurrentlyConnecting = CLIENT_GAMES;
		} else if (mPlusClient != null && (0 != (pendingClients & CLIENT_PLUS))) {
			debugLog("Connecting PlusClient.");
			mClientCurrentlyConnecting = CLIENT_PLUS;
		} else if (mAppStateClient != null
				&& (0 != (pendingClients & CLIENT_APPSTATE))) {
			debugLog("Connecting AppStateClient.");
			mClientCurrentlyConnecting = CLIENT_APPSTATE;
		} else {
			throw new AssertionError(
					"Not all clients connected, yet no one is next. R="
							+ mRequestedClients + ", C=" + mConnectedClients);
		}

		connectCurrentClient();
	}

	void connectCurrentClient() {
		switch (mClientCurrentlyConnecting) {
		case CLIENT_GAMES:
			mGamesClient.connect();
			break;
		case CLIENT_APPSTATE:
			mAppStateClient.connect();
			break;
		case CLIENT_PLUS:
			mPlusClient.connect();
			break;
		}
	}

	void killConnections(int whatClients) {
		if ((whatClients & CLIENT_GAMES) != 0 && mGamesClient != null
				&& mGamesClient.isConnected()) {
			mConnectedClients &= ~CLIENT_GAMES;
			mGamesClient.disconnect();
		}
		if ((whatClients & CLIENT_PLUS) != 0 && mPlusClient != null
				&& mPlusClient.isConnected()) {
			mConnectedClients &= ~CLIENT_PLUS;
			mPlusClient.disconnect();
		}
		if ((whatClients & CLIENT_APPSTATE) != 0 && mAppStateClient != null
				&& mAppStateClient.isConnected()) {
			mConnectedClients &= ~CLIENT_APPSTATE;
			mAppStateClient.disconnect();
		}
	}

	public void reconnectClients(int whatClients) {
		showProgressDialog(true);

		if ((whatClients & CLIENT_GAMES) != 0 && mGamesClient != null
				&& mGamesClient.isConnected()) {
			mConnectedClients &= ~CLIENT_GAMES;
			mGamesClient.reconnect();
		}
		if ((whatClients & CLIENT_APPSTATE) != 0 && mAppStateClient != null
				&& mAppStateClient.isConnected()) {
			mConnectedClients &= ~CLIENT_APPSTATE;
			mAppStateClient.reconnect();
		}
		if ((whatClients & CLIENT_PLUS) != 0 && mPlusClient != null
				&& mPlusClient.isConnected()) {
			mConnectedClients &= ~CLIENT_PLUS;
			mPlusClient.disconnect();
			mPlusClient.connect();
		}
	}

	/** Called when we successfully obtain a connection to a client. */
	@Override
	public void onConnected(Bundle connectionHint) {
		debugLog("onConnected: connected! client=" + mClientCurrentlyConnecting);

		// Mark the current client as connected
		mConnectedClients |= mClientCurrentlyConnecting;

		// If this was the games client and it came with an invite, store it for
		// later retrieval.
		if (mClientCurrentlyConnecting == CLIENT_GAMES
				&& connectionHint != null) {
			debugLog("onConnected: connection hint provided. Checking for invite.");
			Invitation inv = connectionHint
					.getParcelable(GamesClient.EXTRA_INVITATION);
			if (inv != null && inv.getInvitationId() != null) {
				// accept invitation
				debugLog("onConnected: connection hint has a room invite!");
				mInvitationId = inv.getInvitationId();
				debugLog("Invitation ID: " + mInvitationId);
			}
		}

		// connect the next client in line, if any.
		connectNextClient();
	}

	void succeedSignIn() {
		debugLog("All requested clients connected. Sign-in succeeded!");
		mSignedIn = true;
		mSignInError = false;
		mAutoSignIn = true;
		dismissDialog();
		if (mListener != null) {
			mListener.onSignInSucceeded();
		}
	}

	/** Handles a connection failure reported by a client. */
	@Override
	public void onConnectionFailed(ConnectionResult result) {
		// save connection result for later reference
		mConnectionResult = result;
		debugLog("onConnectionFailed: result " + result.getErrorCode());
		dismissDialog();

		if (!mUserInitiatedSignIn) {
			// If the user didn't initiate the sign-in, we don't try to resolve
			// the connection problem automatically -- instead, we fail and wait
			// for the user to want to sign in. That way, they won't get an
			// authentication (or other) popup unless they are actively trying
			// to
			// sign in.
			debugLog("onConnectionFailed: since user didn't initiate sign-in, failing now.");
			mConnectionResult = result;
			if (mListener != null) {
				mListener.onSignInFailed();
			}
			return;
		}

		debugLog("onConnectionFailed: since user initiated sign-in, trying to resolve problem.");

		// Resolve the connection result. This usually means showing a dialog or
		// starting an Activity that will allow the user to give the appropriate
		// consents so that sign-in can be successful.
		resolveConnectionResult();
	}

	/**
	 * Attempts to resolve a connection failure. This will usually involve
	 * starting a UI flow that lets the user give the appropriate consents
	 * necessary for sign-in to work.
	 */
	void resolveConnectionResult() {
		// Try to resolve the problem
		debugLog("resolveConnectionResult: trying to resolve result: "
				+ mConnectionResult);
		if (mConnectionResult.hasResolution()) {
			// This problem can be fixed. So let's try to fix it.
			debugLog("result has resolution. Starting it.");
			try {
				// launch appropriate UI flow (which might, for example, be the
				// sign-in flow)
				mExpectingActivityResult = true;
				mConnectionResult.startResolutionForResult(mActivity,
						RC_RESOLVE);
			} catch (SendIntentException e) {
				// Try connecting again
				debugLog("SendIntentException.");
				connectCurrentClient();
			}
		} else {
			// It's not a problem what we can solve, so give up and show an
			// error.
			debugLog("resolveConnectionResult: result has no resolution. Giving up.");
			giveUp();
		}
	}

	/**
	 * Give up on signing in due to an error. Shows the appropriate error
	 * message to the user, using a standard error dialog as appropriate to the
	 * cause of the error. That dialog will indicate to the user how the problem
	 * can be solved (for example, re-enable Google Play Services, upgrade to a
	 * new version, etc).
	 */
	void giveUp() {
		mSignInError = true;
		mAutoSignIn = false;
		dismissDialog();
		debugLog("giveUp: giving up on connection. "
				+ ((mConnectionResult == null) ? "(no connection result)"
						: ("Status code: " + mConnectionResult.getErrorCode())));

		Dialog errorDialog = null;
		if (mConnectionResult != null) {
			// get error dialog for that specific problem
			errorDialog = getErrorDialog(mConnectionResult.getErrorCode());
			errorDialog.show();
			if (mListener != null) {
				mListener.onSignInFailed();
			}
		} else {
			// this is a bug
			Log.e("GameHelper", "giveUp() called with no mConnectionResult");
		}
	}

	/** Called when we are disconnected from a client. */
	@Override
	public void onDisconnected() {
		debugLog("onDisconnected.");
		mConnectionResult = null;
		mAutoSignIn = false;
		mSignedIn = false;
		mSignInError = false;
		mInvitationId = null;
		mConnectedClients = CLIENT_NONE;
		if (mListener != null) {
			mListener.onSignInFailed();
		}
	}

	/** Returns an error dialog that's appropriate for the given error code. */
	Dialog getErrorDialog(int errorCode) {
		debugLog("Making error dialog for error: " + errorCode);
		Dialog errorDialog = GooglePlayServicesUtil.getErrorDialog(errorCode,
				mActivity, RC_UNUSED, null);

		if (errorDialog != null)
			return errorDialog;

		// as a last-resort, make a sad "unknown error" dialog.
		return (new AlertDialog.Builder(getContext()))
				.setMessage(mUnknownErrorMessage)
				.setNeutralButton(android.R.string.ok, null).create();
	}

	void debugLog(String message) {
		if (mDebugLog)
			Log.d(mDebugTag, message);
	}

	@Override
	public void onSignOutComplete() {
		dismissDialog();
		if (mGamesClient.isConnected())
			mGamesClient.disconnect();
	}
}
