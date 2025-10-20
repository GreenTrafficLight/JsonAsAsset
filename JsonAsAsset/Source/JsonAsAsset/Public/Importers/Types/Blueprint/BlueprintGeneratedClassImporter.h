// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StructOnScope.h"

#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

#include "Importers/Constructor/Importer.h"

class IBlueprintGeneratedClassImporter : public IImporter {
public:

	IBlueprintGeneratedClassImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass) :
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;

private:
	void HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot);

	void ReadComponentTemplate(UActorComponent* ComponentTemplate, const TSharedPtr<FJsonObject> ComponentTemplateObjectPath);
};

REGISTER_IMPORTER(IBlueprintGeneratedClassImporter, (TArray<FString>{
	TEXT("BlueprintGeneratedClass"),
}), TEXT("Blueprint Assets"));
