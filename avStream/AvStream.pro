TARGET   = AvStream

QT += core gui widgets multimedia

INCLUDEPATH +=  $$(FFMpeg)/include src

HEADERS = \
    src/AvStream.h \
    src/AvThread.h \
    src/ImageWidget.h \
    src/AudioDevice.h

SOURCES = \
    src/AvStream.cpp \
    src/AvThread.cpp \
    src/ImageWidget.cpp \
    src/AudioDevice.cpp \
    main.cpp

RESOURCES += \
    AvStream.qrc

FORMS += \
    src/AvStream.ui \

LIBS += -L$$(FFMpeg)/lib
LIBS += -lavcodec -lavformat -lswscale -lavutil -lswresample
