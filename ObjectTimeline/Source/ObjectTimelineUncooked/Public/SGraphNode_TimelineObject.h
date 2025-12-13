// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetNodes/SGraphNodeK2Default.h"

class UK2Node_TimelineObject;

/**
 * Custom graph node visualization for UK2Node_TimelineObject.
 * Provides timeline info display and an edit button to open the Timeline Editor.
 */
class OBJECTTIMELINEUNCOOKED_API SGraphNode_TimelineObject : public SGraphNodeK2Default
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_TimelineObject) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_TimelineObject* InNode);

#pragma region SGraphNodeK2Default Interface

	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

#pragma endregion

private:

#pragma region Timeline Editor Window

	/** Opens or focuses the Timeline Editor window */
	FReply OnEditTimelineClicked();
	
	/** Cleanup callback when the editor window is closed */
	void OnEditorWindowClosed(const TSharedRef<SWindow>& Window);

#pragma endregion

	/** The timeline node this widget represents */
	UK2Node_TimelineObject* TimelineNode = nullptr;
};