// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimelineObjectGraphPanelNodeFactory.h"
#include "K2Node_TimelineObject.h"
#include "SGraphNode_TimelineObject.h"

TSharedPtr<SGraphNode> FTimelineObjectGraphPanelNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	// Check if this is our custom timeline node type
	if (UK2Node_TimelineObject* TimelineNode = Cast<UK2Node_TimelineObject>(Node))
	{
		// Create and return custom visual representation
		TSharedPtr<SGraphNode_TimelineObject> SNode = SNew(SGraphNode_TimelineObject, TimelineNode);
		return SNode;
	}

	// Not our node type - let other factories handle it
	return nullptr;
}