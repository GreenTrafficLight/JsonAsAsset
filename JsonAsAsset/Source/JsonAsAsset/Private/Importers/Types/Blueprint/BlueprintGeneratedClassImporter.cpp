// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Blueprint/BlueprintGeneratedClassImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraphSchema_K2.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool IBlueprintGeneratedClassImporter::Import() {
	const TSharedPtr<FJsonObject> SuperStruct = JsonObject->GetObjectField(TEXT("SuperStruct"));
	UClass* ParentClass = LoadClass(SuperStruct);

	UBlueprint* Blueprint = nullptr;
	Blueprint = FindObject<UBlueprint>(Package, *AssetName);
	if (!Blueprint) {
		Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*AssetName), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

		// If it inherit from an actor
		if (ParentClass && ParentClass->IsChildOf(AActor::StaticClass()))
		{
			const TSharedPtr<FJsonObject> SimpleConstructionScriptObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(JsonObject->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("SimpleConstructionScript")))->AsObject());

			// Root
			USCS_Node* RootNode = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode();
			const TArray<TSharedPtr<FJsonValue>> RootNodesObject = SimpleConstructionScriptObject->GetObjectField(TEXT("Properties"))->GetArrayField(TEXT("RootNodes"));
			HandleSimpleConstructionScript(Blueprint, RootNode, RootNodesObject, true);
		}

		// Create the variables by looping the Children array an checking if it's a StrProperty, BoolProperty
		// Create them after creating the components to avoid them re-creating it
		const TArray<TSharedPtr<FJsonValue>> ChildrensObjectPath = JsonObject->GetArrayField(TEXT("Children"));
		CreateVariables(Blueprint, JsonObject->GetStringField(TEXT("Name")), ChildrensObjectPath);

		const TSharedPtr<FJsonObject> ClassDefaultObjectExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(JsonObject->GetObjectField(TEXT("ClassDefaultObject")))->AsObject());
		const TSharedPtr<FJsonObject> ClassDefaultObjectPropertiesObject = ClassDefaultObjectExport->GetObjectField(TEXT("Properties"));
		if (ClassDefaultObjectPropertiesObject.IsValid()) {
			UObject* ClassDefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
			GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(ClassDefaultObjectPropertiesObject,
				{
					"UberGraphFrame",
				}), ClassDefaultObject);
		}
	}

	return true;
}

void IBlueprintGeneratedClassImporter::CreateVariables(UBlueprint* BP, FString OuterName, const TArray<TSharedPtr<FJsonValue>> ChildrensObjectPath) {
	for (const TSharedPtr<FJsonValue>& ChildrenObjectPath : ChildrensObjectPath) {
		const TSharedPtr<FJsonObject> ChildrenExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(ChildrenObjectPath->AsObject())->AsObject());

		const FString ChildrenOuterName = ChildrenExport->GetStringField(TEXT("Outer"));
		if (!ChildrenOuterName.Equals(OuterName)) {
			continue;
		}

		const FString ChildrenType = ChildrenExport->GetStringField(TEXT("Type"));
		const FString ChildrenName = ChildrenExport->GetStringField(TEXT("Name"));
		const FString ChildrenPropertyFlags = ChildrenExport->GetStringField(TEXT("PropertyFlags"));

		FName NewVarName(*ChildrenName);

		// See EdGraphSchema_K2.cpp for the types
		FEdGraphPinType PinType;
		if (ChildrenType == TEXT("StrProperty"))
			PinType.PinCategory = TEXT("string");
		else if (ChildrenType == TEXT("IntProperty"))
			PinType.PinCategory = TEXT("int");
		else if (ChildrenType == TEXT("FloatProperty"))
			PinType.PinCategory = TEXT("float");
		else if (ChildrenType == TEXT("BoolProperty"))
			PinType.PinCategory = TEXT("bool");
		else if (ChildrenType == TEXT("StructProperty")) {
			PinType.PinCategory = TEXT("struct");
			UObject* StructObject = LoadStruct(ChildrenExport->GetObjectField(TEXT("Struct")));
			if (!StructObject) {
				continue;
			}
			PinType.PinSubCategoryObject = Cast<UScriptStruct>(StructObject);
		}
		// TO DO : Add ObjectProperty, and add ignoring objectproperty that are components
		else {
			UE_LOG(LogTemp, Warning, TEXT("Unknown variable type: %s"), *ChildrenType);
			continue;
		}

		if (!FBlueprintEditorUtils::AddMemberVariable(BP, NewVarName, PinType))
		{
			UE_LOG(LogTemp, Warning, TEXT("Variable '%s' already exists"), *ChildrenName);
			continue;
		}

		FBPVariableDescription* VarDesc = nullptr;
		for (FBPVariableDescription& V : BP->NewVariables)
		{
			if (V.VarName == NewVarName)
			{
				VarDesc = &V;
				break;
			}
		}

		if (!VarDesc) {
			UE_LOG(LogTemp, Warning, TEXT("Could not find newly added variable '%s' in NewVariables."), *ChildrenName);
			continue;
		}

		if (!ChildrenPropertyFlags.IsEmpty()) {
			if (ChildrenPropertyFlags.Contains(TEXT("Edit"))) {
				VarDesc->PropertyFlags |= CPF_Edit;
			}
			if (ChildrenPropertyFlags.Contains(TEXT("BlueprintVisible"))) {
				VarDesc->PropertyFlags |= CPF_BlueprintVisible;
			}
			if (ChildrenPropertyFlags.Contains(TEXT("DisableEditOnInstance"))) {
				VarDesc->PropertyFlags |= CPF_DisableEditOnInstance;
			}
		}
	}
}

void IBlueprintGeneratedClassImporter::HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot) {
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;

	for (const TSharedPtr<FJsonValue>& NodeObject : NodesObject) {
		const TSharedPtr<FJsonObject> SCSNodeObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(NodeObject->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> SCSNodePropertiesObject = SCSNodeObject->GetObjectField(TEXT("Properties"));

		UClass* ComponentClass = LoadClass(SCSNodePropertiesObject->GetObjectField(TEXT("ComponentClass")));

		USCS_Node* SCSNode = SCS->CreateNode(ComponentClass, *SCSNodePropertiesObject->GetStringField(TEXT("InternalVariableName")));

		const TSharedPtr<FJsonObject> ComponentTemplateObjectPath = SCSNodePropertiesObject->GetObjectField(TEXT("ComponentTemplate"));
		ReadComponentTemplate(BP, SCSNode->ComponentTemplate, ComponentTemplateObjectPath);

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

void IBlueprintGeneratedClassImporter::ReadComponentTemplate(UBlueprint* BP, UActorComponent* ComponentTemplate, const TSharedPtr<FJsonObject> ComponentTemplateObjectPath) {
	const TSharedPtr<FJsonObject> ComponentTemplateObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(ComponentTemplateObjectPath)->AsObject());
	const TSharedPtr<FJsonObject> ComponentTemplatePropertiesObject = ComponentTemplateObject->GetObjectField(TEXT("Properties"));

	UE_LOG(LogTemp, Log, TEXT("%s"), *ComponentTemplate->GetName());
	ComponentTemplate->Rename(*ComponentTemplateObject->GetStringField(TEXT("Name")), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	UE_LOG(LogTemp, Log, TEXT("%s"), *ComponentTemplate->GetName());

	//if (ComponentTemplateObject->GetStringField(TEXT("Type")) == "StaticMeshComponent") {
	GetObjectSerializer()->DeserializeObjectProperties(ComponentTemplatePropertiesObject, ComponentTemplate);
	//}
}