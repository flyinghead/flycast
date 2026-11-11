package com.flycast.emulator.vmu;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.Choreographer;
import android.view.View;

import com.flycast.emulator.emu.JNIdc;

/**
 * Draws the LCD screen of a VMU, scaled to the view according to the scaling mode.
 * The screen data is polled from native code on each vsync and only uploaded when it changes.
 */
public class VmuView extends View implements Choreographer.FrameCallback
{
    public static final int SCALE_FIT = 0;
    public static final int SCALE_INTEGER = 1;
    public static final int SCALE_STRETCH = 2;

    private static final int VMU_WIDTH = 48;
    private static final int VMU_HEIGHT = 32;

    private final int vmuId;
    private final int[] pixels = new int[VMU_WIDTH * VMU_HEIGHT];
    private final Bitmap bitmap = Bitmap.createBitmap(VMU_WIDTH, VMU_HEIGHT, Bitmap.Config.ARGB_8888);
    private final Paint paint = new Paint();
    private final Rect srcRect = new Rect(0, 0, VMU_WIDTH, VMU_HEIGHT);
    private final Rect dstRect = new Rect();
    private long lastStamp = 0;
    private boolean polling = false;
    private int scaleMode = SCALE_FIT;

    public VmuView(Context context, int vmuId)
    {
        super(context);
        this.vmuId = vmuId;
        // Nearest-neighbor scaling to keep the pixels sharp
        paint.setFilterBitmap(false);
        paint.setAntiAlias(false);
        paint.setDither(false);
        setBackgroundColor(Color.BLACK);
        setKeepScreenOn(true);
    }

    @Override
    protected void onAttachedToWindow()
    {
        super.onAttachedToWindow();
        polling = true;
        Choreographer.getInstance().postFrameCallback(this);
    }

    @Override
    protected void onDetachedFromWindow()
    {
        polling = false;
        Choreographer.getInstance().removeFrameCallback(this);
        super.onDetachedFromWindow();
    }

    @Override
    public void doFrame(long frameTimeNanos)
    {
        if (!polling)
            return;
        long stamp = JNIdc.getVmuScreen(vmuId, pixels);
        if (stamp != lastStamp)
        {
            if (stamp != 0)
                bitmap.setPixels(pixels, 0, VMU_WIDTH, 0, 0, VMU_WIDTH, VMU_HEIGHT);
            lastStamp = stamp;
            invalidate();
        }
        Choreographer.getInstance().postFrameCallback(this);
    }

    public void setScaleMode(int mode)
    {
        if (mode == scaleMode)
            return;
        scaleMode = mode;
        updateDstRect(getWidth(), getHeight());
        invalidate();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh)
    {
        super.onSizeChanged(w, h, oldw, oldh);
        updateDstRect(w, h);
    }

    private void updateDstRect(int w, int h)
    {
        int width, height;
        switch (scaleMode)
        {
            case SCALE_STRETCH:
                width = w;
                height = h;
                break;
            case SCALE_INTEGER:
            {
                int scale = Math.max(1, Math.min(w / VMU_WIDTH, h / VMU_HEIGHT));
                width = VMU_WIDTH * scale;
                height = VMU_HEIGHT * scale;
                break;
            }
            case SCALE_FIT:
            default:
                // Largest rectangle with the VMU aspect ratio that fits
                width = w;
                height = w * VMU_HEIGHT / VMU_WIDTH;
                if (height > h) {
                    height = h;
                    width = h * VMU_WIDTH / VMU_HEIGHT;
                }
                break;
        }
        int left = (w - width) / 2;
        int top = (h - height) / 2;
        dstRect.set(left, top, left + width, top + height);
    }

    @Override
    protected void onDraw(Canvas canvas)
    {
        super.onDraw(canvas);
        if (lastStamp != 0)
            canvas.drawBitmap(bitmap, srcRect, dstRect, paint);
    }
}
