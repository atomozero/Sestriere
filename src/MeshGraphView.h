/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MeshGraphView.h — Network topology graph visualization
 */

#ifndef MESHGRAPHVIEW_H
#define MESHGRAPHVIEW_H

#include <Messenger.h>
#include <View.h>
#include <Window.h>
#include <ObjectList.h>

#include "Types.h"

class MeshGraphView;

// Graph node
struct GraphNode {
	uint8		publicKey[kPublicKeySize];
	char		name[kMaxNameLen];
	uint8		type;
	int8		pathLen;
	float		x, y;			// Current position
	float		vx, vy;			// Velocity (for physics)
	float		targetX, targetY;
	bool		isSelf;
	bool		isSelected;
	bool		isDragging;
	uint8		txPower;		// For node size
};

// Graph edge (connection)
struct GraphEdge {
	int32		fromIndex;
	int32		toIndex;
	int8		snr;
	uint8		hops;
	bool		isActive;
};

class MeshGraphView : public BView {
public:
							MeshGraphView(const char* name);
	virtual					~MeshGraphView();

	virtual void			AttachedToWindow();
	virtual void			DetachedFromWindow();
	virtual void			Draw(BRect updateRect);
	virtual void			MouseDown(BPoint where);
	virtual void			MouseMoved(BPoint where, uint32 transit,
								const BMessage* dragMessage);
	virtual void			MouseUp(BPoint where);
	virtual void			FrameResized(float newWidth, float newHeight);
	virtual void			MessageReceived(BMessage* message);
	virtual void			Pulse();

			void			SetSelfNode(const char* name, uint8 type,
								uint8 txPower);
			void			AddNode(const Contact& contact);
			void			UpdateNode(const Contact& contact);
			void			RemoveNode(const uint8* publicKey);
			void			ClearNodes();

			void			AddEdge(int32 fromIndex, int32 toIndex,
								int8 snr, uint8 hops);
			void			SetEdgeActive(int32 fromIndex, int32 toIndex,
								bool active);

			void			SetAnimationEnabled(bool enabled);
			void			ResetLayout();

private:
			void			_DrawNodes();
			void			_DrawNode(const GraphNode& node);
			void			_DrawEdges();
			void			_DrawEdge(const GraphEdge& edge);
			void			_DrawLegend();

			void			_UpdatePhysics();
			void			_ApplyForces();
			void			_ConstrainToView();

			int32			_FindNodeIndex(const uint8* publicKey) const;
			GraphNode*		_FindNodeAt(BPoint where);

			float			_NodeRadius(const GraphNode& node) const;
			rgb_color		_NodeColor(const GraphNode& node) const;
			rgb_color		_EdgeColor(const GraphEdge& edge) const;

			BObjectList<GraphNode>	fNodes;
			BObjectList<GraphEdge>	fEdges;
			int32			fSelfIndex;

			bool			fAnimationEnabled;
			GraphNode*		fDraggedNode;
			BPoint			fDragOffset;

			// Physics parameters
			float			fRepulsion;
			float			fAttraction;
			float			fDamping;
			float			fCenterPull;
};


// Window wrapper for MeshGraphView
class MeshGraphWindow : public BWindow {
public:
						MeshGraphWindow(BRect frame, BMessenger target);
	virtual				~MeshGraphWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

	void				AddNode(uint32 nodeId, const char* name,
							int pathLen, bool isSelf);
	void				AddEdge(uint32 fromId, uint32 toId, int hops);

private:
	MeshGraphView*		fGraphView;
	BMessenger			fTarget;
};

#endif // MESHGRAPHVIEW_H
