package com.reicast.emulator.emu;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Vibrator;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.periph.InputDeviceManager;
import com.reicast.emulator.periph.VJoy;

public class VirtualJoystickDelegate {
    private VibratorThread vibratorThread;

    private boolean editVjoyMode = false;
    private int selectedVjoyElement = VJoy.ELEM_NONE;
    private ScaleGestureDetector scaleGestureDetector;

    private Handler handler = new Handler();
    private Runnable hideOsdRunnable = new Runnable() {
        @Override
        public void run() {
            JNIdc.hideOsd();
        }
    };

    private float[][] vjoy_d_custom;

    private static final float[][] vjoy = VJoy.baseVJoy();

    private Context context;
    private View view;

    public VirtualJoystickDelegate(View view) {
        this.view = view;
        this.context = view.getContext();

        vibratorThread = new VibratorThread(context);
        vibratorThread.start();

        readCustomVjoyValues();
        scaleGestureDetector = new ScaleGestureDetector(context, new OscOnScaleGestureListener());
    }

    public void readCustomVjoyValues() {
        vjoy_d_custom = VJoy.readCustomVjoyValues(context);
    }

    public void restoreCustomVjoyValues(float[][] vjoy_d_cached) {
        vjoy_d_custom = vjoy_d_cached;
        VJoy.writeCustomVjoyValues(vjoy_d_cached, context);

        resetEditMode();
        view.requestLayout();
    }


    private void reset_analog()
    {

        int j=11;
        vjoy[j+1][0]=vjoy[j][0]+vjoy[j][2]/2-vjoy[j+1][2]/2;
        vjoy[j+1][1]=vjoy[j][1]+vjoy[j][3]/2-vjoy[j+1][3]/2;
        JNIdc.vjoy(j+1, vjoy[j+1][0], vjoy[j+1][1], vjoy[j+1][2], vjoy[j+1][3]);
    }

    private int get_anal(int j, int axis)
    {
        return (int) (((vjoy[j+1][axis]+vjoy[j+1][axis+2]/2) - vjoy[j][axis] - vjoy[j][axis+2]/2)*254/vjoy[j][axis+2]);
    }

    private float vbase(float p, float m, float scl)
    {
        return (int) ( m - (m -p)*scl);
    }

    private float vbase(float p, float scl)
    {
        return (int) (p*scl );
    }

    private boolean isTablet() {
        return (context.getResources().getConfiguration().screenLayout
                & Configuration.SCREENLAYOUT_SIZE_MASK)
                >= Configuration.SCREENLAYOUT_SIZE_LARGE;
    }

    public void layout(int width, int height)
    {
        //dcpx/cm = dcpx/px * px/cm
        float magic = isTablet() ? 0.8f : 0.7f;
        float scl = 480.0f / height * context.getResources().getDisplayMetrics().density * magic;
        float scl_dc = height / 480.0f;
        float tx = (width - 640.0f * scl_dc) / 2 / scl_dc;

        float a_x = -tx + 24 * scl;
        float a_y = -24 * scl;

        // Not sure how this can happen
        if (vjoy_d_custom == null)
            return;

        float[][] vjoy_d = VJoy.getVjoy_d(vjoy_d_custom);

        for (int i=0;i<vjoy.length;i++)
        {
        	// FIXME this hack causes the slight "jump" when first moving a screen-centered button
            if (vjoy_d[i][0] == 288)
                vjoy[i][0] = vjoy_d[i][0];
            else if (vjoy_d[i][0]-vjoy_d_custom[getElementIdFromButtonId(i)][0] < 320)
                vjoy[i][0] = a_x + vbase(vjoy_d[i][0],scl);
            else
                vjoy[i][0] = -a_x + vbase(vjoy_d[i][0],640,scl);

            vjoy[i][1] = a_y + vbase(vjoy_d[i][1],480,scl);

            vjoy[i][2] = vbase(vjoy_d[i][2],scl);
            vjoy[i][3] = vbase(vjoy_d[i][3],scl);
        }

        for (int i=0;i<VJoy.VJoyCount;i++)
            JNIdc.vjoy(i,vjoy[i][0],vjoy[i][1],vjoy[i][2],vjoy[i][3]);

        reset_analog();
        VJoy.writeCustomVjoyValues(vjoy_d_custom, context);
    }

    private int anal_id=-1, lt_id=-1, rt_id=-1;

    public void resetEditMode() {
        editLastX = 0;
        editLastY = 0;
    }

    private static int getElementIdFromButtonId(int buttonId) {
        if (buttonId <= 3)
            return VJoy.ELEM_DPAD; // DPAD
        else if (buttonId <= 7)
            return VJoy.ELEM_BUTTONS; // X, Y, B, A Buttons
        else if (buttonId == 8)
            return VJoy.ELEM_START; // Start
        else if (buttonId == 9)
            return VJoy.ELEM_LTRIG; // Left Trigger
        else if (buttonId == 10)
            return VJoy.ELEM_RTRIG; // Right Trigger
        else if (buttonId <= 12)
            return VJoy.ELEM_ANALOG; // Analog
        else if (buttonId == 13)
            return VJoy.ELEM_FFORWARD; // Fast-forward
        else
            return VJoy.ELEM_DPAD; // DPAD diagonals
    }

    private static int left_trigger = 0;
    private static int right_trigger = 0;
    private static int[] mouse_pos = { -32768, -32768 };
    private static int mouse_btns = 0;

    private float editLastX = 0, editLastY = 0;

    public boolean onTouchEvent(MotionEvent event, int width, int height)
    {
        if (event.getSource() != InputDevice.SOURCE_TOUCHSCREEN)
            // Ignore real mice, trackballs, etc.
            return false;
        JNIdc.show_osd();
        this.handler.removeCallbacks(hideOsdRunnable);
        if (!editVjoyMode)
            this.handler.postDelayed(hideOsdRunnable, 10000);

        scaleGestureDetector.onTouchEvent(event);

        float ty = 0.0f;
        float scl = height / 480.0f;
        float tx = (width - 640.0f * scl) / 2;

        int rv = 0xFFFFFFFF;
        boolean fastForward = false;

        int aid = event.getActionMasked();
        int pid = event.getActionIndex();

        if (!JNIdc.guiIsOpen()) {
            if (editVjoyMode && selectedVjoyElement != VJoy.ELEM_NONE && aid == MotionEvent.ACTION_MOVE && !scaleGestureDetector.isInProgress()) {
                float x = (event.getX() - tx) / scl;
                float y = (event.getY() - ty) / scl;

                if (editLastX != 0 && editLastY != 0) {
                    float deltaX = x - editLastX;
                    float deltaY = y - editLastY;

                    vjoy_d_custom[selectedVjoyElement][0] += isTablet() ? deltaX * 2 : deltaX;
                    vjoy_d_custom[selectedVjoyElement][1] += isTablet() ? deltaY * 2 : deltaY;

                    view.requestLayout();
                }

                editLastX = x;
                editLastY = y;

                return true;
            }

            for (int i = 0; i < event.getPointerCount(); i++) {
                float x = (event.getX(i) - tx) / scl;
                float y = (event.getY(i) - ty) / scl;
                if (anal_id != event.getPointerId(i)) {
                    if (aid == MotionEvent.ACTION_POINTER_UP && pid == i)
                        continue;
                    for (int j = 0; j < vjoy.length; j++)
                    {
                        if (x > vjoy[j][0] && x <= (vjoy[j][0] + vjoy[j][2]))
                        {
                            if (y > vjoy[j][1] && y <= (vjoy[j][1] + vjoy[j][3]))
                            {
                                if (vjoy[j][4] >= VJoy.BTN_RTRIG) {
                                    // Not for analog
                                    if (vjoy[j][5] == 0)
                                        if (!editVjoyMode) {
                                            vibratorThread.vibrate();
                                        }
                                    vjoy[j][5] = 2;
                                }


                                if (vjoy[j][4] == VJoy.BTN_ANARING) {
                                    if (editVjoyMode) {
                                        selectedVjoyElement = VJoy.ELEM_ANALOG;
                                        resetEditMode();
                                    } else {
                                        vjoy[j + 1][0] = x - vjoy[j + 1][2] / 2;
                                        vjoy[j + 1][1] = y - vjoy[j + 1][3] / 2;

                                        JNIdc.vjoy(j + 1, vjoy[j + 1][0], vjoy[j + 1][1], vjoy[j + 1][2], vjoy[j + 1][3]);
                                        anal_id = event.getPointerId(i);
                                    }
                                } else if (vjoy[j][4] != VJoy.BTN_ANAPOINT) {
                                    if (vjoy[j][4] == VJoy.BTN_LTRIG) {
                                        if (editVjoyMode) {
                                            selectedVjoyElement = VJoy.ELEM_LTRIG;
                                            resetEditMode();
                                        } else {
                                            left_trigger = 255;
                                            lt_id = event.getPointerId(i);
                                        }
                                    } else if (vjoy[j][4] == VJoy.BTN_RTRIG) {
                                        if (editVjoyMode) {
                                            selectedVjoyElement = VJoy.ELEM_RTRIG;
                                            resetEditMode();
                                        } else {
                                            right_trigger = 255;
                                            rt_id = event.getPointerId(i);
                                        }
                                    } else {
                                        if (editVjoyMode) {
                                            selectedVjoyElement = getElementIdFromButtonId(j);
                                            resetEditMode();
                                        } else if (vjoy[j][4] == VJoy.key_CONT_FFORWARD)
                                            fastForward = true;
                                        else
                                            rv &= ~(int)vjoy[j][4];
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (x < vjoy[11][0])
                        x = vjoy[11][0];
                    else if (x > (vjoy[11][0] + vjoy[11][2]))
                        x = vjoy[11][0] + vjoy[11][2];

                    if (y < vjoy[11][1])
                        y = vjoy[11][1];
                    else if (y > (vjoy[11][1] + vjoy[11][3]))
                        y = vjoy[11][1] + vjoy[11][3];

                    int j = 11;
                    vjoy[j + 1][0] = x - vjoy[j + 1][2] / 2;
                    vjoy[j + 1][1] = y - vjoy[j + 1][3] / 2;

                    JNIdc.vjoy(j + 1, vjoy[j + 1][0], vjoy[j + 1][1], vjoy[j + 1][2], vjoy[j + 1][3]);

                }
            }

            for (int j = 0; j < vjoy.length; j++) {
                if (vjoy[j][5] == 2)
                    vjoy[j][5] = 1;
                else if (vjoy[j][5] == 1)
                    vjoy[j][5] = 0;
            }
        }

        switch(aid)
        {
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                selectedVjoyElement = -1;
                reset_analog();
                anal_id = -1;
                rv = 0xFFFFFFFF;
                fastForward = false;
                right_trigger = 0;
                left_trigger = 0;
                lt_id = -1;
                rt_id = -1;
                for (int j= 0 ;j < vjoy.length; j++)
                    vjoy[j][5] = 0;
                mouse_btns = 0;
                break;

            case MotionEvent.ACTION_POINTER_UP:
                if (event.getPointerId(event.getActionIndex())==anal_id)
                {
                    reset_analog();
                    anal_id = -1;
                }
                else if (event.getPointerId(event.getActionIndex())==lt_id)
                {
                    left_trigger = 0;
                    lt_id = -1;
                }
                else if (event.getPointerId(event.getActionIndex())==rt_id)
                {
                    right_trigger = 0;
                    rt_id = -1;
                }
                break;

            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_DOWN:
                if (event.getPointerCount() != 1)
                {
                    mouse_btns = 0;
                }
                else
                {
                    MotionEvent.PointerCoords pointerCoords = new MotionEvent.PointerCoords();
                    event.getPointerCoords(0, pointerCoords);
                    mouse_pos[0] = Math.round(pointerCoords.x);
                    mouse_pos[1] = Math.round(pointerCoords.y);
                    mouse_btns = MotionEvent.BUTTON_PRIMARY;    // Mouse left button down
                }
                break;

            case MotionEvent.ACTION_MOVE:
                if (event.getPointerCount() == 1)
                {
                    MotionEvent.PointerCoords pointerCoords = new MotionEvent.PointerCoords();
                    event.getPointerCoords(0, pointerCoords);
                    mouse_pos[0] = Math.round(pointerCoords.x);
                    mouse_pos[1] = Math.round(pointerCoords.y);
                }
                break;
        }
        int joyx = get_anal(11, 0);
        int joyy = get_anal(11, 1);
        InputDeviceManager.getInstance().virtualGamepadEvent(rv, joyx, joyy, left_trigger, right_trigger, fastForward);
        // Only register the mouse event if no virtual gamepad button is down
        if ((!editVjoyMode && rv == 0xFFFFFFFF && left_trigger == 0 && right_trigger == 0 && joyx == 0 && joyy == 0 && !fastForward)
		|| JNIdc.guiIsOpen())
            InputDeviceManager.getInstance().mouseEvent(mouse_pos[0], mouse_pos[1], mouse_btns);
        return(true);
    }

    public void setEditVjoyMode(boolean editVjoyMode) {
        this.editVjoyMode = editVjoyMode;
        selectedVjoyElement = -1;
        if (editVjoyMode)
            this.handler.removeCallbacks(hideOsdRunnable);
        resetEditMode();
    }

    private class OscOnScaleGestureListener extends
            ScaleGestureDetector.SimpleOnScaleGestureListener {

        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            if (editVjoyMode && selectedVjoyElement != -1) {
                vjoy_d_custom[selectedVjoyElement][2] *= detector.getScaleFactor();
                view.requestLayout();

                return true;
            }

            return false;
        }

        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            selectedVjoyElement = -1;
        }
    }

    private class VibratorThread extends Thread
    {
        private Vibrator vibrator;
        private boolean vibrate = false;
        private boolean stopping = false;

        VibratorThread(Context context) {
            vibrator = (Vibrator)context.getSystemService(Context.VIBRATOR_SERVICE);
        }

        @Override
        public void run() {
            while (!stopping) {
                boolean doVibrate;
                synchronized (this) {
                    doVibrate = false;
                    try {
                        this.wait();
                    } catch (InterruptedException e) {
                    }
                    if (vibrate) {
                        doVibrate = true;
                        vibrate = false;
                    }
                }
                if (doVibrate)
                    vibrator.vibrate(Emulator.vibrationDuration);
            }
        }

        public void stopVibrator() {
            synchronized (this) {
                stopping = true;
                notify();
            }
        }

        public void vibrate() {
            if (Emulator.vibrationDuration > 0) {
                synchronized (this) {
                    vibrate = true;
                    notify();
                }
            }
        }
    }
}
