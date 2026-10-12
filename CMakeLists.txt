cmake_minimum_required(VERSION 3.16)
project("MediEvil Resurrection" VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MED_USE_GLES "Build with OpenGL ES 2.0 / VitaGL backend" ON)
if(MED_USE_GLES)
    add_compile_definitions(MED_GLES)
endif()

if(DEFINED PLATFORM_VITA OR DEFINED VITA OR CMAKE_SYSTEM_NAME STREQUAL "Vita")
    set(MED_PLATFORM_VITA ON)
    add_compile_definitions(__vita__ VITA MED_PLATFORM_VITA=1 MED_GLES=1)
endif()

include(FetchContent)

if(MED_PLATFORM_VITA)
    # PlayStation Vita (VitaSDK + vitaGL)
    find_package(vitaGL REQUIRED)
    find_package(SDL2 REQUIRED)
    set(MED_GL_LIBS vitaGL vitashark SceShaccCg_stub SceGxm_stub SceCtrl_stub SceTouch_stub SceMotion_stub SceAppMgr_stub SceCommonDialog_stub ScePower_stub)
    set(MED_SDL2_TARGETS SDL2)
else()
    # Desktop / Mobile host
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        set(MED_SDL2_TARGETS SDL2::SDL2)
        if(TARGET SDL2::SDL2main)
            list(PREPEND MED_SDL2_TARGETS SDL2::SDL2main)
        endif()
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED sdl2)
        set(MED_SDL2_TARGETS ${SDL2_LIBRARIES})
        include_directories(${SDL2_INCLUDE_DIRS})
    endif()
    set(MED_GL_LIBS GLEW::GLEW OpenGL::GL)
endif()

find_package(glm CONFIG QUIET)
if(NOT TARGET glm::glm)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glm)
endif()

# Core static library
add_library(medievil-core STATIC
    src/Core/GameData.cpp
    src/Core/Texture.cpp
    src/Core/Geometry.cpp
    src/Core/Land.cpp
    src/Core/World.cpp
    src/Core/Entity.cpp
    src/Core/Skeleton.cpp
)
target_include_directories(medievil-core PUBLIC src)
target_link_libraries(medievil-core PUBLIC glm::glm)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-configure
)
FetchContent_MakeAvailable(imgui)

add_executable(medievil-viewer
    src/Viewer/main.cpp
    src/Viewer/Renderer.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(medievil-viewer PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends src)
target_link_libraries(medievil-viewer PRIVATE medievil-core ${MED_SDL2_TARGETS} ${MED_GL_LIBS})
target_compile_definitions(medievil-viewer PRIVATE GL_SILENCE_DEPRECATION)
