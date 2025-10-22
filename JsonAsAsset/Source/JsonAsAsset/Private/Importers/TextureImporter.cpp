#include "Importers/TextureImporter.h"

#include "detex.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureCube.h"
#include "Factories/TextureRenderTargetFactoryNew.h"
#include "nvimage/DirectDrawSurface.h"
#include "nvimage/Image.h"
#include "Utilities/JsonUtilities.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Utilities/TextureDecode/TextureNVTT.h"
#include "UObject/UnrealType.h"
#include "Engine/TextureDefines.h"

bool UTextureImporter::ImportTexture2D(UTexture*& OutTexture2D, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const {
	const TSharedPtr<FJsonObject> SubObjectProperties = Properties->GetObjectField("Properties");

	// NEW: .bin support
	UTexture2D* Texture2D = NewObject<UTexture2D>(OutermostPkg, UTexture2D::StaticClass(), *AssetName, RF_Standalone | RF_Public);
	Texture2D->PlatformData = new FTexturePlatformData();

	ImportTexture2D_Data(Texture2D, SubObjectProperties);
	FTexturePlatformData* PlatformData = Texture2D->PlatformData;

	const int SizeX = Properties->GetNumberField("SizeX");
	const int SizeY = Properties->GetNumberField("SizeY");

	const TArray<TSharedPtr<FJsonValue>>* TextureMipsPtr;
	Properties->TryGetArrayField(TEXT("Mips"), TextureMipsPtr);
	if (TextureMipsPtr) {
		auto TextureMips = *TextureMipsPtr;
		if (TextureMips.Num() == 1) {
			Texture2D->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
		}
	}

	FString PixelFormat;
	if (Properties->TryGetStringField("PixelFormat", PixelFormat)) PlatformData->PixelFormat = static_cast<EPixelFormat>(Texture2D->GetPixelFormatEnum()->GetValueByNameString(PixelFormat));

	int Size = SizeX * SizeY * (PlatformData->PixelFormat == PF_BC6H ? 16 : 4);
	if (PlatformData->PixelFormat == PF_FloatRGBA || PlatformData->PixelFormat == PF_G16) Size = Data.Num();
	uint8* DecompressedData = static_cast<uint8*>(FMemory::Malloc(Size));

	GetDecompressedTextureData(Data.GetData(), DecompressedData, SizeX, SizeY, Size, PlatformData->PixelFormat);

	ETextureSourceFormat Format = TSF_BGRA8;
	if (Texture2D->CompressionSettings == TC_HDR) Format = TSF_RGBA16F;
	if (PlatformData->PixelFormat == PF_G16) Format = TSF_G8;
	Texture2D->Source.Init(SizeX, SizeY, 1, 1, Format);
	uint8_t* Dest = Texture2D->Source.LockMip(0);
	FMemory::Memcpy(Dest, DecompressedData, Size);
	Texture2D->Source.UnlockMip(0);

	Texture2D->UpdateResource();

	if (Texture2D) {
		OutTexture2D = Texture2D;
		return true;
	}

	return false;
}

bool UTextureImporter::ImportTextureCube(UTexture*& OutTextureCube, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const {
	return false;
}

bool UTextureImporter::ImportVolumeTexture(UTexture*& OutTexture2D, const TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const {
	return false;
}

bool UTextureImporter::ImportRenderTarget2D(UTexture*& OutRenderTarget2D, const TSharedPtr<FJsonObject>& Properties) const {
	return false;
}

// Handle UTexture2D
bool UTextureImporter::ImportTexture2D_Data(UTexture2D* InTexture2D, const TSharedPtr<FJsonObject>& Properties) const {
	if (InTexture2D == nullptr) return false;

	ImportTexture_Data(InTexture2D, Properties);

	FString AddressX;
	FString AddressY;
	bool bHasBeenPaintedInEditor;

	// Get the TextureAddress enum
	UEnum* TextureAddressEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("TextureAddress"), true);
	if (Properties->TryGetStringField("AddressX", AddressX) && TextureAddressEnum)
	{
		int32 Value = TextureAddressEnum->GetIndexByNameString(AddressX);
		if (Value != INDEX_NONE)
		{
			InTexture2D->AddressX = static_cast<TextureAddress>(Value);
		}
	}

	if (Properties->TryGetStringField("AddressY", AddressY) && TextureAddressEnum)
	{
		int32 Value = TextureAddressEnum->GetIndexByNameString(AddressY);
		if (Value != INDEX_NONE)
		{
			InTexture2D->AddressY = static_cast<TextureAddress>(Value);
		}
	}

	if (Properties->TryGetBoolField("bHasBeenPaintedInEditor", bHasBeenPaintedInEditor))
	{
		InTexture2D->bHasBeenPaintedInEditor = bHasBeenPaintedInEditor;
	}

	// --------- Platform Data --------- //
	FTexturePlatformData* PlatformData = InTexture2D->PlatformData;

	int SizeX;
	int SizeY;
	FString PixelFormat;

	// Pixel format enum
	UEnum* PixelFormatEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPixelFormat"), true);

	if (Properties->TryGetNumberField("SizeX", SizeX)) PlatformData->SizeX = SizeX;
	if (Properties->TryGetNumberField("SizeY", SizeY)) PlatformData->SizeY = SizeY;
	if (Properties->TryGetStringField("PixelFormat", PixelFormat) && PixelFormatEnum)
	{
		int32 Value = PixelFormatEnum->GetIndexByNameString(PixelFormat);
		if (Value != INDEX_NONE)
		{
			PlatformData->PixelFormat = static_cast<EPixelFormat>(Value);
		}
	}

	int FirstResourceMemMip;
	int LevelIndex;

	if (Properties->TryGetNumberField("FirstResourceMemMip", FirstResourceMemMip))
		InTexture2D->FirstResourceMemMip = FirstResourceMemMip;

	if (Properties->TryGetNumberField("LevelIndex", LevelIndex))
		InTexture2D->LevelIndex = LevelIndex;

	return false;
}

bool UTextureImporter::ImportTexture_Data(UTexture* InTexture, const TSharedPtr<FJsonObject>& Properties) const {
	if (InTexture == nullptr) return false;

	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(Properties,
		{
			"ImportedSize",
			"LODBias"
		}), InTexture);

	return false;
}

void UTextureImporter::GetDecompressedTextureData(uint8* Data, uint8*& OutData, const int SizeX, const int SizeY, const int TotalSize, const EPixelFormat Format) const {
	if (Format == PF_BC7) {
		detexTexture Texture;
		Texture.data = Data;
		Texture.format = DETEX_TEXTURE_FORMAT_BPTC;
		Texture.width = SizeX;
		Texture.height = SizeY;
		Texture.width_in_blocks = SizeX / 4;
		Texture.height_in_blocks = SizeY / 4;
		detexDecompressTextureLinear(&Texture, OutData, DETEX_PIXEL_FORMAT_BGRA8);
	} else if (Format == PF_BC6H) {
		detexTexture Texture;
		Texture.data = Data;
		Texture.format = DETEX_TEXTURE_FORMAT_BPTC_FLOAT;
		Texture.width = SizeX;
		Texture.height = SizeY;
		Texture.width_in_blocks = SizeX / 4;
		Texture.height_in_blocks = SizeY / 4;
		detexDecompressTextureLinear(&Texture, OutData, DETEX_PIXEL_FORMAT_BGRA8);
	} else if (Format == PF_G8) {
		const uint8* s = Data;
		uint8* d = OutData;
		for (int i = 0; i < SizeX * SizeY; i++) {
			const uint8 b = *s++;
			*d++ = b;
			*d++ = b;
			*d++ = b;
			*d++ = 255;
		}
	} else if (Format == PF_B8G8R8A8 || Format == PF_FloatRGBA || Format == PF_G16) {
		FMemory::Memcpy(OutData, Data, TotalSize);
	} else {
		nv::DDSHeader Header;
		nv::Image Image;

		uint FourCC;
		switch (Format) {
		case PF_BC4:
			FourCC = FOURCC_ATI1;
			break;
		case PF_BC5:
			FourCC = FOURCC_ATI2;
			break;
		case PF_DXT1:
			FourCC = FOURCC_DXT1;
			break;
		case PF_DXT3:
			FourCC = FOURCC_DXT3;
			break;
		case PF_DXT5:
			FourCC = FOURCC_DXT5;
			break;
		default: FourCC = 0;
		}

		Header.setFourCC(FourCC);
		Header.setWidth(SizeX);
		Header.setHeight(SizeY);
		Header.setNormalFlag(Format == PF_BC5);
		DecodeDDS(Data, SizeX, SizeY, Header, Image);

		// Fallback to raw data
		FMemory::Memcpy(OutData, Image.pixels(), TotalSize);
	}
}
