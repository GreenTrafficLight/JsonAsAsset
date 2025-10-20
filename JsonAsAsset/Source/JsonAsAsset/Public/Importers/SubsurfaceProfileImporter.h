// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Importers/Constructor/Importer.h"

class USubsurfaceProfileImporter : public IImporter {
public:
	USubsurfaceProfileImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg):
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg) {
	}

	virtual bool Import() override;
};
