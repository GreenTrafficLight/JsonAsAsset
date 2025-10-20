// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StructOnScope.h"

#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

#include "Importers/Constructor/Importer.h"

class UBlueprintGeneratedClassImporter : public IImporter {
public:

	UBlueprintGeneratedClassImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects) :
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects) {
	}

	virtual bool Import() override;

private:
	void HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot);

	void ReadComponentTemplate(UActorComponent* ComponentTemplate, const TSharedPtr<FJsonObject> ComponentTemplateObjectPath);
};
