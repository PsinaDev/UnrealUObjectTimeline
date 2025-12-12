#include "TimelineObject.h"
#include "TimelineObjectBinding.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/TimelineTemplate.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "UObject/UObjectIterator.h"

DECLARE_CYCLE_STAT(TEXT("TimelineObject Tick"), STAT_TimelineObjectTick, STATGROUP_Default);

#pragma region Constructor

UTimelineObject::UTimelineObject()
	: bIgnoreTimeDilation(false)
{
	// Bind internal callbacks to the underlying FTimeline
	FOnTimelineEvent UpdateDelegate;
	UpdateDelegate.BindDynamic(this, &UTimelineObject::Internal_OnTimelineUpdate);
	TheTimeline.SetTimelinePostUpdateFunc(UpdateDelegate);

	FOnTimelineEvent FinishedDelegate;
	FinishedDelegate.BindDynamic(this, &UTimelineObject::Internal_OnTimelineFinished);
	TheTimeline.SetTimelineFinishedFunc(FinishedDelegate);
}

#pragma endregion

#pragma region Playback Control

void UTimelineObject::Play()
{
	TheTimeline.Play();
}

void UTimelineObject::PlayFromStart()
{
	TheTimeline.PlayFromStart();
}

void UTimelineObject::Reverse()
{
	TheTimeline.Reverse();
}

void UTimelineObject::ReverseFromEnd()
{
	TheTimeline.ReverseFromEnd();
}

void UTimelineObject::Stop()
{
	TheTimeline.Stop();
}

bool UTimelineObject::IsPlaying() const
{
	return TheTimeline.IsPlaying();
}

bool UTimelineObject::IsReversing() const
{
	return TheTimeline.IsReversing();
}

#pragma endregion

#pragma region Position and Length

void UTimelineObject::SetPlaybackPosition(float NewPosition, bool bFireEvents, bool bFireUpdate)
{
	TheTimeline.SetPlaybackPosition(NewPosition, bFireEvents, bFireUpdate);
}

float UTimelineObject::GetPlaybackPosition() const
{
	return TheTimeline.GetPlaybackPosition();
}

void UTimelineObject::SetNewTime(float NewTime)
{
	TheTimeline.SetNewTime(NewTime);
}

float UTimelineObject::GetTimelineLength() const
{
	return TheTimeline.GetTimelineLength();
}

float UTimelineObject::GetScaledTimelineLength() const
{
	return TheTimeline.GetScaledTimelineLength();
}

void UTimelineObject::SetTimelineLength(float NewLength)
{
	TheTimeline.SetTimelineLength(NewLength);
}

void UTimelineObject::SetTimelineLengthMode(ETimelineLengthMode NewLengthMode)
{
	TheTimeline.SetTimelineLengthMode(NewLengthMode);
}

#pragma endregion

#pragma region Playback Settings

void UTimelineObject::SetLooping(bool bNewLooping)
{
	TheTimeline.SetLooping(bNewLooping);
}

bool UTimelineObject::IsLooping() const
{
	return TheTimeline.IsLooping();
}

void UTimelineObject::SetPlayRate(float NewRate)
{
	TheTimeline.SetPlayRate(NewRate);
}

float UTimelineObject::GetPlayRate() const
{
	return TheTimeline.GetPlayRate();
}

void UTimelineObject::SetIgnoreTimeDilation(bool bNewIgnoreTimeDilation)
{
	bIgnoreTimeDilation = bNewIgnoreTimeDilation;
}

bool UTimelineObject::GetIgnoreTimeDilation() const
{
	return bIgnoreTimeDilation;
}

TEnumAsByte<ETimelineDirection::Type> UTimelineObject::GetPlaybackDirection() const
{
	return IsReversing() ? ETimelineDirection::Backward : ETimelineDirection::Forward;
}

#pragma endregion

#pragma region Curve Management

void UTimelineObject::SetFloatCurve(UCurveFloat* NewFloatCurve, FName FloatTrackName)
{
	TheTimeline.SetFloatCurve(NewFloatCurve, FloatTrackName);
}

void UTimelineObject::SetVectorCurve(UCurveVector* NewVectorCurve, FName VectorTrackName)
{
	TheTimeline.SetVectorCurve(NewVectorCurve, VectorTrackName);
}

void UTimelineObject::SetLinearColorCurve(UCurveLinearColor* NewLinearColorCurve, FName LinearColorTrackName)
{
	TheTimeline.SetLinearColorCurve(NewLinearColorCurve, LinearColorTrackName);
}

void UTimelineObject::AddEvent(float Time, FOnTimelineEvent EventFunc)
{
	TheTimeline.AddEvent(Time, EventFunc);
}

void UTimelineObject::AddInterpVector(UCurveVector* VectorCurve, FOnTimelineVector InterpFunc, FName PropertyName, FName TrackName)
{
	TheTimeline.AddInterpVector(VectorCurve, InterpFunc, PropertyName, TrackName);
}

void UTimelineObject::AddInterpFloat(UCurveFloat* FloatCurve, FOnTimelineFloat InterpFunc, FName PropertyName, FName TrackName)
{
	TheTimeline.AddInterpFloat(FloatCurve, InterpFunc, PropertyName, TrackName);
}

void UTimelineObject::AddInterpLinearColor(UCurveLinearColor* LinearColorCurve, FOnTimelineLinearColor InterpFunc, FName PropertyName, FName TrackName)
{
	TheTimeline.AddInterpLinearColor(LinearColorCurve, InterpFunc, PropertyName, TrackName);
}

#pragma endregion

#pragma region Value Getters

float UTimelineObject::GetFloatValue(UCurveFloat* FloatCurve) const
{
	if (FloatCurve)
	{
		return FloatCurve->GetFloatValue(GetPlaybackPosition());
	}
	return 0.f;
}

FVector UTimelineObject::GetVectorValue(UCurveVector* VectorCurve) const
{
	if (VectorCurve)
	{
		return VectorCurve->GetVectorValue(GetPlaybackPosition());
	}
	return FVector::ZeroVector;
}

FLinearColor UTimelineObject::GetLinearColorValue(UCurveLinearColor* ColorCurve) const
{
	if (ColorCurve)
	{
		return ColorCurve->GetLinearColorValue(GetPlaybackPosition());
	}
	return FLinearColor::Black;
}

UCurveFloat* UTimelineObject::GetFloatTrackCurve(FName TrackName) const
{
	if (const TObjectPtr<UCurveFloat>* Found = FloatTrackCurves.Find(TrackName))
	{
		return *Found;
	}
	return nullptr;
}

UCurveVector* UTimelineObject::GetVectorTrackCurve(FName TrackName) const
{
	if (const TObjectPtr<UCurveVector>* Found = VectorTrackCurves.Find(TrackName))
	{
		return *Found;
	}
	return nullptr;
}

UCurveLinearColor* UTimelineObject::GetLinearColorTrackCurve(FName TrackName) const
{
	if (const TObjectPtr<UCurveLinearColor>* Found = LinearColorTrackCurves.Find(TrackName))
	{
		return *Found;
	}
	return nullptr;
}

#pragma endregion

#pragma region Property Binding

void UTimelineObject::SetPropertySetObject(UObject* NewPropertySetObject)
{
	TheTimeline.SetPropertySetObject(NewPropertySetObject);
}

void UTimelineObject::SetTimelinePostUpdateFunc(FOnTimelineEvent NewTimelinePostUpdateFunc)
{
	TheTimeline.SetTimelinePostUpdateFunc(NewTimelinePostUpdateFunc);
}

void UTimelineObject::SetTimelineFinishedFunc(FOnTimelineEvent NewTimelineFinishedFunc)
{
	TheTimeline.SetTimelineFinishedFunc(NewTimelineFinishedFunc);
}

void UTimelineObject::SetTimelineFinishedFunc(FOnTimelineEventStatic NewTimelineFinishedFunc)
{
	TheTimeline.SetTimelineFinishedFunc(NewTimelineFinishedFunc);
}

void UTimelineObject::SetDirectionPropertyName(FName DirectionPropertyName)
{
	TheTimeline.SetDirectionPropertyName(DirectionPropertyName);
}

#pragma endregion

#pragma region Utility

void UTimelineObject::GetAllCurves(TSet<class UCurveBase*>& InOutCurves) const
{
	for (const auto& Pair : FloatTrackCurves)
	{
		if (Pair.Value)
		{
			InOutCurves.Add(Pair.Value);
		}
	}

	for (const auto& Pair : VectorTrackCurves)
	{
		if (Pair.Value)
		{
			InOutCurves.Add(Pair.Value);
		}
	}

	for (const auto& Pair : LinearColorTrackCurves)
	{
		if (Pair.Value)
		{
			InOutCurves.Add(Pair.Value);
		}
	}

	for (const auto& Pair : EventTrackCurves)
	{
		if (Pair.Value)
		{
			InOutCurves.Add(Pair.Value);
		}
	}
}

AActor* UTimelineObject::GetOwningActor() const
{
	return GetTypedOuter<AActor>();
}

UTimelineObject* UTimelineObject::GetOrCreateTimelineObject(UObject* Owner, FName TimelineName, FName UpdateFuncName, FName FinishedFuncName)
{
	if (!Owner)
	{
		return nullptr;
	}

	// Generate unique object name based on timeline name
	FName UniqueObjectName = FName(*FString::Printf(TEXT("TimelineObj_%s"), *TimelineName.ToString()));

	// Search for existing timeline object
	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(Owner, SubObjects, false);

	for (UObject* SubObj : SubObjects)
	{
		if (UTimelineObject* ExistingTimeline = Cast<UTimelineObject>(SubObj))
		{
			if (ExistingTimeline->GetFName() == UniqueObjectName)
			{
				// Timeline already exists - just bind the functions if provided
				if (UpdateFuncName != NAME_None)
				{
					ExistingTimeline->BindUpdateFunction(Owner, UpdateFuncName);
				}
				if (FinishedFuncName != NAME_None)
				{
					ExistingTimeline->BindFinishedFunction(Owner, FinishedFuncName);
				}
				return ExistingTimeline;
			}
		}
	}

	// Create new timeline object
	UTimelineObject* NewTimeline = NewObject<UTimelineObject>(Owner, UniqueObjectName);

	// Cache the world for reliable ticking with non-Actor owners
	if (UWorld* World = Owner->GetWorld())
	{
		NewTimeline->CachedWorld = World;
	}

	// Initialize from UTimelineTemplate in BPGC->Timelines (works in both editor and runtime)
	if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Owner->GetClass()))
	{
		// BPGC->Timelines contains UTimelineTemplate array copied from Blueprint during compilation
		for (UTimelineTemplate* Template : BPGC->Timelines)
		{
			if (Template && Template->GetVariableName() == TimelineName)
			{
				NewTimeline->InitializeFromTemplate(Template);
				break;
			}
		}
	}

	// Bind update and finished functions
	if (UpdateFuncName != NAME_None)
	{
		NewTimeline->BindUpdateFunction(Owner, UpdateFuncName);
	}
	if (FinishedFuncName != NAME_None)
	{
		NewTimeline->BindFinishedFunction(Owner, FinishedFuncName);
	}

	// Bind event track functions from dynamic binding
	if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Owner->GetClass()))
	{
		for (UDynamicBlueprintBinding* Binding : BPGC->DynamicBindingObjects)
		{
			if (UTimelineObjectBinding* TimelineBinding = Cast<UTimelineObjectBinding>(Binding))
			{
				for (const FTimelineObjectBindingEntry& Entry : TimelineBinding->TimelineBindings)
				{
					if (Entry.TimelineName == TimelineName)
					{
						for (const auto& TrackPair : Entry.EventTrackFunctionNames)
						{
							if (TrackPair.Value != NAME_None)
							{
								NewTimeline->BindEventTrackFunction(TrackPair.Key, Owner, TrackPair.Value);
							}
						}
						break;
					}
				}
				break;
			}
		}
	}

	// Trigger autoplay after delegates are bound
	if (NewTimeline->bPendingAutoPlay)
	{
		NewTimeline->bPendingAutoPlay = false;
		NewTimeline->Play();
	}

	return NewTimeline;
}

void UTimelineObject::InitializeFromTemplate(UTimelineTemplate* Template)
{
	if (!Template)
	{
		return;
	}

	// Configure timeline properties from template
	SetTimelineLength(Template->TimelineLength);
	SetTimelineLengthMode(Template->LengthMode);
	SetLooping(Template->bLoop);
	SetPlayRate(1.0f);
	SetIgnoreTimeDilation(Template->bIgnoreTimeDilation);

	// Initialize float tracks
	for (const FTTFloatTrack& Track : Template->FloatTracks)
	{
		if (Track.CurveFloat)
		{
			FName TrackName = Track.GetTrackName();
			FloatTrackCurves.Add(TrackName, Track.CurveFloat);
			FOnTimelineFloat InterpDelegate;
			TheTimeline.AddInterpFloat(Track.CurveFloat, InterpDelegate, NAME_None, TrackName);
		}
	}

	// Initialize vector tracks
	for (const FTTVectorTrack& Track : Template->VectorTracks)
	{
		if (Track.CurveVector)
		{
			FName TrackName = Track.GetTrackName();
			VectorTrackCurves.Add(TrackName, Track.CurveVector);
			FOnTimelineVector InterpDelegate;
			TheTimeline.AddInterpVector(Track.CurveVector, InterpDelegate, NAME_None, TrackName);
		}
	}

	// Initialize linear color tracks
	for (const FTTLinearColorTrack& Track : Template->LinearColorTracks)
	{
		if (Track.CurveLinearColor)
		{
			FName TrackName = Track.GetTrackName();
			LinearColorTrackCurves.Add(TrackName, Track.CurveLinearColor);
			FOnTimelineLinearColor InterpDelegate;
			TheTimeline.AddInterpLinearColor(Track.CurveLinearColor, InterpDelegate, NAME_None, TrackName);
		}
	}

	// Initialize event tracks
	for (const FTTEventTrack& Track : Template->EventTracks)
	{
		if (Track.CurveKeys)
		{
			FName TrackName = Track.GetTrackName();
			RegisterEventTrack(TrackName, Track.CurveKeys);
		}
	}

	// Defer autoplay until after delegates are bound
	if (Template->bAutoPlay)
	{
		bPendingAutoPlay = true;
	}
}

#pragma endregion

#pragma region Event Track Management

FOnTimelineObjectEvent& UTimelineObject::GetEventTrackDelegate(FName TrackName)
{
	return EventTrackDelegates.FindOrAdd(TrackName);
}

void UTimelineObject::RemoveAllDelegatesForObject(UObject* BoundObject)
{
	if (!BoundObject)
	{
		return;
	}

	// Remove from public delegates
	OnTimelineUpdate.RemoveAll(BoundObject);
	OnTimelineFinished.RemoveAll(BoundObject);
	OnFloatTrack.RemoveAll(BoundObject);
	OnVectorTrack.RemoveAll(BoundObject);
	OnLinearColorTrack.RemoveAll(BoundObject);

	// Remove from event track delegates
	for (auto& Pair : EventTrackDelegates)
	{
		Pair.Value.RemoveAll(BoundObject);
	}

	// Clean up bound function tracking
	for (auto It = BoundUpdateFunctions.CreateIterator(); It; ++It)
	{
		if (It->Key == BoundObject)
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = BoundFinishedFunctions.CreateIterator(); It; ++It)
	{
		if (It->Key == BoundObject)
		{
			It.RemoveCurrent();
		}
	}
	for (auto& TrackPair : BoundEventTrackFunctions)
	{
		for (auto It = TrackPair.Value.CreateIterator(); It; ++It)
		{
			if (It->Key == BoundObject)
			{
				It.RemoveCurrent();
			}
		}
	}
}

void UTimelineObject::RegisterEventTrack(FName TrackName, UCurveFloat* EventCurve)
{
	if (TrackName != NAME_None && EventCurve)
	{
		EventTrackCurves.Add(TrackName, EventCurve);
		EventTrackDelegates.FindOrAdd(TrackName);
		LastEventTrackPositions.Add(TrackName, -1.0f);
	}
}

#pragma endregion

#pragma region Dynamic Binding

void UTimelineObject::BindUpdateFunction(UObject* Target, FName FunctionName)
{
	if (!Target || FunctionName == NAME_None)
	{
		return;
	}

	// Prevent duplicate bindings
	TPair<TWeakObjectPtr<UObject>, FName> BindingKey(Target, FunctionName);
	if (BoundUpdateFunctions.Contains(BindingKey))
	{
		return;
	}

	UFunction* Function = Target->GetClass()->FindFunctionByName(FunctionName);
	if (Function)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Target, FunctionName);
		OnTimelineUpdate.AddUnique(Delegate);
		BoundUpdateFunctions.Add(BindingKey);
	}
}

void UTimelineObject::BindFinishedFunction(UObject* Target, FName FunctionName)
{
	if (!Target || FunctionName == NAME_None)
	{
		return;
	}

	// Prevent duplicate bindings
	TPair<TWeakObjectPtr<UObject>, FName> BindingKey(Target, FunctionName);
	if (BoundFinishedFunctions.Contains(BindingKey))
	{
		return;
	}

	UFunction* Function = Target->GetClass()->FindFunctionByName(FunctionName);
	if (Function)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Target, FunctionName);
		OnTimelineFinished.AddUnique(Delegate);
		BoundFinishedFunctions.Add(BindingKey);
	}
}

void UTimelineObject::BindEventTrackFunction(FName TrackName, UObject* Target, FName FunctionName)
{
	if (!Target || FunctionName == NAME_None || TrackName == NAME_None)
	{
		return;
	}

	// Prevent duplicate bindings
	TSet<TPair<TWeakObjectPtr<UObject>, FName>>& BoundFunctions = BoundEventTrackFunctions.FindOrAdd(TrackName);
	TPair<TWeakObjectPtr<UObject>, FName> BindingKey(Target, FunctionName);
	if (BoundFunctions.Contains(BindingKey))
	{
		return;
	}

	UFunction* Function = Target->GetClass()->FindFunctionByName(FunctionName);
	if (Function)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Target, FunctionName);
		GetEventTrackDelegate(TrackName).AddUnique(Delegate);
		BoundFunctions.Add(BindingKey);
	}
}

#pragma endregion

#pragma region FTickableGameObject Interface

void UTimelineObject::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TimelineObjectTick);

	// Use undilated time if configured to ignore time dilation
	if (bIgnoreTimeDilation)
	{
		DeltaTime = FApp::GetDeltaTime();

		if (const UWorld* World = GetWorld())
		{
			if (const AWorldSettings* WorldSettings = World->GetWorldSettings())
			{
				DeltaTime = FMath::Clamp(DeltaTime, WorldSettings->MinUndilatedFrameTime, WorldSettings->MaxUndilatedFrameTime);
			}
		}
	}

	TheTimeline.TickTimeline(DeltaTime);
}

bool UTimelineObject::IsTickable() const
{
	return TheTimeline.IsPlaying() && !HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
}

bool UTimelineObject::IsTickableInEditor() const
{
	return false;
}

bool UTimelineObject::IsTickableWhenPaused() const
{
	return false;
}

TStatId UTimelineObject::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTimelineObject, STATGROUP_Tickables);
}

UWorld* UTimelineObject::GetTickableGameObjectWorld() const
{
	// Prefer cached world for reliable ticking with non-Actor owners
	if (CachedWorld.IsValid())
	{
		return CachedWorld.Get();
	}
	return GetWorld();
}

#pragma endregion

#pragma region UObject Overrides

UWorld* UTimelineObject::GetWorld() const
{
	if (const UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}
	return nullptr;
}

void UTimelineObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTimelineObject, TheTimeline);
}

bool UTimelineObject::IsSupportedForNetworking() const
{
	return GetOwningActor() != nullptr;
}

int32 UTimelineObject::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject) || !IsSupportedForNetworking())
	{
		return GEngine->GetGlobalFunctionCallspace(Function, this, Stack);
	}

	if (UObject* Outer = GetOuter())
	{
		return Outer->GetFunctionCallspace(Function, Stack);
	}

	return GEngine->GetGlobalFunctionCallspace(Function, this, Stack);
}

bool UTimelineObject::CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack)
{
	if (AActor* Owner = GetOwningActor())
	{
		if (UNetDriver* NetDriver = Owner->GetNetDriver())
		{
			NetDriver->ProcessRemoteFunction(Owner, Function, Parms, OutParms, Stack, this);
			return true;
		}
	}
	return false;
}

void UTimelineObject::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}

void UTimelineObject::OnRep_Timeline(FTimeline& OldTimeline)
{
	// Sync playback position on clients when not actively playing
	if (!TheTimeline.IsPlaying() && OldTimeline.GetPlaybackPosition() != TheTimeline.GetPlaybackPosition())
	{
		TheTimeline.SetPlaybackPosition(TheTimeline.GetPlaybackPosition(), false, true);
	}
}

#pragma endregion

#pragma region Internal Callbacks

void UTimelineObject::Internal_OnTimelineUpdate()
{
	// Broadcast float track values
	for (const auto& Pair : FloatTrackCurves)
	{
		if (Pair.Value)
		{
			float Value = Pair.Value->GetFloatValue(GetPlaybackPosition());
			OnFloatTrack.Broadcast(Pair.Key, Value);
		}
	}

	// Broadcast vector track values
	for (const auto& Pair : VectorTrackCurves)
	{
		if (Pair.Value)
		{
			FVector Value = Pair.Value->GetVectorValue(GetPlaybackPosition());
			OnVectorTrack.Broadcast(Pair.Key, Value);
		}
	}

	// Broadcast linear color track values
	for (const auto& Pair : LinearColorTrackCurves)
	{
		if (Pair.Value)
		{
			FLinearColor Value = Pair.Value->GetLinearColorValue(GetPlaybackPosition());
			OnLinearColorTrack.Broadcast(Pair.Key, Value);
		}
	}

	// Check and fire event tracks
	CheckEventTracks();

	// Fire the general update delegate
	OnTimelineUpdate.Broadcast();
}

void UTimelineObject::Internal_OnTimelineFinished()
{
	OnTimelineFinished.Broadcast();
}

void UTimelineObject::CheckEventTracks()
{
	const float CurrentPosition = GetPlaybackPosition();
	const bool bIsReversing = IsReversing();

	for (auto& Pair : EventTrackCurves)
	{
		UCurveFloat* EventCurve = Pair.Value;
		if (!EventCurve)
		{
			continue;
		}

		const FName& TrackName = Pair.Key;
		float* LastPositionPtr = LastEventTrackPositions.Find(TrackName);
		if (!LastPositionPtr)
		{
			LastEventTrackPositions.Add(TrackName, -1.0f);
			LastPositionPtr = LastEventTrackPositions.Find(TrackName);
		}

		float LastPosition = *LastPositionPtr;
		*LastPositionPtr = CurrentPosition;

		// Skip first frame to establish baseline position
		if (LastPosition < 0.0f)
		{
			continue;
		}

		const FRichCurve& Curve = EventCurve->FloatCurve;
		
		// Check each key to see if we crossed it this frame
		for (auto It = Curve.GetKeyIterator(); It; ++It)
		{
			const float KeyTime = It->Time;
			
			bool bShouldFire;
			if (bIsReversing)
			{
				// Playing backwards: fire if key is between current (lower) and last (higher) position
				bShouldFire = (KeyTime < LastPosition && KeyTime >= CurrentPosition);
			}
			else
			{
				// Playing forwards: fire if key is between last (lower) and current (higher) position
				bShouldFire = (KeyTime > LastPosition && KeyTime <= CurrentPosition);
			}

			if (bShouldFire)
			{
				FOnTimelineObjectEvent* TrackDelegate = EventTrackDelegates.Find(TrackName);
				if (TrackDelegate)
				{
					TrackDelegate->Broadcast();
				}
				// Only fire once per frame even if multiple keys were crossed
				break;
			}
		}
	}
}

#pragma endregion