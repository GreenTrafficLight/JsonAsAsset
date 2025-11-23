/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Importers/Types/DataAssetImporter.h"
#include "Engine/DataAsset.h"

bool IDataAssetImporter::Import() {
#if UE4_18_BELOW
	UDataAsset* DataAsset = NewObject<UDataAsset>(Package, AssetClass, FName(*AssetName), RF_Public | RF_Standalone);
#else
	UDataAsset* DataAsset = NewObject<UDataAsset>(Package, AssetClass, FName(AssetName), RF_Public | RF_Standalone);
#endif
	auto _ = DataAsset->MarkPackageDirty();

	GetObjectSerializer()->SetExportForDeserialization(JsonObject, DataAsset);
	GetObjectSerializer()->Parent = DataAsset;

	GetObjectSerializer()->DeserializeExports(AllJsonObjects);

	GetObjectSerializer()->DeserializeObjectProperties(AssetData, DataAsset);

	return OnAssetCreation(DataAsset);
}