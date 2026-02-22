/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MapView.h — Geographic map visualization of mesh nodes
 */

#ifndef MAPVIEW_H
#define MAPVIEW_H

#include "Compat.h"
#include <View.h>
#include <Window.h>

#include "Types.h"

class BButton;


// Geographic map node representation
struct GeoMapNode {
	uint8		publicKey[32];
	char		name[64];
	float		latitude;
	float		longitude;
	uint8		type;
	int8		pathLen;
	int8		lastSnr;
	bool		isSelected;
	bool		isSelf;

	GeoMapNode()
		: latitude(0), longitude(0), type(0), pathLen(-1),
		  lastSnr(0), isSelected(false), isSelf(false)
	{
		memset(publicKey, 0, sizeof(publicKey));
		memset(name, 0, sizeof(name));
	}
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
	virtual void			FrameResized(float newWidth, float newHeight);
	virtual void			MessageReceived(BMessage* message);

			void			SetSelfPosition(float lat, float lon,
								const char* name);
			void			AddOrUpdateNode(const ContactInfo& contact,
								float lat, float lon);
			void			ClearNodes();

			void			ZoomIn();
			void			ZoomOut();
			void			ZoomToFit();
			void			CenterOnSelf();

private:
			BPoint			_LatLonToScreen(float lat, float lon) const;
			void			_ScreenToLatLon(BPoint screen, float& lat,
								float& lon) const;
			void			_DrawGrid();
			void			_DrawNodes();
			void			_DrawNode(const GeoMapNode& node);
			void			_DrawConnections();
			void			_DrawScaleBar();
			void			_DrawCompass();
			GeoMapNode*		_FindNodeAt(BPoint where);
			rgb_color		_ColorForType(uint8 type) const;

			OwningObjectList<GeoMapNode>	fNodes;
			GeoMapNode		fSelfNode;
			bool			fHasSelfPosition;

			float			fCenterLat;
			float			fCenterLon;
			float			fZoom;

			bool			fDragging;
			BPoint			fDragStart;
			float			fDragStartLat;
			float			fDragStartLon;

			GeoMapNode*		fSelectedNode;
			GeoMapNode*		fHoverNode;
};


// Window wrapper for MapView
class MapWindow : public BWindow {
public:
						MapWindow(BWindow* parent);
	virtual				~MapWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

	void				SetSelfPosition(float lat, float lon,
							const char* name);
	void				UpdateFromContacts(
							OwningObjectList<ContactInfo>* contacts,
							double defaultLat, double defaultLon);

private:
	MapView*			fMapView;
	BButton*			fZoomInButton;
	BButton*			fZoomOutButton;
	BButton*			fFitButton;
	BButton*			fCenterButton;
	BWindow*			fParent;
};

#endif // MAPVIEW_H
