// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

/**
 * Graph panel node factory for UK2Node_TimelineObject.
 * Creates SGraphNode_TimelineObject widgets for timeline nodes in the Blueprint graph.
 */
class OBJECTTIMELINEUNCOOKED_API FTimelineObjectGraphPanelNodeFactory : public FGraphPanelNodeFactory
{
public:
	/**
	 * Creates a custom Slate widget for timeline object nodes.
	 * @param Node - The graph node to create a widget for
	 * @return Custom SGraphNode_TimelineObject if Node is UK2Node_TimelineObject, nullptr otherwise
	 */
	virtual TSharedPtr<class SGraphNode> CreateNode(class UEdGraphNode* Node) const override;
};