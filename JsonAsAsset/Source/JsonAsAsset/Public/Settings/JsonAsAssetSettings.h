#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"
#include "IDetailCustomization.h"
#include "JsonAsAssetSettings.generated.h"

// Grabbed from (https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/Versions/EGame.cs)
UENUM()
enum EParseVersion {
	GAME_UE4_0,
	GAME_UE4_1,
	GAME_UE4_2,
	GAME_UE4_3,
	GAME_UE4_4,
	GAME_UE4_5,
	GAME_ArkSurvivalEvolved,
	GAME_UE4_6,
	GAME_UE4_7,
	GAME_UE4_8,
	GAME_UE4_9,
	GAME_UE4_10,
	GAME_SeaOfThieves,
	GAME_UE4_11,
	GAME_GearsOfWar4,
	GAME_UE4_12,
	GAME_UE4_13,
	GAME_StateOfDecay2,
	GAME_UE4_14,
	GAME_TEKKEN7,
	GAME_UE4_15,
	GAME_UE4_16,
	GAME_PlayerUnknownsBattlegrounds,
	GAME_TrainSimWorld2020,
	GAME_UE4_17,
	GAME_LifeIsStrange2,
	GAME_AWayOut,
	GAME_UE4_18,
	GAME_KingdomHearts3,
	GAME_FinalFantasy7Remake,
	GAME_AceCombat7,
	GAME_UE4_19,
	GAME_Paragon,
	GAME_UE4_20,
	GAME_Borderlands3,
	GAME_UE4_21,
	GAME_StarWarsJediFallenOrder,
	GAME_UE4_22,
	GAME_UE4_23,
	GAME_ApexLegendsMobile,
	GAME_UE4_24,
	GAME_UE4_25,
	GAME_RogueCompany,
	GAME_DeadIsland2,
	GAME_KenaBridgeofSpirits,
	GAME_UE4_25_Plus,
	GAME_UE4_26,
	GAME_GTATheTrilogyDefinitiveEdition,
	GAME_ReadyOrNot,
	GAME_Valorant,
	GAME_TowerOfFantasy,
	GAME_Dauntless,
	GAME_TheDivisionResurgence,
	GAME_UE4_27,
	GAME_Splitgate,
	GAME_HYENAS,
	GAME_HogwartsLegacy,
	GAME_UE4_28,

	GAME_UE4_LATEST,

	GAME_UE5_0,
	GAME_MeetYourMaker,
	GAME_UE5_1,
	GAME_UE5_2,
	GAME_UE5_3,

	GAME_UE5_LATEST
};

USTRUCT()
struct FParseKey {
	GENERATED_BODY()

	FParseKey() {
	}

	FParseKey(FString NewGUID, FString NewKey) {
		Value = NewKey;
		Guid = NewGUID;
	}

	TArray<uint8> Key;

	// Must start with 0x
	UPROPERTY(EditAnywhere, Config, Category = "Key")
		FString Value;

	UPROPERTY(EditAnywhere, Config, Category = "Key")
		FString Guid;
};

// Buttons in plugin settings
class FJsonAsAssetSettingsDetails : public IDetailCustomization {
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

/* Settings for materials */
USTRUCT()
struct FJMaterialImportSettings
{
	GENERATED_BODY()
public:
	/* Constructor to initialize default values */
	FJMaterialImportSettings()
		: bSkipResultNodeConnection(false)
	{}

	/**
	 * Prevents a known error during Material asset import/download:
	 * "Material expression called Compiler->TextureParameter() without implementing UMaterialExpression::GetReferencedTexture properly."
	 *
	 * To avoid this issue, this option skips connecting the inputs to the material's primary result node, potentially fixing the error.
	 *
	 * Usage:
	 *  - If enabled, import the material, save your project, restart the editor, and then re-import the material.
	 *  - Alternatively, manually connect the inputs to the main result node.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Material Import Settings")
		bool bSkipResultNodeConnection;
};

/* Settings for animation blueprints */
USTRUCT()
struct FJAnimationBlueprintImportSettings
{
	GENERATED_BODY()
public:
	/* Constructor to initialize default values */
	FJAnimationBlueprintImportSettings()
		: bShowAllNodeKeysAsComment(false)
	{}

	UPROPERTY(EditAnywhere, Config, AdvancedDisplay, Category = "Animation Blueprint Settings")
		bool bShowAllNodeKeysAsComment;
};

/* Settings for textures */
USTRUCT()
struct FJTextureImportSettings
{
	GENERATED_BODY()
public:
	/* Constructor to initialize default values */
	FJTextureImportSettings()
		: bDownloadExistingTextures(false)
	{}

	/**
	 * Enables re-downloading of textures even if they already exist in the Unreal Engine project.
	 *
	 * Use Case:
	 * This option is useful when you need to re-import textures that have been updated in a newer version of your Cloud build.
	 */
	UPROPERTY(EditAnywhere, Config, AdvancedDisplay, Category = "Texture Import Settings")
		bool bDownloadExistingTextures;
};

/* Settings for sounds */
USTRUCT()
struct FJSoundImportSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Config, Category = "Sound Import Settings", meta = (DisplayName = "Audio File Name Extension"))
		FString AudioFileExtension = "ogg";
};

/* Settings for pose assets */
USTRUCT()
struct FJPoseAssetImportSettings
{
	GENERATED_BODY()
};

USTRUCT()
struct FJPathRedirector
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Config, Category = "Path Redirector")
		FString Source;

	UPROPERTY(EditAnywhere, Config, Category = "Path Redirector")
		FString Target;
};

USTRUCT()
struct FJAssetSettings
{
	GENERATED_BODY()
public:
	/* Constructor to initialize default values */
	FJAssetSettings()
		: bSavePackagesOnImport(false)
	{
		MaterialImportSettings = FJMaterialImportSettings();
		SoundImportSettings = FJSoundImportSettings();
		TextureImportSettings = FJTextureImportSettings();
		AnimationBlueprintImportSettings = FJAnimationBlueprintImportSettings();
		PoseAssetImportSettings = FJPoseAssetImportSettings();
	}

	UPROPERTY(EditAnywhere, Config, Category = AssetSettings)
	FJTextureImportSettings TextureImportSettings;

	UPROPERTY(EditAnywhere, Config, Category = AssetSettings)
	FJMaterialImportSettings MaterialImportSettings;

	UPROPERTY(EditAnywhere, Config, Category = AssetSettings)
	FJSoundImportSettings SoundImportSettings;

	/* UPROPERTY(EditAnywhere, Config, Category = AssetSettings) */
	FJPoseAssetImportSettings PoseAssetImportSettings;

	UPROPERTY(EditAnywhere, Config, Category = AssetSettings)
	FJAnimationBlueprintImportSettings AnimationBlueprintImportSettings;

	/* Game's Project Name (Set by Cloud) */
	UPROPERTY(EditAnywhere, Config, Category = AssetSettings)
	FString GameName;

	/* If imported assets are from UE5. (Set by Cloud) */
	UPROPERTY(Config)
	bool bUE5Target;

	UPROPERTY(EditAnywhere, Config, Category = AssetSettings, meta = (DisplayName = "Save Assets On Import"))
	bool bSavePackagesOnImport;

	UPROPERTY(EditAnywhere, Config, Category = AssetSettings)
	TArray<FJPathRedirector> PathRedirectors;
};

USTRUCT()
struct FJVersioningSettings
{
	GENERATED_BODY()
public:
	/* Disable checking for newer updates of JsonAsAsset. */
	UPROPERTY(EditAnywhere, Config, Category = VersioningSettings)
		bool bDisable = false;

	/* Enable update reminders for newer updates of JsonAsAsset. */
	UPROPERTY(EditAnywhere, Config, Category = VersioningSettings)
		bool bEnableReminders = true;
};


// A editor plugin to allow JSON files from FModel to a asset in the Content Browser
UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig)
class UJsonAsAssetSettings : public UDeveloperSettings {
	GENERATED_BODY()

public:
	UJsonAsAssetSettings();

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif

	/**
	 * Specifies the directory path for exported assets.
	 * (e.g. Output/Exports)
	 */
	UPROPERTY(EditAnywhere, Config, Category = Configuration)
	FDirectoryPath ExportDirectory;

	UPROPERTY(EditAnywhere, Config, Category = Configuration)
	FJVersioningSettings Versioning;

	UPROPERTY(EditAnywhere, Config, Category = Configuration)
	FJAssetSettings AssetSettings;

	/* Enables experimental/developing features of JsonAsAsset. Features may not work as intended. */
	UPROPERTY(EditAnywhere, Config, Category = Configuration, AdvancedDisplay)
	bool bEnableExperiments;

	/**
	 * Retrieves assets from an API and imports references directly into your project.
	 */
	UPROPERTY(EditAnywhere, Config, Category = Cloud, DisplayName = "Enable Cloud")
	bool bEnableCloudServer;

	static bool EnsureExportDirectoryIsValid(UJsonAsAssetSettings* Settings);

	static bool IsSetup(UJsonAsAssetSettings* Settings, TArray<FString>& Reasons) {
		const bool IsExportDirectoryValid = EnsureExportDirectoryIsValid(Settings);

		if (!IsExportDirectoryValid) {
			Reasons.Add("Export Directory is missing");
		}

		return IsExportDirectoryValid;
	}

	static bool IsSetup(UJsonAsAssetSettings* Settings) {
		if (Settings == nullptr) return false;

		TArray<FString> Params;
		return IsSetup(Settings, Params);
	}

	static void ReadAppData();

	/**
	 * DO NOT MODIFY UNLESS YOU KNOW WHAT YOU'RE DOING.
	 */
	UPROPERTY(EditAnywhere, Config, Category = Cloud, DisplayName = "Use Custom Cloud URL", meta = (EditCondition = "bCustomCloudServer"), AdvancedDisplay)
	FString CustomCloudURL = "http://localhost:1500";

	UPROPERTY(EditAnywhere, Category = Cloud, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bCustomCloudServer : 1;


	/**
	* When importing/downloading any asset type using JsonAsAsset,
	* save the package (asset) after finalization.
	*
	* NOTE: This is recommended to be turned off, as this may override
	*		your own assets, causing irreversible changes.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Behavior")
		bool bAllowPackageSaving;

	/**
	* When importing/downloading the asset type Material, a error may occur
	* | Material expression called Compiler->TextureParameter() without implementing UMaterialExpression::GetReferencedTexture properly
	*
	* To counter that error, we skip connecting the inputs to the main result
	* node in the material. If you do use this, import a material, save everything,
	* restart, and re-import the material without any problems with this turned off/on.
	*			  (or you could just connect them yourself)
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Behavior|Material")
		bool bSkipResultNodeConnection;

	/**
	* Fetches assets from a local service and automatically imports
	* them into your project, without having them locally
	*
	* NOTE: Please set all the settings correctly to properly start
	*		Local Fetch, read more at the README.md file.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch")
		bool bEnableLocalFetch;

	/**
	* Paks folder location where all the assets are
	* (Content/Paks)
	*
	* NOTE: Please use the file selector, do not manually paste it
	*		or replace "\" with "/"
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch|Configuration", meta = (EditCondition = "bEnableLocalFetch"))
		FDirectoryPath ArchiveDirectory;

	// UE Version for the Unreal Engine Game (same as FModel's UE Verisons property)
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch|Configuration", meta = (EditCondition = "bEnableLocalFetch"))
		TEnumAsByte<EParseVersion> UnrealVersion;

	// Mappings file
	// NOTE: Please use the file selector, do not manually paste it 
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch|Configuration", meta = (EditCondition = "bEnableLocalFetch", FilePathFilter = "usmap", RelativeToGameDir))
		FFilePath MappingFilePath;

	// High res textures
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch Encryption|Behavior", meta = (EditCondition = "bEnableLocalFetch"))
		bool bUseContentBuilds;

	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch Encryption|Behavior", meta = (EditCondition = "bEnableLocalFetch"))
		bool bDownloadExistingTextures;

	// Main key for archives
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch Encryption", meta = (EditCondition = "bEnableLocalFetch", DisplayName = "Archive Key"))
		FString ArchiveKey;

	// AES Keys
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch Encryption", meta = (EditCondition = "bEnableLocalFetch", DisplayName = "Dynamic Keys"))
		TArray<FParseKey> DynamicKeys;

	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch Director", meta = (EditConditionHides, EditCondition = "bEnableLocalFetch"))
		bool bHideConsole;

	// Enables the option to change the api's URL
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch Director", meta = (EditConditionHides, EditCondition = "bEnableLocalFetch"))
		bool bChangeURL;

	// "localhost" is default
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch Director|REST API", meta = (EditConditionHides, EditCondition = "bChangeURL && bEnableLocalFetch", DisplayName = "Local URL"))
		FString Url = "http://localhost:1500";
};
