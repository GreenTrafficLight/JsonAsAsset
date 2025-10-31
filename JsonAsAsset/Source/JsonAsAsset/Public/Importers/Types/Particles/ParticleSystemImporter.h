// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StructOnScope.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

#include "Importers/Constructor/Importer.h"

class IParticleSystemImporter : public IImporter {
public:

	IParticleSystemImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass) :
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;

private:
	void CreateEmitters(UParticleSystem* ParticleSystem, const TArray<TSharedPtr<FJsonValue>> EmittersObjectPath);

	void CreateLODLevels(UParticleEmitter* Emitter, const TArray<TSharedPtr<FJsonValue>> LODLevelsObjectPath);

	void CreateModules(UParticleLODLevel* LOD, const TArray<TSharedPtr<FJsonValue>> ModulesObjectPath);
};

REGISTER_IMPORTER(IParticleSystemImporter, (TArray<FString>{
	TEXT("ParticleSystem"),
}), TEXT("Particle Assets"));