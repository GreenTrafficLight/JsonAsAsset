/* Copyright JsonAsAsset Contributors 2024-2026 */

#pragma once

#include "Serializers/PropertySerializer.h"
#include "Settings/JsonAsAssetSettings.h"
#include "EngineUtilities.h"

#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#endif

inline TSubclassOf<UObject> LoadClassFromPath(const FString& ObjectName, const FString& ObjectPath) {
	const FString FullPath = ObjectPath + TEXT(".") + ObjectName;

	if (UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath)) {
		if (UClass* LoadedClass = Cast<UClass>(LoadedObject)) {
			return LoadedClass;
		}
	}

	return nullptr;
}

inline TSubclassOf<UObject> LoadBlueprintClass(FString& ObjectPath) {
	const UJsonAsAssetSettings* Settings = GetSettings();
	
	if (!Settings->AssetSettings.ProjectName.IsEmpty()) {
		ObjectPath = ObjectPath.Replace(*(Settings->AssetSettings.ProjectName + "/Content"), TEXT("/Game"));
	}
	
	FString FullPath = ObjectPath; 
	if (FullPath.EndsWith(TEXT(".1"))) {
		FullPath = FullPath.LeftChop(2);
	}

	if (UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath)) {
		const UBlueprint* LoadedBlueprint = Cast<UBlueprint>(LoadedObject);
		
		if (LoadedBlueprint && LoadedBlueprint->GeneratedClass) {
			return LoadedBlueprint->GeneratedClass;
		}
	}

	return nullptr;
}

inline UClass* LoadClass(const TSharedPtr<FJsonObject>& SuperStruct) {
	const FString ObjectName = SuperStruct->GetStringField(TEXT("ObjectName")).Replace(TEXT("Class'"), TEXT("")).Replace(TEXT("'"), TEXT(""));
	FString ObjectPath = SuperStruct->GetStringField(TEXT("ObjectPath"));

	/* It's a C++ class if it has Script in it */
	if (ObjectPath.Contains("/Script/")) {
		return LoadClassFromPath(ObjectName, ObjectPath);
	}
	
	ObjectPath.Split(".", &ObjectPath, nullptr);

	return LoadBlueprintClass(ObjectPath);
}

inline TSharedPtr<FJsonObject> GetSuperStructJsonObject(const TSharedPtr<FJsonObject>& JsonObject) {
	if (JsonObject->HasField(TEXT("Next"))) {
		return JsonObject->GetObjectField(TEXT("Next"));
	}
	
	return JsonObject->GetObjectField(TEXT("SuperStruct"));
}

inline EBlueprintType GetBlueprintType(const UClass* Class) {
	EBlueprintType BlueprintType = BPTYPE_Normal;

	if (Class->HasAnyClassFlags(CLASS_Const)) {
		BlueprintType = BPTYPE_Const;
	}
	if (Class == UBlueprintFunctionLibrary::StaticClass()) {
		BlueprintType = BPTYPE_FunctionLibrary;
	}
	if (Class == UInterface::StaticClass()) {
		BlueprintType = BPTYPE_Interface;
	}
	
	return BlueprintType;
}

inline FUObjectExport& GetClassDefaultObject(FUObjectExportContainer AssetContainer, const FUObjectJsonValueExport& JsonObject) {
	FUObjectExport& Export = AssetContainer.GetExportByObjectPath(JsonObject.GetObject("ClassDefaultObject"));
	if (!Export.IsJsonValid()) {
		Export = AssetContainer.GetExportStartingWith("Name", "Default__");
	}

	return Export;
}

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

inline UEdGraph* GetUberGraph(UBlueprint* Blueprint) {
	if (Blueprint->BlueprintType != BPTYPE_Normal || Blueprint->BlueprintType != BPTYPE_LevelScript) {
		return nullptr;
	}

	UEdGraph* UberGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (UberGraph) {
		return UberGraph;
	}
	UE_LOG(LogTemp, Warning, TEXT("UberGraph not found"));
	return nullptr;
}

inline void RemoveEventNodes(UBlueprint* Blueprint) {
	UEdGraph* UberGraph = GetUberGraph(Blueprint);
	if (!UberGraph) {
		return;
	}

	TArray<UK2Node_Event*> EventNodes;
	UberGraph->GetNodesOfClass<UK2Node_Event>(EventNodes);
	for (UK2Node_Event* EventNode : EventNodes) {
		FBlueprintEditorUtils::RemoveNode(Blueprint, EventNode);
	}
}