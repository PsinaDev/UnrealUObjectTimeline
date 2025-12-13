// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectTimelineUncooked.h"
#include "TimelineObjectGraphPanelNodeFactory.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "FObjectTimelineUncookedModule"

void FObjectTimelineUncookedModule::StartupModule()
{
	GraphPanelNodeFactory = MakeShareable(new FTimelineObjectGraphPanelNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory);
}

void FObjectTimelineUncookedModule::ShutdownModule()
{
	if (GraphPanelNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory);
		GraphPanelNodeFactory.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FObjectTimelineUncookedModule, ObjectTimelineUncooked)
