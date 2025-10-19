// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/Serializers/ObjectUtilities.h"

#include "Utilities/Serializers/PropertyUtilities.h"
#include "UObject/Package.h"
#include "Utilities/EngineUtilities.h"

DECLARE_LOG_CATEGORY_CLASS(LogObjectSerializer, All, All);
PRAGMA_DISABLE_OPTIMIZATION

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

UObjectSerializer::UObjectSerializer() : ParentAsset(nullptr), PropertySerializer(nullptr) {
}

void UObjectSerializer::SetupExports(const TArray<TSharedPtr<FJsonValue>>& InObjects) {
	Exports = InObjects;

	PropertySerializer->ClearCachedData();
}

UPackage* FindOrLoadPackage(const FString& PackageName) {
	UPackage* Package = FindPackage(NULL, *PackageName);
	if (!Package)
		Package = LoadPackage(NULL, *PackageName, LOAD_None);

	return Package;
}

void UObjectSerializer::SetPropertySerializer(UPropertySerializer* NewPropertySerializer) {
	check(NewPropertySerializer);

	this->PropertySerializer = NewPropertySerializer;
	NewPropertySerializer->ObjectSerializer = this;
}

void UObjectSerializer::SetExportForDeserialization(const TSharedPtr<FJsonObject>& JsonObject, UObject* Object) {
	ExportsToNotDeserialize.Add(JsonObject->GetStringField(TEXT("Name")));
	ConstructedObjects.Add(JsonObject->GetStringField(TEXT("Name")), Object);
}

void UObjectSerializer::DeserializeExports(TArray<TSharedPtr<FJsonValue>> InExports) {
	PropertySerializer->ExportsContainer.Empty();

	TMap<TSharedPtr<FJsonObject>, UObject*> ExportsMap;
	int Index = -1;

	for (TSharedPtr<FJsonValue> Object : InExports) {
		Index++;

		TSharedPtr<FJsonObject> ExportObject = Object->AsObject();

		/* No name = no export!! */
		if (!ExportObject->HasField(TEXT("Name"))) continue;

		FString Name = ExportObject->GetStringField(TEXT("Name"));
		FString Type = ExportObject->GetStringField(TEXT("Type"));

		/* Check if it's not supposed to be deserialized */
		if (ExportsToNotDeserialize.Contains(Name)) continue;
		if (Type == "BodySetup" || Type == "NavCollision") continue;

		FString Outer = ExportObject->GetStringField(TEXT("Outer"));

		/* Add it to the referenced objects */
		PropertySerializer->ExportsContainer.Exports.Add(FUObjectExport(FName(*Name), FName(*Type), FName(*Outer), ExportObject, nullptr, ParentAsset, Index));
	}

	for (FUObjectExport& Export : PropertySerializer->ExportsContainer.Exports) {
		DeserializeExport(Export, ExportsMap);
	}

	for (const auto Pair : ExportsMap) {
		TSharedPtr<FJsonObject> Properties = Pair.Key;
		UObject* Object = Pair.Value;

		DeserializeObjectProperties(Properties, Object);
	}
}

void UObjectSerializer::DeserializeExport(FUObjectExport& Export, TMap<TSharedPtr<FJsonObject>, UObject*>& ExportsMap) {
	if (Export.Object != nullptr) return;

	TSharedPtr<FJsonObject> ExportObject = Export.JsonObject;

	/* No name = no export!! */
	if (!ExportObject->HasField(TEXT("Name"))) return;

	FString Name = ExportObject->GetStringField(TEXT("Name"));
	FString Type = ExportObject->GetStringField(TEXT("Type")).Replace(TEXT("CommonWidgetSwitcher"), TEXT("CommonActivatableWidgetSwitcher"));

	/* Check if it's not supposed to be deserialized */
	if (ExportsToNotDeserialize.Contains(Name)) return;
	if (Type == "BodySetup" || Type == "NavCollision") return;

	FString ClassName = ExportObject->GetStringField(TEXT("Class"));

	if (ExportObject->HasField(TEXT("Template"))) {
		const TSharedPtr<FJsonObject> TemplateObject = ExportObject->GetObjectField(TEXT("Template"));
		ClassName = ReadPathFromObject(&TemplateObject).Replace(TEXT("Default__"), TEXT(""));
	}

#if UE5_6_BEYOND
	UClass* Class = FindFirstObject<UClass>(*ClassName);
#else
	UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);
#endif

	if (!Class) {
#if UE5_6_BEYOND
		Class = FindFirstObject<UClass>(*Type);
#else
		Class = FindObject<UClass>(ANY_PACKAGE, *Type);
#endif
	}

	if (!Class) return;

	FString Outer = ExportObject->GetStringField(TEXT("Outer"));
	UObject* ObjectOuter = nullptr;

	FUObjectExport FoundExport = PropertySerializer->ExportsContainer.Find(Outer);
	if (FoundExport.IsValid() && FoundExport.JsonObject.IsValid()) {
		if (FoundExport.Object == nullptr) {
			DeserializeExport(FoundExport, ExportsMap);
		}

		UObject* FoundObject = FoundExport.Object;
		ObjectOuter = FoundObject;
	}

	if (UObject** ConstructedObject = ConstructedObjects.Find(Outer)) {
		ObjectOuter = *ConstructedObject;
	}

	if (PathsToNotDeserialize.Contains(Outer + "." + Name)) return;
	if (ObjectOuter == nullptr) {
		ObjectOuter = ParentAsset;
	}

	UObject* NewUObject = NewObject<UObject>(ObjectOuter, Class, FName(*Name));

	if (ExportObject->HasField(TEXT("Properties"))) {
		TSharedPtr<FJsonObject> Properties = ExportObject->GetObjectField(TEXT("Properties"));

		ExportsMap.Add(Properties, NewUObject);
	}
	else {
		ExportsMap.Add(ExportObject, NewUObject);
	}

	/* Add it to the referenced objects */
	Export.Object = NewUObject;

	/* Already deserialized */
	PathsToNotDeserialize.Add(Outer + "." + Name);
}

void UObjectSerializer::DeserializeObjectProperties(const TSharedPtr<FJsonObject>& Properties, UObject* Object) const {
	if (Object == nullptr) return;

	const UClass* ObjectClass = Object->GetClass();

	for (UProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext) {
		const FString PropertyName = Property->GetName();

		if (!PropertySerializer->ShouldDeserializeProperty(Property)) continue;

		void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Object);
		const bool HasHandledProperty = PassthroughPropertyHandler(Property, PropertyName, PropertyValue, Properties, PropertySerializer);

		if (Properties->HasField(PropertyName) && !HasHandledProperty && PropertyName != "LODParentPrimitive") {
			const TSharedPtr<FJsonValue>& ValueObject = Properties->Values.FindChecked(PropertyName);

			if (Property->ArrayDim == 1 || ValueObject->Type == EJson::Array) {
				PropertySerializer->DeserializePropertyValue(Property, ValueObject.ToSharedRef(), PropertyValue);
			}
		}
	}
}

PRAGMA_ENABLE_OPTIMIZATION