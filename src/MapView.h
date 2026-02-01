/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MapView.h — Geographic map visualization of contacts
 */

#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <Messenger.h>
#include <View.h>
#include <Window.h>
#include <ObjectList.h>

#include "Types.h"

class BStringView;
class MapView;

// Map node representation
struct MapNode {
	uint8		publicKey[kPublicKeySize];
	char		name[kMaxNameLen];
	float		latitude;
	float		longitude;
	uint8		type;
	int8		pathLen;		// -1 = unknown, 0xFF = direct
	int8		lastSnr;		// SNR * 4
	bool		isSelected;
	bool		isSelf;
};

class MapView : public BView {
public:
							MapView(const char* name);
	virtual					~MapView();

	virtual void			AttachedToWindow();
	virtual void			Draw(BRect updateRect);
	virtual void			MouseDown(BPoint where);
	virtual void			MouseMoved(BPoint where, uint32 transit,
								const BMessage* dragMessage);
	virtual void			MouseUp(BPoint where);
	virtual void			ScrollTo(BPoint where);
	virtual void			FrameResized(float newWidth, float newHeight);
	virtual void			MessageReceived(BMessage* message);

			void			SetSelfPosition(float lat, float lon,
								const char* name);
			void			AddNode(const Contact& contact);
			void			UpdateNode(const Contact& contact);
			void			RemoveNode(const uint8* publicKey);
			void			ClearNodes();

			void			SetZoom(float zoom);
			float			Zoom() const { return fZoom; }
			void			ZoomIn();
			void			ZoomOut();
			void			ZoomToFit();

			void			SetCenterPosition(float lat, float lon);
			void			CenterOnSelf();

private:
			BPoint			_LatLonToScreen(float lat, float lon) const;
			void			_ScreenToLatLon(BPoint screen, float& lat,
								float& lon) const;
			void			_DrawGrid();
			void			_DrawNodes();
			void			_DrawNode(const MapNode& node);
			void			_DrawConnections();
			void			_DrawScaleBar();
			void			_DrawCompass();
			MapNode*		_FindNodeAt(BPoint where);
			rgb_color		_ColorForSnr(int8 snr) const;
			rgb_color		_ColorForType(uint8 type) const;

			BObjectList<MapNode>	fNodes;
			MapNode			fSelfNode;
			bool			fHasSelfPosition;

			float			fCenterLat;
			float			fCenterLon;
			float			fZoom;			// pixels per degree

			bool			fDragging;
			BPoint			fDragStart;
			float			fDragStartLat;
			float			fDragStartLon;

			MapNode*		fSelectedNode;
			MapNode*		fHoverNode;
};


// Window wrapper for MapView
class MapWindow : public BWindow {
public:
						MapWindow(BRect frame, BMessenger target);
	virtual				~MapWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

	void				AddNode(uint32 nodeId, const char* name,
							double lat, double lon, uint8 type,
							uint32 lastSeen, int pathLen);
	void				SetSelfNode(uint32 nodeId);

private:
	MapView*			fMapView;
	BMessenger			fTarget;
};

#endif // MAPVIEW_H
