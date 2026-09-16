
cmake_minimum_required(VERSION 3.22.1)

project("menu")

add_library(shadowhook STATIC IMPORTED)
set_target_properties(shadowhook PROPERTIES
        IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/shadowhook/libs/arm64-v8a/libshadowhook.a)

set(IMGUI_SOURCES
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_android.cpp
        imgui/backends/imgui_impl_opengl3.cpp)

set(MENU_SOURCES
        Utilities/common/Log.cpp
        jni/GlesJniBridge.cpp
        jni/JniClassLoader.cpp
        jni/JniString.cpp
        main.cpp
        payload/JavaVmResolver.cpp
        payload/PayloadBootstrap.cpp
        render/ImGuiRenderer.cpp
        render/MenuRenderer.cpp)

add_library(${CMAKE_PROJECT_NAME} SHARED
        ${IMGUI_SOURCES}
        ${MENU_SOURCES})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Utilities
        ${CMAKE_CURRENT_SOURCE_DIR}/jni
        ${CMAKE_CURRENT_SOURCE_DIR}/payload
        ${CMAKE_CURRENT_SOURCE_DIR}/render
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/common
        ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/Font
        ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/shadowhook/include)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET hidden)

target_link_libraries(${CMAKE_PROJECT_NAME}
        shadowhook
        EGL
        GLESv3
        android
        log)
