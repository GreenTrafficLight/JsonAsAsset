// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Importers/Constructor/Importer.h"
#include "Engine/Texture.h"

class UTextureImporter : public IImporter {
public:
	UTextureImporter(const FString& AssetName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg):
		IImporter(AssetName, FilePath, JsonObject, Package, OutermostPkg) {
	}

	// Public as we don't import 2D textures locally at the moment
	bool ImportTexture2D(UTexture*& OutTexture2D, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportTextureCube(UTexture*& OutTextureCube, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportVolumeTexture(UTexture*& OutTexture2D, const TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportRenderTarget2D(UTexture*& OutRenderTarget2D, const TSharedPtr<FJsonObject>& Properties) const;

	bool ImportTexture2D_Data(UTexture2D* InTexture2D, const TSharedPtr<FJsonObject>& Properties) const;
	bool ImportTexture_Data(UTexture* InTexture, const TSharedPtr<FJsonObject>& Properties) const;

private:
	void GetDecompressedTextureData(uint8* Data, uint8*& OutData, const int SizeX, const int SizeY, const int TotalSize, const EPixelFormat Format) const;
};
