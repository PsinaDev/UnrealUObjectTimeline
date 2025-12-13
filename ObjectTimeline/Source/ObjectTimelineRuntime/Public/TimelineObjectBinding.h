#pragma once

#include "CoreMinimal.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "TimelineObjectBinding.generated.h"

/**
 * Stores binding information for a single object timeline.
 * Maps timeline name to the generated function names for Update, Finished, and Event tracks.
 */
USTRUCT()
struct FTimelineObjectBindingEntry
{
	GENERATED_BODY()

	/** Name of the timeline this entry binds */
	UPROPERTY()
	FName TimelineName;

	/** Generated function name for the Update callback */
	UPROPERTY()
	FName UpdateFunctionName;

	/** Generated function name for the Finished callback */
	UPROPERTY()
	FName FinishedFunctionName;

	/** Maps event track names to their generated function names */
	UPROPERTY()
	TMap<FName, FName> EventTrackFunctionNames;
};

/**
 * Dynamic binding class for UTimelineObject.
 * Handles automatic delegate binding between Blueprint-generated functions and timeline events.
 * This is necessary because UObject-based timelines cannot use the standard Actor binding mechanism.
 */
UCLASS()
class OBJECTTIMELINERUNTIME_API UTimelineObjectBinding : public UDynamicBlueprintBinding
{
	GENERATED_BODY()

public:
	/** Binds all timeline delegates when an instance is created */
	virtual void BindDynamicDelegates(UObject* InInstance) const override;
	
	/** Unbinds all timeline delegates when an instance is destroyed */
	virtual void UnbindDynamicDelegates(UObject* InInstance) const override;
	
	/** Unbinds delegates for a specific property */
	virtual void UnbindDynamicDelegatesForProperty(UObject* InInstance, const FObjectProperty* InObjectProperty) const override;

	/** All timeline bindings registered during Blueprint compilation */
	UPROPERTY()
	TArray<FTimelineObjectBindingEntry> TimelineBindings;
};