#include "Importers/TextureImporter.h"

#include "detex.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureCube.h"
#include "Factories/TextureRenderTargetFactoryNew.h"
#include "nvimage/DirectDrawSurface.h"
#include "nvimage/Image.h"
#include "Utilities/MathUtilities.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Utilities/TextureDecode/TextureNVTT.h"
#include "UObject/UnrealType.h"
#include "Engine/TextureDefines.h"

bool UTextureImporter::ImportTexture2D(UTexture*& OutTexture2D, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const {
	const TSharedPtr<FJsonObject> SubObjectProperties = Properties->GetObjectField("Properties");

	// NEW: .bin support
	UTexture2D* Texture2D = NewObject<UTexture2D>(OutermostPkg, UTexture2D::StaticClass(), *FileName, RF_Standalone | RF_Public);
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

// Handle UTexture
bool UTextureImporter::ImportTexture_Data(UTexture* InTexture, const TSharedPtr<FJsonObject>& Properties) const {
	if (InTexture == nullptr) return false;

	double AdjustBrightness;
	double AdjustBrightnessCurve;
	double AdjustHue;
	double AdjustMaxAlpha;
	double AdjustMinAlpha;
	double AdjustRGBCurve;
	double AdjustSaturation;
	double AdjustVibrance;

	// Adjust parameters
	if (Properties->TryGetNumberField("AdjustBrightness", AdjustBrightness)) InTexture->AdjustBrightness = AdjustBrightness;
	if (Properties->TryGetNumberField("AdjustBrightnessCurve", AdjustBrightnessCurve)) InTexture->AdjustBrightnessCurve = AdjustBrightnessCurve;
	if (Properties->TryGetNumberField("AdjustHue", AdjustHue)) InTexture->AdjustHue = AdjustHue;
	if (Properties->TryGetNumberField("AdjustMaxAlpha", AdjustMaxAlpha)) InTexture->AdjustMaxAlpha = AdjustMaxAlpha;
	if (Properties->TryGetNumberField("AdjustMinAlpha", AdjustMinAlpha)) InTexture->AdjustMinAlpha = AdjustMinAlpha;
	if (Properties->TryGetNumberField("AdjustRGBCurve", AdjustRGBCurve)) InTexture->AdjustRGBCurve = AdjustRGBCurve;
	if (Properties->TryGetNumberField("AdjustSaturation", AdjustSaturation)) InTexture->AdjustSaturation = AdjustSaturation;
	if (Properties->TryGetNumberField("AdjustVibrance", AdjustVibrance)) InTexture->AdjustVibrance = AdjustVibrance;

	const TSharedPtr<FJsonObject>* AlphaCoverageThresholds;
	if (Properties->TryGetObjectField("AlphaCoverageThresholds", AlphaCoverageThresholds))
		InTexture->AlphaCoverageThresholds = FMathUtilities::ObjectToVector(AlphaCoverageThresholds->Get());

	bool bChromaKeyTexture;
	bool bFlipGreenChannel;
	bool bNoTiling;
	bool bPreserveBorder;
	bool bUseLegacyGamma;

	if (Properties->TryGetBoolField("bChromaKeyTexture", bChromaKeyTexture)) InTexture->bChromaKeyTexture = bChromaKeyTexture;
	if (Properties->TryGetBoolField("bFlipGreenChannel", bFlipGreenChannel)) InTexture->bFlipGreenChannel = bFlipGreenChannel;
	if (Properties->TryGetBoolField("bNoTiling", bNoTiling)) InTexture->bNoTiling = bNoTiling;
	if (Properties->TryGetBoolField("bPreserveBorder", bPreserveBorder)) InTexture->bPreserveBorder = bPreserveBorder;
	if (Properties->TryGetBoolField("bUseLegacyGamma", bUseLegacyGamma)) InTexture->bUseLegacyGamma = bUseLegacyGamma;

	const TSharedPtr<FJsonObject>* ChromaKeyColor;
	double ChromaKeyThreshold;
	if (Properties->TryGetObjectField("ChromaKeyColor", ChromaKeyColor)) InTexture->ChromaKeyColor = FMathUtilities::ObjectToColor(ChromaKeyColor->Get());
	if (Properties->TryGetNumberField("ChromaKeyThreshold", ChromaKeyThreshold)) InTexture->ChromaKeyThreshold = ChromaKeyThreshold;

	double CompositePower;
	FString CompositeTextureMode;
	if (Properties->TryGetNumberField("CompositePower", CompositePower)) InTexture->CompositePower = CompositePower;

	// --- Enums in UE4.18 need FindObject<UEnum> ---
	UEnum* CompositeTextureModeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ECompositeTextureMode"), true);
	UEnum* TextureCompressionQualityEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETextureCompressionQuality"), true);
	UEnum* TextureCompressionSettingsEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("TextureCompressionSettings"), true);
	UEnum* TextureFilterEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("TextureFilter"), true);
	UEnum* TextureGroupEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("TextureGroup"), true);
	UEnum* MipGenSettingsEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("TextureMipGenSettings"), true);
	UEnum* PowerOfTwoModeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETexturePowerOfTwoSetting"), true);

	if (Properties->TryGetStringField("CompositeTextureMode", CompositeTextureMode) && CompositeTextureModeEnum)
	{
		int32 Value = CompositeTextureModeEnum->GetIndexByNameString(CompositeTextureMode);
		if (Value != INDEX_NONE)
			InTexture->CompositeTextureMode = static_cast<ECompositeTextureMode>(Value);
	}

	bool CompressionNoAlpha;
	bool CompressionNone;
	FString CompressionQuality;
	FString CompressionSettings;
	bool DeferCompression;
	FString Filter;

	if (Properties->TryGetBoolField("CompressionNoAlpha", CompressionNoAlpha)) InTexture->CompressionNoAlpha = CompressionNoAlpha;
	if (Properties->TryGetBoolField("CompressionNone", CompressionNone)) InTexture->CompressionNone = CompressionNone;

	if (Properties->TryGetStringField("CompressionQuality", CompressionQuality) && TextureCompressionQualityEnum)
	{
		int32 Value = TextureCompressionQualityEnum->GetIndexByNameString(CompressionQuality);
		if (Value != INDEX_NONE)
			InTexture->CompressionQuality = static_cast<ETextureCompressionQuality>(Value);
	}

	if (Properties->TryGetStringField("CompressionSettings", CompressionSettings) && TextureCompressionSettingsEnum)
	{
		int32 Value = TextureCompressionSettingsEnum->GetIndexByNameString(CompressionSettings);
		if (Value != INDEX_NONE)
			InTexture->CompressionSettings = static_cast<TextureCompressionSettings>(Value);
	}

	if (Properties->TryGetBoolField("DeferCompression", DeferCompression)) InTexture->DeferCompression = DeferCompression;

	if (Properties->TryGetStringField("Filter", Filter) && TextureFilterEnum)
	{
		int32 Value = TextureFilterEnum->GetIndexByNameString(Filter);
		if (Value != INDEX_NONE)
			InTexture->Filter = static_cast<TextureFilter>(Value);
	}

	FString LODGroup;
	if (Properties->TryGetStringField("LODGroup", LODGroup) && TextureGroupEnum)
	{
		int32 Value = TextureGroupEnum->GetIndexByNameString(LODGroup);
		if (Value != INDEX_NONE)
			InTexture->LODGroup = static_cast<TextureGroup>(Value);
	}

	int MaxTextureSize;
	FString MipGenSettings;
	if (Properties->TryGetNumberField("MaxTextureSize", MaxTextureSize)) InTexture->MaxTextureSize = MaxTextureSize;

	if (Properties->TryGetStringField("MipGenSettings", MipGenSettings) && MipGenSettingsEnum)
	{
		int32 Value = MipGenSettingsEnum->GetIndexByNameString(MipGenSettings);
		if (Value != INDEX_NONE)
			InTexture->MipGenSettings = static_cast<TextureMipGenSettings>(Value);
	}

	const TSharedPtr<FJsonObject>* PaddingColor;
	FString PowerOfTwoMode;
	if (Properties->TryGetObjectField("PaddingColor", PaddingColor)) InTexture->PaddingColor = FMathUtilities::ObjectToColor(PaddingColor->Get());

	if (Properties->TryGetStringField("PowerOfTwoMode", PowerOfTwoMode) && PowerOfTwoModeEnum)
	{
		int32 Value = PowerOfTwoModeEnum->GetIndexByNameString(PowerOfTwoMode);
		if (Value != INDEX_NONE)
			InTexture->PowerOfTwoMode = static_cast<ETexturePowerOfTwoSetting::Type>(Value);
	}

	bool SRGB;
	if (Properties->TryGetBoolField("SRGB", SRGB)) InTexture->SRGB = SRGB;

	FString LightingGuid;
	if (Properties->TryGetStringField("LightingGuid", LightingGuid)) {
		FGuid GUID;
		FGuid::Parse(LightingGuid, GUID);
		InTexture->SetLightingGuid();
	}

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
