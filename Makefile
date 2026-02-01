# Makefile for Sestriere — MeshCore Client for Haiku OS
# Alternative build system for standalone compilation

NAME = Sestriere
TYPE = APP
APP_MIME_SIG = application/x-vnd.Sestriere

SRCS = \
	src/Sestriere.cpp \
	src/AboutWindow.cpp \
	src/MainWindow.cpp \
	src/SerialHandler.cpp \
	src/Protocol.cpp \
	src/ContactListView.cpp \
	src/ContactItem.cpp \
	src/ChannelItem.cpp \
	src/ChatView.cpp \
	src/MessageView.cpp \
	src/MessageStore.cpp \
	src/StatusBarView.cpp \
	src/SettingsWindow.cpp \
	src/PortSelectionWindow.cpp \
	src/LoginWindow.cpp \
	src/TracePathWindow.cpp \
	src/ContactExportWindow.cpp \
	src/NotificationManager.cpp \
	src/StatsWindow.cpp \
	src/DeskbarReplicant.cpp \
	src/MapView.cpp \
	src/MeshGraphView.cpp \
	src/TelemetryWindow.cpp

RDEFS = resources/Sestriere.rdef

LIBS = be device tracker shared localestub $(STDCPPLIBS)

LIBPATHS =

SYSTEM_INCLUDE_PATHS =

LOCAL_INCLUDE_PATHS = src

OPTIMIZE := FULL

LOCALES = en it

CFLAGS = -Wall -Wextra -Werror=return-type
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror=return-type

## Include the Makefile-Engine
DEVEL_DIRECTORY := /boot/system/develop
include $(DEVEL_DIRECTORY)/etc/makefile-engine
