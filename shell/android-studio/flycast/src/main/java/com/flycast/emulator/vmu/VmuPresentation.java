package com.flycast.emulator.vmu;

import android.app.Presentation;
import android.content.Context;
import android.os.Bundle;
import android.view.Display;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

/**
 * Shows a VMU screen on a secondary display.
 */
public class VmuPresentation extends Presentation
{
    private final int vmuId;
    private VmuView view;
    private int scaleMode = VmuView.SCALE_FIT;

    public VmuPresentation(Context outerContext, Display display, int vmuId)
    {
        super(outerContext, display);
        this.vmuId = vmuId;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        Window window = getWindow();
        if (window != null)
            // Never steal input focus from the main game activity
            window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                    | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                    | WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setCancelable(false);
        view = new VmuView(getContext(), vmuId);
        view.setScaleMode(scaleMode);
        view.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        setContentView(view);
    }

    public void setScaleMode(int mode)
    {
        scaleMode = mode;
        if (view != null)
            view.setScaleMode(mode);
    }
}
