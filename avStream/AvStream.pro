TARGET   = AvStream

QT += core gui widgets

INCLUDEPATH +=  $$(FFMpeg)/include

HEADERS = \
    src/AvStream.h \
    src/AvThread.h \
    src/ImageWidget.h

SOURCES = \
    src/AvStream.cpp \
    src/AvThread.cpp \
    src/ImageWidget.cpp \
    main.cpp

RESOURCES += \
    AvStream.qrc

FORMS += \
    src/AvStream.ui \

LIBS += -LE:\project\work\ffmpeg-7.0-full_build-shared\lib -lavcodec -lavformat -lswscale -lavutil
