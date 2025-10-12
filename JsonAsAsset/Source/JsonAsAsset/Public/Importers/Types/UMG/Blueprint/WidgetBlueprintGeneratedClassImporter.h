// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StructOnScope.h"
#include "WidgetBlueprint.h"
#include "Components/PanelWidget.h"
#include "Importer.h"

class UWidgetBlueprintGeneratedClassImporter : public IImporter {
public:

	UWidgetBlueprintGeneratedClassImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects) :
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects) {
	}

	virtual bool ImportData() override;

	void HandleCanvasPanelSlots(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject> CanvasPanelJsonObject, UPanelWidget* Panel);

private:
	UClass* GetWidgetClass(const TSharedPtr<FJsonObject>& ObjData);
};
