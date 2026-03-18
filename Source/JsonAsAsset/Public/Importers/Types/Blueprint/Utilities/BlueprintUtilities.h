#pragma once

#include "Engine/SimpleConstructionScript.h"

#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "Engine/SCS_Node.h"

inline void GetAllSCSNodes(UBlueprint* Blueprint, TArray<USCS_Node*>& OutNodes)
{
	if (!Blueprint)
		return;

	// Add current Blueprint�s nodes
	if (Blueprint->SimpleConstructionScript)
	{
		OutNodes.Append(Blueprint->SimpleConstructionScript->GetAllNodes());
	}

	// Walk up the inheritance chain to include parent Blueprints
	for (UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass ? Blueprint->ParentClass->ClassGeneratedBy : nullptr);
		ParentBP;
		ParentBP = Cast<UBlueprint>(ParentBP->ParentClass ? ParentBP->ParentClass->ClassGeneratedBy : nullptr))
	{
		if (ParentBP->SimpleConstructionScript)
		{
			OutNodes.Append(ParentBP->SimpleConstructionScript->GetAllNodes());
		}
	}
}

inline USCS_Node* FindSCSNodeByName(const TArray<USCS_Node*>& AllNodes, const FName& ComponentName)
{
	for (USCS_Node* Node : AllNodes)
	{
		if (!Node) {
			continue;
		}

		if (Node->GetVariableName() == ComponentName) {
			return Node;
		}
	}
	return nullptr;
}

/**
 * Get the ubergraph of a blueprint
 *
 *
 */
inline UEdGraph* GetUberGraph(UBlueprint* Blueprint) {
	if (Blueprint->UbergraphPages.Num() > 0) {
		return Blueprint->UbergraphPages[0];
	}
	return nullptr;
}

/**
 * Remove the event nodes from a graph
 */
inline void RemoveEventNodes(UEdGraph* EdGraph) {
	TArray<UK2Node_Event*> EventNodes;
	for (UEdGraphNode* Node : EdGraph->Nodes) {
		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node)) {
			EventNodes.Add(EventNode);
		}
	}

	for (UK2Node_Event* EventNode : EventNodes) {
		EdGraph->RemoveNode(EventNode);
	}
}