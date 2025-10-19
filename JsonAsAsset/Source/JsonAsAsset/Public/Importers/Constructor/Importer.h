// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dom/JsonObject.h"
#include "Utilities/Serializers/ObjectUtilities.h"
#include "Utilities/Serializers/PropertyUtilities.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Utilities/Serializers/SerializerContainer.h"

// Global handler for converting JSON to assets
class IImporter : public USerializerContainer {
public:
	IImporter() {
	}

	IImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects = {}) {
		this->FileName = FileName;
		this->FilePath = FilePath;
		this->JsonObject = JsonObject;
		this->Package = Package;
		this->OutermostPkg = OutermostPkg;
		this->AllJsonObjects = AllJsonObjects;
		this->PropertySerializer = NewObject<UPropertySerializer>();
		this->GObjectSerializer = NewObject<UObjectSerializer>();
		this->GObjectSerializer->SetPropertySerializer(PropertySerializer);
	}

	virtual ~IImporter() {
	}

	// Import the data of the supported type, return if successful or not
	virtual bool ImportData() { return false; }

protected:
	UPROPERTY()
		UPropertySerializer* PropertySerializer;
private:
	UPROPERTY()
	UObjectSerializer* GObjectSerializer;

	TArray<FString> AcceptedTypes = {
		"CurveTable",
		"CurveFloat",
		"CurveVector",
		"CurveLinearColor",
		"CurveLinearColorAtlas",
		"Skeleton",
		"AnimSequence",
		"AnimMontage",
		"Material",
		"MaterialFunction",
		"MaterialInstanceConstant",
		"MaterialParameterCollection",
		"DataTable",
		"LandscapeGrassType",
		"ReverbEffect",
		"SoundAttenuation",
		"SoundConcurrency",
		"SubsurfaceProfile",
		"PhysicalMaterial",
		"BlueprintGeneratedClass",
		"WidgetBlueprintGeneratedClass"
	};

public:
	/* Loads a single <T> object ptr */
	template <class T = UObject>
	void LoadObject(const TSharedPtr<FJsonObject>* PackageIndex, T*& Object);

	/* Loads an array of <T> object ptrs */
	template <class T = UObject>
	TArray<T*> LoadObject(const TArray<TSharedPtr<FJsonValue>>& PackageArray, TArray<T*> Array);

	void ParsePackageIndex(const TSharedPtr<FJsonObject>* PackageIndex, FString& OutType, FString& OutName, FString& OutPath, FString& OutOuter);

	// Refers to AcceptedTypes to see if type is valid ------------------
	bool CanImport(const FString& ImporterType) { return AcceptedTypes.Contains(ImporterType); }

	FGuid CreateGUID(FString String) {
		FGuid GUID;
		FGuid::Parse(String, GUID);

		return GUID;
	}

	bool CanImportAny(TArray<FString>& Types) {
		for (FString& Type : Types) {
			if (!CanImport(Type)) continue;
			return true;
		}

		return false;
	}

	TArray<FString> GetAcceptedTypes() { return AcceptedTypes; }
	// ------------------------------------------------------------------------

	void ImportReference(const FString& File);
	bool HandleReference(const FString& GamePath);

	bool HandleExports(TArray<TSharedPtr<FJsonValue>> Exports, FString File, bool bHideNotifications = false);

	/*
	* Gets a reference from AllJsonObjects
	* 
	* Example (PackageIndex):
	* {
          "ObjectName": "Class'Asset:ExportName'",
          "ObjectPath": "/Game/Asset.Index"
    * }
	*/
	TSharedPtr<FJsonObject> GetExport(FJsonObject* PackageIndex);

public:
	UObject* ParentObject;

protected:
	/* This is called at the end of asset creation, bringing the user to the asset and fully loading it */
	bool HandleAssetCreation(UObject* Asset) const;
	void SavePackage();

	FName GetExportNameOfSubobject(const FString& PackageIndex);
	TArray<TSharedPtr<FJsonValue>> FilterExportsByOuter(const FString& Outer);
	TSharedPtr<FJsonValue> GetExportByObjectPath(const TSharedPtr<FJsonObject>& Object);

public:
	// Wrapper for remote downloading
	template <class T = UObject>
	static T* DownloadWrapper(T* InObject, FString Type, FString Name, FString Path);

protected:

	FORCEINLINE UObjectSerializer* GetObjectSerializer() const { return GObjectSerializer; }
	FString FileName;
	FString FilePath;
	TSharedPtr<FJsonObject> JsonObject;
	UPackage* Package;
	UPackage* OutermostPkg;

	TArray<TSharedPtr<FJsonValue>> AllJsonObjects;
};
