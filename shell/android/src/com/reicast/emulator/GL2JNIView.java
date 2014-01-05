package com.reicast.emulator;


import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.opengl.GLSurfaceView;
import android.os.Vibrator;
import android.util.Log;
import android.view.MotionEvent;


/**
 * A simple GLSurfaceView sub-class that demonstrate how to perform
 * OpenGL ES 2.0 rendering into a GL Surface. Note the following important
 * details:
 *
 * - The class must use a custom context factory to enable 2.0 rendering.
 *   See ContextFactory class definition below.
 *
 * - The class must use a custom EGLConfigChooser to be able to select
 *   an EGLConfig that supports 2.0. This is done by providing a config
 *   specification to eglChooseConfig() that has the attribute
 *   EGL10.ELG_RENDERABLE_TYPE containing the EGL_OPENGL_ES2_BIT flag
 *   set. See ConfigChooser class definition below.
 *
 * - The class must select the surface's format, then choose an EGLConfig
 *   that matches it exactly (with regards to red/green/blue/alpha channels
 *   bit depths). Failure to do so would result in an EGL_BAD_MATCH error.
 */

class GL2JNIView extends GLSurfaceView
{
  private static String fileName;
  //private AudioThread audioThread;  
  private EmuThread ethd = new EmuThread();

  private static final boolean DEBUG           = false;
  private static final int key_CONT_B          = 0x0002;
  private static final int key_CONT_A          = 0x0004;
  private static final int key_CONT_START      = 0x0008;
  private static final int key_CONT_DPAD_UP    = 0x0010;
  private static final int key_CONT_DPAD_DOWN  = 0x0020;
  private static final int key_CONT_DPAD_LEFT  = 0x0040;
  private static final int key_CONT_DPAD_RIGHT = 0x0080;
  private static final int key_CONT_Y          = 0x0200;
  private static final int key_CONT_X          = 0x0400;
  
  Vibrator vib;
  
 
  private static final float[][] vjoy_d = new float[][]
  { 
    new float[] { 20+0,     288+64,   64,64, key_CONT_DPAD_LEFT},
    new float[] { 20+64,    288+0,    64,64, key_CONT_DPAD_UP},
    new float[] { 20+128,   288+64,   64,64, key_CONT_DPAD_RIGHT},
    new float[] { 20+64,    288+128,  64,64, key_CONT_DPAD_DOWN},

    new float[] { 448+0,    288+64,  64,64, key_CONT_X},
    new float[] { 448+64,   288+0,   64,64, key_CONT_Y},
    new float[] { 448+128,  288+64,  64,64, key_CONT_B},
    new float[] { 448+64,   288+128, 64,64, key_CONT_A},

    new float[] { 320-32,   288+128,  64,64, key_CONT_START},
    
    new float[] { 440, 200,  90,64, -1},
    new float[] { 542, 200,  90,64, -2},
    
    new float[] { 16,   24+32,  128,128, -3},
    new float[] { 96, 320,  32,32, -4},
    
    
  };
  
  private static final float[][] vjoy = new float[][]
		  { 
		    new float[] { 24+0,     24+64,   64,64, key_CONT_DPAD_LEFT, 0},
		    new float[] { 24+64,    24+0,    64,64, key_CONT_DPAD_UP, 0},
		    new float[] { 24+128,   24+64,   64,64, key_CONT_DPAD_RIGHT, 0},
		    new float[] { 24+64,    24+128,  64,64, key_CONT_DPAD_DOWN, 0},

		    new float[] { 440+0,    280+64,  64,64, key_CONT_X, 0},
		    new float[] { 440+64,   280+0,   64,64, key_CONT_Y, 0},
		    new float[] { 440+128,  280+64,  64,64, key_CONT_B, 0},
		    new float[] { 440+64,   280+128, 64,64, key_CONT_A, 0},

		    new float[] { 320-32,   360+32,  64,64, key_CONT_START, 0},
		    
		    new float[] { 440, 200,  90,64, -1, 0},
		    new float[] { 542, 200,  90,64, -2, 0},
		    
		    new float[] { 0,   128+224,  128,128, -3, 0},
		    new float[] { 96, 320,  32,32, -4, 0},
		    
		    
		  };
  
  Renderer rend;
  
  	
  public GL2JNIView(Context context,String newFileName,boolean translucent,int depth,int stencil)
  {
    super(context);
    setKeepScreenOn(true);
    vib=(Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
    
    // This is the game we are going to run
    fileName = newFileName;
    
    if (GL2JNIActivity.syms != null)
    	JNIdc.data(1, GL2JNIActivity.syms);
  
    JNIdc.hide_osd();
    JNIdc.kcode(0xFFFF,0,0,128,128); 
    JNIdc.init(fileName);


    // By default, GLSurfaceView() creates a RGB_565 opaque surface.
    // If we want a translucent one, we should change the surface's
    // format here, using PixelFormat.TRANSLUCENT for GL Surfaces
    // is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
    if(translucent) this.getHolder().setFormat(PixelFormat.TRANSLUCENT);

    // Setup the context factory for 2.0 rendering.
    // See ContextFactory class definition below
    setEGLContextFactory(new ContextFactory());

    // We need to choose an EGLConfig that matches the format of
    // our surface exactly. This is going to be done in our
    // custom config chooser. See ConfigChooser class definition
    // below.
    setEGLConfigChooser(
      translucent?
        new ConfigChooser(8, 8, 8, 8, depth, stencil)
      : new ConfigChooser(5, 6, 5, 0, depth, stencil)
    );

    // Set the renderer responsible for frame rendering
    setRenderer(rend=new Renderer());

    // Initialize audio
    configAudio(44100,250);
    
    ethd.start();
  }
  
  public GLSurfaceView.Renderer getRenderer()
  {
	  return rend;
  }

  private static void LOGI(String S) { Log.i("GL2JNIView",S); }
  private static void LOGW(String S) { Log.w("GL2JNIView",S); }
  private static void LOGE(String S) { Log.e("GL2JNIView",S); }

  public void configAudio(int rate,int latency)
  {
    //if(audioThread!=null) audioThread.stopPlayback();
    //audioThread = new AudioThread(rate,latency);
  }

  @Override public void onWindowFocusChanged(boolean hasWindowFocus)
  {
    //super.onWindowFocusChanged(hasWindowFocus);
    //if(audioThread!=null) audioThread.pausePlayback(!hasWindowFocus);
  }

  private void reset_analog()
  {
	  
    int j=11;
    vjoy[j+1][0]=vjoy[j][0]+vjoy[j][2]/2-vjoy[j+1][2]/2;
    vjoy[j+1][1]=vjoy[j][1]+vjoy[j][3]/2-vjoy[j+1][3]/2;
    JNIdc.vjoy(j+1, vjoy[j+1][0], vjoy[j+1][1], vjoy[j+1][2], vjoy[j+1][3]);
  }
  
  int get_anal(int j, int axis)
  {
	  return (int) (((vjoy[j+1][axis]+vjoy[j+1][axis+2]/2) - vjoy[j][axis] - vjoy[j][axis+2]/2)*254/vjoy[j][axis+2]);
  }
  
  float vbase(float p, float m, float scl)
  {
	  return (int) ( m - (m -p)*scl);
  }
  
  float vbase(float p, float scl)
  {
	  return (int) (p*scl );
  }
  
  public boolean isTablet() {
    return (getContext().getResources().getConfiguration().screenLayout
            & Configuration.SCREENLAYOUT_SIZE_MASK)
            >= Configuration.SCREENLAYOUT_SIZE_LARGE;
  }

  @Override
	protected void onLayout(boolean changed, int left, int top, int right, int bottom) 
  {  
		super.onLayout(changed, left, top, right, bottom);
		//dcpx/cm = dcpx/px * px/cm
    float magic = isTablet() ? 0.8f : 0.7f;
		float scl=480.0f/getHeight() * getContext().getResources().getDisplayMetrics().density * magic;
		float scl_dc=getHeight()/480.0f;
		float tx  = ((getWidth()-640.0f*scl_dc)/2)/scl_dc;
		
		float a_x = -tx+ 24*scl;
		float a_y=- 24*scl;
		
		for(int i=0;i<vjoy.length;i++)
		{
			if (vjoy_d[i][0]==288)
				;
			else if (vjoy_d[i][0]<320)
				vjoy[i][0] = a_x + vbase(vjoy_d[i][0],scl);
			else
				vjoy[i][0] = -a_x+vbase(vjoy_d[i][0],640,scl);
			
			vjoy[i][1]=a_y + vbase(vjoy_d[i][1],480,scl);
			
			vjoy[i][2]=vbase(vjoy_d[i][2],scl);
			vjoy[i][3]=vbase(vjoy_d[i][3],scl);
		}
		
		for(int i=0;i<vjoy.length;i++)
		      JNIdc.vjoy(i,vjoy[i][0],vjoy[i][1],vjoy[i][2],vjoy[i][3]);
		    
		reset_analog(); 
	}
  
  /*
   * 
   * 	DOWN / POINTER_DOWN
   * 	UP / CANCEL -> reset state
   * 	POINTER_UP -> check for freed analog
   * */
  int anal_id=-1;
  /*
  bool intersects(CircleType circle, RectType rect)
  {
      circleDistance.x = abs(circle.x - rect.x);
      circleDistance.y = abs(circle.y - rect.y);

      if (circleDistance.x > (rect.width/2 + circle.r)) { return false; }
      if (circleDistance.y > (rect.height/2 + circle.r)) { return false; }

      if (circleDistance.x <= (rect.width/2)) { return true; } 
      if (circleDistance.y <= (rect.height/2)) { return true; }

      cornerDistance_sq = (circleDistance.x - rect.width/2)^2 +
                           (circleDistance.y - rect.height/2)^2;

      return (cornerDistance_sq <= (circle.r^2));
  }
  */

  static int kcode_raw = 0xFFFF;
  static int lt, rt, jx, jy;

  @Override public boolean onTouchEvent(final MotionEvent event) 
  {
  JNIdc.show_osd();
  
  float ty  = 0.0f;
  float scl  = getHeight()/480.0f;
  float tx  = (getWidth()-640.0f*scl)/2;

  int   rv  = 0xFFFF;
  
  int   aid = event.getActionMasked();
  int   pid = event.getActionIndex();
  
  //LOGI("Touch: " + aid + ", " + pid);
    
  for(int i=0;i<event.getPointerCount();i++)
  { 
	float x = (event.getX(i)-tx)/scl;
	float y = (event.getY(i)-ty)/scl;
	if (anal_id!=event.getPointerId(i))
	{
		if (aid==MotionEvent.ACTION_POINTER_UP && pid==i)
			continue;
		for(int j=0;j<vjoy.length;j++)
		{
		  if(x>vjoy[j][0] && x<=(vjoy[j][0]+vjoy[j][2]))
		  {
			int pre=(int)(event.getPressure(i)*255);
			if (pre>20)
			{
				pre-=20;
				pre*=7;
			}
			if (pre>255) pre=255;
			
		    if(y>vjoy[j][1] && y<=(vjoy[j][1]+vjoy[j][3]))
		    {
		    	if (vjoy[j][4]>=-2)
		    	{
		    		if (vjoy[j][5]==0)
		    			vib.vibrate(50);
		    		vjoy[j][5]=2;
		    	}
		    	
		      
		      if(vjoy[j][4]==-3)
		      {
        		  vjoy[j+1][0]=x-vjoy[j+1][2]/2;
        		  vjoy[j+1][1]=y-vjoy[j+1][3]/2;
        		  
        		  JNIdc.vjoy(j+1, vjoy[j+1][0], vjoy[j+1][1] , vjoy[j+1][2], vjoy[j+1][3]);
        		  anal_id=event.getPointerId(i);      	  
	          }
		      else if (vjoy[j][4]==-4) ;
	          else if(vjoy[j][4]==-1) lt=pre;
	          else if(vjoy[j][4]==-2) rt=pre;
	          else                    
	        	  rv&=~(int)vjoy[j][4];
	        }
		  }
		}
	  }
	  else
	  {
		  if (x<vjoy[11][0])
			  x=vjoy[11][0];
		  else if (x>(vjoy[11][0]+vjoy[11][2]))
			  x=vjoy[11][0]+vjoy[11][2];
		  
		  if (y<vjoy[11][1])
			  y=vjoy[11][1];
		  else if (y>(vjoy[11][1]+vjoy[11][3]))
			  y=vjoy[11][1]+vjoy[11][3];
		  
		  int j=11;
		  vjoy[j+1][0]=x-vjoy[j+1][2]/2;
		  vjoy[j+1][1]=y-vjoy[j+1][3]/2;
		  
		  JNIdc.vjoy(j+1, vjoy[j+1][0], vjoy[j+1][1] , vjoy[j+1][2], vjoy[j+1][3]);
		  
	  }
	}
  
  for(int j=0;j<vjoy.length;j++)
	{
		if (vjoy[j][5]==2)
			vjoy[j][5]=1;
		else if (vjoy[j][5]==1)
			vjoy[j][5]=0;
	}
  
  switch(aid)
  {
  	case MotionEvent.ACTION_UP:
  	case MotionEvent.ACTION_CANCEL:
  		reset_analog();
  		anal_id=-1;
  		rv=0xFFFF;
  		rt=0;
  		lt=0;
  		 for(int j=0;j<vjoy.length;j++)
  			 vjoy[j][5]=0;
	break;
	
  	case MotionEvent.ACTION_POINTER_UP:
  		if (event.getPointerId(event.getActionIndex())==anal_id)
  		{
  			reset_analog();
  			anal_id=-1;
  		}
	break;
	
  	case MotionEvent.ACTION_POINTER_DOWN:
  	case MotionEvent.ACTION_DOWN:
	break;
  }

    /*
    if(GL2JNIActivity.keys[3]!=0) rv&=~key_CONT_DPAD_RIGHT;
    if(GL2JNIActivity.keys[2]!=0) rv&=~key_CONT_DPAD_LEFT;
    if(GL2JNIActivity.keys[1]!=0) rv&=~key_CONT_A;
    if(GL2JNIActivity.keys[0]!=0) rv&=~key_CONT_B;
    */
	  
    kcode_raw = rv;
    jx = get_anal(11, 0);
    jy = get_anal(11, 1);
    return(true);
  }
  

private static class ContextFactory implements GLSurfaceView.EGLContextFactory
  {
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

    public EGLContext createContext(EGL10 egl,EGLDisplay display,EGLConfig eglConfig)
    {
      int[] attrList = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };

      LOGI("Creating OpenGL ES 2.0 context");

      checkEglError("Before eglCreateContext",egl);
      EGLContext context = egl.eglCreateContext(display,eglConfig,EGL10.EGL_NO_CONTEXT,attrList);
      checkEglError("After eglCreateContext",egl);
      return(context);
    }

    public void destroyContext(EGL10 egl,EGLDisplay display,EGLContext context)
    {
      LOGI("Destroying OpenGL ES 2.0 context");
      egl.eglDestroyContext(display,context);
    }
  }

  private static void checkEglError(String prompt,EGL10 egl)
  {
    int error;

    while((error=egl.eglGetError()) != EGL10.EGL_SUCCESS)
      LOGE(String.format("%s: EGL error: 0x%x",prompt,error));
  }

  private static class ConfigChooser implements GLSurfaceView.EGLConfigChooser
  {
    // Subclasses can adjust these values:
    protected int mRedSize;
    protected int mGreenSize;
    protected int mBlueSize;
    protected int mAlphaSize;
    protected int mDepthSize;
    protected int mStencilSize;
    private int[] mValue = new int[1];

    public ConfigChooser(int r,int g,int b,int a,int depth,int stencil)
    {
      mRedSize     = r;
      mGreenSize   = g;
      mBlueSize    = b;
      mAlphaSize   = a;
      mDepthSize   = depth;
      mStencilSize = stencil;
    }

    // This EGL config specification is used to specify 2.0 rendering.
    // We use a minimum size of 4 bits for red/green/blue, but will
    // perform actual matching in chooseConfig() below.
    private static final int EGL_OPENGL_ES2_BIT = 4;
    private static final int[] cfgAttrs =
    {
      EGL10.EGL_RED_SIZE,        4,
      EGL10.EGL_GREEN_SIZE,      4,
      EGL10.EGL_BLUE_SIZE,       4,
      EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL10.EGL_DEPTH_SIZE,      24,
      EGL10.EGL_NONE
    };

    public EGLConfig chooseConfig(EGL10 egl,EGLDisplay display)
    {
      // Get the number of minimally matching EGL configurations
      int[] cfgCount = new int[1];
      egl.eglChooseConfig(display,cfgAttrs,null,0,cfgCount);

      if(cfgCount[0]<=0)
      {
        cfgAttrs[9]=16;
        egl.eglChooseConfig(display,cfgAttrs,null,0,cfgCount);
      }


      if(cfgCount[0]<=0)
        throw new IllegalArgumentException("No configs match configSpec");

      // Allocate then read the array of minimally matching EGL configs
      EGLConfig[] configs = new EGLConfig[cfgCount[0]];
      egl.eglChooseConfig(display,cfgAttrs,configs,cfgCount[0],cfgCount);
      
      if(DEBUG)
        printConfigs(egl,display,configs);

      // Now return the "best" one
      return(chooseConfig(egl,display,configs));
    }

    public EGLConfig chooseConfig(EGL10 egl,EGLDisplay display,EGLConfig[] configs)
    {
      for(EGLConfig config : configs)
      {
        int d = findConfigAttrib(egl,display,config,EGL10.EGL_DEPTH_SIZE,0);
        int s = findConfigAttrib(egl,display,config,EGL10.EGL_STENCIL_SIZE,0);

        // We need at least mDepthSize and mStencilSize bits
        if(d>=mDepthSize || s>=mStencilSize)
        {
          // We want an *exact* match for red/green/blue/alpha
          int r = findConfigAttrib(egl,display,config,EGL10.EGL_RED_SIZE,  0);
          int g = findConfigAttrib(egl,display,config,EGL10.EGL_GREEN_SIZE,0);
          int b = findConfigAttrib(egl,display,config,EGL10.EGL_BLUE_SIZE, 0);
          int a = findConfigAttrib(egl,display,config,EGL10.EGL_ALPHA_SIZE,0);

          if(r==mRedSize && g==mGreenSize && b==mBlueSize && a==mAlphaSize)
            return(config);
        }
      }

      return(null);
    }

    private int findConfigAttrib(EGL10 egl,EGLDisplay display,EGLConfig config,int attribute,int defaultValue)
    {
      return(egl.eglGetConfigAttrib(display,config,attribute,mValue)? mValue[0] : defaultValue);
    }

    private void printConfigs(EGL10 egl,EGLDisplay display,EGLConfig[] configs)
    {
      LOGW(String.format("%d configurations",configs.length));

      for(int i=0 ; i<configs.length ; i++)
      {
        LOGW(String.format("Configuration %d:",i));
        printConfig(egl,display,configs[i]);
      }
    }
 
    private void printConfig(EGL10 egl,EGLDisplay display,EGLConfig config)
    {
      final int[] attributes =
      {
        EGL10.EGL_BUFFER_SIZE,
        EGL10.EGL_ALPHA_SIZE,
        EGL10.EGL_BLUE_SIZE,
        EGL10.EGL_GREEN_SIZE,
        EGL10.EGL_RED_SIZE,
        EGL10.EGL_DEPTH_SIZE,
        EGL10.EGL_STENCIL_SIZE,
        EGL10.EGL_CONFIG_CAVEAT,
        EGL10.EGL_CONFIG_ID,
        EGL10.EGL_LEVEL,
        EGL10.EGL_MAX_PBUFFER_HEIGHT,
        EGL10.EGL_MAX_PBUFFER_PIXELS,
        EGL10.EGL_MAX_PBUFFER_WIDTH,
        EGL10.EGL_NATIVE_RENDERABLE,
        EGL10.EGL_NATIVE_VISUAL_ID,
        EGL10.EGL_NATIVE_VISUAL_TYPE,
        0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
        EGL10.EGL_SAMPLES,
        EGL10.EGL_SAMPLE_BUFFERS,
        EGL10.EGL_SURFACE_TYPE,
        EGL10.EGL_TRANSPARENT_TYPE,
        EGL10.EGL_TRANSPARENT_RED_VALUE,
        EGL10.EGL_TRANSPARENT_GREEN_VALUE,
        EGL10.EGL_TRANSPARENT_BLUE_VALUE,
        0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
        0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
        0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
        0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
        EGL10.EGL_LUMINANCE_SIZE,
        EGL10.EGL_ALPHA_MASK_SIZE,
        EGL10.EGL_COLOR_BUFFER_TYPE,
        EGL10.EGL_RENDERABLE_TYPE,
        0x3042 // EGL10.EGL_CONFORMANT
      };

      final String[] names =
      {
        "EGL_BUFFER_SIZE",
        "EGL_ALPHA_SIZE",
        "EGL_BLUE_SIZE",
        "EGL_GREEN_SIZE",
        "EGL_RED_SIZE",
        "EGL_DEPTH_SIZE",
        "EGL_STENCIL_SIZE",
        "EGL_CONFIG_CAVEAT",
        "EGL_CONFIG_ID",
        "EGL_LEVEL",
        "EGL_MAX_PBUFFER_HEIGHT",
        "EGL_MAX_PBUFFER_PIXELS",
        "EGL_MAX_PBUFFER_WIDTH",
        "EGL_NATIVE_RENDERABLE",
        "EGL_NATIVE_VISUAL_ID",
        "EGL_NATIVE_VISUAL_TYPE",
        "EGL_PRESERVED_RESOURCES",
        "EGL_SAMPLES",
        "EGL_SAMPLE_BUFFERS",
        "EGL_SURFACE_TYPE",
        "EGL_TRANSPARENT_TYPE",
        "EGL_TRANSPARENT_RED_VALUE",
        "EGL_TRANSPARENT_GREEN_VALUE",
        "EGL_TRANSPARENT_BLUE_VALUE",
        "EGL_BIND_TO_TEXTURE_RGB",
        "EGL_BIND_TO_TEXTURE_RGBA",
        "EGL_MIN_SWAP_INTERVAL",
        "EGL_MAX_SWAP_INTERVAL",
        "EGL_LUMINANCE_SIZE",
        "EGL_ALPHA_MASK_SIZE",
        "EGL_COLOR_BUFFER_TYPE",
        "EGL_RENDERABLE_TYPE",
        "EGL_CONFORMANT"
      };

      int[] value = new int[1];

      for(int i=0 ; i<attributes.length ; i++)
        if(egl.eglGetConfigAttrib(display,config,attributes[i],value))
          LOGI(String.format("  %s: %d\n",names[i],value[0]));
        else
          while(egl.eglGetError()!=EGL10.EGL_SUCCESS);
    }
  }

  private static class Renderer implements GLSurfaceView.Renderer
  {
    public void onDrawFrame(GL10 gl)
    {
      //Log.w("INPUT", " " + kcode_raw + " " + rt + " " + lt + " " + jx + " " + jy);
      JNIdc.kcode(kcode_raw,lt,rt,jx,jy);
      // Natively update nullDC display
      JNIdc.rendframe();
    }

    public void onSurfaceChanged(GL10 gl,int width,int height)
    {
    	JNIdc.rendinit(width,height);
    }

    public void onSurfaceCreated(GL10 gl,EGLConfig config)
    {
    	onSurfaceChanged(gl, 800, 480);
    }
  }


  class EmuThread extends Thread
  {
	AudioTrack Player;
	long pos;	//write position
	long size;	//size in frames
	
    @Override public void run()
    {
    	int min=AudioTrack.getMinBufferSize(44100,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);
    	
    	if (2048>min)
    		min=2048;
    	
    	Player = new AudioTrack(
    	        AudioManager.STREAM_MUSIC,
    	        44100,
    	        AudioFormat.CHANNEL_OUT_STEREO,
    	        AudioFormat.ENCODING_PCM_16BIT,
    	        min,
    	        AudioTrack.MODE_STREAM
    	      );
    	
    	size=min/4; 
    	pos=0;
    	
    	Log.i("audcfg", "Audio streaming: buffer size " + min + " samples / " + min/44100.0 + " ms");
    	Player.play();
    	 
    	JNIdc.run(this);
    }
    
    int WriteBuffer(short[] samples, int wait)
    {
    	int newdata=samples.length/2;
    	
    	if (wait==0)
    	{
    		//user bytes = write-read
    		//available = size - (write - play)
    		long used=pos-Player.getPlaybackHeadPosition();
    		long avail=size-used;
    		
    		//Log.i("AUD", "u: " + used + " a: " + avail);
    		if (avail<newdata)
    			return 0;
    	}
    	
    	pos+=newdata;
    	
    	Player.write(samples, 0, samples.length);
    	
    	return 1;
    }
  }
  
  //
  // Thread responsible for playing audio.
  //
  class AudioThready extends Thread
  {
    private AudioTrack Player;
    private int Rate;
    private int Latency;
    private int Chunk;
    private short Data[];

    public AudioThready(int AudioRate,int AudioLatency)
    {
      Rate    = (AudioRate==0)||(AudioRate>=8192)? AudioRate:8192;
      Latency = AudioLatency>=50? AudioLatency:50;
      Chunk   = 2048;
      Data    = new short[Chunk*2];
      start();
    }

    public void stopPlayback()
    { Rate=0; }

    public void pausePlayback(boolean Switch)
    {
      // Must have a valid player
      if((Player==null) || (Player.getState()!=AudioTrack.STATE_INITIALIZED)) return;

      // Switch between playback and pause
      if(Switch) { if(Player.getPlayState()==AudioTrack.PLAYSTATE_PLAYING) Player.pause(); }
      else       { if(Player.getPlayState()!=AudioTrack.PLAYSTATE_PLAYING) Player.play(); }
    }

    @Override public void run()
    {
      int Size,Min;

      LOGI("Starting audio thread for Rate="+Rate+"Hz, Latency="+Latency+"ms");

      // When no audio sampling rate supplied, do not play
      if(Rate<=0) return;

      // Compute minimal and requested buffer sizes
      Min  = AudioTrack.getMinBufferSize(Rate,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);
      Size = 2*2*Chunk*2;

      // Create audio player
      Player = new AudioTrack(
        AudioManager.STREAM_MUSIC,
        Rate,
        AudioFormat.CHANNEL_OUT_STEREO,
        AudioFormat.ENCODING_PCM_16BIT,
        Min>Size? Min:Size,
        AudioTrack.MODE_STREAM
      );

      // Start playback
      Player.play();
 
      // Continue writing data, until requested to quit
      while(Rate>0)
      {
        //Size = JNIdc.play(Data,Chunk);
        if(Size>0) Player.write(Data,0,2*Size); else yield();
      }

      // Stop playback
      Player.stop();
      Player.flush();
      Player.release();

      LOGI("Exiting audio thread");
    }
  }

public void onStop() {
	// TODO Auto-generated method stub
	System.exit(0);
	try {
		ethd.join();
	} catch (InterruptedException e) {
		// TODO Auto-generated catch block
		e.printStackTrace();
	}
}
}
