#-------------------------------------------------
#
# Project created by QtCreator 2016-01-31T15:21:30
#
#-------------------------------------------------

QT       += core

QT       += widgets

TARGET = CVKernel
CONFIG   += console
CONFIG   -= app_bundle
QMAKE_CXXFLAGS  += -std=c++11 -fopenmp -O3
QMAKE_LFLAGS    += -fopenmp
LIBS += `pkg-config --cflags --libs opencv`
TEMPLATE = app


SOURCES += main.cpp \
    cvgraphnode.cpp \
    cvprocessmanager.cpp \
    videoproc/rfiremaskingmodel.cpp \
    videoproc/yfiremaskingmodel.cpp \
    videoproc/firevalidation.cpp \
    videoproc/firebbox.cpp \
    videoproc/fireweight.cpp \
    videoproc/flamesrcbbox.cpp

HEADERS += \
    cvgraphnode.h \
    cvprocessmanager.h \
    videoproc/rfiremaskingmodel.h \
    videoproc/yfiremaskingmodel.h \
    videoproc/firevalidation.h \
    videoproc/firebbox.h \
    videoproc/fireweight.h \
    videoproc/flamesrcbbox.h
