QT += charts widgets sql
requires(qtConfig(tableview))

HEADERS += \
    loadtrace.h \
    tablewidget.h \
    sql/connection.h

SOURCES += \
    loadtrace.cpp \
    main.cpp \
    tablewidget.cpp

target.path = ../procvib
INSTALLS += target
