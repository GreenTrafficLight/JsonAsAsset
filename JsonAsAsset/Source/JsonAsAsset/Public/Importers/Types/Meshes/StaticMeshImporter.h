// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StructOnScope.h"
#include "Importers/Constructor/Importer.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

class UStaticMeshImporter : public IImporter {
public:

	UStaticMeshImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg) :
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg) {
	}

	bool ImportStaticMesh(UStaticMesh*& OutStaticMesh, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportSkeletalMesh(USkeletalMesh*& OutSkeletalMesh, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportStaticMesh_Data(UStaticMesh* InStaticMesh, const TSharedPtr<FJsonObject>& Properties) const;
};
