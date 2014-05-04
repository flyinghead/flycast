/*
 * Copyright 2013 David Schreiber
 *           2013 John Paul Nalog
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

package at.technikum.mti.fancycoverflow;

import android.*;
import android.R;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.*;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

/**
 * This class has only internal use (package scope).
 * <p/>
 * It is responsible for applying additional effects to each coverflow item, that can only be applied at view level
 * (e.g. color saturation).
 * <p/>
 * This is a ViewGroup by intention to enable child views in layouts to stay interactive (like buttons) though
 * transformed.
 * <p/>
 * Since this class is only used within the FancyCoverFlowAdapter it doesn't need to check if there are multiple
 * children or not (there can only be one at all times).
 */
@SuppressWarnings("ConstantConditions")
class FancyCoverFlowItemWrapper extends ViewGroup {

    // =============================================================================
    // Private members
    // =============================================================================

    private float saturation;

    private boolean isReflectionEnabled = false;

    private float imageReflectionRatio;

    private int reflectionGap;

    private float originalScaledownFactor;

    /**
     * This is a matrix to apply color filters (like saturation) to the wrapped view.
     */
    private ColorMatrix colorMatrix;

    /**
     * This paint is used to draw the wrapped view including any filters.
     */
    private Paint paint;

    /**
     * This is a cache holding the wrapped view's visual representation.
     */
    private Bitmap wrappedViewBitmap;

    /**
     * This canvas is used to let the wrapped view draw it's content.
     */
    private Canvas wrappedViewDrawingCanvas;


    // =============================================================================
    // Constructor
    // =============================================================================

    public FancyCoverFlowItemWrapper(Context context) {
        super(context);
        this.init();
    }

    public FancyCoverFlowItemWrapper(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.init();
    }

    public FancyCoverFlowItemWrapper(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        this.init();
    }

    private void init() {
        this.paint = new Paint();
        this.colorMatrix = new ColorMatrix();
        // TODO: Define a default value for saturation inside an XML.
        this.setSaturation(1);
    }

    // =============================================================================
    // Getters / Setters
    // =============================================================================

    void setReflectionEnabled(boolean hasReflection) {
        if (hasReflection != this.isReflectionEnabled) {
            this.isReflectionEnabled = hasReflection;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
                // Turn off hardware acceleration if necessary (reflections won't support it).
                this.setLayerType(hasReflection ? View.LAYER_TYPE_SOFTWARE : View.LAYER_TYPE_HARDWARE, null);
            }

            this.remeasureChildren();
        }
    }

    void setReflectionRatio(float imageReflectionRatio) {
        if (imageReflectionRatio != this.imageReflectionRatio) {
            this.imageReflectionRatio = imageReflectionRatio;
            this.remeasureChildren();
        }
    }

    void setReflectionGap(int reflectionGap) {
        if (reflectionGap != this.reflectionGap) {
            this.reflectionGap = reflectionGap;
            this.remeasureChildren();
        }
    }

    public void setSaturation(float saturation) {
        if (saturation != this.saturation) {
            this.saturation = saturation;
            this.colorMatrix.setSaturation(saturation);
            this.paint.setColorFilter(new ColorMatrixColorFilter(this.colorMatrix));
        }
    }

    // =============================================================================
    // Supertype overrides
    // =============================================================================

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        this.remeasureChildren();

        // If we have reflection enabled, the original image is scaled down and a reflection is added beneath. Thus,
        // while maintaining the same height the width decreases and we need to adjust measured width.
        // WARNING: This is a hack because we do not obey the EXACTLY MeasureSpec mode that we will get mostly.
        if (this.isReflectionEnabled) {
            this.setMeasuredDimension((int) (this.getMeasuredWidth() * this.originalScaledownFactor), this.getMeasuredHeight());
        }
    }

    @SuppressLint("DrawAllocation")
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        if (changed) {
            int measuredWidth = this.getMeasuredWidth();
            int measuredHeight = this.getMeasuredHeight();

            if (this.wrappedViewBitmap == null || this.wrappedViewBitmap.getWidth() != measuredWidth || this.wrappedViewBitmap.getHeight() != measuredHeight) {
                this.wrappedViewBitmap = Bitmap.createBitmap(measuredWidth, measuredHeight, Bitmap.Config.ARGB_8888);
                this.wrappedViewDrawingCanvas = new Canvas(this.wrappedViewBitmap);
            }

            View child = getChildAt(0);
            int childWidth = child.getMeasuredWidth();
            int childHeight = child.getMeasuredHeight();
            int childLeft = (measuredWidth - childWidth) / 2;
            int childRight = measuredWidth - childLeft;
            child.layout(childLeft, 0, childRight, childHeight);
        }
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    @Override
    protected void dispatchDraw(Canvas canvas) {
        View childView = getChildAt(0);

        if (childView != null) {
            // If on honeycomb or newer, cache the view.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
                if (childView.isDirty()) {
                    childView.draw(this.wrappedViewDrawingCanvas);

                    if (this.isReflectionEnabled) {
                        this.createReflectedImages();
                    }
                }
            } else {
                childView.draw(this.wrappedViewDrawingCanvas);
            }
        }

        canvas.drawBitmap(this.wrappedViewBitmap, (this.getWidth() - childView.getWidth()) / 2, 0, paint);
    }

    // =============================================================================
    // Methods
    // =============================================================================

    private void remeasureChildren() {
        View child = this.getChildAt(0);

        if (child != null) {
            // When reflection is enabled calculate proportional scale down factor.
            final int originalChildHeight = this.getMeasuredHeight();
            this.originalScaledownFactor = this.isReflectionEnabled ? (originalChildHeight * (1 - this.imageReflectionRatio) - reflectionGap) / originalChildHeight : 1.0f;
            final int childHeight = (int) (this.originalScaledownFactor * originalChildHeight);
            final int childWidth = (int) (this.originalScaledownFactor * getMeasuredWidth());

            int heightSpec = MeasureSpec.makeMeasureSpec(childHeight, MeasureSpec.AT_MOST);
            int widthSpec = MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.AT_MOST);
            this.getChildAt(0).measure(widthSpec, heightSpec);
        }
    }

    /**
     * Creates the reflected images.
     *
     * @return true, if successful
     */
    private void createReflectedImages() {

        final int width = this.wrappedViewBitmap.getWidth();
        final int height = this.wrappedViewBitmap.getHeight();


        final Matrix matrix = new Matrix();
        matrix.postScale(1, -1);


        final int scaledDownHeight = (int) (height * originalScaledownFactor);
        final int invertedHeight = height - scaledDownHeight - reflectionGap;
        final int invertedBitmapSourceTop = scaledDownHeight - invertedHeight;
        final Bitmap invertedBitmap = Bitmap.createBitmap(this.wrappedViewBitmap, 0, invertedBitmapSourceTop, width, invertedHeight, matrix, true);

        this.wrappedViewDrawingCanvas.drawBitmap(invertedBitmap, 0, scaledDownHeight + reflectionGap, null);

        final Paint paint = new Paint();
        final LinearGradient shader = new LinearGradient(0, height * imageReflectionRatio + reflectionGap, 0, height, 0x70ffffff, 0x00ffffff, Shader.TileMode.CLAMP);
        paint.setShader(shader);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.DST_IN));
        this.wrappedViewDrawingCanvas.drawRect(0, height * (1 - imageReflectionRatio), width, height, paint);
    }
}
