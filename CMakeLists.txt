cmake_minimum_required(VERSION 3.10)

project(EnPlayer LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加whisper.cpp子项目
add_subdirectory(whisper)

# 设置whisper.cpp编译选项
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(Qt5 COMPONENTS Widgets Multimedia MultimediaWidgets Network REQUIRED)

if(ANDROID)
  add_library(EnPlayer SHARED
    src/main.cpp
    src/mainwindow.cpp
    src/speechrecognizer.cpp
    src/settingsmanager.cpp
    src/settingsdialog.cpp
    src/playbackwindow.cpp
    src/mainwindow.h
    src/speechrecognizer.h
    include/settingsmanager.h
    include/settingsdialog.h
    include/playbackwindow.h
    forms/mainwindow.ui
    forms/settingsdialog.ui
    forms/playbackwindow.ui
  )
else()
  add_executable(EnPlayer
    src/main.cpp
    src/mainwindow.cpp
    src/speechrecognizer.cpp
    src/settingsmanager.cpp
    src/settingsdialog.cpp
    src/playbackwindow.cpp
    include/mainwindow.h
    include/speechrecognizer.h
    include/settingsmanager.h
    include/settingsdialog.h
    include/playbackwindow.h
    forms/mainwindow.ui
    forms/settingsdialog.ui
    forms/playbackwindow.ui
  )
endif()

target_include_directories(EnPlayer PRIVATE include)

target_link_libraries(EnPlayer PRIVATE Qt5::Widgets Qt5::Multimedia Qt5::MultimediaWidgets Qt5::Network whisper)
