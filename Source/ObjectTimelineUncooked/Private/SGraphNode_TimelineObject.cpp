// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphNode_TimelineObject.h"
#include "STimelineObjectEditorPanel.h"
#include "K2Node_TimelineObject.h"
#include "Curves/CurveFloat.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"

#pragma region Editor Window Management

/**
 * Key for tracking open Timeline Editor windows.
 * Ensures only one editor window per Blueprint/Timeline combination.
 */
struct FTimelineEditorWindowKey
{
	TWeakObjectPtr<UBlueprint> Blueprint;
	FName TimelineName;

	bool operator==(const FTimelineEditorWindowKey& Other) const
	{
		return Blueprint == Other.Blueprint && TimelineName == Other.TimelineName;
	}

	friend uint32 GetTypeHash(const FTimelineEditorWindowKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Blueprint), GetTypeHash(Key.TimelineName));
	}
};

/** Global map of open Timeline Editor windows */
static TMap<FTimelineEditorWindowKey, TWeakPtr<SWindow>> GTimelineEditorWindows;

#pragma endregion

#pragma region Construction

void SGraphNode_TimelineObject::Construct(const FArguments& InArgs, UK2Node_TimelineObject* InNode)
{
	TimelineNode = InNode;
	GraphNode = InNode;
	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

#pragma endregion

#pragma region SGraphNodeK2Default Interface

void SGraphNode_TimelineObject::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	if (!TimelineNode)
	{
		return;
	}

	UTimelineTemplate* Timeline = TimelineNode->GetTimelineTemplate();

	// Build info text showing timeline properties
	FString InfoText;
	if (Timeline)
	{
		InfoText = FString::Printf(TEXT("Length: %.2f"), Timeline->TimelineLength);
		if (Timeline->bLoop)
		{
			InfoText += TEXT(" | Loop");
		}
		if (Timeline->bAutoPlay)
		{
			InfoText += TEXT(" | AutoPlay");
		}
	}
	else
	{
		InfoText = TEXT("Timeline not created");
	}

	// Add info label
	MainBox->AddSlot()
		.AutoHeight()
		.Padding(5.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InfoText))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];

	// Add edit button
	MainBox->AddSlot()
		.AutoHeight()
		.Padding(5.0f, 2.0f)
		[
			SNew(SButton)
			.OnClicked(this, &SGraphNode_TimelineObject::OnEditTimelineClicked)
			.ContentPadding(FMargin(4.0f, 2.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Edit Timeline")))
			]
		];
}

FReply SGraphNode_TimelineObject::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return OnEditTimelineClicked();
}

#pragma endregion

#pragma region Timeline Editor Window

FReply SGraphNode_TimelineObject::OnEditTimelineClicked()
{
	if (!TimelineNode)
	{
		return FReply::Handled();
	}

	UBlueprint* Blueprint = TimelineNode->GetBlueprint();
	if (!Blueprint)
	{
		return FReply::Handled();
	}

	// Check for existing editor window
	FTimelineEditorWindowKey WindowKey;
	WindowKey.Blueprint = Blueprint;
	WindowKey.TimelineName = TimelineNode->TimelineName;

	if (TWeakPtr<SWindow>* ExistingWindowPtr = GTimelineEditorWindows.Find(WindowKey))
	{
		if (TSharedPtr<SWindow> ExistingWindow = ExistingWindowPtr->Pin())
		{
			// Window already exists - bring to front
			ExistingWindow->BringToFront();
			return FReply::Handled();
		}
		else
		{
			// Window was closed - remove stale entry
			GTimelineEditorWindows.Remove(WindowKey);
		}
	}

	UTimelineTemplate* Timeline = TimelineNode->GetTimelineTemplate();

	// Create timeline template if it doesn't exist
	if (!Timeline)
	{
		if (FBlueprintEditorUtils::AddNewTimeline(Blueprint, TimelineNode->TimelineName))
		{
			Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineNode->TimelineName);
			
			// Add default track if timeline is empty
			if (Timeline && Timeline->FloatTracks.Num() == 0 && Timeline->VectorTracks.Num() == 0 &&
				Timeline->LinearColorTracks.Num() == 0 && Timeline->EventTracks.Num() == 0)
			{
				FTTFloatTrack NewTrack;
				NewTrack.SetTrackName(FName(TEXT("NewTrack")), Timeline);
				NewTrack.CurveFloat = NewObject<UCurveFloat>(Timeline, NAME_None, RF_Transactional | RF_Public);
				NewTrack.CurveFloat->FloatCurve.AddKey(0.0f, 0.0f);
				NewTrack.CurveFloat->FloatCurve.AddKey(Timeline->TimelineLength, 1.0f);
				Timeline->FloatTracks.Add(NewTrack);
			}

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			TimelineNode->ReconstructNode();
		}
	}

	// Create editor window
	if (Timeline)
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(FText::Format(NSLOCTEXT("TimelineObjectEditor", "WindowTitle", "Timeline Editor - {0}"), FText::FromName(TimelineNode->TimelineName)))
			.ClientSize(FVector2D(800, 600))
			.SupportsMinimize(true)
			.SupportsMaximize(true);

		TSharedRef<STimelineObjectEditorPanel> EditorPanel = SNew(STimelineObjectEditorPanel, Blueprint, Timeline, TimelineNode);

		Window->SetContent(EditorPanel);
		Window->SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &SGraphNode_TimelineObject::OnEditorWindowClosed));

		GTimelineEditorWindows.Add(WindowKey, Window);

		FSlateApplication::Get().AddWindow(Window);
	}

	return FReply::Handled();
}

void SGraphNode_TimelineObject::OnEditorWindowClosed(const TSharedRef<SWindow>& Window)
{
	// Remove closed window from tracking map
	for (auto It = GTimelineEditorWindows.CreateIterator(); It; ++It)
	{
		if (It->Value.Pin() == Window)
		{
			It.RemoveCurrent();
			break;
		}
	}
}

#pragma endregion