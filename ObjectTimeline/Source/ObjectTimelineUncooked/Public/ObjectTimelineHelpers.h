#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"

/**
 * Helper functions for Object Timeline that bypass Actor-only restrictions in BlueprintEditorUtils.
 */
namespace ObjectTimelineHelpers
{
	/**
	 * Creates a new UTimelineTemplate for any Blueprint type (including UObject Blueprints).
	 * 
	 * Unlike FBlueprintEditorUtils::AddNewTimeline, this works for non-Actor Blueprints
	 * because it bypasses the DoesSupportTimelines() check which requires Actor-based Blueprints.
	 * 
	 * @param Blueprint The Blueprint to add the timeline to
	 * @param TimelineVarName The variable name for the new timeline
	 * @return The newly created UTimelineTemplate, or nullptr if creation failed or timeline already exists
	 */
	inline UTimelineTemplate* CreateTimelineTemplate(UBlueprint* Blueprint, const FName& TimelineVarName)
	{
		if (!Blueprint || !Blueprint->GeneratedClass)
		{
			return nullptr;
		}

		// Check if timeline already exists
		UTimelineTemplate* ExistingTimeline = Blueprint->FindTimelineTemplateByVariableName(TimelineVarName);
		if (ExistingTimeline)
		{
			return nullptr;
		}

		Blueprint->Modify();

		// Create timeline template (same logic as FBlueprintEditorUtils::AddNewTimeline but without Actor check)
		const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineVarName);
		UTimelineTemplate* NewTimeline = NewObject<UTimelineTemplate>(Blueprint->GeneratedClass, TimelineTemplateName, RF_Transactional);
		Blueprint->Timelines.Add(NewTimeline);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return NewTimeline;
	}
}