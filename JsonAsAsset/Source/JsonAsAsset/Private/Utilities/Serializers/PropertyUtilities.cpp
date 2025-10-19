// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/Serializers/PropertyUtilities.h"

#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
#include "UObject/NoExportTypes.h"
#include "UObject/EnumProperty.h"
#include "GameplayTagContainer.h"
#include "Importers/Importer.h"
#include "Utilities/Serializers/ObjectUtilities.h"
#include "UObject/TextProperty.h"

/* Struct Serializers */
#include "Utilities/Serializers/Structs/DateTimeSerializer.h"
#include "Utilities/Serializers/Structs/FallbackStructSerializer.h"
#include "Utilities/Serializers/Structs/TimespanSerializer.h"

template<typename FieldType>
FORCEINLINE FieldType* CastField(UField* Src)
{
	return Src && Src->IsA<FieldType>() ? static_cast<FieldType*>(Src) : nullptr;
}

DECLARE_LOG_CATEGORY_CLASS(LogPropertySerializer, Error, Log);
PRAGMA_DISABLE_OPTIMIZATION

UPropertySerializer::UPropertySerializer() {
	this->FallbackStructSerializer = MakeShared<FFallbackStructSerializer>(this);

	UScriptStruct* DateTimeStruct = FindObject<UScriptStruct>(NULL, TEXT("/Script/CoreUObject.DateTime"));
	UScriptStruct* TimespanStruct = FindObject<UScriptStruct>(NULL, TEXT("/Script/CoreUObject.Timespan"));
	check(DateTimeStruct);
	check(TimespanStruct);

	this->StructSerializers.Add(DateTimeStruct, MakeShared<FDateTimeSerializer>());
	this->StructSerializers.Add(TimespanStruct, MakeShared<FTimespanSerializer>());
}

void UPropertySerializer::DeserializePropertyValue(UProperty* Property, const TSharedRef<FJsonValue>& JsonValue, void* OutValue) {
	const UMapProperty* MapProperty = CastField<const UMapProperty>(Property);
	const USetProperty* SetProperty = CastField<const USetProperty>(Property);
	const UArrayProperty* ArrayProperty = CastField<const UArrayProperty>(Property);

	TSharedRef<FJsonValue> NewJsonValue = JsonValue;

	if (BlacklistedPropertyNames.Contains(Property->GetName())) {
		return;
	}

	if (MapProperty) {
		UProperty* KeyProperty = MapProperty->KeyProp;
		UProperty* ValueProperty = MapProperty->ValueProp;
		FScriptMapHelper MapHelper(MapProperty, OutValue);
		const TArray<TSharedPtr<FJsonValue>>& PairArray = NewJsonValue->AsArray();

		for (int32 i = 0; i < PairArray.Num(); i++) {
			const TSharedPtr<FJsonObject>& Pair = PairArray[i]->AsObject();
			const TSharedPtr<FJsonValue>& EntryKey = Pair->Values.FindChecked(TEXT("Key"));
			const TSharedPtr<FJsonValue>& EntryValue = Pair->Values.FindChecked(TEXT("Value"));
			const int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* PairPtr = MapHelper.GetPairPtr(Index);

			/* Copy over imported key and value from temporary storage */
			DeserializePropertyValue(KeyProperty, EntryKey.ToSharedRef(), PairPtr);
			DeserializePropertyValue(ValueProperty, EntryValue.ToSharedRef(), PairPtr + MapHelper.MapLayout.ValueOffset);
		}
		MapHelper.Rehash();

	}
	else if (SetProperty) {
		UProperty* ElementProperty = SetProperty->ElementProp;
		FScriptSetHelper SetHelper(SetProperty, OutValue);
		const TArray<TSharedPtr<FJsonValue>>& SetArray = NewJsonValue->AsArray();
		SetHelper.EmptyElements();
		uint8* TempElementStorage = static_cast<uint8*>(FMemory::Malloc(ElementProperty->ElementSize));
		ElementProperty->InitializeValue(TempElementStorage);

		for (int32 i = 0; i < SetArray.Num(); i++) {
			const TSharedPtr<FJsonValue>& Element = SetArray[i];
			DeserializePropertyValue(ElementProperty, Element.ToSharedRef(), TempElementStorage);

			const int32 NewElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* NewElementPtr = SetHelper.GetElementPtr(NewElementIndex);

			/* Copy over imported key from temporary storage */
			ElementProperty->CopyCompleteValue_InContainer(NewElementPtr, TempElementStorage);
		}
		SetHelper.Rehash();

		ElementProperty->DestroyValue(TempElementStorage);
		FMemory::Free(TempElementStorage);
	}
	else if (ArrayProperty) {
		UProperty* ElementProperty = ArrayProperty->Inner;
		FScriptArrayHelper ArrayHelper(ArrayProperty, OutValue);
		const TArray<TSharedPtr<FJsonValue>>& SetArray = NewJsonValue->AsArray();
		ArrayHelper.EmptyValues();

		for (int32 i = 0; i < SetArray.Num(); i++) {
			const TSharedPtr<FJsonValue>& Element = SetArray[i];
			const uint32 AddedIndex = ArrayHelper.AddValue();
			uint8* ValuePtr = ArrayHelper.GetRawPtr(AddedIndex);
			DeserializePropertyValue(ElementProperty, Element.ToSharedRef(), ValuePtr);
		}
	}
	else if (Property->IsA<UMulticastDelegateProperty>()) {
	}
	else if (Property->IsA<UDelegateProperty>()) {
	}
	else if (CastField<const UInterfaceProperty>(Property)) {
	}
	else if (USoftObjectProperty* SoftObjectProperty = CastField<USoftObjectProperty>(Property)) {
		TSharedPtr<FJsonObject> SoftJsonObjectProperty;
		FString PathString = "";

		switch (NewJsonValue->Type) {
			/* FModel, extract it from the object */
		case EJson::Object:
			SoftJsonObjectProperty = NewJsonValue->AsObject();
			PathString = SoftJsonObjectProperty->GetStringField(TEXT("AssetPathName"));
			break;

			/* Older game builds */
		default:
			PathString = NewJsonValue->AsString();
			break;
		}

		if (PathString != "") {
			FSoftObjectPtr* ObjectPtr = static_cast<FSoftObjectPtr*>(OutValue);
			*ObjectPtr = FSoftObjectPath(PathString);

			if (!ObjectPtr->LoadSynchronous()) {
				/* Try importing it using Cloud */
				FString PackagePath;
				FString AssetName;
				PathString.Split(".", &PackagePath, &AssetName);
				UObject* T = NULL;

				FString PropertyClassName = SoftObjectProperty->PropertyClass->GetName();

				IImporter::DownloadWrapper(T, PropertyClassName, AssetName, PackagePath);
			}
		}
	}
	else if (const UObjectPropertyBase* ObjectProperty = CastField<const UObjectPropertyBase>(Property)) {
		/* Need to serialize full UObject for object property */
		UObject* Object = nullptr;

		if (NewJsonValue->IsNull()) {
			ObjectProperty->SetObjectPropertyValue(OutValue, nullptr);
		}

		if (NewJsonValue->Type == EJson::Object) {
			auto JsonValueAsObject = NewJsonValue->AsObject();
			bool bUseDefaultLoadObject = !JsonValueAsObject->GetStringField(TEXT("ObjectName")).Contains(":ParticleModule");

			if (bUseDefaultLoadObject) {
				/* Use IImporter to import the object */
				IImporter* Importer = new IImporter();

				Importer->ParentObject = ObjectSerializer->ParentAsset;
				Importer->LoadObject(&JsonValueAsObject, Object);

				if (Object == nullptr) {
					if (ObjectProperty && ObjectProperty->PropertyClass) {
						UStruct* Struct = ObjectProperty->PropertyClass;

						FFailedPropertyInfo PropertyInfo;
						PropertyInfo.ClassName = ObjectProperty->PropertyClass->GetName();
						PropertyInfo.SuperStructName = Struct->GetSuperStruct() ? Struct->GetSuperStruct()->GetName() : TEXT("None");
						PropertyInfo.ObjectPath = JsonValueAsObject->GetStringField(TEXT("ObjectPath"));

						if (!FailedProperties.Contains(PropertyInfo)) {
							FailedProperties.Add(PropertyInfo);
						}
					}
				}

				if (Object != nullptr && !Cast<UActorComponent>(Object)) {
					ObjectProperty->SetObjectPropertyValue(OutValue, Object);
				}

				if (Object != nullptr) {
					/* Get the export */
					TSharedPtr<FJsonObject> Export = GetExport(JsonValueAsObject.Get(), ObjectSerializer->Exports);
					if (Export.IsValid()) {
						if (Export->HasField(TEXT("Properties"))) {
							TSharedPtr<FJsonObject> Properties = Export->GetObjectField(TEXT("Properties"));

							if (Export->HasField(TEXT("LODData"))) {
								Properties->SetArrayField(TEXT("LODData"), Export->GetArrayField(TEXT("LODData")));
							}

							ObjectSerializer->DeserializeObjectProperties(Properties, Object);
						}
					}
				}
			}

			FString ObjectName = JsonValueAsObject->GetStringField(TEXT("ObjectName"));
			FString ObjectPath = JsonValueAsObject->GetStringField(TEXT("ObjectPath"));
			FString ObjectOuter;

			if (ObjectName.Contains(".")) {
				ObjectName.Split(".", &ObjectOuter, &ObjectName);
				ObjectName.Split("'", &ObjectName, nullptr);
			}

			if (ObjectName.Contains(":")) {
				ObjectName.Split(":", nullptr, &ObjectName);
				ObjectName.Split("'", &ObjectName, nullptr);
			}

			FUObjectExport Export = ExportsContainer.Find(ObjectName);
			if (Export.Object != nullptr) {
				UObject* FoundObject = Export.Object;

				if (FoundObject) {
					ObjectProperty->SetObjectPropertyValue(OutValue, FoundObject);
				}
			}

			if (ObjectName.Contains(".")) {
				TArray<FString> Parts;
				ObjectName.ParseIntoArray(Parts, TEXT("."), true);

				FString Penultimate = Parts.Num() > 1 ? Parts[Parts.Num() - 2] : TEXT("");
				FString LastSegment = Parts.Num() > 0 ? Parts.Last() : TEXT("");

				ObjectName = LastSegment;
				ObjectOuter = Penultimate;
			}

			if (!ObjectOuter.IsEmpty()) {
				if (ObjectOuter.Contains(":")) {
					ObjectOuter.Split(":", nullptr, &ObjectOuter);
				}

				FUObjectExport Export = ExportsContainer.Find(ObjectName, ObjectOuter);
				if (Export.Object != nullptr) {
					UObject* FoundObject = Export.Object;

					if (FoundObject) {
						ObjectProperty->SetObjectPropertyValue(OutValue, FoundObject);
					}
				}
			}

			if (bFallbackToParentTrace) {
				if (UObject* Parent = ObjectSerializer->ParentAsset) {
					FString Name = Parent->GetName();

					FUObjectExport Export = ExportsContainer.Find(ObjectName, Name);
					if (Export.Object != nullptr) {
						UObject* FoundObject = Export.Object;

						if (FoundObject) {
							ObjectProperty->SetObjectPropertyValue(OutValue, FoundObject);
						}
					}
				}
			}
		}
	}
	else if (const UStructProperty* StructProperty = CastField<const UStructProperty>(Property)) {
		if (StructProperty->Struct == FGameplayTag::StaticStruct()) {
			FGameplayTag* GameplayTagStr = static_cast<FGameplayTag*>(OutValue);
			FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*NewJsonValue->AsObject()->GetStringField(TEXT("TagName"))), false);
			*GameplayTagStr = NewTag;
			return;
		}

		/* FGameplayTagContainer (handled from FModel data) */
		if (StructProperty->Struct == FGameplayTagContainer::StaticStruct()) {
			FGameplayTagContainer* GameplayTagContainerStr = static_cast<FGameplayTagContainer*>(OutValue);

			auto GameplayTags = JsonValue->AsArray();

			for (TSharedPtr<FJsonValue> GameplayTagValue : GameplayTags) {
				FString GameplayTagString = GameplayTagValue->AsString();
				FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName(*GameplayTagString));

				GameplayTagContainerStr->AddTag(GameplayTag);
			}

			return;
		}

		if (StructProperty->Struct->GetFName() == "SoftObjectPath") {
			TSharedPtr<FJsonObject> SoftJsonObjectProperty;
			FString PathString = "";

			SoftJsonObjectProperty = NewJsonValue->AsObject();
			PathString = SoftJsonObjectProperty->GetStringField(TEXT("AssetPathName"));

			if (PathString != "") {
				FSoftObjectPtr* ObjectPtr = static_cast<FSoftObjectPtr*>(OutValue);
				*ObjectPtr = FSoftObjectPath(PathString);

				if (!ObjectPtr->LoadSynchronous()) {
					/* Try importing it using Cloud */
					FString PackagePath;
					FString AssetName;
					PathString.Split(".", &PackagePath, &AssetName);
					UObject* T = NULL;

					FString PropertyClassName = "DataAsset";

					IImporter::DownloadWrapper(T, PropertyClassName, AssetName, PackagePath);
				}
			}
		}

		/* JSON for FGuids are FStrings */
		FString OutString;

		FGuid GUID;
		if (FGuid::Parse(OutString, GUID))
		{
			TSharedRef<FJsonObject> SharedObject = MakeShareable(new FJsonObject());
			SharedObject->SetNumberField(TEXT("A"), GUID.A);
			SharedObject->SetNumberField(TEXT("B"), GUID.B);
			SharedObject->SetNumberField(TEXT("C"), GUID.C);
			SharedObject->SetNumberField(TEXT("D"), GUID.D);

			const TSharedRef<FJsonValue> NewValue = MakeShareable(new FJsonValueObject(SharedObject));
			NewJsonValue = NewValue;
		}

		/* To serialize struct, we need its type and value pointer, because struct value doesn't contain type information */
		DeserializeStruct(StructProperty->Struct, NewJsonValue->AsObject().ToSharedRef(), OutValue);
	}
	else if (const UByteProperty* ByteProperty = CastField<const UByteProperty>(Property)) {
		/* If we have a string provided, make sure Enum is not null */
		if (JsonValue->Type == EJson::String) {
			FString EnumAsString = JsonValue->AsString();

			check(ByteProperty->Enum);
			int64 EnumerationValue = ByteProperty->Enum->GetValueByNameString(EnumAsString);

			ByteProperty->SetIntPropertyValue(OutValue, EnumerationValue);
		}
		else {
			/* Should be a number, set property value accordingly */
			const int64 NumberValue = static_cast<int64>(NewJsonValue->AsNumber());
			ByteProperty->SetIntPropertyValue(OutValue, NumberValue);
		}
		/* Primitives below, they are serialized as plain json values */
	}
	else if (const UNumericProperty* NumberProperty = CastField<const UNumericProperty>(Property)) {
		const double NumberValue = NewJsonValue->AsNumber();
		if (NumberProperty->IsFloatingPoint()) {
			NumberProperty->SetFloatingPointPropertyValue(OutValue, NumberValue);
		}

		else {
			NumberProperty->SetIntPropertyValue(OutValue, static_cast<int64>(NumberValue));
		}
	}
	else if (const UBoolProperty* BoolProperty = CastField<const UBoolProperty>(Property)) {
		const bool bBooleanValue = NewJsonValue->AsBool();
		BoolProperty->SetPropertyValue(OutValue, bBooleanValue);
	}
	else if (Property->IsA<UStrProperty>()) {
		const FString StringValue = NewJsonValue->AsString();
		*static_cast<FString*>(OutValue) = StringValue;
	}
	else if (const UEnumProperty* EnumProperty = CastField<const UEnumProperty>(Property)) {
		FString EnumAsString = NewJsonValue->AsString();

		if (EnumAsString.Contains("::")) {
			EnumAsString.Split("::", nullptr, &EnumAsString);
		}

		/* Prefer readable enum names in result json to raw numbers */
		int64 EnumerationValue = EnumProperty->GetEnum()->GetValueByNameString(EnumAsString);

		if (EnumerationValue != INDEX_NONE) {
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(OutValue, EnumerationValue);
		}
	}
	else if (Property->IsA<UNameProperty>()) {
		/* Name is perfectly representable as string */
		const FString NameString = NewJsonValue->AsString();
		*static_cast<FName*>(OutValue) = *NameString;
	}
	else if (const UTextProperty* TextProperty = CastField<const UTextProperty>(Property)) {
		/* For FText, standard ExportTextItem is okay to use, because it's serialization is quite complex */
		const FString SerializedValue = NewJsonValue->AsString();

		if (!SerializedValue.IsEmpty()) {
			*static_cast<FText*>(OutValue) = FText::FromString(SerializedValue);
		}
		else {
			/* TODO: Somehow add other needed things like Namespace, Key, and LocalizedString */
			TSharedPtr<FJsonObject> Object = NewJsonValue->AsObject().ToSharedRef();

			/* Retrieve properties */
			FString TextNamespace = Object->GetStringField(TEXT("Namespace"));
			FString UniqueKey = Object->GetStringField(TEXT("Key"));
			FString SourceString = Object->GetStringField(TEXT("SourceString"));

			TextProperty->SetPropertyValue(OutValue, FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *TextNamespace, *UniqueKey));
		}
	}
	else {
		UE_LOG(LogPropertySerializer, Fatal, TEXT("Found unsupported property type when deserializing value: %s"), *Property->GetClass()->GetName());
	}
}

void UPropertySerializer::ClearCachedData() {
	FailedProperties.Empty();
}

void UPropertySerializer::DisablePropertySerialization(UStruct* Struct, FName PropertyName) {
	UProperty* Property = Struct->FindPropertyByName(PropertyName);
	checkf(Property, TEXT("Cannot find Property %s in Struct %s"), *PropertyName.ToString(), *Struct->GetPathName());
	this->PinnedStructs.Add(Struct);
	this->BlacklistedProperties.Add(Property);
}

void UPropertySerializer::AddStructSerializer(UScriptStruct* Struct, const TSharedPtr<FStructSerializer>& Serializer) {
	this->PinnedStructs.Add(Struct);
	this->StructSerializers.Add(Struct, Serializer);
}

bool UPropertySerializer::ShouldDeserializeProperty(UProperty* Property) const {
	/* Skip deprecated properties */
	if (Property->HasAnyPropertyFlags(CPF_Deprecated)) {
		return false;
	}
	/* Skip blacklisted properties */
	if (this != nullptr && this && BlacklistedProperties.IsValidIndex(0) && BlacklistedProperties.Contains(Property)) {
		return false;
	}
	return true;
}

void UPropertySerializer::DeserializeStruct(UScriptStruct* Struct, const TSharedRef<FJsonObject>& Properties, void* OutValue) {
	FStructSerializer* StructSerializer = GetStructSerializer(Struct);
	StructSerializer->Deserialize(Struct, OutValue, Properties);
}

FStructSerializer* UPropertySerializer::GetStructSerializer(UScriptStruct* Struct) const {
	check(Struct);
	TSharedPtr<FStructSerializer> const* StructSerializer = StructSerializers.Find(Struct);
	return StructSerializer && ensure(StructSerializer->IsValid()) ? StructSerializer->Get() : FallbackStructSerializer.Get();
}

PRAGMA_ENABLE_OPTIMIZATION