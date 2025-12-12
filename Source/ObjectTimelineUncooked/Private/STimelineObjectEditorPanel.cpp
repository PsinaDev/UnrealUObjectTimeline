#include "STimelineObjectEditorPanel.h"
#include "K2Node_TimelineObject.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "BlueprintEditor.h"
#include "Components/TimelineComponent.h"
#include "Containers/EnumAsByte.h"
#include "Curves/CurveBase.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Blueprint.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimelineTemplate.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformMisc.h"
#include "IAssetTools.h"
#include "Internationalization/Internationalization.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/CString.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "SCurveEditor.h"
#include "SPositiveActionButton.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "SlateOptMacros.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/StyleColors.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Textures/SlateIcon.h"
#include "UObject/Class.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

class FTagMetaData;
class ITableRow;
class STableViewBase;
class SWidget;
struct FGeometry;
struct FKeyEvent;

#define LOCTEXT_NAMESPACE "STimelineObjectEditorPanel"

#pragma region Static Data

static TArray<TSharedPtr<FString>> TickGroupNameStrings;
static bool TickGroupNamesInitialized = false;

#pragma endregion

#pragma region Helper Functions

/**
 * Finds the UK2Node_TimelineObject associated with a given timeline template.
 */
static UK2Node_TimelineObject* FindTimelineObjectNode(UBlueprint* Blueprint, UTimelineTemplate* TimelineObj)
{
	if (!Blueprint || !TimelineObj)
	{
		return nullptr;
	}

	TArray<UK2Node_TimelineObject*> TimelineNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, TimelineNodes);
	
	for (UK2Node_TimelineObject* Node : TimelineNodes)
	{
		if (Node->TimelineName == TimelineObj->GetVariableName())
		{
			return Node;
		}
	}
	
	return nullptr;
}

namespace TimelineEditorHelpers
{
	/**
	 * Gets the track base from the timeline using display index.
	 */
	FTTTrackBase* GetTrackFromTimeline(UTimelineTemplate* InTimeline, TSharedPtr<FTimelineObjectEdTrack> InTrack)
	{
		FTTTrackId TrackId = InTimeline->GetDisplayTrackId(InTrack->DisplayIndex);
		FTTTrackBase::ETrackType TrackType = (FTTTrackBase::ETrackType)TrackId.TrackType;
		
		if (TrackType == FTTTrackBase::TT_Event)
		{
			if (InTimeline->EventTracks.IsValidIndex(TrackId.TrackIndex))
			{
				return &InTimeline->EventTracks[TrackId.TrackIndex];
			}
		}
		else if (TrackType == FTTTrackBase::TT_FloatInterp)
		{
			if (InTimeline->FloatTracks.IsValidIndex(TrackId.TrackIndex))
			{
				return &InTimeline->FloatTracks[TrackId.TrackIndex];
			}
		}
		else if (TrackType == FTTTrackBase::TT_VectorInterp)
		{
			if (InTimeline->VectorTracks.IsValidIndex(TrackId.TrackIndex))
			{
				return &InTimeline->VectorTracks[TrackId.TrackIndex];
			}
		}
		else if (TrackType == FTTTrackBase::TT_LinearColorInterp)
		{
			if (InTimeline->LinearColorTracks.IsValidIndex(TrackId.TrackIndex))
			{
				return &InTimeline->LinearColorTracks[TrackId.TrackIndex];
			}
		}
		return nullptr;
	}

	/**
	 * Gets the track name from the timeline using display index.
	 */
	FName GetTrackNameFromTimeline(UTimelineTemplate* InTimeline, TSharedPtr<FTimelineObjectEdTrack> InTrack)
	{
		FTTTrackBase* TrackBase = GetTrackFromTimeline(InTimeline, InTrack);
		if (TrackBase)
		{
			return TrackBase->GetTrackName();
		}
		return NAME_None;
	}

	/**
	 * Maps track type to allowed curve class for the object picker.
	 */
	TSubclassOf<UCurveBase> TrackTypeToAllowedClass(FTTTrackBase::ETrackType TrackType)
	{
		switch (TrackType)
		{
		case FTTTrackBase::TT_Event:
		case FTTTrackBase::TT_FloatInterp:
			return UCurveFloat::StaticClass();
		case FTTTrackBase::TT_VectorInterp:
			return UCurveVector::StaticClass();
		case FTTTrackBase::TT_LinearColorInterp:
			return UCurveLinearColor::StaticClass();
		default:
			return UCurveBase::StaticClass();
		}
	}
}

#pragma endregion

#pragma region STimelineObjectEdTrack Construction

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STimelineObjectEdTrack::Construct(const FArguments& InArgs, TSharedPtr<FTimelineObjectEdTrack> InTrack, TSharedPtr<STimelineObjectEditorPanel> InTimelineEd)
{
	Track = InTrack;
	TimelineEdPtr = InTimelineEd;

	ResetExternalCurveInfo();

	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj);

	CurveBasePtr = nullptr;
	FTTTrackBase* TrackBase = nullptr;
	bool bDrawCurve = true;

	FTTTrackId TrackId = TimelineObj->GetDisplayTrackId(InTrack->DisplayIndex);
	FTTTrackBase::ETrackType TrackType = (FTTTrackBase::ETrackType)TrackId.TrackType;

	// Get curve and track base based on track type
	if (TrackType == FTTTrackBase::TT_Event)
	{
		check(TrackId.TrackIndex < TimelineObj->EventTracks.Num());
		FTTEventTrack* EventTrack = &(TimelineObj->EventTracks[TrackId.TrackIndex]);
		CurveBasePtr = EventTrack->CurveKeys;
		TrackBase = EventTrack;
		bDrawCurve = false;
	}
	else if (TrackType == FTTTrackBase::TT_FloatInterp)
	{
		check(TrackId.TrackIndex < TimelineObj->FloatTracks.Num());
		FTTFloatTrack* FloatTrack = &(TimelineObj->FloatTracks[TrackId.TrackIndex]);
		CurveBasePtr = FloatTrack->CurveFloat;
		TrackBase = FloatTrack;
	}
	else if (TrackType == FTTTrackBase::TT_VectorInterp)
	{
		check(TrackId.TrackIndex < TimelineObj->VectorTracks.Num());
		FTTVectorTrack* VectorTrack = &(TimelineObj->VectorTracks[TrackId.TrackIndex]);
		CurveBasePtr = VectorTrack->CurveVector;
		TrackBase = VectorTrack;
	}
	else if (TrackType == FTTTrackBase::TT_LinearColorInterp)
	{
		check(TrackId.TrackIndex < TimelineObj->LinearColorTracks.Num());
		FTTLinearColorTrack* LinearColorTrack = &(TimelineObj->LinearColorTracks[TrackId.TrackIndex]);
		CurveBasePtr = LinearColorTrack->CurveLinearColor;
		TrackBase = LinearColorTrack;
	}

	if (TrackBase && TrackBase->bIsExternalCurve)
	{
		UseExternalCurve(CurveBasePtr);
	}

	TSharedRef<STimelineObjectEditorPanel> TimelineRef = TimelineEd.ToSharedRef();
	TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

	// Build the track widget
	this->ChildSlot
	[
		SNew(SVerticalBox)
		
		// Track header with expand checkbox and name
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop_Hovered"))
			.ForegroundColor(FLinearColor::White)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(this, &STimelineObjectEdTrack::GetIsExpandedState)
					.OnCheckStateChanged(this, &STimelineObjectEdTrack::OnIsExpandedStateChanged)
					.CheckedImage(FAppStyle::GetBrush("TreeArrow_Expanded"))
					.CheckedHoveredImage(FAppStyle::GetBrush("TreeArrow_Expanded_Hovered"))
					.CheckedPressedImage(FAppStyle::GetBrush("TreeArrow_Expanded"))
					.UncheckedImage(FAppStyle::GetBrush("TreeArrow_Collapsed"))
					.UncheckedHoveredImage(FAppStyle::GetBrush("TreeArrow_Collapsed_Hovered"))
					.UncheckedPressedImage(FAppStyle::GetBrush("TreeArrow_Collapsed"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
					.Text(FText::FromName(TrackBase->GetTrackName()))
					.ToolTipText(LOCTEXT("TrackNameTooltip", "Enter track name"))
					.OnVerifyTextChanged(TimelineRef, &STimelineObjectEditorPanel::OnVerifyTrackNameCommit, TrackBase, this)
					.OnTextCommitted(TimelineRef, &STimelineObjectEditorPanel::OnTrackNameCommitted, TrackBase, this)
				]
			]
		]

		// Track content (collapsible)
		+ SVerticalBox::Slot()
		[
			SNew(SBox)
			.Visibility(this, &STimelineObjectEdTrack::GetContentVisibility)
			[
				SNew(SHorizontalBox)

				// Left panel: External curve picker and controls
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExternalCurveLabel", "External Curve"))
						.ColorAndOpacity(FStyleColors::Foreground)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 0, 0, 4)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("NoBrush"))
						.ForegroundColor(FStyleColors::Foreground)
						[
							SNew(SHorizontalBox)
							
							+ SHorizontalBox::Slot()
							.FillWidth(1)
							[
								SNew(SObjectPropertyEntryBox)
								.AllowedClass(TimelineEditorHelpers::TrackTypeToAllowedClass(TrackType))
								.ObjectPath(this, &STimelineObjectEdTrack::GetExternalCurvePath)
								.OnObjectChanged(FOnSetObject::CreateSP(this, &STimelineObjectEdTrack::OnChooseCurve))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "NoBorder")
								.OnClicked(this, &STimelineObjectEdTrack::OnClickClear)
								.ContentPadding(1.f)
								.ToolTipText(NSLOCTEXT("TimelineEdTrack", "TimelineEdTrack_Clear", "Convert to Internal Curve"))
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush(TEXT("PropertyWindow.Button_Clear")))
									.ColorAndOpacity(FStyleColors::Foreground)
								]
							]
						]
					]

					// Synchronize view checkbox
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 0, 2, 0)
					[
						SNew(SCheckBox)
						.IsChecked(this, &STimelineObjectEdTrack::GetIsCurveViewSynchronizedState)
						.OnCheckStateChanged(this, &STimelineObjectEdTrack::OnIsCurveViewSynchronizedStateChanged)
						.ToolTipText(LOCTEXT("SynchronizeViewToolTip", "Keep the zoom and pan of this curve synchronized with other curves."))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SynchronizeViewLabel", "Synchronize View"))
							.ColorAndOpacity(FStyleColors::Foreground)
						]
					]

					// Reorder buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 0, 2, 0)
					[
						SNew(SHorizontalBox)
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "NoBorder")
							.OnClicked(this, &STimelineObjectEdTrack::OnMoveUp)
							.IsEnabled(this, &STimelineObjectEdTrack::CanMoveUp)
							.ContentPadding(1.f)
							.ToolTipText(NSLOCTEXT("TimelineEdTrack", "TimelineEdTrack_MoveUp", "Move track up list"))
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush(TEXT("ArrowUp")))
								.ColorAndOpacity(FStyleColors::Foreground)
							]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "NoBorder")
							.OnClicked(this, &STimelineObjectEdTrack::OnMoveDown)
							.IsEnabled(this, &STimelineObjectEdTrack::CanMoveDown)
							.ContentPadding(1.f)
							.ToolTipText(NSLOCTEXT("TimelineEdTrack", "TimelineEdTrack_MoveDown", "Move track down list"))
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush(TEXT("ArrowDown")))
								.ColorAndOpacity(FStyleColors::Foreground)
							]
						]
						
						+ SHorizontalBox::Slot()
						.FillWidth(1)
						.HAlign(HAlign_Left)
						.Padding(2)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ReorderLabel", "Reorder"))
							.ColorAndOpacity(FStyleColors::Foreground)
						]
					]
				]

				// Right panel: Curve editor
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SBorder)
					.VAlign(VAlign_Fill)
					[
						SAssignNew(TrackWidget, SCurveEditor)
						.ViewMinInput(this, &STimelineObjectEdTrack::GetMinInput)
						.ViewMaxInput(this, &STimelineObjectEdTrack::GetMaxInput)
						.ViewMinOutput(this, &STimelineObjectEdTrack::GetMinOutput)
						.ViewMaxOutput(this, &STimelineObjectEdTrack::GetMaxOutput)
						.TimelineLength(TimelineRef, &STimelineObjectEditorPanel::GetTimelineLength)
						.OnSetInputViewRange(this, &STimelineObjectEdTrack::OnSetInputViewRange)
						.OnSetOutputViewRange(this, &STimelineObjectEdTrack::OnSetOutputViewRange)
						.DesiredSize(TimelineRef, &STimelineObjectEditorPanel::GetTimelineDesiredSize)
						.DrawCurve(bDrawCurve)
						.HideUI(false)
						.OnCreateAsset(this, &STimelineObjectEdTrack::OnCreateExternalCurve)
					]
				]
			]
		]
	];

	// Configure curve editor
	if (TrackBase)
	{
		bool bZoomToFit = false;
		if ((GetMaxInput() == 0) && (GetMinInput() == 0))
		{
			bZoomToFit = true;
		}

		TrackWidget->SetZoomToFit(bZoomToFit, bZoomToFit);
		TrackWidget->SetCurveOwner(CurveBasePtr, !TrackBase->bIsExternalCurve);

		if (!TrackWidget->GetAutoFrame() && bZoomToFit)
		{
			TrackWidget->ZoomToFitVertical();
			TrackWidget->ZoomToFitHorizontal();
		}
	}

	// Set up rename request delegate
	InTrack->OnRenameRequest.BindSP(InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#pragma endregion

#pragma region STimelineObjectEdTrack External Curve Management

FString STimelineObjectEdTrack::CreateUniqueCurveAssetPathName()
{
	FString BasePath = FString(TEXT("/Game/Unsorted"));

	TSharedRef<STimelineObjectEditorPanel> TimelineRef = TimelineEdPtr.Pin().ToSharedRef();

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	FString AssetName = TimelineEditorHelpers::GetTrackNameFromTimeline(TimelineEdPtr.Pin()->GetTimeline(), Track).ToString();
	FString PackageName;
	BasePath = BasePath + TEXT("/") + AssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);

	return PackageName;
}

void STimelineObjectEdTrack::OnCloseCreateCurveWindow()
{
	if (AssetCreationWindow.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = AssetCreationWindow->GetParentWindow();
		AssetCreationWindow->RequestDestroyWindow();
		AssetCreationWindow.Reset();
	}
}

void STimelineObjectEdTrack::OnCreateExternalCurve()
{
	UCurveBase* NewCurveAsset = CreateCurveAsset();
	if (NewCurveAsset)
	{
		SwitchToExternalCurve(NewCurveAsset);
	}
	OnCloseCreateCurveWindow();
}

void STimelineObjectEdTrack::SwitchToExternalCurve(UCurveBase* AssetCurvePtr)
{
	if (!AssetCurvePtr)
	{
		return;
	}

	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj);

	FTTTrackId TrackId = TimelineObj->GetDisplayTrackId(Track->DisplayIndex);
	FTTTrackBase::ETrackType TrackType = (FTTTrackBase::ETrackType)TrackId.TrackType;

	FTTTrackBase* TrackBase = nullptr;
	
	// Update the appropriate track array with the external curve
	if (TrackType == FTTTrackBase::TT_Event)
	{
		if (AssetCurvePtr->IsA(UCurveFloat::StaticClass()))
		{
			FTTEventTrack& NewTrack = TimelineObj->EventTracks[TrackId.TrackIndex];
			NewTrack.CurveKeys = Cast<UCurveFloat>(AssetCurvePtr);
			TrackBase = &NewTrack;
		}
	}
	else if (TrackType == FTTTrackBase::TT_FloatInterp)
	{
		if (AssetCurvePtr->IsA(UCurveFloat::StaticClass()))
		{
			FTTFloatTrack& NewTrack = TimelineObj->FloatTracks[TrackId.TrackIndex];
			NewTrack.CurveFloat = Cast<UCurveFloat>(AssetCurvePtr);
			TrackBase = &NewTrack;
		}
	}
	else if (TrackType == FTTTrackBase::TT_VectorInterp)
	{
		if (AssetCurvePtr->IsA(UCurveVector::StaticClass()))
		{
			FTTVectorTrack& NewTrack = TimelineObj->VectorTracks[TrackId.TrackIndex];
			NewTrack.CurveVector = Cast<UCurveVector>(AssetCurvePtr);
			TrackBase = &NewTrack;
		}
	}
	else if (TrackType == FTTTrackBase::TT_LinearColorInterp)
	{
		if (AssetCurvePtr->IsA(UCurveLinearColor::StaticClass()))
		{
			FTTLinearColorTrack& NewTrack = TimelineObj->LinearColorTracks[TrackId.TrackIndex];
			NewTrack.CurveLinearColor = Cast<UCurveLinearColor>(AssetCurvePtr);
			TrackBase = &NewTrack;
		}
	}

	if (TrackBase)
	{
		TrackBase->bIsExternalCurve = true;
		TrackWidget->SetCurveOwner(AssetCurvePtr, false);
		CurveBasePtr = AssetCurvePtr;
		UseExternalCurve(CurveBasePtr);
	}
}

void STimelineObjectEdTrack::UseExternalCurve(UObject* AssetObj)
{
	if (AssetObj)
	{
		ExternalCurvePath = AssetObj->GetPathName();
	}
	else
	{
		ResetExternalCurveInfo();
	}
}

void STimelineObjectEdTrack::UseInternalCurve()
{
	if (!CurveBasePtr)
	{
		return;
	}

	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj);

	FTTTrackId TrackId = TimelineObj->GetDisplayTrackId(Track->DisplayIndex);
	FTTTrackBase::ETrackType TrackType = (FTTTrackBase::ETrackType)TrackId.TrackType;

	FTTTrackBase* TrackBase = nullptr;
	UCurveBase* CurveBase = nullptr;

	// Create internal curve and copy data from external
	if (TrackType == FTTTrackBase::TT_Event)
	{
		FTTEventTrack& NewTrack = TimelineObj->EventTracks[TrackId.TrackIndex];
		if (NewTrack.bIsExternalCurve)
		{
			UCurveFloat* SrcCurve = NewTrack.CurveKeys;
			UCurveFloat* DestCurve = Cast<UCurveFloat>(TimelineEd->CreateNewCurve(TrackType));
			if (SrcCurve && DestCurve)
			{
				CopyCurveData(&SrcCurve->FloatCurve, &DestCurve->FloatCurve);
				NewTrack.CurveKeys = DestCurve;
				CurveBase = DestCurve;
			}
		}
		TrackBase = &NewTrack;
	}
	else if (TrackType == FTTTrackBase::TT_FloatInterp)
	{
		FTTFloatTrack& NewTrack = TimelineObj->FloatTracks[TrackId.TrackIndex];
		if (NewTrack.bIsExternalCurve)
		{
			UCurveFloat* SrcCurve = NewTrack.CurveFloat;
			UCurveFloat* DestCurve = Cast<UCurveFloat>(TimelineEd->CreateNewCurve(TrackType));
			if (SrcCurve && DestCurve)
			{
				CopyCurveData(&SrcCurve->FloatCurve, &DestCurve->FloatCurve);
				NewTrack.CurveFloat = DestCurve;
				CurveBase = DestCurve;
			}
		}
		TrackBase = &NewTrack;
	}
	else if (TrackType == FTTTrackBase::TT_VectorInterp)
	{
		FTTVectorTrack& NewTrack = TimelineObj->VectorTracks[TrackId.TrackIndex];
		if (NewTrack.bIsExternalCurve)
		{
			UCurveVector* SrcCurve = NewTrack.CurveVector;
			UCurveVector* DestCurve = Cast<UCurveVector>(TimelineEd->CreateNewCurve(TrackType));
			if (SrcCurve && DestCurve)
			{
				for (int32 i = 0; i < 3; i++)
				{
					CopyCurveData(&SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i]);
				}
				NewTrack.CurveVector = DestCurve;
				CurveBase = DestCurve;
			}
		}
		TrackBase = &NewTrack;
	}
	else if (TrackType == FTTTrackBase::TT_LinearColorInterp)
	{
		FTTLinearColorTrack& NewTrack = TimelineObj->LinearColorTracks[TrackId.TrackIndex];
		if (NewTrack.bIsExternalCurve)
		{
			UCurveLinearColor* SrcCurve = NewTrack.CurveLinearColor;
			UCurveLinearColor* DestCurve = Cast<UCurveLinearColor>(TimelineEd->CreateNewCurve(TrackType));
			if (SrcCurve && DestCurve)
			{
				for (int32 i = 0; i < 4; i++)
				{
					CopyCurveData(&SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i]);
				}
				NewTrack.CurveLinearColor = DestCurve;
				CurveBase = DestCurve;
			}
		}
		TrackBase = &NewTrack;
	}

	if (TrackBase && CurveBase)
	{
		TrackBase->bIsExternalCurve = false;
		TrackWidget->SetCurveOwner(CurveBase);
		CurveBasePtr = CurveBase;
		ResetExternalCurveInfo();
	}
}

FReply STimelineObjectEdTrack::OnClickClear()
{
	UseInternalCurve();
	return FReply::Handled();
}

void STimelineObjectEdTrack::OnChooseCurve(const FAssetData& InObject)
{
	UCurveBase* SelectedObj = Cast<UCurveBase>(InObject.GetAsset());
	if (SelectedObj)
	{
		SwitchToExternalCurve(SelectedObj);
	}
	else
	{
		UseInternalCurve();
	}
}

FString STimelineObjectEdTrack::GetExternalCurvePath() const
{
	return ExternalCurvePath;
}

UCurveBase* STimelineObjectEdTrack::CreateCurveAsset()
{
	UCurveBase* AssetCurve = nullptr;

	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj);

	FTTTrackId TrackId = TimelineObj->GetDisplayTrackId(Track->DisplayIndex);
	FTTTrackBase::ETrackType TrackType = (FTTTrackBase::ETrackType)TrackId.TrackType;

	if (!TrackWidget.IsValid())
	{
		return nullptr;
	}

	TSharedRef<SDlgPickAssetPath> NewLayerDlg = 
		SNew(SDlgPickAssetPath)
		.Title(LOCTEXT("CreateExternalCurve", "Create External Curve"))
		.DefaultAssetPath(FText::FromString(CreateUniqueCurveAssetPathName()));

	if (NewLayerDlg->ShowModal() == EAppReturnType::Cancel)
	{
		return nullptr;
	}

	FString PackageName = NewLayerDlg->GetFullAssetPath().ToString();
	FName AssetName = FName(*NewLayerDlg->GetAssetName().ToString());

	UPackage* Package = CreatePackage(*PackageName);
	
	// Determine curve type based on track type
	TSubclassOf<UCurveBase> CurveType;
	if (TrackType == FTTTrackBase::TT_Event || TrackType == FTTTrackBase::TT_FloatInterp)
	{
		CurveType = UCurveFloat::StaticClass();
	}
	else if (TrackType == FTTTrackBase::TT_LinearColorInterp)
	{
		CurveType = UCurveLinearColor::StaticClass();
	}
	else 
	{
		CurveType = UCurveVector::StaticClass();
	}

	UObject* NewObj = TrackWidget->CreateCurveObject(CurveType, Package, AssetName);
	if (!NewObj)
	{
		return nullptr;
	}

	// Copy curve data to the new asset
	if (TrackType == FTTTrackBase::TT_Event || TrackType == FTTTrackBase::TT_FloatInterp)
	{
		UCurveFloat* DestCurve = CastChecked<UCurveFloat>(NewObj);
		AssetCurve = DestCurve;
		UCurveFloat* SourceCurve = CastChecked<UCurveFloat>(CurveBasePtr);

		if (SourceCurve && DestCurve)
		{
			CopyCurveData(&SourceCurve->FloatCurve, &DestCurve->FloatCurve);
		}

		DestCurve->bIsEventCurve = (TrackType == FTTTrackBase::TT_Event);
	}
	else if (TrackType == FTTTrackBase::TT_VectorInterp)
	{
		UCurveVector* DestCurve = Cast<UCurveVector>(NewObj);
		AssetCurve = DestCurve;
		UCurveVector* SrcCurve = CastChecked<UCurveVector>(CurveBasePtr);

		if (SrcCurve && DestCurve)
		{
			for (int32 i = 0; i < 3; i++)
			{
				CopyCurveData(&SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i]);
			}
		}
	}
	else if (TrackType == FTTTrackBase::TT_LinearColorInterp)
	{
		UCurveLinearColor* DestCurve = Cast<UCurveLinearColor>(NewObj);
		AssetCurve = DestCurve;
		UCurveLinearColor* SrcCurve = CastChecked<UCurveLinearColor>(CurveBasePtr);

		if (SrcCurve && DestCurve)
		{
			for (int32 i = 0; i < 4; i++)
			{
				CopyCurveData(&SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i]);
			}
		}
	}

	// Select and register the new asset
	USelection* SelectionSet = GEditor->GetSelectedObjects();
	SelectionSet->DeselectAll();
	SelectionSet->Select(NewObj);

	FAssetRegistryModule::AssetCreated(NewObj);
	Package->GetOutermost()->MarkPackageDirty();

	return AssetCurve;
}

void STimelineObjectEdTrack::CopyCurveData(const FRichCurve* SrcCurve, FRichCurve* DestCurve)
{
	if (SrcCurve && DestCurve)
	{
		for (auto It(SrcCurve->GetKeyIterator()); It; ++It)
		{
			const FRichCurveKey& Key = *It;
			FKeyHandle KeyHandle = DestCurve->AddKey(Key.Time, Key.Value);
			DestCurve->GetKey(KeyHandle) = Key;
		}
	}
}

void STimelineObjectEdTrack::ResetExternalCurveInfo()
{
	ExternalCurvePath = FString(TEXT("None"));
}

#pragma endregion

#pragma region STimelineObjectEdTrack Expansion and View

ECheckBoxState STimelineObjectEdTrack::GetIsExpandedState() const
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	return (TrackBase && TrackBase->bIsExpanded) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STimelineObjectEdTrack::OnIsExpandedStateChanged(ECheckBoxState IsExpandedState)
{
	FTTTrackBase* TrackBase = GetTrackBase();
	if (TrackBase)
	{
		TrackBase->bIsExpanded = IsExpandedState == ECheckBoxState::Checked;
	}

	TSharedPtr<STimelineObjectEditorPanel> TimelineEditor = TimelineEdPtr.Pin();
	TimelineEditor->OnTimelineChanged();
}

EVisibility STimelineObjectEdTrack::GetContentVisibility() const
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	return (TrackBase && TrackBase->bIsExpanded) ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState STimelineObjectEdTrack::GetIsCurveViewSynchronizedState() const
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	return (TrackBase && TrackBase->bIsCurveViewSynchronized) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STimelineObjectEdTrack::OnIsCurveViewSynchronizedStateChanged(ECheckBoxState IsCurveViewSynchronizedState)
{
	FTTTrackBase* TrackBase = GetTrackBase();
	if (TrackBase)
	{
		TrackBase->bIsCurveViewSynchronized = IsCurveViewSynchronizedState == ECheckBoxState::Checked;
	}

	TSharedPtr<STimelineObjectEditorPanel> TimelineEditor = TimelineEdPtr.Pin();
	if ((TimelineEditor->GetViewMaxInput() == 0) && (TimelineEditor->GetViewMinInput() == 0))
	{
		TimelineEditor->SetInputViewRange(LocalInputMin, LocalInputMax);
		TimelineEditor->SetOutputViewRange(LocalOutputMin, LocalOutputMax);
	}
	if ((TrackBase && TrackBase->bIsCurveViewSynchronized) || ((LocalInputMax == 0.0f) && (LocalInputMin == 0.0f)))
	{
		LocalInputMin = TimelineEditor->GetViewMinInput();
		LocalInputMax = TimelineEditor->GetViewMaxInput();
		LocalOutputMin = TimelineEditor->GetViewMinOutput();
		LocalOutputMax = TimelineEditor->GetViewMaxOutput();
	}
}

#pragma endregion

#pragma region STimelineObjectEdTrack Reordering

FReply STimelineObjectEdTrack::OnMoveUp()
{
	MoveTrack(-1);
	return FReply::Handled();
}

bool STimelineObjectEdTrack::CanMoveUp() const
{
	return (Track->DisplayIndex > 0);
}

FReply STimelineObjectEdTrack::OnMoveDown()
{
	MoveTrack(1);
	return FReply::Handled();
}

bool STimelineObjectEdTrack::CanMoveDown() const
{
	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj);

	return (Track->DisplayIndex < (TimelineObj->GetNumDisplayTracks() - 1));
}

void STimelineObjectEdTrack::MoveTrack(int32 DirectionDelta)
{
	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	TimelineEd->OnReorderTracks(Track->DisplayIndex, DirectionDelta);
}

#pragma endregion

#pragma region STimelineObjectEdTrack View Range

float STimelineObjectEdTrack::GetMinInput() const
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	return (TrackBase && TrackBase->bIsCurveViewSynchronized)
		? TimelineEdPtr.Pin()->GetViewMinInput()
		: LocalInputMin;
}

float STimelineObjectEdTrack::GetMaxInput() const
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	return (TrackBase && TrackBase->bIsCurveViewSynchronized)
		? TimelineEdPtr.Pin()->GetViewMaxInput()
		: LocalInputMax;
}

float STimelineObjectEdTrack::GetMinOutput() const
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	return (TrackBase && TrackBase->bIsCurveViewSynchronized)
		? TimelineEdPtr.Pin()->GetViewMinOutput()
		: LocalOutputMin;
}

float STimelineObjectEdTrack::GetMaxOutput() const
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	return (TrackBase && TrackBase->bIsCurveViewSynchronized)
		? TimelineEdPtr.Pin()->GetViewMaxOutput()
		: LocalOutputMax;
}

void STimelineObjectEdTrack::OnSetInputViewRange(float Min, float Max)
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	if (TrackBase && TrackBase->bIsCurveViewSynchronized)
	{
		TimelineEdPtr.Pin()->SetInputViewRange(Min, Max);
	}
	LocalInputMin = Min;
	LocalInputMax = Max;
}

void STimelineObjectEdTrack::OnSetOutputViewRange(float Min, float Max)
{
	const FTTTrackBase* TrackBase = GetTrackBase();
	if (TrackBase && TrackBase->bIsCurveViewSynchronized)
	{
		TimelineEdPtr.Pin()->SetOutputViewRange(Min, Max);
	}
	LocalOutputMin = Min;
	LocalOutputMax = Max;
}

#pragma endregion

#pragma region STimelineObjectEdTrack Track Base Access

FTTTrackBase* STimelineObjectEdTrack::GetTrackBase()
{
	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj);
	return TimelineEditorHelpers::GetTrackFromTimeline(TimelineObj, Track);
}

const FTTTrackBase* STimelineObjectEdTrack::GetTrackBase() const
{
	TSharedPtr<STimelineObjectEditorPanel> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj);
	return TimelineEditorHelpers::GetTrackFromTimeline(TimelineObj, Track);
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Construction

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STimelineObjectEditorPanel::Construct(const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InKismet2, UTimelineTemplate* InTimelineObj)
{
	NewTrackPendingRename = NAME_None;
	Kismet2Ptr = InKismet2;
	TimelineObj = nullptr;

	NominalTimelineDesiredHeight = 300.0f;
	TimelineDesiredSize = FVector2f(128.0f, NominalTimelineDesiredHeight);

	ViewMinInput = 0.f;
	ViewMaxInput = 0.f;
	ViewMinOutput = 0.f;
	ViewMaxOutput = 0.f;

	// Set up command bindings
	CommandList = MakeShareable(new FUICommandList);

	CommandList->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &STimelineObjectEditorPanel::OnRequestTrackRename),
		FCanExecuteAction::CreateSP(this, &STimelineObjectEditorPanel::CanRenameSelectedTrack));

	CommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &STimelineObjectEditorPanel::OnDeleteSelectedTracks),
		FCanExecuteAction::CreateSP(this, &STimelineObjectEditorPanel::CanDeleteSelectedTracks));

	// Initialize tick group dropdown
	int32 CurrentTickGroupNameStringIndex = 0;
	const UEnum* TickGroupEnum = StaticEnum<ETickingGroup>();
	if (!TickGroupNamesInitialized && TickGroupEnum)
	{
		TickGroupNameStrings.Empty();
		for (int32 TickGroupIndex = 0; TickGroupIndex < TickGroupEnum->NumEnums() - 1; TickGroupIndex++)
		{
			if (!TickGroupEnum->HasMetaData(TEXT("Hidden"), TickGroupIndex))
			{
				TickGroupNameStrings.Add(MakeShareable(new FString(TickGroupEnum->GetNameStringByIndex(TickGroupIndex))));
			}
		}
		TickGroupNamesInitialized = true;
	}
	if (TickGroupNamesInitialized && InTimelineObj)
	{
		FString CurrentTickGroupNameString = TickGroupEnum->GetNameStringByValue((int64)InTimelineObj->TimelineTickGroup);
		CurrentTickGroupNameStringIndex = TickGroupNameStrings.IndexOfByPredicate([CurrentTickGroupNameString](const TSharedPtr<FString> NameString)
		{
			return *NameString.Get() == CurrentTickGroupNameString;
		});
	}
	else
	{
		TickGroupNameStrings.Empty();
		TickGroupNameStrings.Add(MakeShareable(new FString(TEXT("EnumNotReady"))));
	}

	// Build the main panel widget
	this->ChildSlot
	[
		SNew(SVerticalBox)
		
		// Title bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Graph.TitleBackground")))
			.HAlign(HAlign_Center)
			.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.Title"))
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("GraphEditor.TimelineGlyph")))
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.ColorAndOpacity(FLinearColor(1, 1, 1, 0.5))
					.Text(this, &STimelineObjectEditorPanel::GetTimelineName)
				]
			]
		]
		
		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			
			// Add track button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f)
			[
				SNew(SPositiveActionButton)
				.OnGetMenuContent(this, &STimelineObjectEditorPanel::MakeAddButton)
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.Text(LOCTEXT("Track", "Track"))
			]
			
			// Length label
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Length", "Length"))
			]
			
			// Length edit box
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(6.0f, 2.0f, 2.0f, 2.0f))
			.VAlign(VAlign_Center)
			[
				SAssignNew(TimelineLengthEdit, SEditableTextBox)
				.Text(this, &STimelineObjectEditorPanel::GetLengthString)
				.OnTextCommitted(this, &STimelineObjectEditorPanel::OnLengthStringChanged)
				.SelectAllTextWhenFocused(true)
				.MinDesiredWidth(64)
				.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.Length"))
			]
			
			// Use Last Keyframe toggle
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(UseLastKeyframeCheckBox, SCheckBox)
				.IsChecked(this, &STimelineObjectEditorPanel::IsUseLastKeyframeChecked)
				.OnCheckStateChanged(this, &STimelineObjectEditorPanel::OnUseLastKeyframeChanged)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(LOCTEXT("UseLastKeyframe", "Use Last Keyframe"))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("TimelineEditor.UseLastKeyframe"))
					.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.UseLastKeyframe"))
				]
			]
			
			// AutoPlay toggle
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(PlayCheckBox, SCheckBox)
				.IsChecked(this, &STimelineObjectEditorPanel::IsAutoPlayChecked)
				.OnCheckStateChanged(this, &STimelineObjectEditorPanel::OnAutoPlayChanged)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(LOCTEXT("AutoPlay", "AutoPlay"))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("TimelineEditor.AutoPlay"))
					.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.AutoPlay"))
				]
			]
			
			// Loop toggle
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(LoopCheckBox, SCheckBox)
				.IsChecked(this, &STimelineObjectEditorPanel::IsLoopChecked)
				.OnCheckStateChanged(this, &STimelineObjectEditorPanel::OnLoopChanged)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(LOCTEXT("Loop", "Loop"))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("TimelineEditor.Loop"))
					.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.Loop"))
				]
			]
			
			// Replicated toggle
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ReplicatedCheckBox, SCheckBox)
				.IsChecked(this, &STimelineObjectEditorPanel::IsReplicatedChecked)
				.OnCheckStateChanged(this, &STimelineObjectEditorPanel::OnReplicatedChanged)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(LOCTEXT("Replicated", "Replicated"))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("TimelineEditor.Replicated"))
					.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.Replicated"))
				]
			]
			
			// Ignore Time Dilation toggle
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(IgnoreTimeDilationCheckBox, SCheckBox)
				.IsChecked(this, &STimelineObjectEditorPanel::IsIgnoreTimeDilationChecked)
				.OnCheckStateChanged(this, &STimelineObjectEditorPanel::OnIgnoreTimeDilationChanged)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(LOCTEXT("IgnoreTimeDilation", "Ignore Time Dilation"))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("TimelineEditor.IgnoreTimeDilation"))
					.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.IgnoreTimeDilation"))
				]
			]
			
			// Tick Group label
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TickGroupLabel", "Tick Group"))
				.AddMetaData<FTagMetaData>(TEXT("TimelineEditor.TickGroup"))
			]
			
			// Tick Group dropdown
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextComboBox)
				.OptionsSource(&TickGroupNameStrings)
				.InitiallySelectedItem(TickGroupNameStrings[CurrentTickGroupNameStringIndex])
				.OnSelectionChanged(this, &STimelineObjectEditorPanel::OnTimelineTickGroupChanged)
				.ToolTipText(LOCTEXT("TimelineTickGroupDropdownTooltip", "Select the TickGroup you want this timeline to run in.\nTo assign options use context menu on timelines."))
			]
		]
		
		// Track list
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SAssignNew(TrackListView, STimelineObjectEdTrackListType)
			.ListItemsSource(&TrackList)
			.OnGenerateRow(this, &STimelineObjectEditorPanel::MakeTrackWidget)
			.OnItemScrolledIntoView(this, &STimelineObjectEditorPanel::OnItemScrolledIntoView)
			.OnContextMenuOpening(this, &STimelineObjectEditorPanel::MakeContextMenu)
			.SelectionMode(ESelectionMode::SingleToggle)
		]
	];

	TimelineObj = InTimelineObj;
	check(TimelineObj);

	OnTimelineChanged();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STimelineObjectEditorPanel::Construct(const FArguments& InArgs, UBlueprint* InBlueprint, UTimelineTemplate* InTimelineObj, UK2Node_TimelineObject* InTimelineNode)
{
	BlueprintPtr = InBlueprint;
	TimelineNodePtr = InTimelineNode;
	Construct(InArgs, TSharedPtr<FBlueprintEditor>(), InTimelineObj);
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Public Interface

UBlueprint* STimelineObjectEditorPanel::GetBlueprint() const
{
	if (TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin())
	{
		return Kismet2->GetBlueprintObj();
	}
	return BlueprintPtr.Get();
}

void STimelineObjectEditorPanel::RefreshNode()
{
	if (TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin())
	{
		Kismet2->RefreshEditors();
	}
	else if (UK2Node_TimelineObject* Node = TimelineNodePtr.Get())
	{
		Node->ReconstructNode();
	}
}

void STimelineObjectEditorPanel::MarkBlueprintModified()
{
	if (UBlueprint* Blueprint = GetBlueprint())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void STimelineObjectEditorPanel::MarkBlueprintStructurallyModified()
{
	if (UBlueprint* Blueprint = GetBlueprint())
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

UTimelineTemplate* STimelineObjectEditorPanel::GetTimeline()
{
	return TimelineObj;
}

void STimelineObjectEditorPanel::OnTimelineChanged()
{
	TrackList.Empty();

	TSharedPtr<FTimelineObjectEdTrack> NewlyCreatedTrack;

	if (TimelineObj != nullptr)
	{
		for (int32 i = 0; i < TimelineObj->GetNumDisplayTracks(); ++i)
		{
			FTTTrackId TrackId = TimelineObj->GetDisplayTrackId(i);

			TSharedRef<FTimelineObjectEdTrack> Track = FTimelineObjectEdTrack::Make(i);
			TrackList.Add(Track);

			FTTTrackBase* TrackBase = TimelineEditorHelpers::GetTrackFromTimeline(TimelineObj, Track);
			if (TrackBase->GetTrackName() == NewTrackPendingRename)
			{
				NewlyCreatedTrack = Track;
			}
		}
	}

	TrackListView->RequestListRefresh();
	TrackListView->RequestScrollIntoView(NewlyCreatedTrack);
}

#pragma endregion

#pragma region STimelineObjectEditorPanel View Range

float STimelineObjectEditorPanel::GetViewMaxInput() const
{
	return ViewMaxInput;
}

float STimelineObjectEditorPanel::GetViewMinInput() const
{
	return ViewMinInput;
}

float STimelineObjectEditorPanel::GetViewMaxOutput() const
{
	return ViewMaxOutput;
}

float STimelineObjectEditorPanel::GetViewMinOutput() const
{
	return ViewMinOutput;
}

float STimelineObjectEditorPanel::GetTimelineLength() const
{
	return (TimelineObj != nullptr) ? TimelineObj->TimelineLength : 0.f;
}

void STimelineObjectEditorPanel::SetInputViewRange(float InViewMinInput, float InViewMaxInput)
{
	ViewMaxInput = InViewMaxInput;
	ViewMinInput = InViewMinInput;
}

void STimelineObjectEditorPanel::SetOutputViewRange(float InViewMinOutput, float InViewMaxOutput)
{
	ViewMaxOutput = InViewMaxOutput;
	ViewMinOutput = InViewMinOutput;
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Track List

TSharedRef<ITableRow> STimelineObjectEditorPanel::MakeTrackWidget(TSharedPtr<FTimelineObjectEdTrack> Track, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Track.IsValid());

	return
		SNew(STableRow<TSharedPtr<FTimelineObjectEdTrack>>, OwnerTable)
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TimelineEditor.TrackRowSubtleHighlight"))
		.Padding(FMargin(0, 0, 0, 2))
		[
			SNew(STimelineObjectEdTrack, Track, SharedThis(this))
		];
}

void STimelineObjectEditorPanel::CreateNewTrack(FTTTrackBase::ETrackType Type)
{
	FName TrackName;
	do
	{
		TrackName = MakeUniqueObjectName(TimelineObj, UTimelineTemplate::StaticClass(), FName(*(LOCTEXT("NewTrack_DefaultName", "NewTrack").ToString())));
	} while (!TimelineObj->IsNewTrackNameValid(TrackName));
		
	UBlueprint* Blueprint = GetBlueprint();
	if (!Blueprint)
	{
		return;
	}
	
	UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
	FText ErrorMessage;

	if (TimelineNode)
	{
		const FScopedTransaction Transaction(LOCTEXT("TimelineEditor_AddNewTrack", "Add new track"));

		TimelineNode->Modify();
		TimelineObj->Modify();

		NewTrackPendingRename = TrackName;
		
		FTTTrackId NewTrackId;
		NewTrackId.TrackType = Type;

		// Create track based on type
		if (Type == FTTTrackBase::TT_Event)
		{
			NewTrackId.TrackIndex = TimelineObj->EventTracks.Num();
				
			FTTEventTrack NewTrack;
			NewTrack.SetTrackName(TrackName, TimelineObj);
			NewTrack.CurveKeys = NewObject<UCurveFloat>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
			NewTrack.CurveKeys->bIsEventCurve = true;
			TimelineObj->EventTracks.Add(NewTrack);
		}
		else if (Type == FTTTrackBase::TT_FloatInterp)
		{
			NewTrackId.TrackIndex = TimelineObj->FloatTracks.Num();

			FTTFloatTrack NewTrack;
			NewTrack.SetTrackName(TrackName, TimelineObj);
			NewTrack.CurveFloat = FindFirstObject<UCurveFloat>(*TrackName.ToString(), EFindFirstObjectOptions::NativeFirst | EFindFirstObjectOptions::EnsureIfAmbiguous);
			if (NewTrack.CurveFloat == nullptr)
			{
				NewTrack.CurveFloat = NewObject<UCurveFloat>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
			}
			TimelineObj->FloatTracks.Add(NewTrack);
		}
		else if (Type == FTTTrackBase::TT_VectorInterp)
		{
			NewTrackId.TrackIndex = TimelineObj->VectorTracks.Num();

			FTTVectorTrack NewTrack;
			NewTrack.SetTrackName(TrackName, TimelineObj);
			NewTrack.CurveVector = NewObject<UCurveVector>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
			TimelineObj->VectorTracks.Add(NewTrack);
		}
		else if (Type == FTTTrackBase::TT_LinearColorInterp)
		{
			NewTrackId.TrackIndex = TimelineObj->LinearColorTracks.Num();

			FTTLinearColorTrack NewTrack;
			NewTrack.SetTrackName(TrackName, TimelineObj);
			NewTrack.CurveLinearColor = NewObject<UCurveLinearColor>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
			TimelineObj->LinearColorTracks.Add(NewTrack);
		}

		TimelineObj->AddDisplayTrack(NewTrackId);

		TimelineNode->ReconstructNode();
		RefreshNode();
		OnTimelineChanged();
	}
	else
	{
		ErrorMessage = LOCTEXT("InvalidTimelineNodeCreate", "Failed to create track. Timeline node is invalid. Please remove timeline node.");
	}

	if (!ErrorMessage.IsEmpty())
	{
		FNotificationInfo Info(ErrorMessage);
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

UCurveBase* STimelineObjectEditorPanel::CreateNewCurve(FTTTrackBase::ETrackType Type)
{
	if (!TimelineObj)
	{
		return nullptr;
	}

	UCurveBase* NewCurve = nullptr;
	
	if (Type == FTTTrackBase::TT_Event)
	{
		NewCurve = NewObject<UCurveFloat>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
	}
	else if (Type == FTTTrackBase::TT_FloatInterp)
	{
		NewCurve = NewObject<UCurveFloat>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
	}
	else if (Type == FTTTrackBase::TT_VectorInterp)
	{
		NewCurve = NewObject<UCurveVector>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
	}
	else if (Type == FTTTrackBase::TT_LinearColorInterp)
	{
		NewCurve = NewObject<UCurveLinearColor>(TimelineObj, NAME_None, RF_Transactional | RF_Public);
	}

	return NewCurve;
}

bool STimelineObjectEditorPanel::CanDeleteSelectedTracks() const
{
	return (TrackListView->GetNumItemsSelected() == 1);
}

void STimelineObjectEditorPanel::OnDeleteSelectedTracks()
{
	if (TimelineObj == nullptr)
	{
		return;
	}

	UBlueprint* Blueprint = GetBlueprint();
	UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);

	TArray<TSharedPtr<FTimelineObjectEdTrack>> SelTracks = TrackListView->GetSelectedItems();
	if (SelTracks.Num() != 1)
	{
		return;
	}

	if (TimelineNode)
	{
		const FScopedTransaction Transaction(LOCTEXT("TimelineEditor_DeleteTrack", "Delete track"));

		TimelineNode->Modify();
		TimelineObj->Modify();

		TSharedPtr<FTimelineObjectEdTrack> SelTrack = SelTracks[0];
		FTTTrackId TrackId = TimelineObj->GetDisplayTrackId(SelTrack->DisplayIndex);
		FTTTrackBase::ETrackType TrackType = (FTTTrackBase::ETrackType)TrackId.TrackType;

		TimelineObj->RemoveDisplayTrack(SelTrack->DisplayIndex);

		// Remove from appropriate track array
		if (TrackType == FTTTrackBase::TT_Event)
		{
			TimelineObj->EventTracks.RemoveAt(TrackId.TrackIndex);
		}
		else if (TrackType == FTTTrackBase::TT_FloatInterp)
		{
			TimelineObj->FloatTracks.RemoveAt(TrackId.TrackIndex);
		}
		else if (TrackType == FTTTrackBase::TT_VectorInterp)
		{
			TimelineObj->VectorTracks.RemoveAt(TrackId.TrackIndex);
		}
		else if (TrackType == FTTTrackBase::TT_LinearColorInterp)
		{
			TimelineObj->LinearColorTracks.RemoveAt(TrackId.TrackIndex);
		}

		TimelineNode->ReconstructNode();
		RefreshNode();
		OnTimelineChanged();
		TrackListView->RebuildList();
	}
	else
	{
		FNotificationInfo Info(LOCTEXT("InvalidTimelineNodeDestroy", "Failed to destroy track. Timeline node is invalid. Please remove timeline node."));
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

void STimelineObjectEditorPanel::OnItemScrolledIntoView(TSharedPtr<FTimelineObjectEdTrack> InTrackNode, const TSharedPtr<ITableRow>& InWidget)
{
	if (NewTrackPendingRename != NAME_None)
	{
		InTrackNode->OnRenameRequest.ExecuteIfBound();
		NewTrackPendingRename = NAME_None;
	}
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Track Name Management

bool STimelineObjectEditorPanel::OnVerifyTrackNameCommit(const FText& TrackName, FText& OutErrorMessage, FTTTrackBase* TrackBase, STimelineObjectEdTrack* Track)
{
	FName RequestedName(*TrackName.ToString());
	bool bValid = true;

	if (TrackName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("NameMissing_Error", "You must provide a name.");
		bValid = false;
	}
	else if (TrackBase->GetTrackName() != RequestedName && !TimelineObj->IsNewTrackNameValid(RequestedName))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("TrackName"), TrackName);
		OutErrorMessage = FText::Format(LOCTEXT("AlreadyInUse", "\"{TrackName}\" is already in use."), Args);
		bValid = false;
	}
	else
	{
		// Check for conflicts with default pin names
		UBlueprint* Blueprint = GetBlueprint();
		UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			for (UEdGraphPin* Pin : TimelineNode->Pins)
			{
				if (Pin->PinName == RequestedName)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("TrackName"), TrackName);
					OutErrorMessage = FText::Format(LOCTEXT("PinAlreadyInUse", "\"{TrackName}\" is already in use as a default pin!"), Args);
					bValid = false;
					break;
				}
			}
		}
	}

	return bValid;
}

void STimelineObjectEditorPanel::OnTrackNameCommitted(const FText& StringName, ETextCommit::Type CommitInfo, FTTTrackBase* TrackBase, STimelineObjectEdTrack* Track)
{
	FName RequestedName(*StringName.ToString());
	if (!TimelineObj->IsNewTrackNameValid(RequestedName))
	{
		return;
	}

	TimelineObj->Modify();
	UBlueprint* Blueprint = GetBlueprint();

	UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
	
	if (TimelineNode)
	{
		// Update pin name
		for (int32 PinIdx = TimelineNode->Pins.Num() - 1; PinIdx >= 0; --PinIdx)
		{
			UEdGraphPin* Pin = TimelineNode->Pins[PinIdx];
		
			if (Pin->PinName == TrackBase->GetTrackName())
			{
				Pin->Modify();
				Pin->PinName = RequestedName;
				break;
			}
		}

		TrackBase->SetTrackName(RequestedName, TimelineObj);

		RefreshNode();
		OnTimelineChanged();
	}
}

void STimelineObjectEditorPanel::OnReorderTracks(int32 DisplayIndex, int32 DirectionDelta)
{
	if (TimelineObj == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("TimelineEditor_ReorderTrack", "Reorder track"));

	UBlueprint* Blueprint = GetBlueprint();
	UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);

	TimelineNode->Modify();
	TimelineObj->Modify();

	TimelineObj->MoveDisplayTrack(DisplayIndex, DirectionDelta);

	TimelineNode->ReconstructNode();
	RefreshNode();
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Timeline Properties

void STimelineObjectEditorPanel::OnTimelineTickGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (TickGroupNamesInitialized && TimelineObj && NewValue.IsValid())
	{
		if (const UEnum* TickGroupEnum = StaticEnum<ETickingGroup>())
		{
			ETickingGroup NewTickGroup = (ETickingGroup)TickGroupEnum->GetValueByNameString(*NewValue.Get());
			if (NewTickGroup != TimelineObj->TimelineTickGroup)
			{
				TimelineObj->TimelineTickGroup = NewTickGroup;
				MarkBlueprintModified();
			}
		}
	}
}

FText STimelineObjectEditorPanel::GetTimelineName() const
{
	if (TimelineObj != nullptr)
	{
		return FText::FromString(TimelineObj->GetVariableName().ToString());
	}
	return LOCTEXT("NoTimeline", "No Timeline");
}

ECheckBoxState STimelineObjectEditorPanel::IsAutoPlayChecked() const
{
	return (TimelineObj && TimelineObj->bAutoPlay) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STimelineObjectEditorPanel::OnAutoPlayChanged(ECheckBoxState NewType)
{
	if (TimelineObj)
	{
		TimelineObj->bAutoPlay = (NewType == ECheckBoxState::Checked);

		UBlueprint* Blueprint = GetBlueprint();
		UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			TimelineNode->GetTimelineTemplate()->bAutoPlay = TimelineObj->bAutoPlay;
			MarkBlueprintModified();
		}
	}
}

ECheckBoxState STimelineObjectEditorPanel::IsLoopChecked() const
{
	return (TimelineObj && TimelineObj->bLoop) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STimelineObjectEditorPanel::OnLoopChanged(ECheckBoxState NewType)
{
	if (TimelineObj)
	{
		TimelineObj->bLoop = (NewType == ECheckBoxState::Checked);

		UBlueprint* Blueprint = GetBlueprint();
		UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			TimelineNode->GetTimelineTemplate()->bLoop = TimelineObj->bLoop;
			MarkBlueprintModified();
		}
	}
}

ECheckBoxState STimelineObjectEditorPanel::IsReplicatedChecked() const
{
	return (TimelineObj && TimelineObj->bReplicated) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STimelineObjectEditorPanel::OnReplicatedChanged(ECheckBoxState NewType)
{
	if (TimelineObj)
	{
		TimelineObj->bReplicated = (NewType == ECheckBoxState::Checked);

		UBlueprint* Blueprint = GetBlueprint();
		UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			TimelineNode->GetTimelineTemplate()->bReplicated = TimelineObj->bReplicated;
			MarkBlueprintModified();
		}
	}
}

ECheckBoxState STimelineObjectEditorPanel::IsUseLastKeyframeChecked() const
{
	return (TimelineObj && TimelineObj->LengthMode == ETimelineLengthMode::TL_LastKeyFrame) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STimelineObjectEditorPanel::OnUseLastKeyframeChanged(ECheckBoxState NewType)
{
	if (TimelineObj)
	{
		TimelineObj->LengthMode = (NewType == ECheckBoxState::Checked) ? ETimelineLengthMode::TL_LastKeyFrame : ETimelineLengthMode::TL_TimelineLength;
		MarkBlueprintModified();
	}
}

ECheckBoxState STimelineObjectEditorPanel::IsIgnoreTimeDilationChecked() const
{
	return (TimelineObj && TimelineObj->bIgnoreTimeDilation) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STimelineObjectEditorPanel::OnIgnoreTimeDilationChanged(ECheckBoxState NewType)
{
	if (TimelineObj)
	{
		TimelineObj->bIgnoreTimeDilation = (NewType == ECheckBoxState::Checked);

		UBlueprint* Blueprint = GetBlueprint();
		MarkBlueprintModified();

		UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			TimelineNode->GetTimelineTemplate()->bIgnoreTimeDilation = TimelineObj->bIgnoreTimeDilation;
		}
	}
}

FText STimelineObjectEditorPanel::GetLengthString() const
{
	FString LengthString(TEXT("0.0"));
	if (TimelineObj != nullptr)
	{
		LengthString = FString::Printf(TEXT("%.2f"), TimelineObj->TimelineLength);
	}
	return FText::FromString(LengthString);
}

void STimelineObjectEditorPanel::OnLengthStringChanged(const FText& NewString, ETextCommit::Type CommitInfo)
{
	bool bCommitted = (CommitInfo == ETextCommit::OnEnter) || (CommitInfo == ETextCommit::OnUserMovedFocus);
	if (TimelineObj != nullptr && bCommitted)
	{
		float NewLength = FCString::Atof(*NewString.ToString());
		if (NewLength > KINDA_SMALL_NUMBER)
		{
			TimelineObj->TimelineLength = NewLength;
			MarkBlueprintModified();
		}
	}
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Curve Asset Integration

bool STimelineObjectEditorPanel::IsCurveAssetSelected() const
{
	if (GIsSavingPackage || IsGarbageCollecting())
	{
		return false;
	}

	TArray<UClass*> SelectionList;
	GEditor->GetContentBrowserSelectionClasses(SelectionList);

	for (UClass* Item : SelectionList)
	{
		if (Item->IsChildOf(UCurveBase::StaticClass()))
		{
			return true;
		}
	}
	
	return false;
}

void STimelineObjectEditorPanel::CreateNewTrackFromAsset()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	UCurveBase* SelectedObj = GEditor->GetSelectedObjects()->GetTop<UCurveBase>();

	UBlueprint* Blueprint = GetBlueprint();
	UK2Node_TimelineObject* TimelineNode = FindTimelineObjectNode(Blueprint, TimelineObj);
		
	if (!SelectedObj || !TimelineNode)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("TimelineEditor_CreateFromAsset", "Add new track from asset"));

	TimelineNode->Modify();
	TimelineObj->Modify();

	const FName TrackName = SelectedObj->GetFName();

	if (SelectedObj->IsA(UCurveFloat::StaticClass()))
	{
		UCurveFloat* FloatCurveObj = CastChecked<UCurveFloat>(SelectedObj);
		if (FloatCurveObj->bIsEventCurve)
		{
			FTTEventTrack NewEventTrack;
			NewEventTrack.SetTrackName(TrackName, TimelineObj);
			NewEventTrack.CurveKeys = CastChecked<UCurveFloat>(SelectedObj);
			NewEventTrack.bIsExternalCurve = true;
			TimelineObj->EventTracks.Add(NewEventTrack);
		}
		else
		{
			FTTFloatTrack NewFloatTrack;
			NewFloatTrack.SetTrackName(TrackName, TimelineObj);
			NewFloatTrack.CurveFloat = CastChecked<UCurveFloat>(SelectedObj);
			NewFloatTrack.bIsExternalCurve = true;
			TimelineObj->FloatTracks.Add(NewFloatTrack);
		}
	}
	else if (SelectedObj->IsA(UCurveVector::StaticClass()))
	{
		FTTVectorTrack NewTrack;
		NewTrack.SetTrackName(TrackName, TimelineObj);
		NewTrack.CurveVector = CastChecked<UCurveVector>(SelectedObj);
		NewTrack.bIsExternalCurve = true;
		TimelineObj->VectorTracks.Add(NewTrack);
	}
	else if (SelectedObj->IsA(UCurveLinearColor::StaticClass()))
	{
		FTTLinearColorTrack NewTrack;
		NewTrack.SetTrackName(TrackName, TimelineObj);
		NewTrack.CurveLinearColor = CastChecked<UCurveLinearColor>(SelectedObj);
		NewTrack.bIsExternalCurve = true;
		TimelineObj->LinearColorTracks.Add(NewTrack);
	}

	TimelineNode->ReconstructNode();
	RefreshNode();
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Context Menu

bool STimelineObjectEditorPanel::CanRenameSelectedTrack() const
{
	return TrackListView->GetNumItemsSelected() == 1;
}

void STimelineObjectEditorPanel::OnRequestTrackRename() const
{
	check(TrackListView->GetNumItemsSelected() == 1);
	TrackListView->GetSelectedItems()[0]->OnRenameRequest.Execute();
}

FReply STimelineObjectEditorPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

TSharedPtr<SWidget> STimelineObjectEditorPanel::MakeContextMenu() const
{
	FMenuBuilder MenuBuilder(true, CommandList);
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	}

	// Height slider
	{
		TSharedRef<SWidget> SizeSlider = SNew(SSlider)
			.Value(this, &STimelineObjectEditorPanel::GetSizeScaleValue)
			.OnValueChanged(const_cast<STimelineObjectEditorPanel*>(this), &STimelineObjectEditorPanel::SetSizeScaleValue);

		MenuBuilder.AddWidget(SizeSlider, LOCTEXT("TimelineEditorVerticalSize", "Height"));
	}

	return MenuBuilder.MakeWidget();
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Size Scaling

FVector2D STimelineObjectEditorPanel::GetTimelineDesiredSize() const
{
	return FVector2D{TimelineDesiredSize};
}

void STimelineObjectEditorPanel::SetSizeScaleValue(float NewValue)
{
	TimelineDesiredSize.Y = NominalTimelineDesiredHeight * (1.0f + NewValue * 5.0f);
	TrackListView->RequestListRefresh();
}

float STimelineObjectEditorPanel::GetSizeScaleValue() const
{
	return ((TimelineDesiredSize.Y / NominalTimelineDesiredHeight) - 1.0f) / 5.0f;
}

#pragma endregion

#pragma region STimelineObjectEditorPanel Add Button Menu

TSharedRef<SWidget> STimelineObjectEditorPanel::MakeAddButton()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddFloatTrack", "Add Float Track"),
		LOCTEXT("AddFloatTrackToolTip", "Adds a Float Track."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TimelineEditor.AddFloatTrack"),
		FUIAction(FExecuteAction::CreateRaw(this, &STimelineObjectEditorPanel::CreateNewTrack, FTTTrackBase::TT_FloatInterp)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddVectorTrack", "Add Vector Track"),
		LOCTEXT("AddVectorTrackToolTip", "Adds a Vector Track."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TimelineEditor.AddVectorTrack"),
		FUIAction(FExecuteAction::CreateRaw(this, &STimelineObjectEditorPanel::CreateNewTrack, FTTTrackBase::TT_VectorInterp)));
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddEventTrack", "Add Event Track"),
		LOCTEXT("AddEventTrackToolTip", "Adds an Event Track."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TimelineEditor.AddEventTrack"),
		FUIAction(FExecuteAction::CreateRaw(this, &STimelineObjectEditorPanel::CreateNewTrack, FTTTrackBase::TT_Event)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddColorTrack", "Add Color Track"),
		LOCTEXT("AddColorTrackToolTip", "Adds a Color Track."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TimelineEditor.AddColorTrack"),
		FUIAction(FExecuteAction::CreateRaw(this, &STimelineObjectEditorPanel::CreateNewTrack, FTTTrackBase::TT_LinearColorInterp)));

	FUIAction AddCurveAssetAction(
		FExecuteAction::CreateRaw(this, &STimelineObjectEditorPanel::CreateNewTrackFromAsset), 
		FCanExecuteAction::CreateRaw(this, &STimelineObjectEditorPanel::IsCurveAssetSelected));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddExternalAsset", "Add Selected Curve Asset"),
		LOCTEXT("AddExternalAssetToolTip", "Add the currently selected curve asset."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TimelineEditor.AddCurveAssetTrack"),
		AddCurveAssetAction);

	return MenuBuilder.MakeWidget();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE