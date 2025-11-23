/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Settings/JsonAsAssetSettings.h"

#include "Utilities/EngineUtilities.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "JsonAsAsset"

UJsonAsAssetSettings::UJsonAsAssetSettings() :
	/* Default initializers */
	bEnableExperiments(false),
	bEnableCloudServer(false)
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("JsonAsAsset");
}

FText UJsonAsAssetSettings::GetSectionText() const {
	return LOCTEXT("SettingsDisplayName", "JsonAsAsset");
}

bool UJsonAsAssetSettings::EnsureExportDirectoryIsValid(UJsonAsAssetSettings* Settings) {
	const FString ExportDirectoryPath = Settings->ExportDirectory.Path;

	if (ExportDirectoryPath.IsEmpty()) {
		ReadAppData();

		if (ExportDirectoryPath.IsEmpty()) {
			return false;
		}
	}

	/* Invalid Export Directory */
	if (ExportDirectoryPath.Contains("\\")) {
		/* Fix up export directory */
		Settings->ExportDirectory.Path = ExportDirectoryPath.Replace(TEXT("\\"), TEXT("/"));

		SavePluginConfig(Settings);
	}

	return true;
}

void UJsonAsAssetSettings::ReadAppData() {
	UJsonAsAssetSettings* PluginSettings = GetMutableDefault<UJsonAsAssetSettings>();

	/* Get the path to AppData\Roaming */
#if UE4_18_BELOW
	FString AppDataPath;
	TCHAR Buffer[MAX_PATH] = { 0 };
	FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"), Buffer, MAX_PATH);

	if (Buffer[0] != '\0'){
		AppDataPath = FString(Buffer);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("APPDATA environment variable not found."));
	}
#else
	FString AppDataPath = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
#endif
	AppDataPath = FPaths::Combine(AppDataPath, TEXT("FModel/AppSettings.json"));

	FString JsonContent;

	if (FFileHelper::LoadFileToString(JsonContent, *AppDataPath)) {
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
		TSharedPtr<FJsonObject> JsonObject;

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) {
			/* Load the PropertiesDirectory and GameDirectory */
			PluginSettings->ExportDirectory.Path = JsonObject->GetStringField(TEXT("PropertiesDirectory")).Replace(TEXT("\\"), TEXT("/"));
		}
	}

	SavePluginConfig(PluginSettings);
}

#undef LOCTEXT_NAMESPACE