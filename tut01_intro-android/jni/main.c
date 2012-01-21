/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012 Sylvain Beucler
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
 *
 */

//BEGIN_INCLUDE(all)
#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
//#include <GLES2/gl2ext.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

/* <MiniGLUT> */
#include "GL/glut.h"
static void (*miniglutDisplayCallback)(void) = NULL;
static void (*miniglutIdleCallback)(void) = NULL;
static void (*miniglutReshapeCallback)(int,int) = NULL;
static unsigned int miniglutDisplayMode = 0;
static struct engine engine;
#include <sys/time.h>  /* gettimeofday */
static long miniglutStartTimeMillis = 0;
static int miniglutTermWindow = 0;
#include <stdlib.h>  /* exit */
#include <stdio.h>  /* BUFSIZ */
/* </MiniGLUT> */


/**
 * Our saved state data.
 */
struct saved_state {
    int32_t x;
    int32_t y;
};

/**
 * Shared state for our app.
 */
struct engine {
    struct android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    struct saved_state state;

    int miniglutInit;
};

/**
 * Initialize an EGL context for the current display.
 */
static int engine_init_display(struct engine* engine) {
    engine->miniglutInit = 1;

    // initialize OpenGL ES and EGL

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */
    // Ensure OpenGLES 2.0 context (mandatory)
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
	    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	    EGL_DEPTH_SIZE, (miniglutDisplayMode & GLUT_DEPTH) ? 24 : 0,
            EGL_NONE
    };
    EGLint w, h, format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    // TODO : apply miniglutDisplayMode

    eglInitialize(display, 0, 0);

    /* Here, the application chooses the configuration it desires. In this
     * sample, we have a very simplified selection process, where we pick
     * the first EGLConfig that matches our criteria */
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    // Ensure OpenGLES 2.0 context (mandatory)
    static const EGLint ctx_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attribs);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    engine->display = display;
    engine->context = context;
    engine->surface = surface;
    engine->width = w;
    engine->height = h;

    // miniglut:
    glViewport(0, 0, 480, 800);

    return 0;
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(struct engine* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

/**
 * Process the next input event.
 */
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* engine = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        engine->state.x = AMotionEvent_getX(event, 0);
        engine->state.y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            // The system has asked us to save our current state.  Do so.
            engine->app->savedState = malloc(sizeof(struct saved_state));
            *((struct saved_state*)engine->app->savedState) = engine->state;
            engine->app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (engine->app->window != NULL) {
                engine_init_display(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            //engine_term_display(engine);
            miniglutTermWindow = 1;
            break;
        case APP_CMD_GAINED_FOCUS:
            // When our app gains focus, we start monitoring the accelerometer.
            if (engine->accelerometerSensor != NULL) {
                ASensorEventQueue_enableSensor(engine->sensorEventQueue,
                        engine->accelerometerSensor);
                // We'd like to get 60 events per second (in us).
                ASensorEventQueue_setEventRate(engine->sensorEventQueue,
                        engine->accelerometerSensor, (1000L/60)*1000);
            }
            break;
        case APP_CMD_LOST_FOCUS:
            // When our app loses focus, we stop monitoring the accelerometer.
            // This is to avoid consuming battery while not being used.
            if (engine->accelerometerSensor != NULL) {
                ASensorEventQueue_disableSensor(engine->sensorEventQueue,
                        engine->accelerometerSensor);
            }
            break;
    }
}

void print_info_paths(struct android_app* state_param) {
    JNIEnv* env = state_param->activity->env;
    jclass activityClass = (*env)->GetObjectClass(env, state_param->activity->clazz);

    jclass fileClass = (*env)->FindClass(env, "java/io/File");
    jmethodID getAbsolutePath = (*env)->GetMethodID(env, fileClass, "getAbsolutePath", "()Ljava/lang/String;");

    {
	// /data/data/org.wikibooks.OpenGL/files
	jmethodID method = (*env)->GetMethodID(env, activityClass, "getFilesDir", "()Ljava/io/File;");
	jobject file = (*env)->CallObjectMethod(env, state_param->activity->clazz, method);
	jobject jpath = (*env)->CallObjectMethod(env, file, getAbsolutePath);
	const char* dir = (*env)->GetStringUTFChars(env, (jstring) jpath, NULL);
	LOGI("%s", dir);
	(*env)->ReleaseStringUTFChars(env, jpath, dir);
    }

    {
	// /data/data/org.wikibooks.OpenGL/cache
	jmethodID method = (*env)->GetMethodID(env, activityClass, "getCacheDir", "()Ljava/io/File;");
	jobject file = (*env)->CallObjectMethod(env, state_param->activity->clazz, method);
	jobject jpath = (*env)->CallObjectMethod(env, file, getAbsolutePath);
	const char* dir = (*env)->GetStringUTFChars(env, (jstring) jpath, NULL);
	LOGI("%s", dir);
	(*env)->ReleaseStringUTFChars(env, jpath, dir);
    }

    // getExternalCacheDir -> ApplicationContext: unable to create external cache directory

    {
	// /data/app/org.wikibooks.OpenGL-X.apk
	// /mnt/asec/org.wikibooks.OpenGL-X/pkg.apk
	jmethodID method = (*env)->GetMethodID(env, activityClass, "getPackageResourcePath", "()Ljava/lang/String;");
	jobject jpath = (*env)->CallObjectMethod(env, state_param->activity->clazz, method);
	const char* dir = (*env)->GetStringUTFChars(env, (jstring) jpath, NULL);
	LOGI("%s", dir);
	(*env)->ReleaseStringUTFChars(env, jpath, dir);
    }

    {
	// /data/app/org.wikibooks.OpenGL-X.apk
	// /mnt/asec/org.wikibooks.OpenGL-X/pkg.apk
	jmethodID method = (*env)->GetMethodID(env, activityClass, "getPackageCodePath", "()Ljava/lang/String;");
	jobject jpath = (*env)->CallObjectMethod(env, state_param->activity->clazz, method);
	const char* dir = (*env)->GetStringUTFChars(env, (jstring) jpath, NULL);
	LOGI("%s", dir);
	(*env)->ReleaseStringUTFChars(env, jpath, dir);
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
struct android_app* state;
void android_main(struct android_app* state_param) {
    LOGI("android_main");
    state = state_param;

    // Get usable JNI context
    JNIEnv* env = state_param->activity->env;
    JavaVM* vm = state_param->activity->vm;
    (*vm)->AttachCurrentThread(vm, &env, NULL);

    // Get path to cache dir (/data/data/org.wikibooks.OpenGL/cache)
    jclass activityClass = (*env)->GetObjectClass(env, state_param->activity->clazz);
    jclass fileClass = (*env)->FindClass(env, "java/io/File");
    jmethodID getAbsolutePath = (*env)->GetMethodID(env, fileClass, "getAbsolutePath", "()Ljava/lang/String;");

    jmethodID getFilesDir = (*env)->GetMethodID(env, activityClass, "getCacheDir", "()Ljava/io/File;");
    jobject file = (*env)->CallObjectMethod(env, state_param->activity->clazz, getFilesDir);
    jobject jpath = (*env)->CallObjectMethod(env, file, getAbsolutePath);
    const char* app_dir = (*env)->GetStringUTFChars(env, (jstring) jpath, NULL);

    // chdir in the application file directory
    LOGI("app_dir: %s", app_dir);
    chdir(app_dir);
    (*env)->ReleaseStringUTFChars(env, jpath, app_dir);
    print_info_paths(state_param);

    // Pre-extract assets, to avoid Android-specific file opening
    jmethodID getAssets = (*env)->GetMethodID(env, activityClass, "getAssets", "()Landroid/content/res/AssetManager;"); 
    jobject assetManager = (*env)->CallObjectMethod(env, state_param->activity->clazz, getAssets);
    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    AAssetDir* assetDir = AAssetManager_openDir(mgr, "");
    char* filename = NULL;
    while ((filename = AAssetDir_getNextFileName(assetDir)) != NULL) {
	AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_STREAMING);
	char buf[BUFSIZ];
	int nb_read = 0;
	FILE* out = fopen(filename, "w");
	while ((nb_read = AAsset_read(asset, buf, BUFSIZ)) > 0)
	    fwrite(buf, nb_read, 1, out);
	fclose(out);
	AAsset_close(asset);
    }
    AAssetDir_close(assetDir);

    // Call user's main
    main();

    // Destroy OpenGL context
    engine_term_display(&engine);

    LOGI("android_main: end");
    exit(0);
}

void process_events() {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

	// We loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident=ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0) {

            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }

            // If a sensor has data, process it now.
            if (ident == LOOPER_ID_USER) {
                if (engine.accelerometerSensor != NULL) {
                    ASensorEvent event;
                    while (ASensorEventQueue_getEvents(engine.sensorEventQueue,
                            &event, 1) > 0) {
		      ; // Don't spam the logs
		      //LOGI("accelerometer: x=%f y=%f z=%f",
                      //          event.acceleration.x, event.acceleration.y,
                      //          event.acceleration.z);
                    }
                }
            }
        }
}

void glutMainLoop() {
    LOGI("glutMainLoop");

    if (miniglutReshapeCallback != NULL)
        miniglutReshapeCallback(480, 800);

    // loop waiting for stuff to do.
    while (1) {
        process_events();

	// Check if we are exiting.
	//if (state->destroyRequested != 0) {
	//    break;
	//}

	// Check if we are exiting.
	// GLUT doesn't provide a callback to restore a lost context,
	// so we just quit the application
	if (miniglutTermWindow != 0) {
	    break;
	}

	// TODO: don't call DisplayCallback unless necessary (cf. glutPostRedisplay)
	if (miniglutIdleCallback != NULL)
	    miniglutIdleCallback();
	if (miniglutDisplayCallback != NULL)
	    miniglutDisplayCallback();
    }

    LOGI("glutMainLoop: end");
}
//END_INCLUDE(all)


void glutInit( int* pargc, char** argv ) {
    LOGI("glutInit");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    miniglutStartTimeMillis = tv.tv_sec * 1000 + tv.tv_usec/1000;
}

void glutInitDisplayMode( unsigned int displayMode ) {
    LOGI("glutInitDisplayMode");
    miniglutDisplayMode = displayMode;
}

void glutInitWindowSize( int width, int height ) {
    LOGI("glutInitWindowSize");
    // TODO?
}

int glutCreateWindow( const char* title ) {
    LOGI("glutCreateWindow");
    static int window_id = 0;
    if (window_id == 0) {
	window_id++;
    } else {
	// Only one full-screen window
	return 0;
    }
    

    // Make sure glue isn't stripped.
    app_dummy();

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    engine.app = state;

    // Prepare to monitor accelerometer
    engine.sensorManager = ASensorManager_getInstance();
    engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
            ASENSOR_TYPE_ACCELEROMETER);
    engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
            state->looper, LOOPER_ID_USER, NULL, NULL);

    if (state->savedState != NULL) {
        // We are starting with a previous saved state; restore from it.
        engine.state = *(struct saved_state*)state->savedState;
    }

    // Wait until window is available and OpenGL context is created
    while (engine.miniglutInit == 0)
	process_events();

    if (engine.display != EGL_NO_DISPLAY)
	return window_id;
    else
	return 0;
}

void glutDisplayFunc( void (* callback)( void ) ) {
    LOGI("glutDisplayFunc");
    miniglutDisplayCallback = callback;
}

void glutIdleFunc( void (* callback)( void ) ) {
    LOGI("glutIdleFunc");
    miniglutIdleCallback = callback;
}

void glutReshapeFunc(void(*callback)(int,int)) {
    LOGI("glutReshapeFunc");
    miniglutReshapeCallback = callback;
}

void glutSwapBuffers( void ) {
    //LOGI("glutSwapBuffers");
    eglSwapBuffers(engine.display, engine.surface);
}

int glutGet( GLenum query ) {
    //LOGI("glutGet");
    if (query == GLUT_ELAPSED_TIME) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	long cur_time = tv.tv_sec * 1000 + tv.tv_usec/1000;
	//LOGI("glutGet: %d", (int) cur_time - miniglutStartTimeMillis);
	return cur_time - miniglutStartTimeMillis;
    }
}

void glutPostRedisplay() {
  // TODO
}

// TODO: handle resize when screen is rotated
// TODO: handle Reshape

// Local Variables: ***
// c-basic-offset:4 ***
// End: ***