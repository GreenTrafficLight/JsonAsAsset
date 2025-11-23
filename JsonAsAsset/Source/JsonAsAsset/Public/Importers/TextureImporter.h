// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Importers/Constructor/Importer.h"
#include "Engine/Texture.h"

class UTextureImporter : public IImporter {
public:
	UTextureImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const bool bUseOctetStream) :
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg), bUseOctetStream(bUseOctetStream) {
	}

	bool bUseOctetStream = true;

	template <class T = UObject>
	bool ImportTexture2D(UTexture*& OutTexture2D, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties);
	bool ImportTextureCube(UTexture*& OutTextureCube, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportVolumeTexture(UTexture*& OutTexture2D, const TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportRenderTarget2D(UTexture*& OutRenderTarget2D, const TSharedPtr<FJsonObject>& Properties) const;

	bool DeserializeTexture2D(UTexture2D* InTexture2D, const TSharedPtr<FJsonObject>& Properties) const;
	bool DeserializeTexture(UTexture* InTexture, const TSharedPtr<FJsonObject>& Properties) const;
	bool DeserializeTexturePlatformData(UTexture* Texture, TArray<uint8>& Data, FTexturePlatformData& TexturePlatformData, const TSharedPtr<FJsonObject>& Properties);

private:
	static void GetDecompressedTextureData(uint8* Data, uint8*& OutData, const int SizeX, const int SizeY, const int SizeZ, const int TotalSize, const EPixelFormat Format);
};