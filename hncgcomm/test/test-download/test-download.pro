# ----------------------------------------------------------
# Project generated by QMsNet v1.0.0
#    Template used:    Basic Application
#    Template version: 1.1
# ----------------------------------------------------------

TEMPLATE       = app
TARGET         = test-download
LANGUAGE       = C++
CONFIG        += qt warn_on

unix {
   UI_DIR      = .ui
   MOC_DIR     = .moc
   OBJECTS_DIR = .obj
}


FORMS += test_download.ui
SOURCES += $(FORMS) main.cpp
