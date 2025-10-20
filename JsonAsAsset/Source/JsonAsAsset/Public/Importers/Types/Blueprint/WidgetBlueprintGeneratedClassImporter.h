// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StructOnScope.h"
#include "WidgetBlueprint.h"
#include "Components/PanelWidget.h"
#include "Importers/Constructor/Importer.h"

class IWidgetBlueprintGeneratedClassImporter : public IImporter {
public:

	IWidgetBlueprintGeneratedClassImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass) :
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;

	void HandlePanelSlots(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject> PanelJsonObject, UPanelWidget* Panel);

private:
	UClass* GetWidgetClass(const TSharedPtr<FJsonObject>& ObjData);
};

REGISTER_IMPORTER(IWidgetBlueprintGeneratedClassImporter, (TArray<FString>{
	TEXT("WidgetBlueprintGeneratedClass"),
}), TEXT("Blueprint Assets"));
