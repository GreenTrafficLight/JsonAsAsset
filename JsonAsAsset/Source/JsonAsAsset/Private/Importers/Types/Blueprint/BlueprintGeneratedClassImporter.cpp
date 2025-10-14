// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Blueprint/BlueprintGeneratedClassImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#include "Importers/Importer.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool UBlueprintGeneratedClassImporter::ImportData() {
	try {
		const TSharedPtr<FJsonObject> SuperStruct = JsonObject->GetObjectField(TEXT("SuperStruct"));
		UClass* ParentClass = LoadClass(SuperStruct);

		UBlueprint* Blueprint = nullptr;
		Blueprint = FindObject<UBlueprint>(Package, *FileName);
		if (!Blueprint) {
			Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*FileName), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

			const TSharedPtr<FJsonObject> SimpleConstructionScriptObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(JsonObject->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("SimpleConstructionScript")))->AsObject());
		
			
			// Root
			USCS_Node* RootNode = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode();
			const TArray<TSharedPtr<FJsonValue>> RootNodesObject = SimpleConstructionScriptObject->GetObjectField(TEXT("Properties"))->GetArrayField(TEXT("RootNodes"));
			HandleSimpleConstructionScript(Blueprint, RootNode, RootNodesObject);

			UE_LOG(LogTemp, Log, TEXT("TEST"));
		}

		return true;

	}
	catch (const char* Exception) {
		UE_LOG(LogJson, Error, TEXT("%s"), *FString(Exception));
		return false;
	}


	return true;
}

void UBlueprintGeneratedClassImporter::HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject) {
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;

	for (const TSharedPtr<FJsonValue>& NodeObject : NodesObject) {
		const TSharedPtr<FJsonObject> SCSNodeObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(NodeObject->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> SCSNodePropertiesObject = SCSNodeObject->GetObjectField(TEXT("Properties"));

		UClass* ComponentClass = LoadClass(SCSNodePropertiesObject->GetObjectField(TEXT("ComponentClass")));

		/*FString ParentVariableName;
		if (SCSNodePropertiesObject->TryGetStringField("ParentComponentOrVariableName", ParentVariableName)) {
			AActor* CDO = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
			if (CDO) {
				UActorComponent* ParentComponent = FindComponentByName(CDO, *ParentVariableName);
				if (ParentComponent) {
					USceneComponent* NewComp = NewObject<USceneComponent>(
						CDO,
						ComponentClass,
						*SCSNodePropertiesObject->GetStringField(TEXT("InternalVariableName"))
					);
					NewComp->SetupAttachment(CastChecked<USceneComponent>(ParentComponent));
					NewComp->RegisterComponent();
				}

			}
		}
		else {
			USCS_Node* SCSNode = SCS->CreateNode(ComponentClass, *SCSNodePropertiesObject->GetStringField(TEXT("InternalVariableName")));

			SCS->AddNode(SCSNode);
		}*/

		USCS_Node* SCSNode = SCS->CreateNode(ComponentClass, *SCSNodePropertiesObject->GetStringField(TEXT("InternalVariableName")));

		GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(SCSNodePropertiesObject,
			{
				"ParentComponentOrVariableName",
				"bIsParentComponentNative",
				"VariableGuid"
			}), SCSNode);

		SCS->AddNode(SCSNode);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

		/*const TArray<TSharedPtr<FJsonValue>>* ChildNodesObject;
		SCSNodePropertiesObject->TryGetArrayField("ChildNodes", ChildNodesObject);
		if (ChildNodesObject) {
			HandleSimpleConstructionScript(BP, SCSNode, *ChildNodesObject);
		}*/

		UE_LOG(LogTemp, Log, TEXT("TEST"));
	}
}