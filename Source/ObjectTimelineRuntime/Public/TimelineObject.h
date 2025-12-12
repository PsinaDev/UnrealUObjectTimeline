#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/TimelineComponent.h"
#include "Tickable.h"
#include "TimelineObject.generated.h"

class UTimelineTemplate;
class UCurveFloat;
class UCurveVector;
class UCurveLinearColor;

#pragma region Delegates

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimelineObjectEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimelineObjectFloatTrack, FName, TrackName, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimelineObjectVectorTrack, FName, TrackName, FVector, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimelineObjectLinearColorTrack, FName, TrackName, FLinearColor, Value);

#pragma endregion

/**
 * Timeline object that can be used with any UObject-derived class.
 * Unlike UTimelineComponent, this is not restricted to Actors.
 * Implements FTickableGameObject for autonomous ticking when playing.
 */
UCLASS(BlueprintType, Blueprintable)
class OBJECTTIMELINERUNTIME_API UTimelineObject : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UTimelineObject();

#pragma region Playback Control

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void PlayFromStart();

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void Reverse();

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void ReverseFromEnd();

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Timeline")
	bool IsPlaying() const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	bool IsReversing() const;

#pragma endregion

#pragma region Position and Length

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetPlaybackPosition(float NewPosition, bool bFireEvents, bool bFireUpdate = true);

	UFUNCTION(BlueprintPure, Category = "Timeline")
	float GetPlaybackPosition() const;

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetNewTime(float NewTime);

	UFUNCTION(BlueprintPure, Category = "Timeline")
	float GetTimelineLength() const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	float GetScaledTimelineLength() const;

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetTimelineLength(float NewLength);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetTimelineLengthMode(ETimelineLengthMode NewLengthMode);

#pragma endregion

#pragma region Playback Settings

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetLooping(bool bNewLooping);

	UFUNCTION(BlueprintPure, Category = "Timeline")
	bool IsLooping() const;

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetPlayRate(float NewRate);

	UFUNCTION(BlueprintPure, Category = "Timeline")
	float GetPlayRate() const;

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetIgnoreTimeDilation(bool bNewIgnoreTimeDilation);

	UFUNCTION(BlueprintPure, Category = "Timeline")
	bool GetIgnoreTimeDilation() const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	TEnumAsByte<ETimelineDirection::Type> GetPlaybackDirection() const;

#pragma endregion

#pragma region Curve Management

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetFloatCurve(UCurveFloat* NewFloatCurve, FName FloatTrackName);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetVectorCurve(UCurveVector* NewVectorCurve, FName VectorTrackName);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetLinearColorCurve(UCurveLinearColor* NewLinearColorCurve, FName LinearColorTrackName);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void AddEvent(float Time, FOnTimelineEvent EventFunc);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void AddInterpVector(UCurveVector* VectorCurve, FOnTimelineVector InterpFunc, FName PropertyName = NAME_None, FName TrackName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void AddInterpFloat(UCurveFloat* FloatCurve, FOnTimelineFloat InterpFunc, FName PropertyName = NAME_None, FName TrackName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void AddInterpLinearColor(UCurveLinearColor* LinearColorCurve, FOnTimelineLinearColor InterpFunc, FName PropertyName = NAME_None, FName TrackName = NAME_None);

#pragma endregion

#pragma region Value Getters

	UFUNCTION(BlueprintPure, Category = "Timeline")
	float GetFloatValue(UCurveFloat* FloatCurve) const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	FVector GetVectorValue(UCurveVector* VectorCurve) const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	FLinearColor GetLinearColorValue(UCurveLinearColor* ColorCurve) const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	UCurveFloat* GetFloatTrackCurve(FName TrackName) const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	UCurveVector* GetVectorTrackCurve(FName TrackName) const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	UCurveLinearColor* GetLinearColorTrackCurve(FName TrackName) const;

#pragma endregion

#pragma region Property Binding

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetPropertySetObject(UObject* NewPropertySetObject);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetTimelinePostUpdateFunc(FOnTimelineEvent NewTimelinePostUpdateFunc);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetTimelineFinishedFunc(FOnTimelineEvent NewTimelineFinishedFunc);

	void SetTimelineFinishedFunc(FOnTimelineEventStatic NewTimelineFinishedFunc);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void SetDirectionPropertyName(FName DirectionPropertyName);

#pragma endregion

#pragma region Utility

	void GetAllCurves(TSet<class UCurveBase*>& InOutCurves) const;

	UFUNCTION(BlueprintPure, Category = "Timeline")
	AActor* GetOwningActor() const;

	/**
	 * Gets or creates a timeline object for the given owner.
	 * If a timeline with the given name already exists, returns it.
	 * Otherwise, creates a new one and initializes it from the Blueprint template.
	 */
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, Category = "Timeline", meta = (WorldContext = "Owner"))
	static UTimelineObject* GetOrCreateTimelineObject(UObject* Owner, FName TimelineName, FName UpdateFuncName = NAME_None, FName FinishedFuncName = NAME_None);

	/** Initializes this timeline from a UTimelineTemplate (works in both editor and runtime via BPGC->Timelines) */
	void InitializeFromTemplate(UTimelineTemplate* Template);

#pragma endregion

#pragma region Event Track Management

	FOnTimelineObjectEvent& GetEventTrackDelegate(FName TrackName);

	void RemoveAllDelegatesForObject(UObject* BoundObject);

	void RegisterEventTrack(FName TrackName, UCurveFloat* EventCurve);

#pragma endregion

#pragma region Dynamic Binding

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void BindUpdateFunction(UObject* Target, FName FunctionName);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void BindFinishedFunction(UObject* Target, FName FunctionName);

	UFUNCTION(BlueprintCallable, Category = "Timeline")
	void BindEventTrackFunction(FName TrackName, UObject* Target, FName FunctionName);

#pragma endregion

#pragma region FTickableGameObject Interface

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

#pragma endregion

#pragma region UObject Overrides

	virtual UWorld* GetWorld() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override;
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override;
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Public Delegates

	UPROPERTY(BlueprintAssignable, Category = "Timeline")
	FOnTimelineObjectEvent OnTimelineUpdate;

	UPROPERTY(BlueprintAssignable, Category = "Timeline")
	FOnTimelineObjectEvent OnTimelineFinished;

	UPROPERTY(BlueprintAssignable, Category = "Timeline")
	FOnTimelineObjectFloatTrack OnFloatTrack;

	UPROPERTY(BlueprintAssignable, Category = "Timeline")
	FOnTimelineObjectVectorTrack OnVectorTrack;

	UPROPERTY(BlueprintAssignable, Category = "Timeline")
	FOnTimelineObjectLinearColorTrack OnLinearColorTrack;

#pragma endregion

protected:

#pragma region Internal State

	/** The underlying FTimeline that handles actual timeline logic */
	UPROPERTY(ReplicatedUsing = OnRep_Timeline)
	FTimeline TheTimeline;

	UPROPERTY()
	bool bIgnoreTimeDilation;

	/** Track name to curve mappings for runtime access */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCurveFloat>> FloatTrackCurves;
	
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCurveVector>> VectorTrackCurves;
	
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCurveLinearColor>> LinearColorTrackCurves;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCurveFloat>> EventTrackCurves;

	/** Event track delegates keyed by track name */
	TMap<FName, FOnTimelineObjectEvent> EventTrackDelegates;

	/** Tracks last position for each event track to detect key crossings */
	TMap<FName, float> LastEventTrackPositions;

	/** Track bound functions to prevent duplicate bindings */
	TSet<TPair<TWeakObjectPtr<UObject>, FName>> BoundUpdateFunctions;
	TSet<TPair<TWeakObjectPtr<UObject>, FName>> BoundFinishedFunctions;
	TMap<FName, TSet<TPair<TWeakObjectPtr<UObject>, FName>>> BoundEventTrackFunctions;

	/** Cached world reference for reliable ticking with non-Actor owners */
	TWeakObjectPtr<UWorld> CachedWorld;

	/** Deferred autoplay flag - Play() called after delegates are bound */
	bool bPendingAutoPlay = false;

#pragma endregion

#pragma region Internal Callbacks

	UFUNCTION()
	void OnRep_Timeline(FTimeline& OldTimeline);

	UFUNCTION()
	void Internal_OnTimelineUpdate();

	UFUNCTION()
	void Internal_OnTimelineFinished();

	/** Checks all event tracks and fires delegates for any keys that were crossed */
	void CheckEventTracks();

#pragma endregion
};