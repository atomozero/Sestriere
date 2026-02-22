/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * NetworkMapWindow.h — Dynamic network topology visualization
 */

#ifndef NETWORKMAPWINDOW_H
#define NETWORKMAPWINDOW_H

#include <Window.h>
#include <View.h>
#include "Compat.h"

#include "Types.h"

class BButton;
class BCheckBox;
class BMessageRunner;
class BSlider;
class BStringView;

// Node online status
enum NodeStatus {
	STATUS_ONLINE = 0,
	STATUS_RECENT,
	STATUS_AWAY,
	STATUS_OFFLINE
};

// Node type from MeshCore protocol (ADV_TYPE)
enum NodeType {
	NODE_UNKNOWN = 0,       // Unknown / unset
	NODE_COMPANION = 1,     // Companion chat device
	NODE_REPEATER = 2,      // Repeater/relay node
	NODE_ROOM = 3           // Room server
};

// Link quality level
enum LinkQuality {
	LINK_EXCELLENT = 0,		// SNR > 5 dB
	LINK_GOOD,				// SNR 0..5
	LINK_FAIR,				// SNR -5..0
	LINK_POOR,				// SNR -10..-5
	LINK_BAD				// SNR < -10
};

// Node in the network map
struct MapNode {
	uint8		pubKeyPrefix[kPubKeyPrefixSize];
	char		name[64];
	int8		rssi;
	int8		snr;			// SNR in dB (from last message)
	uint8		hops;
	uint8		nodeType;		// 1=companion, 2=repeater, 3=room
	uint32		lastSeen;
	BPoint		position;		// Current animated position
	BPoint		targetPosition;	// Target position for animation
	bool		isSelected;
	NodeStatus	status;
	int32		messageCount;
	float		activityLevel;	// 0.0 to 1.0
	float		pulsePhase;		// Animation phase for pulse effect
	bool		hasSNRData;		// True if SNR data is available
	float		flowPhase;		// Animated data flow along link (0..1)

	MapNode() : rssi(0), snr(0), hops(1), nodeType(0), lastSeen(0), isSelected(false),
				status(STATUS_OFFLINE), messageCount(0), activityLevel(0),
				pulsePhase(0), hasSNRData(false), flowPhase(0) {
		memset(pubKeyPrefix, 0, sizeof(pubKeyPrefix));
		memset(name, 0, sizeof(name));
	}
};

// Trace path hop for visualization
struct TracePathHop {
	uint8		hash;		// Hop hash byte
	int8		snr;		// SNR * 4 for this hop
};

// Stored trace route for a contact
struct TraceRoute {
	uint8		destKeyPrefix[kPubKeyPrefixSize];
	uint8		pathLen;
	TracePathHop	hops[16];		// Max 16 hops
	int8		destSnr;			// SNR at destination
	uint32		timestamp;			// When trace was performed
	float		animPhase;			// Animation phase for trace highlight

	TraceRoute() : pathLen(0), destSnr(0), timestamp(0), animPhase(0) {
		memset(destKeyPrefix, 0, sizeof(destKeyPrefix));
		memset(hops, 0, sizeof(hops));
	}
};

// Discovered inter-node connection from trace route analysis
struct TopologyEdge {
	uint8		fromPrefix[kPubKeyPrefixSize];	// Source node pubkey prefix
	uint8		toPrefix[kPubKeyPrefixSize];	// Dest node pubkey prefix
	int8		snr;							// Link quality (SNR)
	uint32		timestamp;						// When discovered
	bool		ambiguous;						// True if hash collision detected

	TopologyEdge() : snr(0), timestamp(0), ambiguous(false) {
		memset(fromPrefix, 0, sizeof(fromPrefix));
		memset(toPrefix, 0, sizeof(toPrefix));
	}
};

// Custom view for drawing the network map
class NetworkMapView : public BView {
public:
							NetworkMapView();
	virtual					~NetworkMapView();

	virtual void			AttachedToWindow();
	virtual void			Pulse();
	virtual void			Draw(BRect updateRect);
	virtual void			MouseDown(BPoint where);
	virtual void			MouseMoved(BPoint where, uint32 transit,
								const BMessage* dragMessage);
	virtual void			MouseUp(BPoint where);
	virtual void			FrameResized(float newWidth, float newHeight);

			void			SetNodes(const BObjectList<ContactInfo, true>* contacts);
			void			SetSelfInfo(const char* name, int8 rssi, int8 snr);
			void			SetShowLabels(bool show);
			void			SetShowSignalStrength(bool show);
			void			SetZoom(float zoom);
			void			TriggerNodePulse(const uint8* pubKeyPrefix);
			void			UpdateNodeSNR(const uint8* pubKeyPrefix,
								int8 snr, int8 rssi);
			void			SetTraceRoute(const TraceRoute& route);
			void			ClearTraceRoutes();
			void			BuildEdgesFromTrace(const TraceRoute& route);
			void			ExpireStaleEdges();
			int32			CountEdges() const { return fEdges.CountItems(); }

			MapNode*		GetSelectedNode() const { return fSelectedNode; }
			int32			GetMultiHopNodes(
								BObjectList<MapNode, false>* outList) const;

private:
			void			_CalculatePositions();
			void			_DrawSelfNode();
			void			_DrawNode(const MapNode& node);
			void			_DrawConnection(BPoint from, BPoint to,
								const MapNode* node);
			void			_DrawLinkLabel(BPoint midPoint,
								const MapNode* node);
			void			_DrawFlowDots(BPoint from, BPoint to,
								float phase, rgb_color color);
			void			_DrawTraceRoutes();
			void			_DrawTopologyEdges();
			void			_DrawInfoPanel();
			void			_DrawLinkQualityLegend();
			void			_DrawStats();
			void			_ShowNodeContextMenu(BPoint where, MapNode* node);

			float			_RadiusForNode(const MapNode& node) const;
			float			_OpacityForNode(const MapNode& node) const;
			rgb_color		_ColorForNode(const MapNode& node) const;
			rgb_color		_ColorForSignal(int8 rssi) const;
			rgb_color		_ColorForSNR(int8 snr) const;
			LinkQuality		_QualityForSNR(int8 snr) const;
			float			_ThicknessForSNR(int8 snr) const;
			rgb_color		_StatusColor(NodeStatus status) const;
			float			_DistanceForRssi(int8 rssi) const;
			uint8			_HashForContact(const uint8* pubKeyPrefix) const;
			MapNode*		_MatchHopToContact(uint8 hopHash) const;
			MapNode*		_FindNodeByPrefix(const uint8* prefix) const;
			MapNode*		_FindRelayForNode(const MapNode* node) const;

			BObjectList<MapNode, true>	fNodes;
			MapNode			fSelfNode;
			BPoint			fCenter;
			float			fZoom;
			bool			fShowLabels;
			bool			fShowSignalStrength;

			MapNode*		fSelectedNode;
			BPoint			fDragStart;
			bool			fDragging;

			BObjectList<TraceRoute, true>	fTraceRoutes;
			BObjectList<TopologyEdge, true>	fEdges;
};


class NetworkMapWindow : public BWindow {
public:
							NetworkMapWindow(BWindow* parent);
	virtual					~NetworkMapWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			UpdateFromContacts(const BObjectList<ContactInfo, true>* contacts);
			void			SetSelfInfo(const char* name);
			void			TriggerNodePulse(const uint8* pubKeyPrefix);
			void			UpdateLinkQuality(const uint8* pubKeyPrefix,
								int8 snr, int8 rssi);
			void			HandleTraceData(const uint8* data, size_t length);

private:
			void			_RequestUpdate();
			void			_RequestAutoTrace();
			void			_RequestFullDiscovery();
			void			_DiscoveryTick();

			BWindow*		fParent;
			NetworkMapView*	fMapView;
			BStringView*	fInfoLabel;
			BCheckBox*		fShowLabelsCheck;
			BCheckBox*		fShowSignalCheck;
			BCheckBox*		fAutoTraceCheck;
			BSlider*		fZoomSlider;
			BButton*		fRefreshButton;
			BButton*		fMapNetworkButton;
			BButton*		fCloseButton;

			BMessageRunner*	fRefreshTimer;
			BMessageRunner*	fAutoTraceTimer;
			BMessageRunner*	fDiscoveryTimer;
			char			fSelfName[64];
			int32			fAutoTraceIndex;

			// Discovery state
			BObjectList<MapNode, false>	fDiscoveryQueue;
			int32			fDiscoveryTotal;
			bool			fDiscoveryActive;
};

#endif // NETWORKMAPWINDOW_H
