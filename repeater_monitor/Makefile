# Repeater Monitor - Standalone Makefile for Haiku OS
#
# Standalone repeater log analyzer with serial terminal

NAME = RepeaterMonitor
TYPE = APP
SRCS = \
	RepMonApp.cpp \
	RepMonWindow.cpp \
	RepeaterMonitorView.cpp \
	SerialMonitorWindow.cpp \
	SerialHandler.cpp

RDEFS = RepMon.rdef

LIBS = be tracker shared columnlistview $(STDCPPLIBS)

SYSTEM_INCLUDE_PATHS = /boot/system/develop/headers/private/interface

LOCAL_INCLUDE_PATHS = .

OPTIMIZE := FULL

LOCALES =

DEFINES =

WARNINGS = ALL

SYMBOLS := FALSE

DEBUGGER := FALSE

COMPILER_FLAGS =

LINKER_FLAGS =

APP_VERSION =

DRIVER_PATH =

## Include the Makefile-Engine
DEVEL_DIRECTORY := /boot/system/develop
include $(DEVEL_DIRECTORY)/etc/makefile-engine
