QT += core gui widgets
TEMPLATE = app
TARGET = WaveformRendererDemo
CONFIG += c++17

SOURCES += \
    main.cpp

HEADERS += \
    DataMapper.h \
    PathBuilder.h \
    BitmapRenderer.h \
    WaveformRenderer.h \
    WaveformWidget.h
