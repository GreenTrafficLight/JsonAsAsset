// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Importers/Constructor/Importer.h"

class UCurveLinearColorAtlasImporter : public IImporter {
public:
	UCurveLinearColorAtlasImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg):
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg) {
	}

	virtual bool Import() override;
};
