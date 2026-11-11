package com.flycast.emulator.vmu;

import android.app.Activity;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

import com.flycast.emulator.emu.JNIdc;

/**
 * Shows the VMU screen of controller A on a secondary display (such as the
 * bottom screen of dual-screen handhelds) when one is available, a game is
 * loaded and the option is enabled.
 * All methods must be called on the UI thread.
 */
public class SecondaryDisplayManager implements DisplayManager.DisplayListener
{
    // VMU in controller port A, expansion slot 1
    private static final int VMU_ID = 0;

    private final Activity activity;
    private final DisplayManager displayManager;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private VmuPresentation presentation;
    private boolean started = false;
    private boolean gameLoaded = false;

    public SecondaryDisplayManager(Activity activity)
    {
        this.activity = activity;
        this.displayManager = (DisplayManager)activity.getSystemService(Context.DISPLAY_SERVICE);
    }

    public void start()
    {
        if (started || displayManager == null)
            return;
        started = true;
        displayManager.registerDisplayListener(this, handler);
        update();
    }

    public void stop()
    {
        if (!started)
            return;
        started = false;
        displayManager.unregisterDisplayListener(this);
        dismiss();
    }

    public void setGameLoaded(boolean loaded)
    {
        gameLoaded = loaded;
        update();
    }

    /**
     * Show or hide the VMU screen according to the current option values, game state and available displays.
     */
    public void update()
    {
        if (!started || !gameLoaded || activity.isFinishing() || !JNIdc.isVmuSecondaryScreenEnabled()) {
            dismiss();
            return;
        }
        int scaleMode = JNIdc.getVmuSecondaryScreenScaling();
        Display display = findSecondaryDisplay();
        if (presentation != null && (display == null || presentation.getDisplay().getDisplayId() != display.getDisplayId()))
            dismiss();
        if (display == null)
            return;
        if (presentation != null) {
            presentation.setScaleMode(scaleMode);
            return;
        }

        Log.i("flycast", "Showing VMU screen on display " + display.getDisplayId() + " (" + display.getName() + ")");
        presentation = new VmuPresentation(activity, display, VMU_ID);
        presentation.setScaleMode(scaleMode);
        try {
            presentation.show();
        } catch (WindowManager.InvalidDisplayException e) {
            Log.w("flycast", "Can't show VMU screen on secondary display", e);
            presentation = null;
        }
    }

    private Display findSecondaryDisplay()
    {
        Display[] displays = displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
        for (Display display : displays)
            if (display.getDisplayId() != Display.DEFAULT_DISPLAY)
                return display;
        // Some devices don't flag their built-in secondary screen as a presentation display
        for (Display display : displayManager.getDisplays())
            if (display.getDisplayId() != Display.DEFAULT_DISPLAY)
                return display;
        return null;
    }

    private void dismiss()
    {
        if (presentation != null) {
            presentation.dismiss();
            presentation = null;
        }
    }

    @Override
    public void onDisplayAdded(int displayId) {
        update();
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        update();
    }

    @Override
    public void onDisplayChanged(int displayId) {
        update();
    }
}
