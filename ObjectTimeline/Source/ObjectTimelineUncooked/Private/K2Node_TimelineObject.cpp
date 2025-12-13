#include "K2Node_TimelineObject.h"
#include "ObjectTimelineHelpers.h"
#include "TimelineObject.h"
#include "TimelineObjectBinding.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/TimelineTemplate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiler.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "K2Node_TimelineObject"

static FName PlayPinName(TEXT("Play"));
static FName PlayFromStartPinName(TEXT("PlayFromStart"));
static FName StopPinName(TEXT("Stop"));
static FName ReversePinName(TEXT("Reverse"));
static FName ReverseFromEndPinName(TEXT("ReverseFromEnd"));
static FName UpdatePinName(TEXT("Update"));
static FName FinishedPinName(TEXT("Finished"));
static FName NewTimePinName(TEXT("NewTime"));
static FName SetNewTimePinName(TEXT("SetNewTime"));
static FName DirectionPinName(TEXT("Direction"));

UK2Node_TimelineObject::UK2Node_TimelineObject()
{
	TimelineName = NAME_None;
}

void UK2Node_TimelineObject::AllocateDefaultPins()
{
	CreateInputPins();
	CreateOutputPins();
	CreateTrackOutputPins();

	Super::AllocateDefaultPins();
}

void UK2Node_TimelineObject::CreateInputPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, PlayPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, PlayFromStartPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, StopPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ReversePinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ReverseFromEndPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, SetNewTimePinName);

	UEdGraphPin* NewTimePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float, NewTimePinName);
	NewTimePin->DefaultValue = TEXT("0.0");
}

void UK2Node_TimelineObject::CreateOutputPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UpdatePinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FinishedPinName);

	UEdGraphPin* DirectionPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Byte, FName(TEXT("ETimelineDirection")), DirectionPinName);
	DirectionPin->PinType.PinSubCategoryObject = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ETimelineDirection"));
}

void UK2Node_TimelineObject::CreateTrackOutputPins()
{
	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		for (const FTTFloatTrack& Track : Timeline->FloatTracks)
		{
			CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float, Track.GetTrackName());
		}

		for (const FTTVectorTrack& Track : Timeline->VectorTracks)
		{
			CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), Track.GetTrackName());
		}

		for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
		{
			CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FLinearColor>::Get(), Track.GetTrackName());
		}

		for (const FTTEventTrack& Track : Timeline->EventTracks)
		{
			CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, Track.GetTrackName());
		}
	}
}

FText UK2Node_TimelineObject::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TimelineName != NAME_None)
	{
		if (TitleType == ENodeTitleType::FullTitle)
		{
			return FText::Format(LOCTEXT("ObjectTimelineFullTitle", "{0}\nObject Timeline"), FText::FromName(TimelineName));
		}
		return FText::FromName(TimelineName);
	}
	return LOCTEXT("NoTimeline", "No Timeline");
}

FText UK2Node_TimelineObject::GetTooltipText() const
{
	return LOCTEXT("ObjectTimelineTooltip", "Timeline for UObjects (works with any UObject-derived class)");
}

FLinearColor UK2Node_TimelineObject::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.51f, 0.0f);
}

FSlateIcon UK2Node_TimelineObject::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Timeline_16x");
	return Icon;
}

bool UK2Node_TimelineObject::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return Super::IsCompatibleWithGraph(TargetGraph) && Blueprint != nullptr;
}

void UK2Node_TimelineObject::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (TimelineName == NAME_None)
	{
		MessageLog.Error(*LOCTEXT("NoTimelineName", "@@: Timeline has no name").ToString(), this);
		return;
	}

	UTimelineTemplate* Template = GetTimelineTemplate();
	if (!Template)
	{
		MessageLog.Error(*FText::Format(LOCTEXT("NoTimelineTemplate", "@@: Could not find timeline template for '{0}'"), FText::FromName(TimelineName)).ToString(), this);
	}
}

void UK2Node_TimelineObject::PreloadRequiredAssets()
{
	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		PreloadObject(Timeline);

		for (const FTTFloatTrack& Track : Timeline->FloatTracks)
		{
			PreloadObject(Track.CurveFloat);
		}
		for (const FTTVectorTrack& Track : Timeline->VectorTracks)
		{
			PreloadObject(Track.CurveVector);
		}
		for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
		{
			PreloadObject(Track.CurveLinearColor);
		}
		for (const FTTEventTrack& Track : Timeline->EventTracks)
		{
			PreloadObject(Track.CurveKeys);
		}
	}

	Super::PreloadRequiredAssets();
}

void UK2Node_TimelineObject::DestroyNode()
{
	if (UBlueprint* Blueprint = GetBlueprint())
	{
		if (UTimelineTemplate* Template = GetTimelineTemplate())
		{
			FBlueprintEditorUtils::RemoveTimeline(Blueprint, Template, true);
		}
	}

	Super::DestroyNode();
}

void UK2Node_TimelineObject::PostPasteNode()
{
	Super::PostPasteNode();

	if (UBlueprint* Blueprint = GetBlueprint())
	{
		UTimelineTemplate* OldTimeline = Blueprint->FindTimelineTemplateByVariableName(TimelineName);
		if (OldTimeline)
		{
			FName NewName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);
			if (NewName != TimelineName)
			{
				UTimelineTemplate* NewTimeline = DuplicateObject<UTimelineTemplate>(OldTimeline, Blueprint, NewName);
				if (NewTimeline)
				{
					Blueprint->Timelines.Add(NewTimeline);
					TimelineName = NewName;
				}
			}
		}
	}
}

void UK2Node_TimelineObject::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	if (UBlueprint* Blueprint = GetBlueprint())
	{
		if (TimelineName == NAME_None)
		{
			TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);
		}

		UTimelineTemplate* Template = Blueprint->FindTimelineTemplateByVariableName(TimelineName);
		if (!Template)
		{
			// Use our helper instead of FBlueprintEditorUtils::AddNewTimeline
			// (AddNewTimeline doesn't work for UObject Blueprints due to DoesSupportTimelines check)
			Template = ObjectTimelineHelpers::CreateTimelineTemplate(Blueprint, TimelineName);
			if (Template && Template->FloatTracks.Num() == 0 && Template->VectorTracks.Num() == 0 &&
				Template->LinearColorTracks.Num() == 0 && Template->EventTracks.Num() == 0)
			{
				FTTFloatTrack NewTrack;
				NewTrack.SetTrackName(FName(TEXT("NewTrack")), Template);
				NewTrack.CurveFloat = NewObject<UCurveFloat>(Template, NAME_None, RF_Transactional | RF_Public);
				NewTrack.CurveFloat->FloatCurve.AddKey(0.0f, 0.0f);
				NewTrack.CurveFloat->FloatCurve.AddKey(Template->TimelineLength, 1.0f);
				Template->FloatTracks.Add(NewTrack);
			}
		}
	}
}

UObject* UK2Node_TimelineObject::GetJumpTargetForDoubleClick() const
{
	return GetTimelineTemplate();
}

bool UK2Node_TimelineObject::CanJumpToDefinition() const
{
	return GetTimelineTemplate() != nullptr;
}

void UK2Node_TimelineObject::JumpToDefinition() const
{
	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Timeline);
	}
}

void UK2Node_TimelineObject::ReconstructNode()
{
	Super::ReconstructNode();
}

UEdGraphPin* UK2Node_TimelineObject::GetPlayPin() const
{
	return FindPinChecked(PlayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_TimelineObject::GetPlayFromStartPin() const
{
	return FindPinChecked(PlayFromStartPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_TimelineObject::GetStopPin() const
{
	return FindPinChecked(StopPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_TimelineObject::GetReversePin() const
{
	return FindPinChecked(ReversePinName, EGPD_Input);
}

UEdGraphPin* UK2Node_TimelineObject::GetReverseFromEndPin() const
{
	return FindPinChecked(ReverseFromEndPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_TimelineObject::GetUpdatePin() const
{
	return FindPinChecked(UpdatePinName, EGPD_Output);
}

UEdGraphPin* UK2Node_TimelineObject::GetFinishedPin() const
{
	return FindPinChecked(FinishedPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_TimelineObject::GetNewTimePin() const
{
	return FindPinChecked(NewTimePinName, EGPD_Input);
}

UEdGraphPin* UK2Node_TimelineObject::GetSetNewTimePin() const
{
	return FindPinChecked(SetNewTimePinName, EGPD_Input);
}

UEdGraphPin* UK2Node_TimelineObject::GetDirectionPin() const
{
	return FindPinChecked(DirectionPinName, EGPD_Output);
}

UTimelineTemplate* UK2Node_TimelineObject::GetTimelineTemplate() const
{
	if (UBlueprint* Blueprint = GetBlueprint())
	{
		return Blueprint->FindTimelineTemplateByVariableName(TimelineName);
	}
	return nullptr;
}

void UK2Node_TimelineObject::RenameTimeline(const FString& NewName)
{
	if (UBlueprint* Blueprint = GetBlueprint())
	{
		FName NewFName(*NewName);
		if (TimelineName != NewFName)
		{
			FScopedTransaction Transaction(LOCTEXT("RenameTimeline", "Rename Timeline"));
			Blueprint->Modify();
			Modify();

			FBlueprintEditorUtils::RenameTimeline(Blueprint, TimelineName, NewFName);
			TimelineName = NewFName;
		}
	}
}

void UK2Node_TimelineObject::FindFloatTracks(TArray<FName>& OutTrackNames) const
{
	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		for (const FTTFloatTrack& Track : Timeline->FloatTracks)
		{
			OutTrackNames.Add(Track.GetTrackName());
		}
	}
}

void UK2Node_TimelineObject::FindVectorTracks(TArray<FName>& OutTrackNames) const
{
	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		for (const FTTVectorTrack& Track : Timeline->VectorTracks)
		{
			OutTrackNames.Add(Track.GetTrackName());
		}
	}
}

void UK2Node_TimelineObject::FindLinearColorTracks(TArray<FName>& OutTrackNames) const
{
	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
		{
			OutTrackNames.Add(Track.GetTrackName());
		}
	}
}

void UK2Node_TimelineObject::FindEventTracks(TArray<FName>& OutTrackNames) const
{
	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		for (const FTTEventTrack& Track : Timeline->EventTracks)
		{
			OutTrackNames.Add(Track.GetTrackName());
		}
	}
}

UEdGraphPin* UK2Node_TimelineObject::CreateGetTimelineObjectCall(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, FName UpdateFunc, FName FinishedFunc)
{
	UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
	SelfNode->AllocateDefaultPins();
	UEdGraphPin* SelfOutputPin = SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self);

	UK2Node_CallFunction* GetTimelineCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetTimelineCall->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UTimelineObject, GetOrCreateTimelineObject), UTimelineObject::StaticClass());
	GetTimelineCall->AllocateDefaultPins();

	UEdGraphPin* OwnerPin = GetTimelineCall->FindPin(TEXT("Owner"));
	if (OwnerPin)
	{
		SelfOutputPin->MakeLinkTo(OwnerPin);
	}

	UEdGraphPin* NamePin = GetTimelineCall->FindPin(TEXT("TimelineName"));
	if (NamePin)
	{
		NamePin->DefaultValue = TimelineName.ToString();
	}

	UEdGraphPin* UpdateFuncPin = GetTimelineCall->FindPin(TEXT("UpdateFuncName"));
	if (UpdateFuncPin)
	{
		UpdateFuncPin->DefaultValue = UpdateFunc.ToString();
	}

	UEdGraphPin* FinishedFuncPin = GetTimelineCall->FindPin(TEXT("FinishedFuncName"));
	if (FinishedFuncPin)
	{
		FinishedFuncPin->DefaultValue = FinishedFunc.ToString();
	}

	return GetTimelineCall->GetReturnValuePin();
}

void UK2Node_TimelineObject::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UTimelineTemplate* Timeline = GetTimelineTemplate();
	if (!Timeline)
	{
		CompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("ExpandNodeNoTimeline", "@@: Could not find timeline template for '{0}'"), FText::FromName(TimelineName)).ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UpdateFunctionName = FName(*FString::Printf(TEXT("%s__UpdateFunc"), *TimelineName.ToString()));
	FinishedFunctionName = FName(*FString::Printf(TEXT("%s__FinishedFunc"), *TimelineName.ToString()));

	// Check if any INPUT exec pins are connected (these trigger timeline actions)
	bool bHasInputExec = 
		GetPlayPin()->LinkedTo.Num() > 0 ||
		GetPlayFromStartPin()->LinkedTo.Num() > 0 ||
		GetStopPin()->LinkedTo.Num() > 0 ||
		GetReversePin()->LinkedTo.Num() > 0 ||
		GetReverseFromEndPin()->LinkedTo.Num() > 0 ||
		GetSetNewTimePin()->LinkedTo.Num() > 0;

	// If no input exec pins connected, timeline will never be triggered - skip expansion
	if (!bHasInputExec)
	{
		BreakAllNodeLinks();
		return;
	}

	bool bUpdateConnected = GetUpdatePin()->LinkedTo.Num() > 0;
	bool bFinishedConnected = GetFinishedPin()->LinkedTo.Num() > 0;

	FName UpdateFuncToPass = bUpdateConnected ? UpdateFunctionName : NAME_None;
	FName FinishedFuncToPass = bFinishedConnected ? FinishedFunctionName : NAME_None;

	UEdGraphPin* TimelineReturnPin = CreateGetTimelineObjectCall(CompilerContext, SourceGraph, UpdateFuncToPass, FinishedFuncToPass);

	// Expand input exec pins
	if (GetPlayPin()->LinkedTo.Num() > 0)
	{
		ExpandInputExecPin(CompilerContext, SourceGraph, GetPlayPin(), GET_FUNCTION_NAME_CHECKED(UTimelineObject, Play), TimelineReturnPin);
	}

	if (GetPlayFromStartPin()->LinkedTo.Num() > 0)
	{
		ExpandInputExecPin(CompilerContext, SourceGraph, GetPlayFromStartPin(), GET_FUNCTION_NAME_CHECKED(UTimelineObject, PlayFromStart), TimelineReturnPin);
	}

	if (GetStopPin()->LinkedTo.Num() > 0)
	{
		ExpandInputExecPin(CompilerContext, SourceGraph, GetStopPin(), GET_FUNCTION_NAME_CHECKED(UTimelineObject, Stop), TimelineReturnPin);
	}

	if (GetReversePin()->LinkedTo.Num() > 0)
	{
		ExpandInputExecPin(CompilerContext, SourceGraph, GetReversePin(), GET_FUNCTION_NAME_CHECKED(UTimelineObject, Reverse), TimelineReturnPin);
	}

	if (GetReverseFromEndPin()->LinkedTo.Num() > 0)
	{
		ExpandInputExecPin(CompilerContext, SourceGraph, GetReverseFromEndPin(), GET_FUNCTION_NAME_CHECKED(UTimelineObject, ReverseFromEnd), TimelineReturnPin);
	}

	if (GetSetNewTimePin()->LinkedTo.Num() > 0)
	{
		UK2Node_CallFunction* SetNewTimeCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		SetNewTimeCall->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UTimelineObject, SetNewTime), UTimelineObject::StaticClass());
		SetNewTimeCall->AllocateDefaultPins();

		UEdGraphPin* TargetPin = SetNewTimeCall->FindPinChecked(UEdGraphSchema_K2::PN_Self);
		TimelineReturnPin->MakeLinkTo(TargetPin);

		UEdGraphPin* NewTimeInputPin = SetNewTimeCall->FindPin(TEXT("NewTime"));
		if (NewTimeInputPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*GetNewTimePin(), *NewTimeInputPin);
		}

		CompilerContext.MovePinLinksToIntermediate(*GetSetNewTimePin(), *SetNewTimeCall->GetExecPin());
	}

	// Create internal events for output exec pins (only if connected)
	if (bUpdateConnected)
	{
		CreateInternalEventForPin(CompilerContext, SourceGraph, GetUpdatePin(), UpdateFunctionName);
	}
	if (bFinishedConnected)
	{
		CreateInternalEventForPin(CompilerContext, SourceGraph, GetFinishedPin(), FinishedFunctionName);
	}

	// Create internal events for event track pins
	for (const FTTEventTrack& EventTrack : Timeline->EventTracks)
	{
		FName EventTrackFuncName = FName(*FString::Printf(TEXT("%s__%s__Event"), *TimelineName.ToString(), *EventTrack.GetTrackName().ToString()));
		UEdGraphPin* EventPin = FindPin(EventTrack.GetTrackName(), EGPD_Output);
		if (EventPin && EventPin->LinkedTo.Num() > 0)
		{
			CreateInternalEventForPin(CompilerContext, SourceGraph, EventPin, EventTrackFuncName);
		}
	}

	// Expand track value pins
	if (TimelineReturnPin)
	{
		ExpandTrackPins(CompilerContext, SourceGraph, TimelineReturnPin);
	}
}

void UK2Node_TimelineObject::ExpandInputExecPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* ExecPin, FName FunctionName, UEdGraphPin* TimelineReturnPin)
{
	if (ExecPin->LinkedTo.Num() == 0 || !TimelineReturnPin)
	{
		return;
	}

	UK2Node_CallFunction* FunctionCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FunctionCall->FunctionReference.SetExternalMember(FunctionName, UTimelineObject::StaticClass());
	FunctionCall->AllocateDefaultPins();

	UEdGraphPin* TargetPin = FunctionCall->FindPinChecked(UEdGraphSchema_K2::PN_Self);
	TimelineReturnPin->MakeLinkTo(TargetPin);

	CompilerContext.MovePinLinksToIntermediate(*ExecPin, *FunctionCall->GetExecPin());
}

void UK2Node_TimelineObject::CreateInternalEventForPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* Pin, FName FunctionName)
{
	if (!Pin || Pin->LinkedTo.Num() == 0)
	{
		return;
	}

	UK2Node_Event* EventNode = CompilerContext.SpawnIntermediateNode<UK2Node_Event>(this, SourceGraph);
	
	UFunction* EventSigFunc = UTimelineComponent::GetTimelineEventSignature();
	EventNode->EventReference.SetExternalMember(EventSigFunc->GetFName(), UTimelineComponent::StaticClass());
	EventNode->CustomFunctionName = FunctionName;
	EventNode->bInternalEvent = true;
	EventNode->AllocateDefaultPins();

	UEdGraphPin* EventExecPin = CompilerContext.GetSchema()->FindExecutionPin(*EventNode, EGPD_Output);
	if (EventExecPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*Pin, *EventExecPin);
	}
}

void UK2Node_TimelineObject::ExpandTrackPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* TimelineReturnPin)
{
	UTimelineTemplate* Timeline = GetTimelineTemplate();
	if (!Timeline || !TimelineReturnPin)
	{
		return;
	}

	for (const FTTFloatTrack& FloatTrack : Timeline->FloatTracks)
	{
		UEdGraphPin* TrackPin = FindPin(FloatTrack.GetTrackName(), EGPD_Output);
		if (!TrackPin || TrackPin->LinkedTo.Num() == 0)
		{
			continue;
		}

		UK2Node_CallFunction* GetValueCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetValueCall->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UTimelineObject, GetFloatValue), UTimelineObject::StaticClass());
		GetValueCall->AllocateDefaultPins();

		UEdGraphPin* TargetPin = GetValueCall->FindPinChecked(UEdGraphSchema_K2::PN_Self);
		TimelineReturnPin->MakeLinkTo(TargetPin);

		UEdGraphPin* CurvePin = GetValueCall->FindPin(TEXT("FloatCurve"));
		if (CurvePin)
		{
			CurvePin->DefaultObject = FloatTrack.CurveFloat;
		}

		UEdGraphPin* ReturnPin = GetValueCall->GetReturnValuePin();
		if (ReturnPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*TrackPin, *ReturnPin);
		}
	}

	for (const FTTVectorTrack& VectorTrack : Timeline->VectorTracks)
	{
		UEdGraphPin* TrackPin = FindPin(VectorTrack.GetTrackName(), EGPD_Output);
		if (!TrackPin || TrackPin->LinkedTo.Num() == 0)
		{
			continue;
		}

		UK2Node_CallFunction* GetValueCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetValueCall->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UTimelineObject, GetVectorValue), UTimelineObject::StaticClass());
		GetValueCall->AllocateDefaultPins();

		UEdGraphPin* TargetPin = GetValueCall->FindPinChecked(UEdGraphSchema_K2::PN_Self);
		TimelineReturnPin->MakeLinkTo(TargetPin);

		UEdGraphPin* CurvePin = GetValueCall->FindPin(TEXT("VectorCurve"));
		if (CurvePin)
		{
			CurvePin->DefaultObject = VectorTrack.CurveVector;
		}

		UEdGraphPin* ReturnPin = GetValueCall->GetReturnValuePin();
		if (ReturnPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*TrackPin, *ReturnPin);
		}
	}

	for (const FTTLinearColorTrack& ColorTrack : Timeline->LinearColorTracks)
	{
		UEdGraphPin* TrackPin = FindPin(ColorTrack.GetTrackName(), EGPD_Output);
		if (!TrackPin || TrackPin->LinkedTo.Num() == 0)
		{
			continue;
		}

		UK2Node_CallFunction* GetValueCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetValueCall->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UTimelineObject, GetLinearColorValue), UTimelineObject::StaticClass());
		GetValueCall->AllocateDefaultPins();

		UEdGraphPin* TargetPin = GetValueCall->FindPinChecked(UEdGraphSchema_K2::PN_Self);
		TimelineReturnPin->MakeLinkTo(TargetPin);

		UEdGraphPin* CurvePin = GetValueCall->FindPin(TEXT("ColorCurve"));
		if (CurvePin)
		{
			CurvePin->DefaultObject = ColorTrack.CurveLinearColor;
		}

		UEdGraphPin* ReturnPin = GetValueCall->GetReturnValuePin();
		if (ReturnPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*TrackPin, *ReturnPin);
		}
	}
}

UClass* UK2Node_TimelineObject::GetDynamicBindingClass() const
{
	return UTimelineObjectBinding::StaticClass();
}

void UK2Node_TimelineObject::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UTimelineObjectBinding* TimelineBinding = CastChecked<UTimelineObjectBinding>(BindingObject);

	TimelineBinding->TimelineBindings.RemoveAll(
		[this](const FTimelineObjectBindingEntry& Entry)
		{
			return Entry.TimelineName == TimelineName;
		});

	FTimelineObjectBindingEntry Entry;
	Entry.TimelineName = TimelineName;
	Entry.UpdateFunctionName = NAME_None;
	Entry.FinishedFunctionName = NAME_None;

	if (UTimelineTemplate* Timeline = GetTimelineTemplate())
	{
		for (const FTTEventTrack& EventTrack : Timeline->EventTracks)
		{
			UEdGraphPin* EventPin = FindPin(EventTrack.GetTrackName(), EGPD_Output);
			if (EventPin && EventPin->LinkedTo.Num() > 0)
			{
				FName EventTrackFuncName = FName(*FString::Printf(TEXT("%s__%s__Event"), *TimelineName.ToString(), *EventTrack.GetTrackName().ToString()));
				Entry.EventTrackFunctionNames.Add(EventTrack.GetTrackName(), EventTrackFuncName);
			}
		}
	}

	if (Entry.EventTrackFunctionNames.Num() > 0)
	{
		TimelineBinding->TimelineBindings.Add(Entry);
	}
}

void UK2Node_TimelineObject::GetNodeAttributes(TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes) const
{
	OutNodeAttributes.Add(TKeyValuePair<FString, FString>(TEXT("Type"), TEXT("ObjectTimeline")));
	OutNodeAttributes.Add(TKeyValuePair<FString, FString>(TEXT("Class"), GetClass()->GetName()));
	OutNodeAttributes.Add(TKeyValuePair<FString, FString>(TEXT("Name"), GetName()));
}

void UK2Node_TimelineObject::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner);

		auto CustomizeNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode)
		{
			UK2Node_TimelineObject* TimelineNode = CastChecked<UK2Node_TimelineObject>(NewNode);

			UBlueprint* Blueprint = TimelineNode->GetBlueprint();
			if (Blueprint)
			{
				TimelineNode->TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);
				if (!bIsTemplateNode)
				{
					// Use our helper instead of FBlueprintEditorUtils::AddNewTimeline
					// (AddNewTimeline doesn't work for UObject Blueprints due to DoesSupportTimelines check)
					UTimelineTemplate* Template = ObjectTimelineHelpers::CreateTimelineTemplate(Blueprint, TimelineNode->TimelineName);
					if (Template)
					{
						if (Template->FloatTracks.Num() == 0 && Template->VectorTracks.Num() == 0 &&
							Template->LinearColorTracks.Num() == 0 && Template->EventTracks.Num() == 0)
						{
							FTTFloatTrack NewTrack;
							NewTrack.SetTrackName(FName(TEXT("NewTrack")), Template);
							NewTrack.CurveFloat = NewObject<UCurveFloat>(Template, NAME_None, RF_Transactional | RF_Public);
							NewTrack.CurveFloat->FloatCurve.AddKey(0.0f, 0.0f);
							NewTrack.CurveFloat->FloatCurve.AddKey(Template->TimelineLength, 1.0f);
							Template->FloatTracks.Add(NewTrack);
						}
						TimelineNode->ErrorMsg.Empty();
						TimelineNode->bHasCompilerMessage = false;
					}
				}
			}
		};

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeNodeLambda);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_TimelineObject::GetMenuCategory() const
{
	return LOCTEXT("ObjectTimelineCategory", "Object Timeline");
}

FNodeHandlingFunctor* UK2Node_TimelineObject::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

#undef LOCTEXT_NAMESPACE