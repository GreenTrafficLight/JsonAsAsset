#include "Importers/TextureImporter.h"

#include "detex.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureCube.h"
#if !UE4_18_BELOW
#include "Engine/VolumeTexture.h"
#endif
#include "Factories/TextureFactory.h"
#include "Factories/TextureRenderTargetFactoryNew.h"
#include "nvimage/DirectDrawSurface.h"
#include "nvimage/Image.h"
#include "Utilities/EngineUtilities.h"
#include "Utilities/JsonUtilities.h"
#include "Utilities/Textures/TextureDecode/TextureNVTT.h"

template bool UTextureImporter::ImportTexture2D<UTexture2D>(UTexture*&, TArray<uint8>&, const TSharedPtr<FJsonObject>&);
template bool UTextureImporter::ImportTexture2D<UTextureLightProfile>(UTexture*&, TArray<uint8>&, const TSharedPtr<FJsonObject>&);

template <typename T>
bool UTextureImporter::ImportTexture2D(UTexture*& OutTexture2D, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) {
	UTexture2D* Texture2D;

	if (bUseOctetStream) {
		Texture2D = NewObject<T>(OutermostPkg, T::StaticClass(), *AssetName, RF_Standalone | RF_Public);
	} else {
		UTextureFactory* TextureFactory = NewObject<UTextureFactory>();
		TextureFactory->AddToRoot();
		TextureFactory->SuppressImportOverwriteDialog();

		const uint8* ImageData = Data.GetData();
		Texture2D = Cast<T>(TextureFactory->FactoryCreateBinary(T::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr,
			*FPaths::GetExtension(AssetName + ".png").ToLower(), ImageData, ImageData + Data.Num(), GWarn));
	}

#if ENGINE_UE5
	Texture2D->SetPlatformData(new FTexturePlatformData());
#else
	Texture2D->PlatformData = new FTexturePlatformData();
#endif

	DeserializeTexture2D(Texture2D, Properties->GetObjectField(TEXT("Properties")));

#if ENGINE_UE5
	FTexturePlatformData* PlatformData = Texture2D->GetPlatformData();
#else
	FTexturePlatformData* PlatformData = Texture2D->PlatformData;
#endif

#if UE4_18_BELOW
	const TArray<TSharedPtr<FJsonValue>>* TextureMipsPtr;
	Properties->TryGetArrayField(TEXT("Mips"), TextureMipsPtr);
#else
	if (const TArray<TSharedPtr<FJsonValue>>* TextureMipsPtr; Properties->TryGetArrayField(TEXT("Mips"), TextureMipsPtr))
#endif
	{
		const auto TextureMips = *TextureMipsPtr;

		if (TextureMips.Num() == 1) {
			Texture2D->MipGenSettings = TMGS_NoMipmaps;
		}
	}

	if (bUseOctetStream) {
		DeserializeTexturePlatformData(Texture2D, Data, *PlatformData, Properties);
	}

	OutTexture2D = Texture2D;

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
bool UTextureImporter::DeserializeTexture2D(UTexture2D* InTexture2D, const TSharedPtr<FJsonObject>& Properties) const {
	if (InTexture2D == nullptr) return false;

	DeserializeTexture(InTexture2D, Properties);

	FString AddressX;
	FString AddressY;
	bool bHasBeenPaintedInEditor;

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

	/* ~~~~~~~~~~~~~ Platform Data ~~~~~~~~~~~~~ */
#if ENGINE_UE5
	FTexturePlatformData* PlatformData = InTexture2D->GetPlatformData();
#else
	FTexturePlatformData* PlatformData = InTexture2D->PlatformData;
#endif
	int SizeX;
	int SizeY;
#if !UE4_18_BELOW
	uint32 PackedData;
#endif
	FString PixelFormat;

	// Pixel format enum
	UEnum* PixelFormatEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPixelFormat"), true);

	if (Properties->TryGetNumberField(TEXT("SizeX"), SizeX)) PlatformData->SizeX = SizeX;
	if (Properties->TryGetNumberField(TEXT("SizeY"), SizeY)) PlatformData->SizeY = SizeY;
#if !UE4_18_BELOW
	if (Properties->TryGetNumberField(TEXT("PackedData"), PackedData)) PlatformData->PackedData = PackedData;
#endif
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

	if (Properties->TryGetNumberField(TEXT("FirstResourceMemMip"), FirstResourceMemMip)) InTexture2D->FirstResourceMemMip = FirstResourceMemMip;
	if (Properties->TryGetNumberField(TEXT("LevelIndex"), LevelIndex)) InTexture2D->LevelIndex = LevelIndex;

	return false;
}

bool UTextureImporter::DeserializeTexture(UTexture* InTexture, const TSharedPtr<FJsonObject>& Properties) const {
	if (InTexture == nullptr) return false;

	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(Properties,
		{
			"ImportedSize",
			"LODBias"
		}), InTexture);

	return false;
}

bool UTextureImporter::DeserializeTexturePlatformData(UTexture* Texture, TArray<uint8>& Data, FTexturePlatformData& TexturePlatformData,
	const TSharedPtr<FJsonObject>& Properties)
{
	const int SizeX = Properties->GetNumberField(TEXT("SizeX"));
	const int SizeY = Properties->GetNumberField(TEXT("SizeY"));
	constexpr int SizeZ = 1;

	FString PixelFormat;
	if (Properties->TryGetStringField(TEXT("PixelFormat"), PixelFormat)) {
		TexturePlatformData.PixelFormat = static_cast<EPixelFormat>(Texture->GetPixelFormatEnum()->GetValueByNameString(PixelFormat));
	}

	int Size = SizeX * SizeY * (TexturePlatformData.PixelFormat == PF_BC6H ? 16 : 4);
	if (TexturePlatformData.PixelFormat == PF_B8G8R8A8 || TexturePlatformData.PixelFormat == PF_FloatRGBA || TexturePlatformData.PixelFormat == PF_G16) Size = Data.Num();
	uint8* DecompressedData = static_cast<uint8*>(FMemory::Malloc(Size));

	if (bUseOctetStream) {
		GetDecompressedTextureData(Data.GetData(), DecompressedData, SizeX, SizeY, SizeZ, Size, TexturePlatformData.PixelFormat);
	}
	else {
		DecompressedData = Data.GetData();
	}

	ETextureSourceFormat Format = TSF_BGRA8;
	if (Texture->CompressionSettings == TC_HDR) Format = TSF_RGBA16F;
#if UE4_18_BELOW
	if (TexturePlatformData.PixelFormat == PF_G16) Format = TSF_G8;
#else
	if (TexturePlatformData.PixelFormat == PF_G16) Format = TSF_G16;
#endif
	Texture->Source.Init(SizeX, SizeY, 1, 1, Format);
	uint8_t* Dest = Texture->Source.LockMip(0);
	FMemory::Memcpy(Dest, DecompressedData, Size);
	Texture->Source.UnlockMip(0);

	if (Texture->LODGroup == 255) {
		Texture->LODGroup = TEXTUREGROUP_World;
	}

	Texture->UpdateResource();

	if (Texture && Texture->IsValidLowLevel() && Texture != nullptr) {
		return true;
	}

	return false;
}

void UTextureImporter::GetDecompressedTextureData(uint8* Data, uint8*& OutData, const int SizeX, const int SizeY, const int SizeZ, const int TotalSize, const EPixelFormat Format) {
	/* NOTE: Not all formats are supported, feel free to add if needed. Formats may need other dependencies. */
	switch (Format) {
	case PF_BC7: {
		detexTexture Texture;
		Texture.data = Data;
		Texture.format = DETEX_TEXTURE_FORMAT_BPTC;
		Texture.width = SizeX;
		Texture.height = SizeY;
		Texture.width_in_blocks = SizeX / 4;
		Texture.height_in_blocks = SizeY / 4;

		detexDecompressTextureLinear(&Texture, OutData, DETEX_PIXEL_FORMAT_BGRA8);
	}
				 break;

	case PF_BC6H: {
		detexTexture Texture;
		Texture.data = Data;
		Texture.format = DETEX_TEXTURE_FORMAT_BPTC_FLOAT;
		Texture.width = SizeX;
		Texture.height = SizeY;
		Texture.width_in_blocks = SizeX / 4;
		Texture.height_in_blocks = SizeY / 4;

		detexDecompressTextureLinear(&Texture, OutData, DETEX_PIXEL_FORMAT_BGRA8);
	}
				  break;

	case PF_DXT5: {
		detexTexture Texture;
		{
			Texture.data = Data;
			Texture.format = DETEX_TEXTURE_FORMAT_BC3;
			Texture.width = SizeX;
			Texture.height = SizeY;
			Texture.width_in_blocks = SizeX / 4;
			Texture.height_in_blocks = SizeY / 4;
		}

		detexDecompressTextureLinear(&Texture, OutData, DETEX_PIXEL_FORMAT_BGRA8);
	}
				  break;

				  /* Gray/Grey, not Green, typically actually uses a red format with replication of R to RGB*/
	case PF_G8: {
		const uint8* s = Data;
		uint8* d = OutData;

		for (int i = 0; i < SizeX * SizeY; i++) {
			const uint8 b = *s++;
			*d++ = b;
			*d++ = b;
			*d++ = b;
			*d++ = 255;
		}
	}
				break;

				/*
				 * FloatRGBA: 16F
				 * G16: Gray/Grey like G8
				*/
	case PF_B8G8R8A8:
	case PF_FloatRGBA:
	case PF_G16: {
		FMemory::Memcpy(OutData, Data, TotalSize);
	}
				 break;

	default: {
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
		default: FourCC = 0;
		}

		Header.setFourCC(FourCC);
		Header.setWidth(SizeX);
		Header.setHeight(SizeY);
		Header.setDepth(SizeZ);
		Header.setNormalFlag(Format == PF_BC5);
		DecodeDDS(Data, SizeX, SizeY, SizeZ, Header, Image);

		FMemory::Memcpy(OutData, Image.pixels(), TotalSize);
	}
			 break;
	}
}
