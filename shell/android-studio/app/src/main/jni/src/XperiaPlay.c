/*
 * Copyright (c) 2011, Sony Ericsson Mobile Communications AB.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Sony Ericsson Mobile Communications AB nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <android/log.h>
#include <jni.h>
#include <errno.h>
#include <android_native_app_glue.h>
#include <time.h>
#include <unistd.h>
#include <sys/system_properties.h>

#define EXPORT_XPLAY __attribute__ ((visibility("default")))

#define TAG "reicast"
#define LOGW(...) ((void)__android_log_print( ANDROID_LOG_WARN, TAG, __VA_ARGS__ ))

#undef NUM_METHODS
#define NUM_METHODS(x) (sizeof(x)/sizeof(*(x)))

static JavaVM *jVM;

typedef unsigned char BOOL;
#define FALSE 0
#define TRUE 1

static jobject		g_pActivity		= 0;
static jmethodID	javaOnNDKTouch	= 0;
static jmethodID	javaOnNDKKey	= 0;

static bool isXperiaPlay;

/**
 * Our saved state data.
 */
struct TOUCHSTATE
{
    int	 down;
    int	 x;
    int	 y;
};

/**
 * Shared state for our app.
 */
struct ENGINE
{
    struct android_app* app;
    int	 render;
    int	 width;
    int	 height;
    int	 has_focus;
    //ugly way to track touch states
    struct TOUCHSTATE touchstate_screen[64];
    struct TOUCHSTATE touchstate_pad[64];
};

void attach(){

}

/**
 * Process the next input event.
 */
static
int32_t
engine_handle_input( struct android_app* app, AInputEvent* event )
{
	JNIEnv *jni;
	(*jVM)->AttachCurrentThread(jVM, &jni, NULL);

	struct ENGINE* engine = (struct ENGINE*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY){
        int device = AInputEvent_getDeviceId(event);
        int action = AKeyEvent_getAction(event);
        int keyCode = AKeyEvent_getKeyCode(event);
        if(jni && g_pActivity){
            if((*jni)->ExceptionCheck(jni)) {
                (*jni)->ExceptionDescribe(jni);
                (*jni)->ExceptionClear(jni);
            }
            (*jni)->CallIntMethod(jni, g_pActivity, javaOnNDKKey, device, keyCode, action, AKeyEvent_getMetaState(event));
            if (!(keyCode == AKEYCODE_MENU || keyCode == AKEYCODE_BACK || keyCode == AKEYCODE_BUTTON_THUMBR || keyCode == AKEYCODE_VOLUME_UP || keyCode == AKEYCODE_VOLUME_DOWN || keyCode == AKEYCODE_BUTTON_SELECT)) {
                return 1;
            }
        }
    } else if( AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION ) {
        int device = AInputEvent_getDeviceId( event );
        int nSourceId		= AInputEvent_getSource( event );
        int nPointerCount	= AMotionEvent_getPointerCount( event );
        int n;

        jboolean newTouch = JNI_TRUE;
        for( n = 0 ; n < nPointerCount ; ++n )
        {
            int nPointerId	= AMotionEvent_getPointerId( event, n );
            int nAction		= AMOTION_EVENT_ACTION_MASK & AMotionEvent_getAction( event );
            int nRawAction	= AMotionEvent_getAction( event );

            struct TOUCHSTATE *touchstate = 0;
            if( nSourceId == AINPUT_SOURCE_TOUCHPAD ) {
                touchstate = engine->touchstate_pad;
            } else {
                touchstate = engine->touchstate_screen;
            }

            if( nAction == AMOTION_EVENT_ACTION_POINTER_DOWN || nAction == AMOTION_EVENT_ACTION_POINTER_UP )
            {
                int nPointerIndex = (AMotionEvent_getAction( event ) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                nPointerId = AMotionEvent_getPointerId( event, nPointerIndex );
            }

            if( nAction == AMOTION_EVENT_ACTION_DOWN || nAction == AMOTION_EVENT_ACTION_POINTER_DOWN )
            {
                touchstate[nPointerId].down = 1;
            }
            else if( nAction == AMOTION_EVENT_ACTION_UP || nAction == AMOTION_EVENT_ACTION_POINTER_UP || nAction == AMOTION_EVENT_ACTION_CANCEL )
            {
                touchstate[nPointerId].down = 0;
            }

            if (touchstate[nPointerId].down == 1)
            {
                touchstate[nPointerId].x = AMotionEvent_getX( event, n );
                touchstate[nPointerId].y = AMotionEvent_getY( event, n );
            }

            if( jni && g_pActivity && isXperiaPlay) {
//                (*jni)->CallVoidMethod( jni, g_pActivity, javaOnNDKTouch, device, nSourceId, nRawAction, touchstate[nPointerId].x, touchstate[nPointerId].y, newTouch);
                (*jni)->CallVoidMethod( jni, g_pActivity, javaOnNDKTouch, device, nSourceId, nRawAction, touchstate[nPointerId].x, touchstate[nPointerId].y);
            }
            newTouch = JNI_FALSE;
        }

        if( isXperiaPlay ) {
            return 1;
        } else {
            return 0;
        }
    }
	return 0;
}

/**
 * Process the next main command.
 */
static
void
engine_handle_cmd( struct android_app* app, int32_t cmd )
{
    struct ENGINE* engine = (struct ENGINE*)app->userData;
    switch( cmd )
    {
        case APP_CMD_SAVE_STATE:
            // The system has asked us to save our current state. Do so if needed
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if( engine->app->window != NULL )
            {
                engine->has_focus = 1;
            }
            break;

        case APP_CMD_GAINED_FOCUS:
            engine->has_focus = 1;
            break;

        case APP_CMD_LOST_FOCUS:
            // When our app loses focus, we stop rendering.
            engine->render = 0;
            engine->has_focus = 0;
            //engine_draw_frame( engine );
            break;
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue. It runs in its own thread, with its own
 * event loop for receiving input events and doing other things (rendering).
 */
void
android_main( struct android_app* state )
{
    struct ENGINE engine;

    // Make sure glue isn't stripped.
    app_dummy();

    memset( &engine, 0, sizeof(engine) );
    state->userData	 = &engine;
    state->onAppCmd	 = engine_handle_cmd;
    state->onInputEvent	= engine_handle_input;
    engine.app	 = state;

    //setup(state);
    //JNIEnv *env;
    //(*jVM)->AttachCurrentThread(jVM, &env, NULL);

    if( state->savedState != NULL )
    {
        // We are starting with a previous saved state; restore from it.
    }
    // our 'main loop'
    while( 1 )
    {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;
        // If not rendering, we will block forever waiting for events.
        // If rendering, we loop until all events are read, then continue
        // to draw the next frame.
        while( (ident = ALooper_pollAll( 250, NULL, &events, (void**)&source) ) >= 0 )
        {
            // Process this event.
            // This will call the function pointer android_app:nInputEvent() which in our case is
            // engine_handle_input()
            if( source != NULL )
            {
                source->process( state, source );
            }
            // Check if we are exiting.
            if( state->destroyRequested != 0 )
            {
                return;
            }
            //usleep(20000);	//20 miliseconds
        }
    }
}

static
int
RegisterNative( JNIEnv* env, jobject clazz, jboolean touchpad )
{
	g_pActivity = (jobject)(*env)->NewGlobalRef( env, clazz );
    isXperiaPlay = (bool) touchpad;
	return 0;
}

static const JNINativeMethod activity_methods[] =
{
    { "RegisterNative",	"(Z)I",	(void*)RegisterNative },
};

jint EXPORT_XPLAY JNICALL JNI_OnLoad(JavaVM * vm, void * reserved)
{
    JNIEnv *env;
    jVM = vm;
    if((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        LOGW("%s - Failed to get the environment using GetEnv()", __FUNCTION__);
        return -1;
    }
    const char* interface_path = "com/reicast/emulator/GL2JNINative";
    jclass java_activity_class = (*env)->FindClass( env, interface_path );

    if( !java_activity_class )
	{
		LOGW( "%s - Failed to get %s class reference", __FUNCTION__, interface_path );
		return -1;
	}

    if( (*env)->RegisterNatives( env, java_activity_class, activity_methods, NUM_METHODS(activity_methods) ) != JNI_OK )
    {
		LOGW( "%s - Failed to register native activity methods", __FUNCTION__ );
		return -1;
	}

    char device_type[PROP_VALUE_MAX];
    __system_property_get("ro.product.model", device_type);
    if( isXperiaPlay ) {
        LOGW( "%s touchpad enabled", device_type );
    } else {
        LOGW( "%s touchpad ignored", device_type );
    }

//    javaOnNDKTouch	= (*env)->GetMethodID( env, java_activity_class, "OnNativeMotion", "(IIIIIZ)Z");
    javaOnNDKTouch	= (*env)->GetMethodID( env, java_activity_class, "OnNativeMotion", "(IIIII)Z");
    javaOnNDKKey	= (*env)->GetMethodID( env, java_activity_class, "OnNativeKeyPress", "(IIII)Z");

    return JNI_VERSION_1_4;
}
