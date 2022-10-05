package com.reicast.emulator;

import android.content.Context;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;

import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.NativeGLView;
import com.reicast.emulator.periph.InputDeviceManager;
import com.reicast.emulator.periph.VJoy;

public final class NativeGLActivity extends BaseGLActivity {

    private ViewGroup mLayout;   // used for text input
    private NativeGLView mView;
    View mTextEdit;
    private boolean mScreenKeyboardShown;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        Log.i("flycast", "NativeGLActivity.onCreate");
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(savedInstanceState);

        // Create the actual GL view
        mView = new NativeGLView(this);
        mLayout = new RelativeLayout(this);
        mLayout.addView(mView);

        setContentView(mLayout);
        Log.i("flycast", "NativeGLActivity.onCreate done");
    }

    @Override
    protected void doPause() {
        mView.pause();
    }

    @Override
    protected void doResume() {
        mView.resume();
    }

    @Override
    public boolean isSurfaceReady() {
        return mView.isSurfaceReady();
    }

    // Called from native code
    private void VJoyStartEditing() {
        vjoy_d_cached = VJoy.readCustomVjoyValues(getApplicationContext());
        JNIdc.show_osd();
        mView.setEditVjoyMode(true);
    }
    // Called from native code
    private void VJoyResetEditing() {
        VJoy.resetCustomVjoyValues(getApplicationContext());
        mView.readCustomVjoyValues();
        mView.resetEditMode();
        handler.post(new Runnable() {
            @Override
            public void run() {
                mView.requestLayout();
            }
        });
    }
    // Called from native code
    private void VJoyStopEditing(final boolean canceled) {
        handler.post(new Runnable() {
            @Override
            public void run() {
                if (canceled)
                    mView.restoreCustomVjoyValues(vjoy_d_cached);
                mView.setEditVjoyMode(false);
            }
        });
    }

    // On-screen keyboard borrowed from SDL core android code
    class ShowTextInputTask implements Runnable {
        /*
         * This is used to regulate the pan&scan method to have some offset from
         * the bottom edge of the input region and the top edge of an input
         * method (soft keyboard)
         */
        static final int HEIGHT_PADDING = 15;

        public int x, y, w, h;

        public ShowTextInputTask(int x, int y, int w, int h) {
            this.x = x;
            this.y = y;
            this.w = w;
            this.h = h;

            /* Minimum size of 1 pixel, so it takes focus. */
            if (this.w <= 0) {
                this.w = 1;
            }
            if (this.h + HEIGHT_PADDING <= 0) {
                this.h = 1 - HEIGHT_PADDING;
            }
        }

        @Override
        public void run() {
            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(w, h + HEIGHT_PADDING);
            params.leftMargin = x;
            params.topMargin = y;
            if (mTextEdit == null) {
                mTextEdit = new DummyEdit(getApplicationContext(), NativeGLActivity.this);

                mLayout.addView(mTextEdit, params);
            } else {
                mTextEdit.setLayoutParams(params);
            }

            mTextEdit.setVisibility(View.VISIBLE);
            mTextEdit.requestFocus();

            InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            imm.showSoftInput(mTextEdit, 0);

            mScreenKeyboardShown = true;
            Log.d("flycast", "ShowTextInputTask: run");
        }
    }

    // Called from native code
    public void showTextInput(int x, int y, int w, int h) {
        // Transfer the task to the main thread as a Runnable
        handler.post(new ShowTextInputTask(x, y, w, h));
    }

    // Called from native code
    public void hideTextInput() {
        Log.d("flycast", "hideTextInput " + (mTextEdit != null ? "mTextEdit != null" : ""));
        if (mTextEdit != null) {
            mTextEdit.postDelayed(new Runnable() {
                @Override
                public void run() {
                    // Note: On some devices setting view to GONE creates a flicker in landscape.
                    // Setting the View's sizes to 0 is similar to GONE but without the flicker.
                    // The sizes will be set to useful values when the keyboard is shown again.
                    mTextEdit.setLayoutParams(new RelativeLayout.LayoutParams(0, 0));

                    InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                    imm.hideSoftInputFromWindow(mTextEdit.getWindowToken(), 0);

                    mScreenKeyboardShown = false;

                    mView.requestFocus();
                }
            }, 50);
        }
    }

    // Called from native code
    public boolean isScreenKeyboardShown() {
        if (mTextEdit == null)
            return false;

        if (!mScreenKeyboardShown)
            return false;

        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        return imm.isAcceptingText();
    }
}

/* This is a fake invisible editor view that receives the input and defines the
+ * pan&scan region
+ */
class DummyEdit extends View implements View.OnKeyListener {
    InputConnection ic;
    NativeGLActivity activity;

    class InputConnection extends BaseInputConnection {
        public InputConnection(boolean fullEditor) {
            super(DummyEdit.this, fullEditor);
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER)
                activity.hideTextInput();
            return super.sendKeyEvent(event);
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            InputDeviceManager devManager = InputDeviceManager.getInstance();
            for (int i = 0; i < text.length(); i++) {
                char c = text.charAt(i);
                if (c == '\n') {
                    activity.hideTextInput();
                    return true;
                }
                devManager.keyboardText(c);
            }
            return super.commitText(text, newCursorPosition);
        }

        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            // Workaround to capture backspace key. Ref: http://stackoverflow.com/questions/14560344/android-backspace-in-webview-baseinputconnection
            // and https://bugzilla.libsdl.org/show_bug.cgi?id=2265
            if (beforeLength > 0 && afterLength == 0) {
                Log.d("flycast", "deleteSurroundingText before=" + beforeLength);
                KeyEvent downEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL);
                downEvent.setSource(InputDevice.SOURCE_KEYBOARD);
                KeyEvent upEvent = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL);
                upEvent.setSource(InputDevice.SOURCE_KEYBOARD);
                boolean ret = true;
                // backspace(s)
                while (beforeLength-- > 0) {
                    boolean ret_key = sendKeyEvent(downEvent);
                    sendKeyEvent(upEvent);
                    ret = ret && ret_key;
                }
                return ret;
            }
            return super.deleteSurroundingText(beforeLength, afterLength);
        }
    }

    public DummyEdit(Context context, NativeGLActivity activity) {
        super(context);
        this.activity = activity;
        setFocusableInTouchMode(true);
        setFocusable(true);
        setOnKeyListener(this);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return true;
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        /*
         * This handles the hardware keyboard input
         */
        InputDeviceManager devManager = InputDeviceManager.getInstance();
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (!event.isCtrlPressed() && (event.isPrintingKey() || event.getKeyCode() == KeyEvent.KEYCODE_SPACE))
                ic.commitText(String.valueOf((char) event.getUnicodeChar()), 1);
            else
                devManager.keyboardEvent(event.getKeyCode(), true);
            return true;
        } else if (event.getAction() == KeyEvent.ACTION_UP) {
            devManager.keyboardEvent(event.getKeyCode(), false);
            return true;
        }
        return false;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        // As seen on StackOverflow: http://stackoverflow.com/questions/7634346/keyboard-hide-event
        // FIXME: Discussion at http://bugzilla.libsdl.org/show_bug.cgi?id=1639
        // FIXME: This is not a 100% effective solution to the problem of detecting if the keyboard is showing or not
        // FIXME: A more effective solution would be to assume our Layout to be RelativeLayout or LinearLayout
        // FIXME: And determine the keyboard presence doing this: http://stackoverflow.com/questions/2150078/how-to-check-visibility-of-software-keyboard-in-android
        // FIXME: An even more effective way would be if Android provided this out of the box, but where would the fun be in that :)
        if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
            if (activity.mTextEdit != null && activity.mTextEdit.getVisibility() == View.VISIBLE) {
//                activity.hideTextInput();
                KeyEvent downEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
                downEvent.setSource(InputDevice.SOURCE_KEYBOARD);
                ic.sendKeyEvent(downEvent);
                KeyEvent upEvent = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER);
                upEvent.setSource(InputDevice.SOURCE_KEYBOARD);
                ic.sendKeyEvent(upEvent);
            }
        }
        return super.onKeyPreIme(keyCode, event);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN /* API 11 */;
        ic = new InputConnection(true);
        return ic;
    }
}
