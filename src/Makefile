# Sestriere Minimal - Makefile for Haiku OS
#
# Build a minimal version with log window and radio preset settings

NAME = Sestriere
TYPE = APP
SRCS = \
	Sestriere.cpp \
	MainWindow.cpp \
	AddChannelWindow.cpp \
	SerialHandler.cpp \
	ChatView.cpp \
	ChatHeaderView.cpp \
	MessageView.cpp \
	ContactItem.cpp \
	NotificationManager.cpp \
	DebugLogWindow.cpp \
	DeskbarReplicant.cpp \
	SettingsWindow.cpp \
	StatsWindow.cpp \
	TracePathWindow.cpp \
	NetworkMapWindow.cpp \
	MqttClient.cpp \
	MqttLogWindow.cpp \
	TelemetryWindow.cpp \
	LoginWindow.cpp \
	MapView.cpp \
	ContactExportWindow.cpp \
	ProfileWindow.cpp \
	TopBarView.cpp \
	ContactInfoPanel.cpp \
	GrowingTextView.cpp \
	DatabaseManager.cpp \
	SNRChartView.cpp \
	PacketAnalyzerWindow.cpp \
	ProtocolHandler.cpp \
	MissionControlWindow.cpp \
	SerialMonitorWindow.cpp \
	RepeaterMonitorView.cpp \
	TileCache.cpp \
	SarMarker.cpp \
	ImageCodec.cpp \
	ImageSession.cpp \
	GiphyClient.cpp \
	GifPickerWindow.cpp \
	EmojiRenderer.cpp

RDEFS = Sestriere.rdef

LIBS = be tracker shared columnlistview mosquitto sqlite3 translation curl gif $(STDCPPLIBS)

LIBPATHS =

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
