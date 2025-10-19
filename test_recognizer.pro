QT += core
CONFIG += console
CONFIG -= app_bundle

SOURCES += test_recognizer.cpp

TARGET = test_recognizer
DESTDIR = build

# 设置UTF-8编码
QMAKE_CXXFLAGS += -std=c++11