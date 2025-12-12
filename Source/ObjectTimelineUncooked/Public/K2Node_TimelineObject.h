#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_TimelineObject.generated.h"

class UTimelineTemplate;
class FKismetCompilerContext;

/**
 * Blueprint node for Object Timeline functionality.
 * Provides Timeline capabilities for any UObject-derived class, not just Actors.
 * Integrates with the Blueprint compilation system to generate appropriate event handlers.
 */
UCLASS()
class OBJECTTIMELINEUNCOOKED_API UK2Node_TimelineObject : public UK2Node
{
	GENERATED_BODY()

public:
	UK2Node_TimelineObject();

	/** The name of the timeline, used to identify the UTimelineTemplate */
	UPROPERTY()
	FName TimelineName;

#pragma region UK2Node Interface

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void DestroyNode() override;
	virtual void PostPasteNode() override;
	virtual void PostPlacedNewNode() override;
	virtual void ReconstructNode() override;

#pragma endregion

#pragma region Node Navigation

	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual bool CanJumpToDefinition() const override;
	virtual void JumpToDefinition() const override;

#pragma endregion

#pragma region Compilation

	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetNodeAttributes(TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes) const override;
	virtual FNodeHandlingFunctor* CreateNodeHandler(FKismetCompilerContext& CompilerContext) const override;

#pragma endregion

#pragma region Dynamic Binding

	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;

#pragma endregion

#pragma region Menu Actions

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;

#pragma endregion

#pragma region Pin Accessors

	UEdGraphPin* GetPlayPin() const;
	UEdGraphPin* GetPlayFromStartPin() const;
	UEdGraphPin* GetStopPin() const;
	UEdGraphPin* GetReversePin() const;
	UEdGraphPin* GetReverseFromEndPin() const;
	UEdGraphPin* GetUpdatePin() const;
	UEdGraphPin* GetFinishedPin() const;
	UEdGraphPin* GetNewTimePin() const;
	UEdGraphPin* GetSetNewTimePin() const;
	UEdGraphPin* GetDirectionPin() const;

#pragma endregion

#pragma region Timeline Management

	/** Gets the UTimelineTemplate associated with this node */
	UTimelineTemplate* GetTimelineTemplate() const;
	
	/** Renames the timeline and updates the UTimelineTemplate */
	void RenameTimeline(const FString& NewName);

	/** Populates arrays with track names from the timeline template */
	void FindFloatTracks(TArray<FName>& OutTrackNames) const;
	void FindVectorTracks(TArray<FName>& OutTrackNames) const;
	void FindLinearColorTracks(TArray<FName>& OutTrackNames) const;
	void FindEventTracks(TArray<FName>& OutTrackNames) const;

#pragma endregion

private:

#pragma region Pin Creation

	void CreateInputPins();
	void CreateOutputPins();
	void CreateTrackOutputPins();

#pragma endregion

#pragma region Node Expansion Helpers

	/** Creates the GetOrCreateTimelineObject function call node */
	UEdGraphPin* CreateGetTimelineObjectCall(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, FName UpdateFunc, FName FinishedFunc);
	
	/** Expands an input exec pin to call the appropriate timeline function */
	void ExpandInputExecPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* ExecPin, FName FunctionName, UEdGraphPin* TimelineReturnPin);
	
	/** Expands track output pins to call value getter functions */
	void ExpandTrackPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* TimelineReturnPin);
	
	/** Creates an internal event node for output exec pins (Update, Finished, Event tracks) */
	void CreateInternalEventForPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* Pin, FName FunctionName);

#pragma endregion

#pragma region Generated Function Names

	/** Generated function name for the Update callback */
	FName UpdateFunctionName;
	
	/** Generated function name for the Finished callback */
	FName FinishedFunctionName;

#pragma endregion
};