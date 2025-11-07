// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Blueprint/BlueprintGeneratedClassImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#endif

#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"

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
		CreateVariables(Blueprint, JsonObject->GetStringField(TEXT("Name")), JsonObject->GetArrayField(TEXT("Children")));

		ReadFuncMap(Blueprint);

		const TSharedPtr<FJsonObject> ClassDefaultObjectExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(JsonObject->GetObjectField(TEXT("ClassDefaultObject")))->AsObject());
		const TSharedPtr<FJsonObject> ClassDefaultObjectProperties = ClassDefaultObjectExport->GetObjectField(TEXT("Properties"));
		if (ClassDefaultObjectProperties.IsValid()) {
			UObject* ClassDefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
			GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(ClassDefaultObjectProperties, {
				"UberGraphFrame",
			}), ClassDefaultObject);
		}
	}

	return true;
}

void IBlueprintGeneratedClassImporter::CreateVariables(UBlueprint* BP, FString OuterName, const TArray<TSharedPtr<FJsonValue>> ChildrensObjectPath, UEdGraph* FunctionGraph) {
	for (const TSharedPtr<FJsonValue>& ChildrenObjectPath : ChildrensObjectPath) {
		const TSharedPtr<FJsonObject> ChildrenExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(ChildrenObjectPath->AsObject())->AsObject());

		const FString ChildrenOuterName = ChildrenExport->GetStringField(TEXT("Outer"));
		if (!ChildrenOuterName.Equals(OuterName)) {
			continue;
		}

		const FString ChildrenName = ChildrenExport->GetStringField(TEXT("Name"));
		const FString ChildrenPropertyFlags = ChildrenExport->GetStringField(TEXT("PropertyFlags"));

		FName NewVarName(*ChildrenName);
		UE_LOG(LogTemp, Log, TEXT("Variable name : '%s'."), *ChildrenName);

		// See EdGraphSchema_K2.cpp for the types
		FEdGraphPinType PinType = GetPinType(ChildrenExport);
		if (PinType.PinCategory.IsEmpty() || !PinType.PinSubCategoryObject.IsValid()) {
			UE_LOG(LogTemp, Warning, TEXT("Skipping %s: invalid type"), *ChildrenName);
			continue;
		}

		if (FunctionGraph == nullptr) {
			if (!FBlueprintEditorUtils::AddMemberVariable(BP, NewVarName, PinType)) {
				UE_LOG(LogTemp, Warning, TEXT("Variable '%s' already exists"), *ChildrenName);
				continue;
			}
		}
		else {
			// It's a variable inside the function
			if (ChildrenPropertyFlags.Contains(TEXT("Edit"))) {
				if (!FBlueprintEditorUtils::AddLocalVariable(BP, FunctionGraph, NewVarName, PinType)) {
					UE_LOG(LogTemp, Warning, TEXT("Local variable '%s' already exists in function '%s'."), *ChildrenName, *FunctionGraph->GetName());
				}
				continue;
			}

			/// In/Output nodes
			UK2Node_FunctionEntry* EntryNode = nullptr;
			for (UEdGraphNode* Node : FunctionGraph->Nodes)
			{
				EntryNode = Cast<UK2Node_FunctionEntry>(Node);
				if (EntryNode) {
					break;
				}
			}

			if (ChildrenPropertyFlags.Contains(TEXT("InParm"))) {
				EntryNode->CreateUserDefinedPin(ChildrenName, PinType, EGPD_Input);
			}

			// Create the output node
			if (ChildrenPropertyFlags.Contains(TEXT("OutParm"))) {
				UK2Node_FunctionResult* ResultNode = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(EntryNode);
				if (!ResultNode) {
					UE_LOG(LogTemp, Error, TEXT("Failed to find or create FunctionResult node for '%s'."), *ChildrenName);
					continue;
				}

				ResultNode->CreateUserDefinedPin(ChildrenName, PinType, EGPD_Output);
			}
		}


		/// Put the property flags of the variables
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
		///
	}
}

FEdGraphPinType IBlueprintGeneratedClassImporter::GetPinType(const TSharedPtr<FJsonObject>& Export)
{
	FEdGraphPinType PinType;

	const FString Type = Export->GetStringField(TEXT("Type"));

	if (Type == TEXT("ArrayProperty"))
	{
		// Arrays have an Inner property that describes element type
		const TSharedPtr<FJsonObject> InnerExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(Export->GetObjectField(TEXT("Inner")))->AsObject());

		if (InnerExport.IsValid()) {
			PinType = GetPinType(InnerExport);
			PinType.ContainerType = EPinContainerType::Array;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ArrayProperty missing Inner export"));
		}

		return PinType;
	}

	if (Type == TEXT("StrProperty"))
	{
		PinType.PinCategory = TEXT("string");
	}
	else if (Type == TEXT("IntProperty"))
	{
		PinType.PinCategory = TEXT("int");
	}
	else if (Type == TEXT("FloatProperty"))
	{
		PinType.PinCategory = TEXT("float");
	}
	else if (Type == TEXT("BoolProperty"))
	{
		PinType.PinCategory = TEXT("bool");
	}
	else if (Type == TEXT("StructProperty"))
	{
		PinType.PinCategory = TEXT("struct");
		UObject* StructObject = LoadStruct(Export->GetObjectField(TEXT("Struct")));
		if (StructObject)
			PinType.PinSubCategoryObject = Cast<UScriptStruct>(StructObject);
	}
	else if (Type == TEXT("ObjectProperty"))
	{
		PinType.PinCategory = TEXT("object");
		TObjectPtr<UObject> Object;
		LoadObject(&Export->GetObjectField(TEXT("PropertyClass")), Object);
		if (Object) {
			PinType.PinSubCategoryObject = Object.Get();
		}
			
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown variable type: %s"), *Type);
	}

	return PinType;
}

void IBlueprintGeneratedClassImporter::ReadFuncMap(UBlueprint* BP) {
	// Get the ubergraph
	UEdGraph* UberGraph = GetUberGraph(BP);

	// Remove all the event nodes before creating the new ones
	RemoveEventNodes(UberGraph);

	const TSharedPtr<FJsonObject> FunctionsObjectPath = JsonObject->GetObjectField(TEXT("FuncMap"));

	for (const auto& Pair : FunctionsObjectPath->Values) {
		const TSharedPtr<FJsonObject> FunctionExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(Pair.Value->AsObject())->AsObject());

		const FString FunctionName = FunctionExport->GetStringField(TEXT("Name"));

		UEdGraph* ExistingGraph = FindObject<UEdGraph>(BP, *FunctionName);
		if (ExistingGraph) {
			continue;
		}

		const FString FunctionFlags = FunctionExport->GetStringField(TEXT("FunctionFlags"));

		// Create function
		if (!FunctionFlags.Contains("FUNC_Event")) {
			UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
				BP,
				*FunctionName,
				UEdGraph::StaticClass(),
				UEdGraphSchema_K2::StaticClass()
			);

			FBlueprintEditorUtils::AddFunctionGraph<UFunction>(BP, NewGraph, true, nullptr);

			UEdGraphPin* EntryPin = nullptr;
			for (UEdGraphNode* Node : NewGraph->Nodes)
			{
				if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
				{
					EntryNode->CustomGeneratedFunctionName = FName(*FunctionName);
					break;
				}
			}

			CreateVariables(BP, FunctionName, FunctionExport->GetArrayField(TEXT("Children")), NewGraph);
		}
		// Create event
		else {
			const TSharedPtr<FJsonObject> SuperStruct = FunctionExport->GetObjectField(TEXT("SuperStruct"));
			const FString ObjectName = SuperStruct->GetStringField(TEXT("ObjectName")).Replace(TEXT("Function'"), TEXT("")).Replace(TEXT("'"), TEXT(""));
			const FString ObjectPath = SuperStruct->GetStringField(TEXT("ObjectPath"));
			FString OuterName, FunctionName;
			ObjectName.Split(TEXT(":"), &OuterName, &FunctionName);

			UFunction* Function = BP->ParentClass->FindFunctionByName(FName(*FunctionName));
			if (Function && UberGraph) {
				UK2Node_Event* EventNode = NewObject<UK2Node_Event>(UberGraph);
				EventNode->EventReference.SetFromField<UFunction>(Function, false);
				EventNode->bOverrideFunction = false;
				EventNode->CreateNewGuid();
				EventNode->PostPlacedNewNode();
				EventNode->AllocateDefaultPins();
				UberGraph->AddNode(EventNode, true, false);
			}
		}
	}
}

void IBlueprintGeneratedClassImporter::HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot) {
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;

	for (const TSharedPtr<FJsonValue>& NodeObject : NodesObject) {
		const TSharedPtr<FJsonObject> SCSNodeExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(NodeObject->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> SCSNodePropertiesObject = SCSNodeExport->GetObjectField(TEXT("Properties"));

		UClass* ComponentClass = LoadClass(SCSNodePropertiesObject->GetObjectField(TEXT("ComponentClass")));

		USCS_Node* SCSNode = SCS->CreateNode(ComponentClass, *SCSNodePropertiesObject->GetStringField(TEXT("InternalVariableName")));

		const TSharedPtr<FJsonObject> ComponentTemplateObjectPath = SCSNodePropertiesObject->GetObjectField(TEXT("ComponentTemplate"));
		ReadComponentTemplate(BP, SCSNode->ComponentTemplate, ComponentTemplateObjectPath);

		GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(SCSNodePropertiesObject, {
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
	const TSharedPtr<FJsonObject> ComponentTemplateExport = TSharedPtr<FJsonObject>(GetExportByObjectPath(ComponentTemplateObjectPath)->AsObject());
	const TSharedPtr<FJsonObject> ComponentTemplateProperties = ComponentTemplateExport->GetObjectField(TEXT("Properties"));

	UE_LOG(LogTemp, Log, TEXT("%s"), *ComponentTemplate->GetName());
	ComponentTemplate->Rename(*ComponentTemplateExport->GetStringField(TEXT("Name")), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	UE_LOG(LogTemp, Log, TEXT("%s"), *ComponentTemplate->GetName());

	//if (ComponentTemplateObject->GetStringField(TEXT("Type")) == "StaticMeshComponent") {
	GetObjectSerializer()->DeserializeObjectProperties(ComponentTemplateProperties, ComponentTemplate);
	//}
}