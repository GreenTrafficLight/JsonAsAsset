// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Importers/Constructor/Importer.h"

class UAnimationBaseImporter : public IImporter {
public:
	UAnimationBaseImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;
};

REGISTER_IMPORTER(UAnimationBaseImporter, (TArray<FString>{
	TEXT("AnimSequence"),
	TEXT("AnimMontage")
}), TEXT("Animation Assets"));
