// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Constructor/Importer.h"

#include "Settings/JsonAsAssetSettings.h"

#include "CoreMinimal.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistryModule.h"
#include "Templates/SharedPointer.h"
#include "FileHelpers.h"
#include "Json.h"

// ----> Importers
#include "Importers/CurveFloatImporter.h"
#include "Importers/CurveVectorImporter.h"
#include "Importers/CurveLinearColorImporter.h"
#include "Importers/CurveLinearColorAtlasImporter.h"
#include "Importers/DataTableImporter.h"
#include "Importers/SoundAttenuationImporter.h"
#include "Importers/SoundConcurrencyImporter.h"
#include "Importers/ReverbEffectImporter.h"
#include "Importers/SubsurfaceProfileImporter.h"
#include "Importers/AnimationBaseImporter.h"
#include "Importers/LandscapeGrassTypeImporter.h"
#include "Importers/MaterialFunctionImporter.h"
#include "Importers/MaterialImporter.h"
#include "Importers/MaterialParameterCollectionImporter.h"
#include "Importers/MaterialInstanceConstantImporter.h"
#include "Importers/PhysicalMaterialImporter.h"
#include "Importers/TextureImporter.h"
#include "Importers/Types/Blueprint/BlueprintGeneratedClassImporter.h"
#include "Importers/Types/Blueprint/WidgetBlueprintGeneratedClassImporter.h"
// <---- Importers

#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "Engine/DataAsset.h"
#include "Misc/FileHelper.h"

/* Slate Icons */
#include "Styling/SlateIconFinder.h"

#include "Importers/Types/DataAssetImporter.h"

/* Templated Class */
#include "Importers/Constructor/TemplatedImporter.h"

/* ~~~~~~~~~~~~~ Templated Engine Classes ~~~~~~~~~~~~~ */
#include "Logging/MessageLog.h"
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define LOCTEXT_NAMESPACE "IImporter"

/* Importer Constructor */
IImporter::IImporter(const FString& AssetName, const FString& FilePath,
	const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package,
	UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects,
	UClass* AssetClass)
	: USerializerContainer(Package, OutermostPkg), AllJsonObjects(AllJsonObjects), JsonObject(JsonObject),
	FilePath(FilePath), AssetClass(AssetClass), AssetName(AssetName),
	ParentObject(nullptr)
{
	/* Create Properties field if it doesn't exist */
	if (!JsonObject->HasField(TEXT("Properties"))) {
		JsonObject->SetObjectField(TEXT("Properties"), TSharedPtr<FJsonObject>());
	}

	AssetData = JsonObject->GetObjectField(TEXT("Properties"));

	/* Move asset properties defined outside "Properties" and move it inside */
	for (const auto& Pair : JsonObject->Values) {
		const FString& PropertyName = Pair.Key;

		if (!PropertyName.Equals(TEXT("Type")) &&
			!PropertyName.Equals(TEXT("Name")) &&
			!PropertyName.Equals(TEXT("Class")) &&
			!PropertyName.Equals(TEXT("Flags")) &&
			!PropertyName.Equals(TEXT("Properties"))
			) {
			AssetData->SetField(PropertyName, Pair.Value);
		}
	}
}

bool IImporter::ReadExportsAndImport(TArray<TSharedPtr<FJsonValue>> Exports, FString File, const bool bHideNotifications) {
	for (const TSharedPtr<FJsonValue>& ExportPtr : Exports) {
		TSharedPtr<FJsonObject> DataObject = ExportPtr->AsObject();

		FString Type = DataObject->GetStringField("Type");
		FString Name = DataObject->GetStringField("Name");

		/* BlueprintGeneratedClass is postfixed with _C */
		if (Type.Contains("BlueprintGeneratedClass")) {
			Name.Split("_C", &Name, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		}
#if UE5_6_BEYOND
		UClass* Class = FindFirstObject<UClass>(*Type);
#else
		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *Type);
#endif

		if (Class == nullptr) continue;

		bool InheritsDataAsset = Class->IsChildOf(UDataAsset::StaticClass());
		if (!CanImport(Type)) continue;

		/* Convert from relative path to full path */
		if (FPaths::IsRelative(File)) File = FPaths::ConvertRelativePathToFull(File);

		//RedirectPath(File);

		UPackage* LocalOutermostPkg;
		UPackage* LocalPackage = FAssetUtilities::CreateAssetPackage(Name, File, LocalOutermostPkg);

		/* Importer ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
		IImporter* Importer = nullptr;

		/* Try to find the importer using a factory delegate */
		if (const FImporterFactoryDelegate* Factory = FindFactoryForAssetType(Type)) {
			Importer = (*Factory)(Name, File, DataObject, LocalPackage, LocalOutermostPkg, Exports, Class);
		}

		/* If it inherits DataAsset, use the data asset importer */
		if (Importer == nullptr && InheritsDataAsset) {
			Importer = new IDataAssetImporter(Name, File, DataObject, LocalPackage, LocalOutermostPkg, Exports, Class);
		}

		/*
		 * By default, (with no existing importer) use the templated importer with the asset class.
		 */
		if (Importer == nullptr) {
			Importer = new ITemplatedImporter<UObject>(
				Name, File, DataObject, LocalPackage, LocalOutermostPkg, Exports, Class
				);
		}

		/* Import the asset ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
		bool Successful = false; {
			try {
				Successful = Importer->Import();
			}
			catch (const char* Exception) {
				UE_LOG(LogJson, Error, TEXT("Importer exception: %s"), *FString(Exception));
			}
		}

		if (bHideNotifications) {
			return Successful;
		}

		if (Successful) {
			UE_LOG(LogJson, Log, TEXT("Successfully imported \"%s\" as \"%s\""), *Name, *Type);

			if (!(Type == "AnimSequence" || Type == "AnimMontage"))
				Importer->SavePackage();

			/* Import Successful Notification */
			AppendNotification(
				FText::FromString("Imported Type: " + Type),
				FText::FromString(Name),
				2.0f,
				FSlateIconFinder::FindCustomIconBrushForClass(FindObject<UClass>(nullptr, *("/Script/Engine." + Type)), TEXT("ClassThumbnail")),
				SNotificationItem::CS_Success,
				false,
				350.0f
			);

			FMessageLog MessageLogger = FMessageLog(FName("JsonAsAsset"));

			MessageLogger.Message(EMessageSeverity::Info, FText::FromString("Imported Asset: " + Name + " (" + Type + ")"));
		}
		else {
			/* Import Failed Notification */
			AppendNotification(
				FText::FromString("Import Failed: " + Type),
				FText::FromString(Name),
				2.0f,
				FSlateIconFinder::FindCustomIconBrushForClass(FindObject<UClass>(nullptr, *("/Script/Engine." + Type)), TEXT("ClassThumbnail")),
				SNotificationItem::CS_Fail,
				false,
				350.0f
			);
		}
	}

	return true;
}

bool IImporter::HandleAssetCreation(UObject* Asset) const {
	FAssetRegistryModule::AssetCreated(Asset);
	if (!Asset->MarkPackageDirty()) return false;
	Package->SetDirtyFlag(true);
	Asset->PostEditChange();
	Asset->AddToRoot();
	Package->FullyLoad();

	// Browse to newly added Asset
	const TArray<FAssetData>& Assets = { Asset };
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(Assets);

	return true;
}

template TObjectPtr<UObject> IImporter::DownloadWrapper<UObject>(TObjectPtr<UObject> Obj, FString PropertyClassName, FString AssetName, FString PackagePath);

template <typename T>
TObjectPtr<T> IImporter::DownloadWrapper(TObjectPtr<T> InObject, FString Type, const FString Name, const FString Path) {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	bool bEnableLocalFetch = Settings->bEnableLocalFetch;
	FMessageLog MessageLogger = FMessageLog(FName("JsonAsAsset"));

	if (bEnableLocalFetch && (
		InObject == nullptr ||
			Settings->bDownloadExistingTextures &&
			Type == "Texture2D"
		)
	) {
		const UObject* DefaultObject = T::StaticClass()->ClassDefaultObject;

		if (DefaultObject != nullptr && Path != FString("")) {
			bool bRemoteDownloadStatus = false;
			bool bTriedDownload = false;

			bTriedDownload = FAssetUtilities::ConstructAsset(FSoftObjectPath(Type + "'" + Path + "." + Name + "'").ToString(), Type, InObject, bRemoteDownloadStatus);

			// Notification
			if (bTriedDownload) {
				if (bRemoteDownloadStatus) {
					AppendNotification(
						FText::FromString("Locally Downloaded: " + Type),
						FText::FromString(Name),
						2.0f,
						FSlateIconFinder::FindCustomIconBrushForClass(FindObject<UClass>(nullptr, *("/Script/Engine." + Type)), TEXT("ClassThumbnail")),
						SNotificationItem::CS_Success,
						false,
						310.0f
					);

					MessageLogger.Message(EMessageSeverity::Info, FText::FromString("Downloaded asset: " + Name + " (" + Type + ")"));
				} else {
					AppendNotification(
						FText::FromString("Download Failed: " + Type),
						FText::FromString(Name),
						5.0f,
						FSlateIconFinder::FindCustomIconBrushForClass(FindObject<UClass>(nullptr, *("/Script/Engine." + Type)), TEXT("ClassThumbnail")),
						SNotificationItem::CS_Fail,
						false,
						310.0f
					);

					MessageLogger.Error(FText::FromString("Failed to download asset: " + Name + " (" + Type + ")"));
				}
			}
		}
	}

	return InObject;
}

/*template void IImporter::LoadObject<UMaterialInterface>(const TSharedPtr<FJsonObject>*, TObjectPtr<UMaterialInterface>&);
template void IImporter::LoadObject<USubsurfaceProfile>(const TSharedPtr<FJsonObject>*, TObjectPtr<USubsurfaceProfile>&);
template void IImporter::LoadObject<UTexture>(const TSharedPtr<FJsonObject>*, TObjectPtr<UTexture>&);
template void IImporter::LoadObject<UMaterialParameterCollection>(const TSharedPtr<FJsonObject>*, TObjectPtr<UMaterialParameterCollection>&);
template void IImporter::LoadObject<UAnimSequence>(const TSharedPtr<FJsonObject>*, TObjectPtr<UAnimSequence>&);
template void IImporter::LoadObject<USoundWave>(const TSharedPtr<FJsonObject>*, TObjectPtr<USoundWave>&);
template void IImporter::LoadObject<UObject>(const TSharedPtr<FJsonObject>*, TObjectPtr<UObject>&);
template void IImporter::LoadObject<UMaterialFunctionInterface>(const TSharedPtr<FJsonObject>*, TObjectPtr<UMaterialFunctionInterface>&);
template void IImporter::LoadObject<USoundNode>(const TSharedPtr<FJsonObject>*, TObjectPtr<USoundNode>&);*/

template <typename T>
void IImporter::LoadObject(const TSharedPtr<FJsonObject>* PackageIndex, TObjectPtr<T>& Object) {
	FString ObjectType, ObjectName, ObjectPath, Outer;
	ParsePackageIndex(PackageIndex, ObjectType, ObjectName, ObjectPath, Outer);

#pragma warning( push )
#pragma warning( disable : 4101) // Hide LoadObject Fail
	/* Try to load object using the object path and the object name combined */
	TObjectPtr<T> LoadedObject = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *(ObjectPath + "." + ObjectName)));

	/* Material Expression case */
	if (!LoadedObject && ObjectName.Contains("MaterialExpression")) {
		FString SplitObjectName;
		ObjectPath.Split("/", nullptr, &SplitObjectName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		LoadedObject = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *(ObjectPath + "." + SplitObjectName + ":" + ObjectName)));
	}
#pragma warning( pop )

	Object = LoadedObject;

	/* If object is still null, send off to Cloud to download */
	if (!Object) {
		Object = DownloadWrapper(LoadedObject, ObjectType, ObjectName, ObjectPath);
	}
}

//template TArray<TObjectPtr<UCurveLinearColor>> IImporter::LoadObject<UCurveLinearColor>(const TArray<TSharedPtr<FJsonValue>>&, TArray<TObjectPtr<UCurveLinearColor>>);

template <typename T>
TArray<TObjectPtr<T>> IImporter::LoadObject(const TArray<TSharedPtr<FJsonValue>>& PackageArray, TArray<TObjectPtr<T>> Array) {
	for (const TSharedPtr<FJsonValue> ArrayElement : PackageArray) {
		const TSharedPtr<FJsonObject> Ptr = ArrayElement->AsObject();

		FString Type;
		FString Name;
		Ptr->GetStringField("ObjectName").Split("'", &Type, &Name);
		FString Path;
		Ptr->GetStringField("ObjectPath").Split(".", &Path, nullptr);
		Name = Name.Replace(TEXT("'"), TEXT(""));

		T* Object = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *(Path + "." + Name)));
		Array.Add(DownloadWrapper(Object, Type, Name, Path));
	}

	return Array;
}

void IImporter::ImportReference(const FString& File) {
	/* ----  Parse JSON into UE JSON Reader ---- */
	FString ContentBefore;
	FFileHelper::LoadFileToString(ContentBefore, *File);

	FString Content = FString(TEXT("{\"data\": "));
	Content.Append(ContentBefore);
	Content.Append(FString("}"));

	TSharedPtr<FJsonObject> JsonParsed;
	const TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Content);
	/* ---------------------------------------- */

	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {
		const TArray<TSharedPtr<FJsonValue>> DataObjects = JsonParsed->GetArrayField("data");

		ReadExportsAndImport(DataObjects, File);
	}
}

bool IImporter::HandleReference(const FString& GamePath) {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	FString UnSanitizedCodeName;
	FilePath.Split(Settings->ExportDirectory.Path + "/", nullptr, &UnSanitizedCodeName);
	UnSanitizedCodeName.Split("/", &UnSanitizedCodeName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);

	// TODO: As of writing this, I don't know how to add Plugin support
	FString UnSanitizedPath = GamePath.Replace(TEXT("/Game/"), *(UnSanitizedCodeName + "/Content/"));
	UnSanitizedPath = Settings->ExportDirectory.Path + "/" + UnSanitizedPath + ".json";

	FString ContentBefore;
	if (FFileHelper::LoadFileToString(ContentBefore, *UnSanitizedPath)) {
		ImportReference(UnSanitizedPath);

		return true;
	}

	return false;
}

void IImporter::ParsePackageIndex(const TSharedPtr<FJsonObject>* PackageIndex, FString& OutType, FString& OutName, FString& OutPath, FString& OutOuter)
{
	PackageIndex->Get()->GetStringField("ObjectName").Split("'", &OutType, &OutName);
	OutPath = PackageIndex->Get()->GetStringField(TEXT("ObjectPath"));
	OutPath.Split(".", &OutPath, nullptr);

	OutPath = OutPath.Replace(TEXT("Nimbus/Content"), TEXT("/Game"));
	OutPath = OutPath.Replace(TEXT("Engine/Content"), TEXT("/Engine"));
	OutName = OutName.Replace(TEXT("'"), TEXT(""));

	if (OutName.Contains("."))
		OutName.Split(".", nullptr, &OutName);

	if (OutName.Contains("."))
		OutName.Split(".", &OutOuter, &OutName);
}

void IImporter::SavePackage() const {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();
	Package->FullyLoad();

	const FString PackageName = Package->GetName();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	if (Settings->bAllowPackageSaving)
		UPackage::SavePackage(Package, nullptr, RF_Standalone, *PackageFileName, GWarn, nullptr, false, true, SAVE_NoError);
}

bool IImporter::OnAssetCreation(UObject* Asset) const {
	const bool Synced = HandleAssetCreation(Asset);

	if (Synced) {
		SavePackage();
	}

	return Synced;
}

FName IImporter::GetExportNameOfSubobject(const FString& PackageIndex) {
	FString Name;
	PackageIndex.Split("'", nullptr, &Name);
	Name.Split(":", nullptr, &Name);
	Name = Name.Replace(TEXT("'"), TEXT(""));
	return FName(*Name);
}

TArray<TSharedPtr<FJsonValue>> IImporter::FilterExportsByOuter(const FString& Outer) {
	TArray<TSharedPtr<FJsonValue>> ReturnValue = TArray<TSharedPtr<FJsonValue>>();

	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = TSharedPtr<FJsonObject>(Value->AsObject());

		FString ExOuter;
		if (ValueObject->TryGetStringField("Outer", ExOuter) && ExOuter == Outer) 
			ReturnValue.Add(TSharedPtr<FJsonValue>(Value));
	}

	return ReturnValue;
}

TSharedPtr<FJsonValue> IImporter::GetExportByObjectPath(const TSharedPtr<FJsonObject>& Object) {
	const TSharedPtr<FJsonObject> ValueObject = TSharedPtr<FJsonObject>(Object);

	FString StringIndex; {
		ValueObject->GetStringField("ObjectPath").Split(".", nullptr, &StringIndex);
	}

	return AllJsonObjects[FCString::Atod(*StringIndex)];
}

#undef LOCTEXT_NAMESPACE
