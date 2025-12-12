#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Engine/TimelineTemplate.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FBlueprintEditor;
class FUICommandList;
class ITableRow;
class SCheckBox;
class SEditableTextBox;
class SInlineEditableTextBlock;
class STableViewBase;
class SWidget;
class SWindow;
class UCurveBase;
class UObject;
class UBlueprint;
class UK2Node_TimelineObject;
struct FAssetData;
struct FGeometry;
struct FKeyEvent;
struct FRichCurve;

#pragma region FTimelineObjectEdTrack

/**
 * Represents a single track in the Timeline Editor.
 * Wraps display index and provides rename callback support.
 */
class OBJECTTIMELINEUNCOOKED_API FTimelineObjectEdTrack
{
public:
	/** Index of this track in the display order */
	int32 DisplayIndex;

	/** Delegate fired when a rename operation is requested */
	FSimpleDelegate OnRenameRequest;

	static TSharedRef<FTimelineObjectEdTrack> Make(int32 DisplayIndex)
	{
		return MakeShareable(new FTimelineObjectEdTrack(DisplayIndex));
	}

private:
	FTimelineObjectEdTrack(int32 InDisplayIndex)
		: DisplayIndex(InDisplayIndex)
	{
	}

	FTimelineObjectEdTrack() : DisplayIndex(0)
	{
	}
};

#pragma endregion

#pragma region STimelineObjectEdTrack

/**
 * Slate widget for displaying and editing a single timeline track.
 * Includes curve editor, external curve picker, and track controls.
 */
class OBJECTTIMELINEUNCOOKED_API STimelineObjectEdTrack : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STimelineObjectEdTrack) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FTimelineObjectEdTrack> InTrack, TSharedPtr<class STimelineObjectEditorPanel> InTimelineEd);

	/** Inline text block for track name editing */
	TSharedPtr<SInlineEditableTextBlock> InlineNameBlock;

private:
	TSharedPtr<FTimelineObjectEdTrack> Track;
	TWeakPtr<class STimelineObjectEditorPanel> TimelineEdPtr;
	TSharedPtr<class SCurveEditor> TrackWidget;
	TSharedPtr<SWindow> AssetCreationWindow;

	UCurveBase* CurveBasePtr = nullptr;
	FString ExternalCurvePath;

	float LocalInputMin = 0;
	float LocalInputMax = 0;
	float LocalOutputMin = 0;
	float LocalOutputMax = 0;

#pragma region External Curve Management

	void OnCloseCreateCurveWindow();
	UCurveBase* CreateCurveAsset();
	void OnCreateExternalCurve();
	FString CreateUniqueCurveAssetPathName();
	FString GetExternalCurvePath() const;
	void SwitchToExternalCurve(UCurveBase* AssetCurvePtr);
	void UseExternalCurve(UObject* AssetObj);
	void UseInternalCurve();
	FReply OnClickClear();
	void ResetExternalCurveInfo();
	static void CopyCurveData(const FRichCurve* SrcCurve, FRichCurve* DestCurve);
	void OnChooseCurve(const FAssetData& InObject);

#pragma endregion

#pragma region Track Expansion

	ECheckBoxState GetIsExpandedState() const;
	void OnIsExpandedStateChanged(ECheckBoxState IsExpandedState);
	EVisibility GetContentVisibility() const;

#pragma endregion

#pragma region View Synchronization

	ECheckBoxState GetIsCurveViewSynchronizedState() const;
	void OnIsCurveViewSynchronizedStateChanged(ECheckBoxState IsCurveViewSynchronized);

#pragma endregion

#pragma region Track Reordering

	FReply OnMoveUp();
	bool CanMoveUp() const;
	FReply OnMoveDown();
	bool CanMoveDown() const;
	void MoveTrack(int32 DirectionDelta);

#pragma endregion

#pragma region View Range

	float GetMinInput() const;
	float GetMaxInput() const;
	float GetMinOutput() const;
	float GetMaxOutput() const;
	void OnSetInputViewRange(float Min, float Max);
	void OnSetOutputViewRange(float Min, float Max);

#pragma endregion

#pragma region Track Base Access

	FTTTrackBase* GetTrackBase();
	const FTTTrackBase* GetTrackBase() const;

#pragma endregion
};

#pragma endregion

#pragma region STimelineObjectEditorPanel

typedef SListView<TSharedPtr<FTimelineObjectEdTrack>> STimelineObjectEdTrackListType;

/**
 * Main Timeline Editor panel widget.
 * Provides full timeline editing functionality including tracks, properties, and curve editors.
 * Can work standalone (with Blueprint pointer) or integrated with FBlueprintEditor.
 */
class OBJECTTIMELINEUNCOOKED_API STimelineObjectEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STimelineObjectEditorPanel) {}
	SLATE_END_ARGS()

	/** Construct with FBlueprintEditor integration */
	void Construct(const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InKismet2, UTimelineTemplate* InTimelineObj);
	
	/** Construct standalone with direct Blueprint/Node references */
	void Construct(const FArguments& InArgs, UBlueprint* InBlueprint, UTimelineTemplate* InTimelineObj, UK2Node_TimelineObject* InTimelineNode);

#pragma region Public Interface

	UTimelineTemplate* GetTimeline();
	UBlueprint* GetBlueprint() const;
	void RefreshNode();
	void MarkBlueprintModified();
	void MarkBlueprintStructurallyModified();
	void OnTimelineChanged();

#pragma endregion

#pragma region View Range

	float GetViewMinInput() const;
	float GetViewMaxInput() const;
	float GetViewMinOutput() const;
	float GetViewMaxOutput() const;
	float GetTimelineLength() const;
	void SetInputViewRange(float InViewMinInput, float InViewMaxInput);
	void SetOutputViewRange(float InViewMinOutput, float InViewMaxOutput);

#pragma endregion

#pragma region Track Management

	bool OnVerifyTrackNameCommit(const FText& TrackName, FText& OutErrorMessage, FTTTrackBase* TrackBase, STimelineObjectEdTrack* Track);
	void OnTrackNameCommitted(const FText& Name, ETextCommit::Type CommitInfo, FTTTrackBase* TrackBase, STimelineObjectEdTrack* Track);
	UCurveBase* CreateNewCurve(FTTTrackBase::ETrackType Type);
	void OnReorderTracks(int32 DisplayIndex, int32 DirectionDelta);

#pragma endregion

#pragma region Size Management

	FVector2D GetTimelineDesiredSize() const;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

#pragma endregion

private:
	TSharedPtr<STimelineObjectEdTrackListType> TrackListView;
	TArray<TSharedPtr<FTimelineObjectEdTrack>> TrackList;

	TWeakPtr<FBlueprintEditor> Kismet2Ptr;
	TWeakObjectPtr<UBlueprint> BlueprintPtr;
	TWeakObjectPtr<UK2Node_TimelineObject> TimelineNodePtr;

	TSharedPtr<SEditableTextBox> TimelineLengthEdit;
	TSharedPtr<SCheckBox> LoopCheckBox;
	TSharedPtr<SCheckBox> ReplicatedCheckBox;
	TSharedPtr<SCheckBox> PlayCheckBox;
	TSharedPtr<SCheckBox> UseLastKeyframeCheckBox;
	TSharedPtr<SCheckBox> IgnoreTimeDilationCheckBox;

	UTimelineTemplate* TimelineObj = nullptr;

	float ViewMinInput = 0;
	float ViewMaxInput = 0;
	float ViewMinOutput = 0;
	float ViewMaxOutput = 0;

	FName NewTrackPendingRename;

	TSharedPtr<FUICommandList> CommandList;

	FVector2f TimelineDesiredSize = {};
	float NominalTimelineDesiredHeight = 0;

#pragma region Track List View

	TSharedRef<ITableRow> MakeTrackWidget(TSharedPtr<FTimelineObjectEdTrack> Track, const TSharedRef<STableViewBase>& OwnerTable);
	void CreateNewTrack(FTTTrackBase::ETrackType Type);
	bool CanDeleteSelectedTracks() const;
	void OnDeleteSelectedTracks();
	void OnItemScrolledIntoView(TSharedPtr<FTimelineObjectEdTrack> InTrackNode, const TSharedPtr<ITableRow>& InWidget);

#pragma endregion

#pragma region Timeline Properties

	FText GetTimelineName() const;
	
	ECheckBoxState IsAutoPlayChecked() const;
	void OnAutoPlayChanged(ECheckBoxState NewType);
	
	ECheckBoxState IsLoopChecked() const;
	void OnLoopChanged(ECheckBoxState NewType);
	
	ECheckBoxState IsReplicatedChecked() const;
	void OnReplicatedChanged(ECheckBoxState NewType);
	
	ECheckBoxState IsUseLastKeyframeChecked() const;
	void OnUseLastKeyframeChanged(ECheckBoxState NewType);
	
	ECheckBoxState IsIgnoreTimeDilationChecked() const;
	void OnIgnoreTimeDilationChanged(ECheckBoxState NewType);
	
	FText GetLengthString() const;
	void OnLengthStringChanged(const FText& NewString, ETextCommit::Type CommitInfo);
	
	void OnTimelineTickGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);

#pragma endregion

#pragma region Curve Asset Integration

	bool IsCurveAssetSelected() const;
	void CreateNewTrackFromAsset();

#pragma endregion

#pragma region Context Menu and Renaming

	bool CanRenameSelectedTrack() const;
	void OnRequestTrackRename() const;
	TSharedPtr<SWidget> MakeContextMenu() const;

#pragma endregion

#pragma region Size Scaling

	void SetSizeScaleValue(float NewValue);
	float GetSizeScaleValue() const;

#pragma endregion

#pragma region Add Button Menu

	TSharedRef<SWidget> MakeAddButton();

#pragma endregion
};

#pragma endregion