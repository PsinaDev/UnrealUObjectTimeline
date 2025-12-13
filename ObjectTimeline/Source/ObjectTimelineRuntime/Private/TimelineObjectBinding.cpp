#include "TimelineObjectBinding.h"
#include "TimelineObject.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

#pragma region Helper Functions

namespace TimelineObjectBindingHelpers
{
	/**
	 * Finds a UTimelineObject instance owned by the given object.
	 * Timeline objects are created with a specific naming convention: TimelineObj_<TimelineName>
	 */
	UTimelineObject* FindTimelineObject(UObject* Owner, FName TimelineName)
	{
		if (!Owner)
		{
			return nullptr;
		}

		FName UniqueObjectName = FName(*FString::Printf(TEXT("TimelineObj_%s"), *TimelineName.ToString()));

		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(Owner, SubObjects, false);

		for (UObject* SubObj : SubObjects)
		{
			if (UTimelineObject* Timeline = Cast<UTimelineObject>(SubObj))
			{
				if (Timeline->GetFName() == UniqueObjectName)
				{
					return Timeline;
				}
			}
		}

		return nullptr;
	}
}

#pragma endregion

#pragma region UDynamicBlueprintBinding Interface

void UTimelineObjectBinding::BindDynamicDelegates(UObject* InInstance) const
{
	if (!InInstance)
	{
		return;
	}

	UClass* InstanceClass = InInstance->GetClass();

	for (const FTimelineObjectBindingEntry& Entry : TimelineBindings)
	{
		UTimelineObject* TimelineObj = TimelineObjectBindingHelpers::FindTimelineObject(InInstance, Entry.TimelineName);
		if (!TimelineObj)
		{
			continue;
		}

		// Bind Update function if specified
		if (Entry.UpdateFunctionName != NAME_None)
		{
			if (InstanceClass->FindFunctionByName(Entry.UpdateFunctionName))
			{
				FScriptDelegate Delegate;
				Delegate.BindUFunction(InInstance, Entry.UpdateFunctionName);
				TimelineObj->OnTimelineUpdate.AddUnique(Delegate);
			}
		}

		// Bind Finished function if specified
		if (Entry.FinishedFunctionName != NAME_None)
		{
			if (InstanceClass->FindFunctionByName(Entry.FinishedFunctionName))
			{
				FScriptDelegate Delegate;
				Delegate.BindUFunction(InInstance, Entry.FinishedFunctionName);
				TimelineObj->OnTimelineFinished.AddUnique(Delegate);
			}
		}

		// Bind Event Track functions
		for (const auto& TrackPair : Entry.EventTrackFunctionNames)
		{
			if (TrackPair.Value != NAME_None)
			{
				if (InstanceClass->FindFunctionByName(TrackPair.Value))
				{
					FScriptDelegate Delegate;
					Delegate.BindUFunction(InInstance, TrackPair.Value);
					TimelineObj->GetEventTrackDelegate(TrackPair.Key).AddUnique(Delegate);
				}
			}
		}
	}
}

void UTimelineObjectBinding::UnbindDynamicDelegates(UObject* InInstance) const
{
	if (!InInstance)
	{
		return;
	}

	for (const FTimelineObjectBindingEntry& Entry : TimelineBindings)
	{
		UTimelineObject* TimelineObj = TimelineObjectBindingHelpers::FindTimelineObject(InInstance, Entry.TimelineName);
		if (TimelineObj)
		{
			TimelineObj->RemoveAllDelegatesForObject(InInstance);
		}
	}
}

void UTimelineObjectBinding::UnbindDynamicDelegatesForProperty(UObject* InInstance, const FObjectProperty* InObjectProperty) const
{
	if (!InInstance || !InObjectProperty)
	{
		return;
	}

	// Find binding entry matching this property name
	for (const FTimelineObjectBindingEntry& Entry : TimelineBindings)
	{
		if (InObjectProperty->GetFName() == Entry.TimelineName)
		{
			UTimelineObject* TimelineObj = Cast<UTimelineObject>(InObjectProperty->GetObjectPropertyValue_InContainer(InInstance));
			if (TimelineObj)
			{
				TimelineObj->RemoveAllDelegatesForObject(InInstance);
			}
			break;
		}
	}
}

#pragma endregion