#pragma once

#if defined(MED_GLES)
    #if defined(__APPLE__)
        #include <TargetConditionals.h>
        #if TARGET_OS_IPHONE
            #include <OpenGLES/ES3/gl.h>
        #else
            #include <GL/glew.h>
        #endif
    #elif defined(__ANDROID__) || defined(__EMSCRIPTEN__)
        #include <GLES3/gl3.h>
    #else
        #include <GLES3/gl3.h>
    #endif
#else
    #include <GL/glew.h>
#endif
