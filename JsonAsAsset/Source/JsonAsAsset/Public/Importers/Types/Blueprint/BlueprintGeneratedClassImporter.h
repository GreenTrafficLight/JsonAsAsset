// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StructOnScope.h"

#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

#include "EdGraph/EdGraph.h"

#include "Importers/Constructor/Importer.h"

class IBlueprintGeneratedClassImporter : public IImporter {
public:

	IBlueprintGeneratedClassImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass) :
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;

private:
	void CreateVariables(UBlueprint* BP, FString OuterName, const TArray<TSharedPtr<FJsonValue>> ChildrensObjectPath, UEdGraph* FunctionGraph = nullptr);

	FEdGraphPinType GetPinType(const TSharedPtr<FJsonObject>& Export);

	void ReadFuncMap(UBlueprint* BP);

	void HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot);

	void HandleInheritableComponentHandler(UBlueprint* BP, const TSharedPtr<FJsonObject> InheritableComponentHandlerExport);

	void ReadComponentTemplate(UBlueprint* BP, UActorComponent* ComponentTemplate, const TSharedPtr<FJsonObject> ComponentTemplateObjectPath);
};

REGISTER_IMPORTER(IBlueprintGeneratedClassImporter, (TArray<FString>{
	TEXT("BlueprintGeneratedClass"),
}), TEXT("Blueprint Assets"));
