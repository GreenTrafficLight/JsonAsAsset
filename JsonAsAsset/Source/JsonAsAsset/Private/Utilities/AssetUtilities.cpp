// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/AssetUtilities.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IPluginManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Settings/JsonAsAssetSettings.h"
#include "Dom/JsonObject.h"

#include "Importers/TextureImporter.h"
#include "Importers/Types/Materials/MaterialParameterCollectionImporter.h"
#include "Importers/Types/Meshes/StaticMeshImporter.h"

#include "HttpModule.h"
#include "AssetRegistryModule.h"
#include "Misc/MessageDialog.h"
#include "Interfaces/IHttpResponse.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Utilities/AssetUtilities.h"
#include "Utilities/RemoteUtilities.h"

/* CreateAssetPackage Implementations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
UPackage* FAssetUtilities::CreateAssetPackage(const FString& FullPath) {
	UPackage* Package = CreatePackage(
		/* 4.25, 4.26.0 and below need an Outer */
#if UE4_25_BELOW || (UE4_26_0)
		nullptr,
#endif
		*FullPath);
	Package->FullyLoad();

	return Package;
}

UPackage* FAssetUtilities::CreateAssetPackage(const FString& Name, const FString& OutputPath, UPackage*& OutOutermostPkg, FString& FailureReason) {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();
	
	FString ModifiablePath = OutputPath;

	// References Automatically Formatted
	if ((!OutputPath.StartsWith("/Game/") && !OutputPath.StartsWith("/Plugins/")) && OutputPath.Contains("Content")) {
		/* if (!Settings->AssetSettings.GameName.IsEmpty()) {
			ModifiablePath = ModifiablePath.Replace(*(Settings->AssetSettings.GameName + "/Content"), TEXT("/Game"));
			ModifiablePath.Split(*(Settings->ExportDirectory.Path + "/"), nullptr, &ModifiablePath, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			ModifiablePath.Split("/", &ModifiablePath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			ModifiablePath += "/";
		} */

		ModifiablePath.Split(*(Settings->ExportDirectory.Path + "/"), nullptr, &ModifiablePath, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		ModifiablePath.Split("/", nullptr, &ModifiablePath, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		ModifiablePath.Split("/", &ModifiablePath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		/* Ex: RestPath: Plugins/Folder/BaseTextures */
		/* Ex: RestPath: Content/SecondaryFolder */
		const bool bIsPlugin = ModifiablePath.StartsWith("Plugins");

		/* Plugins/Folder/BaseTextures -> Folder/BaseTextures */
		if (bIsPlugin) {
			FString PluginName = ModifiablePath;
			FString RemainingPath;
			/* PluginName = TestName */
			/* RemainingPath = SetupAssets/Materials */
			ModifiablePath.Split("/Content/", &PluginName, &RemainingPath, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			PluginName.Split("/", nullptr, &PluginName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

			/* /PluginName/Materials */
			ModifiablePath = PluginName + "/" + RemainingPath;
		}
		/* Content/SecondaryFolder -> Game/SecondaryFolder */
		else {
			ModifiablePath = ModifiablePath.Replace(TEXT("Content"), TEXT("Game"));
		}

		ModifiablePath = "/" + ModifiablePath + "/";

		/* Check if plugin exists */
		if (bIsPlugin) {
			FString PluginName;
			ModifiablePath.Split("/", nullptr, &PluginName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			PluginName.Split("/", &PluginName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);

			if (!IPluginManager::Get().FindPlugin(PluginName).IsValid())
				CreatePlugin(PluginName);
		}
	}
	else {
		FString RootName; {
			ModifiablePath.Split("/", nullptr, &RootName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			RootName.Split("/", &RootName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		}

		if (RootName != "Game" && RootName != "Engine" && !IPluginManager::Get().FindPlugin(RootName).IsValid()) {
			CreatePlugin(RootName);
		}

		ModifiablePath.Split("/", &ModifiablePath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		ModifiablePath = ModifiablePath + "/";
	}

	const FString PathWithGame = ModifiablePath + Name;

	if (PathWithGame.Contains(TEXT("//"), ESearchCase::CaseSensitive)) {
		FailureReason = "Attempted to create a package with name containing double slashes.\n\nUpdate your configuration to use a valid Export Directory.";
		return nullptr;
	}

	UPackage* Package = CreateAssetPackage(*PathWithGame);
	OutOutermostPkg = Package->GetOutermost();
	Package->FullyLoad();

	return Package;
}

UPackage* FAssetUtilities::CreateAssetPackage(const FString& Name, const FString& OutputPath) {
	UPackage* Ignore = nullptr; /* Put here because &nullptr doesn't work */
	FString StringIgnore = "";

	return CreateAssetPackage(Name, OutputPath, Ignore, StringIgnore);
}

/* Importing assets from Cloud */
template <typename T>
bool FAssetUtilities::ConstructAsset(const FString& Path, const FString& Type, TObjectPtr<T>& OutObject, bool& bSuccess) {
	/* Skip if no type provided */
	if (Type == "") {
		return false;
	}

	/* Manually handled asset types */
	const bool bIsTexture = Type ==
		"Texture2D" ||
		Type == "TextureRenderTarget2D" ||
		Type == "TextureCube" ||
		Type == "VolumeTexture";

	const bool bIsMesh = Type == "StaticMesh";

	/* Supported Assets */
	if (IImporter::CanImport(Type, true) || bIsTexture || bIsMesh) {
		if (bIsTexture) {
			UTexture* Texture;
			FString NewPath = Path;

			FString RootName; {
				NewPath.Split("/", nullptr, &RootName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
				RootName.Split("/", &RootName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			}

			/* Missing Plugin: Create it */
			if (RootName != "Game" && RootName != "Engine" && 
#if UE4_18_BELOW
				!IPluginManager::Get().FindPlugin(RootName).IsValid()
#else
				IPluginManager::Get().FindPlugin(RootName) == nullptr
#endif
				) {
				CreatePlugin(RootName);
			}

			bSuccess = Construct_TypeTexture(NewPath, Path, Texture);
			if (bSuccess) OutObject = Cast<T>(Texture);

			return true;
		}

		if (bIsMesh) {
			UStaticMesh* StaticMesh;
			FString NewPath = Path;

			FString RootName; {
				NewPath.Split("/", nullptr, &RootName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
				RootName.Split("/", &RootName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			}

			bSuccess = Construct_TypeStreamableRenderAsset(NewPath, StaticMesh);
			if (bSuccess) OutObject = Cast<T>(StaticMesh);

			return true;
		}

		const TSharedPtr<FJsonObject> Response = API_RequestExports(Path);
		if (
#if UE4_18_BELOW
			!Response.IsValid()
#else
			Response == nullptr 
#endif
			|| Path.IsEmpty()) return true;

		if (Response->HasField(TEXT("errored"))) {
			UE_LOG(LogJsonAsAsset, Log, TEXT("Error from response \"%s\""), *Path);
			return true;
		}

		TSharedPtr<FJsonObject> JsonObject = Response->GetArrayField(TEXT("jsonOutput"))[0]->AsObject();
		FString PackagePath;
		FString AssetName;
		Path.Split(".", &PackagePath, &AssetName);

		if (
#if UE4_18_BELOW
			JsonObject.IsValid()
#else
			JsonObject
#endif
			) {
			const FString NewPath = PackagePath;

			FString RootName; {
				NewPath.Split("/", nullptr, &RootName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
				RootName.Split("/", &RootName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			}

			if (RootName != "Game" && RootName != "Engine" && 
#if UE4_18_BELOW
				!IPluginManager::Get().FindPlugin(RootName).IsValid()
#else
				IPluginManager::Get().FindPlugin(RootName) == nullptr
#endif
				) {
				CreatePlugin(RootName);
			}

			/* Import asset by IImporter */
			bSuccess = IImporter::ReadExportsAndImport(Response->GetArrayField(TEXT("jsonOutput")), PackagePath, true);

			/* Define found object */
			OutObject = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *Path));

			return OutObject != nullptr;
		}
	}

	return false;
}

bool FAssetUtilities::Construct_TypeTexture(const FString& Path, const FString& FetchPath, UTexture*& OutTexture) {
	if (Path.IsEmpty())
		return false;

	TSharedPtr<FJsonObject> JsonObject = API_RequestExports(FetchPath);
	if (
#if UE4_18_BELOW
		JsonObject.Get() == nullptr
#else
		JsonObject == nullptr
#endif
		)
		return false;

	TArray<TSharedPtr<FJsonValue>> Response = JsonObject->GetArrayField("jsonOutput");
	if (Response.Num() == 0)
		return false;

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();
	TSharedPtr<FJsonObject> JsonExport = Response[0]->AsObject();
	FString Type = JsonExport->GetStringField("Type");

	UTexture* Texture = nullptr;
	TArray<uint8> Data = TArray<uint8>();

	/* ~~~~~~~~~~~~~~~ Download Texture Data ~~~~~~~~~~~~ */
	FHttpModule* HttpModule = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::NotThreadSafe> HttpRequest = HttpModule->CreateRequest();

	HttpRequest->SetURL(Settings->Url + "/api/export?path=" + FetchPath);
	HttpRequest->SetHeader("content-type", "application/octet-stream");
	HttpRequest->SetVerb(TEXT("GET"));

	const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse = FRemoteUtilities::ExecuteRequestSync(HttpRequest);
	if (!HttpResponse.IsValid() || HttpResponse->GetResponseCode() != 200)
		return false;

	Data = HttpResponse->GetContent();
	if (Data.Num() == 0)
		return false;

	FString PackagePath; 
	FString AssetName; {
		Path.Split(".", &PackagePath, &AssetName);
	}

	UPackage* Package = CreatePackage(nullptr, *PackagePath);
	UPackage* OutermostPkg = Package->GetOutermost();
	Package->FullyLoad();

	// Create Importer
	const UTextureImporter* Importer = new UTextureImporter(AssetName, Path, Response[0]->AsObject(), Package, OutermostPkg);

	if (Type == "Texture2D")
		Importer->ImportTexture2D(Texture, Data, JsonExport);
	if (Type == "TextureCube")
		Importer->ImportTextureCube(Texture, Data, JsonExport);
	if (Type == "VolumeTexture")
		Importer->ImportVolumeTexture(Texture, Data, JsonExport);
	if (Type == "TextureRenderTarget2D")
		Importer->ImportRenderTarget2D(Texture, JsonExport->GetObjectField("Properties"));

	if (Texture == nullptr) {
		return false;
	}

	FAssetRegistryModule::AssetCreated(Texture);
	if (!Texture->MarkPackageDirty()) {
		return false;
	}
		
	Package->SetDirtyFlag(true);
	Texture->PostEditChange();
	Texture->AddToRoot();
	Package->FullyLoad();

	/* Save texture */
	if (Settings->bAllowPackageSaving) {
		const FString PackageName = Package->GetName();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		UPackage::SavePackage(Package, nullptr, RF_Standalone, *PackageFileName, GWarn, nullptr, false, true, SAVE_NoError);
	}

	OutTexture = Texture;

	return true;
}

bool FAssetUtilities::Construct_TypeStreamableRenderAsset(const FString& Path, UStaticMesh*& OutStaticMesh) {
	if (Path.IsEmpty())
		return false;

	FString FetchPath = Path;
	if (Path.StartsWith("/Game/Plugins/"))
		FetchPath = FetchPath.Replace(TEXT("/Game/Plugins/"), TEXT("/"));

	TSharedPtr<FJsonObject> JsonObject = API_RequestExports(FetchPath);
	if (JsonObject.Get() == nullptr)
		return false;

	TArray<TSharedPtr<FJsonValue>> Response = JsonObject->GetArrayField("jsonOutput");
	if (Response.Num() == 0)
		return false;

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();
	TSharedPtr<FJsonObject> JsonExport = Response[2]->AsObject();

	UStaticMesh* StaticMesh = nullptr;
	TArray<uint8> Data = TArray<uint8>();

	/* ~~~~~~~~~~~~~~~ Download Data ~~~~~~~~~~~~ */
	FHttpModule* HttpModule = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::NotThreadSafe> HttpRequest = HttpModule->CreateRequest();

	HttpRequest->SetURL(Settings->Url + "/api/export?path=" + FetchPath);
	HttpRequest->SetHeader("content-type", "application/octet-stream");
	HttpRequest->SetVerb(TEXT("GET"));

	const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse = FRemoteUtilities::ExecuteRequestSync(HttpRequest);
	if (!HttpResponse.IsValid() || HttpResponse->GetResponseCode() != 200)
		return false;

	Data = HttpResponse->GetContent();
	if (Data.Num() == 0)
		return false;

	FString PackagePath;
	FString AssetName; {
		Path.Split(".", &PackagePath, &AssetName);
	}

	UPackage* Package = CreatePackage(nullptr, *PackagePath);
	UPackage* OutermostPkg = Package->GetOutermost();
	Package->FullyLoad();

	const UStaticMeshImporter* Importer = new UStaticMeshImporter(AssetName, Path, JsonExport, Package, OutermostPkg);

	Importer->ImportStaticMesh(StaticMesh, Data, JsonExport);

	if (StaticMesh == nullptr) {
		return false;
	}

	Package->SetDirtyFlag(true);
	StaticMesh->PostEditChange();
	StaticMesh->AddToRoot();
	Package->FullyLoad();

	/* Save texture */
	if (Settings->bAllowPackageSaving) {
		const FString PackageName = Package->GetName();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		UPackage::SavePackage(Package, nullptr, RF_Standalone, *PackageFileName, GWarn, nullptr, false, true, SAVE_NoError);
	}

	OutStaticMesh = StaticMesh;

	return true;
}

void FAssetUtilities::CreatePlugin(FString PluginName) {
}

const TSharedPtr<FJsonObject> FAssetUtilities::API_RequestExports(const FString& Path) {
	FHttpModule* HttpModule = &FHttpModule::Get();
	const TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();

	FString PackagePath;
	FString AssetName;
	Path.Split(".", &PackagePath, &AssetName);

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	const TSharedRef<IHttpRequest> NewRequest = HttpModule->CreateRequest();
	NewRequest->SetURL(Settings->Url + "/api/export?raw=true&path=" + Path);
	NewRequest->SetVerb(TEXT("GET"));

	const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> NewResponse = FRemoteUtilities::ExecuteRequestSync(NewRequest);
	if (!NewResponse.IsValid()) return TSharedPtr<FJsonObject>();

	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(NewResponse->GetContentAsString());
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
		return JsonObject;

	return TSharedPtr<FJsonObject>();
}