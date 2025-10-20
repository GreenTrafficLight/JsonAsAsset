// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Blueprint/BlueprintGeneratedClassImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool IBlueprintGeneratedClassImporter::Import() {
	try {
		const TSharedPtr<FJsonObject> SuperStruct = JsonObject->GetObjectField(TEXT("SuperStruct"));
		UClass* ParentClass = LoadClass(SuperStruct);

		UBlueprint* Blueprint = nullptr;
		Blueprint = FindObject<UBlueprint>(Package, *AssetName);
		if (!Blueprint) {
			Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*AssetName), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

			const TSharedPtr<FJsonObject> SimpleConstructionScriptObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(JsonObject->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("SimpleConstructionScript")))->AsObject());
		
			
			// Root
			USCS_Node* RootNode = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode();
			const TArray<TSharedPtr<FJsonValue>> RootNodesObject = SimpleConstructionScriptObject->GetObjectField(TEXT("Properties"))->GetArrayField(TEXT("RootNodes"));
			HandleSimpleConstructionScript(Blueprint, RootNode, RootNodesObject, true);

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

void IBlueprintGeneratedClassImporter::HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot) {
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;

	for (const TSharedPtr<FJsonValue>& NodeObject : NodesObject) {
		const TSharedPtr<FJsonObject> SCSNodeObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(NodeObject->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> SCSNodePropertiesObject = SCSNodeObject->GetObjectField(TEXT("Properties"));

		UClass* ComponentClass = LoadClass(SCSNodePropertiesObject->GetObjectField(TEXT("ComponentClass")));

		USCS_Node* SCSNode = SCS->CreateNode(ComponentClass, *SCSNodePropertiesObject->GetStringField(TEXT("InternalVariableName")));

		const TSharedPtr<FJsonObject> ComponentTemplateObjectPath = SCSNodePropertiesObject->GetObjectField(TEXT("ComponentTemplate"));
		ReadComponentTemplate(SCSNode->ComponentTemplate, ComponentTemplateObjectPath);

		GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(SCSNodePropertiesObject,
			{
				"ParentComponentOrVariableName",
				"bIsParentComponentNative",
				"VariableGuid"
			}), SCSNode);

		if (bIsRoot) {
			SCS->AddNode(SCSNode);
		}
		else {
			Node->AddChildNode(SCSNode);
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

		if (SCSNodePropertiesObject->HasField(TEXT("ChildNodes")))
		{
			const TArray<TSharedPtr<FJsonValue>> ChildNodesObject = SCSNodePropertiesObject->GetArrayField("ChildNodes");
			HandleSimpleConstructionScript(BP, SCSNode, ChildNodesObject, false);

		}

		UE_LOG(LogTemp, Log, TEXT("TEST"));
	}
}

void IBlueprintGeneratedClassImporter::ReadComponentTemplate(UActorComponent* ComponentTemplate, const TSharedPtr<FJsonObject> ComponentTemplateObjectPath) {
	const TSharedPtr<FJsonObject> ComponentTemplateObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(ComponentTemplateObjectPath)->AsObject());
	const TSharedPtr<FJsonObject> ComponentTemplatePropertiesObject = ComponentTemplateObject->GetObjectField(TEXT("Properties"));

	if (ComponentTemplateObject->GetStringField(TEXT("Type")) == "StaticMeshComponent") {
		GetObjectSerializer()->DeserializeObjectProperties(ComponentTemplatePropertiesObject, ComponentTemplate);
	}
}