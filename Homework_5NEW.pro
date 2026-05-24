TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt
SOURCES += main.cpp

MINGW_OPENCV = C:/OpenCV-MinGW-Build-OpenCV-4.5.5-x64

INCLUDEPATH += $$MINGW_OPENCV/include

LIBS += -L"$$MINGW_OPENCV/x64/mingw/lib"

# Подключаем ключевые модули, которые используются в коде
LIBS += -lopencv_core455 \
        -lopencv_highgui455 \
        -lopencv_imgproc455 \
        -lopencv_imgcodecs455