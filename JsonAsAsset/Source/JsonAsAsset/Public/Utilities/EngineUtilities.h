// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Utilities/Serializers/PropertyUtilities.h"
#include "Settings/JsonAsAssetSettings.h"
#include "Modules/LogCategory.h"

/**
 * Get the asset currently selected in the Content Browser.
 *
 * @return Selected Asset
 */
template <typename T>
T* GetSelectedAsset(const bool SuppressErrors = false, FString OptionalAssetNameCheck = "") {
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() == 0) {
		if (SuppressErrors == true) {
			return nullptr;
		}

		GLog->Log("JsonAsAsset: [GetSelectedAsset] None selected, returning nullptr.");

		const FText DialogText = FText::Format(
			FText::FromString(TEXT("Importing an asset of type '{0}' requires a base asset selected to modify. Select one in your content browser.")),
			FText::FromString(T::StaticClass()->GetName())
		);

		FMessageDialog::Open(EAppMsgType::Ok, DialogText);

		return nullptr;
	}

	UObject* SelectedAsset = SelectedAssets[0].GetAsset();
	T* CastedAsset = Cast<T>(SelectedAsset);

	if (!CastedAsset) {
		if (SuppressErrors == true) {
			return nullptr;
		}

		GLog->Log("JsonAsAsset: [GetSelectedAsset] Selected asset is not of the required class, returning nullptr.");

		const FText DialogText = FText::Format(
			FText::FromString(TEXT("The selected asset is not of type '{0}'. Please select a valid asset.")),
			FText::FromString(T::StaticClass()->GetName())
		);

		FMessageDialog::Open(EAppMsgType::Ok, DialogText);

		return nullptr;
	}

	if (CastedAsset && OptionalAssetNameCheck != "" && !CastedAsset->GetName().Equals(OptionalAssetNameCheck)) {
		CastedAsset = nullptr;
	}

	return CastedAsset;
}

inline void SpawnPrompt(const FString& Title, const FString& Text) {
	FText DialogTitle = FText::FromString(Title);
	const FText DialogMessage = FText::FromString(Text);

	FMessageDialog::Open(EAppMsgType::Ok, DialogMessage);
}

inline TSharedPtr<FJsonObject> GetExport(const FString& Type, TArray<TSharedPtr<FJsonValue>> AllJsonObjects, const bool bGetProperties = false) {
	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = Value->AsObject();

		if (ValueObject->GetStringField(TEXT("Type")) == Type) {
			if (bGetProperties) {
				return ValueObject->GetObjectField(TEXT("Properties"));
			}

			return ValueObject;
		}
	}

	return nullptr;
}

inline TSharedPtr<FJsonObject> GetExport(const FJsonObject* PackageIndex, TArray<TSharedPtr<FJsonValue>> AllJsonObjects) {
	FString ObjectName = PackageIndex->GetStringField(TEXT("ObjectName")); /* Class'Asset:ExportName' */
	FString ObjectPath = PackageIndex->GetStringField(TEXT("ObjectPath")); /* Path/Asset.Index */
	FString Outer;

	/* Clean up ObjectName (Class'Asset:ExportName' --> Asset:ExportName --> ExportName) */
	ObjectName.Split("'", nullptr, &ObjectName);
	ObjectName.Split("'", &ObjectName, nullptr);

	if (ObjectName.Contains(":")) {
		ObjectName.Split(":", nullptr, &ObjectName); /* Asset:ExportName --> ExportName */
	}

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", nullptr, &ObjectName);
	}

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", &Outer, &ObjectName);
	}

	int Index = 0;

	/* Search for the object in the AllJsonObjects array */
	for (const TSharedPtr<FJsonValue>& Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = Value->AsObject();

		FString Name;
		if (ValueObject->TryGetStringField(TEXT("Name"), Name) && Name == ObjectName) {
			if (ValueObject->HasField(TEXT("Outer")) && !Outer.IsEmpty()) {
				FString OuterName = ValueObject->GetStringField(TEXT("Outer"));

				if (OuterName == Outer) {
					return AllJsonObjects[Index]->AsObject();
				}
			}
			else {
				return ValueObject;
			}
		}

		Index++;
	}

	return nullptr;
}

/* Show the user a Notification */
inline auto AppendNotification(const FText& Text, const FText& SubText, const float ExpireDuration,
	const SNotificationItem::ECompletionState CompletionState, const bool bUseSuccessFailIcons,
	const float WidthOverride) -> void
{
	FNotificationInfo Info = FNotificationInfo(Text);
	Info.ExpireDuration = ExpireDuration;
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = bUseSuccessFailIcons;
	Info.WidthOverride = FOptionalSize(WidthOverride);

	Info.Hyperlink = FSimpleDelegate::CreateStatic([]() {
	});
	Info.HyperlinkText = SubText;

	const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(CompletionState);
}

/* Show the user a Notification with Subtext */
inline auto AppendNotification(const FText& Text, const FText& SubText, float ExpireDuration,
	const FSlateBrush* SlateBrush, SNotificationItem::ECompletionState CompletionState,
	const bool bUseSuccessFailIcons, const float WidthOverride) -> void
{
	FNotificationInfo Info = FNotificationInfo(Text);
	Info.ExpireDuration = ExpireDuration;
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = bUseSuccessFailIcons;
	Info.WidthOverride = FOptionalSize(WidthOverride);
	Info.Image = SlateBrush;

	Info.Hyperlink = FSimpleDelegate::CreateStatic([]() {
	});
	Info.HyperlinkText = SubText;

	const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(CompletionState);
}

inline FString ReadPathFromObject(const TSharedPtr<FJsonObject>* PackageIndex) {
	FString ObjectType, ObjectName, ObjectPath, Outer;
	PackageIndex->Get()->GetStringField(TEXT("ObjectName")).Split("'", &ObjectType, &ObjectName);

	ObjectPath = PackageIndex->Get()->GetStringField(TEXT("ObjectPath"));
	ObjectPath.Split(".", &ObjectPath, nullptr);

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	ObjectPath = ObjectPath.Replace(TEXT("Nimbus/Content"), TEXT("/Game"));

	ObjectPath = ObjectPath.Replace(TEXT("Engine/Content"), TEXT("/Engine"));
	ObjectName = ObjectName.Replace(TEXT("'"), TEXT(""));

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", nullptr, &ObjectName);
	}

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", &Outer, &ObjectName);
	}

	return ObjectPath + "." + ObjectName;
}

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

/* Simple handler for JsonArray */
inline auto ProcessJsonArrayField(const TSharedPtr<FJsonObject>& ObjectField, const FString& ArrayFieldName,
	const TFunction<void(const TSharedPtr<FJsonObject>&)>& ProcessObjectFunction) -> void
{
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;

	if (ObjectField->TryGetArrayField(ArrayFieldName, JsonArray)) {
		for (const auto& JsonValue : *JsonArray) {
			const TSharedPtr<FJsonObject> JsonItem = JsonValue->AsObject();
			if (JsonItem.IsValid()) {
				ProcessObjectFunction(JsonItem);
			}
		}
	}
}

/* ReSharper disable once CppParameterNeverUsed */
inline void SetNotificationSubText(FNotificationInfo& Notification, const FText& SubText) {
#if ENGINE_UE5
	Notification.SubText = SubText;
#endif
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