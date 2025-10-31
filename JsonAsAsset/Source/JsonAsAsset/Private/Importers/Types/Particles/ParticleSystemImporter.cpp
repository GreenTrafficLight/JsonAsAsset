// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Particles/ParticleSystemImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "Particles/ParticleSystem.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleSpriteEmitter.h"

bool IParticleSystemImporter::Import() {
	UParticleSystem* ParticleSystem = NewObject<UParticleSystem>(Package, UParticleSystem::StaticClass(), FName(*AssetName), RF_Public | RF_Standalone);

	const TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));

	const TArray<TSharedPtr<FJsonValue>> EmittersObjectPath = Properties->GetArrayField(TEXT("Emitters"));
	CreateEmitters(ParticleSystem, EmittersObjectPath);

	return OnAssetCreation(ParticleSystem);
}

void IParticleSystemImporter::CreateEmitters(UParticleSystem* ParticleSystem, const TArray<TSharedPtr<FJsonValue>> EmittersObjectPath) {
	for (const TSharedPtr<FJsonValue>& EmitterObjectPath : EmittersObjectPath) {
		const TSharedPtr<FJsonObject> EmitterObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(EmitterObjectPath->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> EmitterObjectProperties = EmitterObject->GetObjectField(TEXT("Properties"));

		UParticleEmitter* Emitter = NewObject<UParticleSpriteEmitter>(
			ParticleSystem,
			UParticleSpriteEmitter::StaticClass(),
			*EmitterObject->GetStringField(TEXT("Name")),
			RF_Transactional
		);
		ParticleSystem->Emitters.Add(Emitter);

		Emitter->EmitterName = *EmitterObjectProperties->GetStringField(TEXT("EmitterName"));

		const TArray<TSharedPtr<FJsonValue>> LODLevelsObjectPath = EmitterObjectProperties->GetArrayField(TEXT("LODLevels"));
		CreateLODLevels(Emitter, LODLevelsObjectPath);
	}

}

void IParticleSystemImporter::CreateLODLevels(UParticleEmitter* Emitter, const TArray<TSharedPtr<FJsonValue>> LODLevelsObjectPath) {
	for (const TSharedPtr<FJsonValue>& LODLevelObjectPath : LODLevelsObjectPath) {
		const TSharedPtr<FJsonObject> LODLevelObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(LODLevelObjectPath->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> LODLevelObjectProperties = LODLevelObject->GetObjectField(TEXT("Properties"));

		UParticleLODLevel* LOD = NewObject<UParticleLODLevel>(
			Emitter,
			UParticleLODLevel::StaticClass(),
			*LODLevelObject->GetStringField(TEXT("Name")),
			RF_Transactional
		);
		Emitter->LODLevels.Add(LOD);

		//
		const TSharedPtr<FJsonObject> RequiredModuleObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(LODLevelObjectProperties->GetObjectField(TEXT("RequiredModule")))->AsObject());
		UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>(
			Emitter,
			UParticleModuleRequired::StaticClass(),
			*RequiredModuleObject->GetStringField(TEXT("Name")),
			RF_Transactional
		);

		const TSharedPtr<FJsonObject> RequiredModuleProperties = RequiredModuleObject->GetObjectField(TEXT("Properties"));
		GetObjectSerializer()->DeserializeObjectProperties(RequiredModuleProperties, RequiredModule);

		LOD->RequiredModule = RequiredModule;

		//
		const TSharedPtr<FJsonObject> TypeDataModuleObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(LODLevelObjectProperties->GetObjectField(TEXT("TypeDataModule")))->AsObject());
		UParticleModuleTypeDataMesh* TypeDataModule = NewObject<UParticleModuleTypeDataMesh>(
			Emitter,
			UParticleModuleTypeDataMesh::StaticClass(),
			*TypeDataModuleObject->GetStringField(TEXT("Name")),
			RF_Transactional
		);

		const TSharedPtr<FJsonObject> TypeDataModuleProperties = TypeDataModuleObject->GetObjectField(TEXT("Properties"));
		GetObjectSerializer()->DeserializeObjectProperties(TypeDataModuleProperties, TypeDataModule);

		LOD->TypeDataModule = TypeDataModule;

		//
		const TSharedPtr<FJsonObject> SpawnModuleObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(LODLevelObjectProperties->GetObjectField(TEXT("SpawnModule")))->AsObject());
		UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>(
			Emitter,
			UParticleModuleSpawn::StaticClass(),
			*SpawnModuleObject->GetStringField(TEXT("Name")),
			RF_Transactional
		);

		const TSharedPtr<FJsonObject> SpawnModuleProperties = SpawnModuleObject->GetObjectField(TEXT("Properties"));
		GetObjectSerializer()->DeserializeObjectProperties(SpawnModuleProperties, SpawnModule);

		/*// Ace Combat 7 ? : Get the distribution spawn rate from the required module
		const TSharedPtr<FJsonObject> SpawnRateData = RequiredModuleObject->GetObjectField(TEXT("SpawnRate"));
		const TSharedPtr<FJsonObject> DistributionExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(SpawnRateData->GetObjectField(TEXT("Distribution")))->AsObject());
		const FString DistributionType = DistributionExport->GetStringField(TEXT("Type"));

		static TMap<FString, UClass*> DistributionClassMap = {
			{ TEXT("DistributionFloatConstant"), UDistributionFloatConstant::StaticClass() },
		};

		UDistribution* Distribution = nullptr;
		if (UClass* Class = DistributionClassMap.FindRef(DistributionType)) {
			Distribution = NewObject<UDistribution>(Outer, Class);
			SpawnModule->Rate.Distribution = Distribution;
		}*/

		LOD->SpawnModule = SpawnModule;

		const TArray<TSharedPtr<FJsonValue>> ModulesObjectPath = LODLevelObjectProperties->GetArrayField(TEXT("Modules"));
		CreateModules(LOD, ModulesObjectPath);
	}
	
}

void IParticleSystemImporter::CreateModules(UParticleLODLevel* LOD, const TArray<TSharedPtr<FJsonValue>> ModulesObjectPath) {
	for (const TSharedPtr<FJsonValue>& ModuleObjectPath : ModulesObjectPath) {
		const TSharedPtr<FJsonObject> ModuleData = TSharedPtr<FJsonObject>(GetExportByObjectPath(ModuleObjectPath->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> ModuleProperties = ModuleData->GetObjectField(TEXT("Properties"));

		UClass* ModuleClass = LoadClassFromPath(ModuleData->GetStringField(TEXT("Type")), TEXT("/Script/Engine"));
		if (ModuleClass) {
			UParticleModule* Module = NewObject<UParticleModule>(
				LOD,
				ModuleClass,
				*ModuleData->GetStringField(TEXT("Name")),
				RF_Transactional
			);
			GetObjectSerializer()->DeserializeObjectProperties(ModuleProperties, Module);
			LOD->Modules.Add(Module);
		}

	}

}