// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utilities/PropertyUtilities.h"
#include "Settings/JsonAsAssetSettings.h"

inline TSharedPtr<FJsonObject> RemovePropertiesShared(const TSharedPtr<FJsonObject>& Input, const TArray<FString>& RemovedProperties) {
	TSharedPtr<FJsonObject> ClonedJsonObject = MakeShareable(new FJsonObject(*Input));

	for (const FString& Property : RemovedProperties) {
		if (ClonedJsonObject->HasField(Property)) {
			ClonedJsonObject->RemoveField(Property);
		}
	}

	return ClonedJsonObject;
}

/* Filter to whitelist */
inline TSharedPtr<FJsonObject> KeepPropertiesShared(const TSharedPtr<FJsonObject>& Input, TArray<FString> WhitelistProperties) {
	const TSharedPtr<FJsonObject> RawSharedPtrData = MakeShared<FJsonObject>();

	for (const FString& Property : WhitelistProperties) {
		if (Input->HasField(Property)) {
			RawSharedPtrData->SetField(Property, Input->TryGetField(Property));
		}
	}

	return RawSharedPtrData;
}

inline TSubclassOf<UObject> LoadClassFromPath(const FString& ObjectName, const FString& ObjectPath) {
	const FString FullPath = ObjectPath + TEXT(".") + ObjectName;
	UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);

	if (LoadedObject) {
		UClass* LoadedClass = Cast<UClass>(LoadedObject);
		if (LoadedClass) {
			return LoadedClass;
		}
	}

	return nullptr;
}


inline TSubclassOf<UObject> LoadBlueprintClass(FString& ObjectPath) {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	ObjectPath = ObjectPath.Replace(TEXT("Nimbus/Content"), TEXT("/Game"));

	FString FullPath = ObjectPath;
	if (FullPath.EndsWith(TEXT(".1"))) {
		FullPath = FullPath.LeftChop(2);
	}

	UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);

	if (LoadedObject) {
		const UBlueprint* LoadedBlueprint = Cast<UBlueprint>(LoadedObject);

		if (LoadedBlueprint && LoadedBlueprint->GeneratedClass) {
			return LoadedBlueprint->GeneratedClass;
		}
	}

	return nullptr;
}

inline UClass* LoadClass(const TSharedPtr<FJsonObject>& SuperStruct) {
	const FString ObjectName = SuperStruct->GetStringField(TEXT("ObjectName")).Replace(TEXT("Class'"), TEXT("")).Replace(TEXT("'"), TEXT(""));
	FString ObjectPath = SuperStruct->GetStringField(TEXT("ObjectPath"));

	/* It's a C++ class if it has Script in it */
	if (ObjectPath.Contains("/Script/")) {
		return LoadClassFromPath(ObjectName, ObjectPath);
	}

	ObjectPath.Split(".", &ObjectPath, nullptr);

	return LoadBlueprintClass(ObjectPath);
}