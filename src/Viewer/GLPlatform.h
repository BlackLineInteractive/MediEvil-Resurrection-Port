#pragma once

#if defined(__vita__) || defined(VITA)
    #include <vitaGL.h>
    #ifndef MED_PLATFORM_VITA
        #define MED_PLATFORM_VITA 1
    #endif
    #ifndef MED_GLES
        #define MED_GLES 1
    #endif
#elif defined(MED_GLES)
    #if defined(__APPLE__)
        #include <TargetConditionals.h>
        #if TARGET_OS_IPHONE
            #include <OpenGLES/ES2/gl.h>
            #include <OpenGLES/ES2/glext.h>
        #else
            #include <GL/glew.h>
        #endif
    #elif defined(__ANDROID__) || defined(__EMSCRIPTEN__)
        #include <GLES2/gl2.h>
        #include <GLES2/gl2ext.h>
    #else
        #include <GL/glew.h>
    #endif
#else
    #include <GL/glew.h>
#endif
