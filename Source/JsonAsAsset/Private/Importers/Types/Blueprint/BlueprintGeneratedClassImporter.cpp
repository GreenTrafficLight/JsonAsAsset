/* Copyright JsonAsAsset Contributors 2024-2026 */

#include "Importers/Types/Blueprint/BlueprintGeneratedClassImporter.h"

#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#include "Importers/Types/Blueprint/Utilities/BlueprintUtilities.h"

#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#endif

#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"

UObject* IBlueprintGeneratedClassImporter::CreateAsset(UObject* CreatedAsset) {
	TSharedPtr<FJsonObject> SuperAsObject = nullptr;
	// Parent is a C++ class
	if (GetAssetData()->HasField(TEXT("SuperStruct"))) {
		SuperAsObject = GetAssetData()->GetObjectField(TEXT("SuperStruct"));
	}
	// Parent is a blueprint
	else if (GetAssetData()->HasField(TEXT("Super"))) {
		SuperAsObject = GetAssetData()->GetObjectField(TEXT("Super"));
	}
	UClass* ParentClass = LoadParent(SuperAsObject);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, GetPackage(), *GetAssetName(), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	
	return IImporter::CreateAsset(Blueprint);
}

bool IBlueprintGeneratedClassImporter::Import() {
	UBlueprint* Blueprint = nullptr;
	Blueprint = FindObject<UBlueprint>(GetPackage(), *GetAssetName());
	if (!Blueprint) {
		Blueprint = Create<UBlueprint>();
		UClass* BlueprintClass = Blueprint->GeneratedClass;

		// If it inherit from an actor
		if (BlueprintClass->IsChildOf(AActor::StaticClass()))
		{
			if (GetAssetData()->HasField(TEXT("SimpleConstructionScript"))) {
				FUObjectExport SimpleConstructionScriptObjectExport = AssetContainer.GetExportByObjectPath(GetAssetData()->GetObjectField(TEXT("SimpleConstructionScript")));

				// Root
				USCS_Node* RootNode = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode();
				const TArray<TSharedPtr<FJsonValue>> RootNodesObject = SimpleConstructionScriptObjectExport.GetProperties()->GetArrayField(TEXT("RootNodes"));
				HandleSimpleConstructionScript(Blueprint, RootNode, RootNodesObject, true);
			}

			if (GetAssetData()->HasField(TEXT("InheritableComponentHandler"))) {
				FUObjectExport InheritableComponentHandlerExport = AssetContainer.GetExportByObjectPath(GetAssetData()->GetObjectField(TEXT("InheritableComponentHandler")));
				if (InheritableComponentHandlerExport.IsValid()) {
					HandleInheritableComponentHandler(Blueprint, InheritableComponentHandlerExport);
				}
			}
			
			if (GetAssetData()->HasField(TEXT("Interfaces"))) {
				ReadInterfaces(Blueprint);
			}
		}

		// Create the variables by looping the Children array an checking if it's a StrProperty, BoolProperty
		// Create them after creating the components to avoid them re-creating it
		CreateVariables(Blueprint, GetAssetName(), GetAssetData()->GetArrayField(TEXT("ChildProperties")));

		ReadFuncMap(Blueprint);

		FUObjectExport ClassDefaultObjectExport = AssetContainer.GetExportByObjectPath(GetAssetData()->GetObjectField(TEXT("ClassDefaultObject")));
		if (ClassDefaultObjectExport.GetProperties().IsValid()) {
			UObject* ClassDefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
			GetObjectSerializer()->SetExportForDeserialization(GetAssetExport(), ClassDefaultObject);
			GetObjectSerializer()->Parent = Blueprint->GeneratedClass->GetDefaultObject();
			GetObjectSerializer()->SetupExports(AssetContainer.JsonObjects);
			GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(ClassDefaultObjectExport.GetProperties(), {
				"UberGraphFrame",
			}), ClassDefaultObject);
		}
	}
	
	return OnAssetCreation(Blueprint);
}

void IBlueprintGeneratedClassImporter::CreateVariables(UBlueprint* BP, FString OuterName, const TArray<TSharedPtr<FJsonValue>> ChildrensObjectPath, UEdGraph* FunctionGraph) {
	for (const TSharedPtr<FJsonValue>& ChildrenObjectPath : ChildrensObjectPath) {
		const TSharedPtr<FJsonObject> ChildrenObject = ChildrenObjectPath->AsObject();

		const FString ChildrenName = ChildrenObject->GetStringField(TEXT("Name"));
		if (!ChildrenObject->HasField(TEXT("PropertyFlags"))){
			continue;
		}
		const FString ChildrenPropertyFlagsString = ChildrenObject->GetStringField(TEXT("PropertyFlags"));
		TArray<FString> ChildrenPropertyFlags;
		ChildrenPropertyFlagsString.ParseIntoArray(ChildrenPropertyFlags, TEXT(" | "), true);

		FName NewVarName(*ChildrenName);

		// See EdGraphSchema_K2.cpp for the types
		FEdGraphPinType PinType = GetPinType(BP, ChildrenObject);
		if (PinType.PinCategory.IsNone()) {
			UE_LOG(LogTemp, Error, TEXT("Skipping %s: invalid type"), *ChildrenName);
			continue;
		}

		if (FunctionGraph == nullptr) {
			if (!FBlueprintEditorUtils::AddMemberVariable(BP, NewVarName, PinType)) {
				UE_LOG(LogTemp, Warning, TEXT("Variable '%s' already exists"), *ChildrenName);
				continue;
			}
			UE_LOG(LogTemp, Log, TEXT("Variable name : '%s'."), *ChildrenName);
		}
		else {
			// It's a variable inside the function
			if (ChildrenPropertyFlags.Contains(TEXT("Edit"))) {
				if (!FBlueprintEditorUtils::AddLocalVariable(BP, FunctionGraph, NewVarName, PinType)) {
					UE_LOG(LogTemp, Warning, TEXT("Local variable '%s' already exists in function '%s'."), *ChildrenName, *FunctionGraph->GetName());
				}
				UE_LOG(LogTemp, Log, TEXT("Variable name : '%s'."), *ChildrenName);
				continue;
			}
			

			UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(FBlueprintEditorUtils::GetEntryNode(FunctionGraph));
			// Create the inputs node
			if (ChildrenPropertyFlags.Contains(TEXT("Parm")) && !ChildrenPropertyFlags.Contains(TEXT("OutParm")) && EntryNode) {
				EntryNode->CreateUserDefinedPin(FName(*ChildrenName), PinType, EGPD_Input);
				UE_LOG(LogTemp, Log, TEXT("Added input : '%s'."), *ChildrenName);
			}
			// Create the outputs node
			else if (ChildrenPropertyFlags.Contains(TEXT("OutParm"))) {
				UK2Node_FunctionResult* ResultNode = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(EntryNode);
				if (!ResultNode) {
					UE_LOG(LogTemp, Error, TEXT("Failed to find or create FunctionResult node for '%s'."), *ChildrenName);
					continue;
				}
				ResultNode->CreateUserDefinedPin(FName(*ChildrenName), PinType, EGPD_Output);
				UE_LOG(LogTemp, Log, TEXT("Added output : '%s'."), *ChildrenName);
			}
		}

		/*if (!ChildrenPropertyFlags.IsEmpty()) {
			if (ChildrenPropertyFlags.Contains(TEXT("Edit"))) {
				FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, NewVarName, false);
			}
			if (ChildrenPropertyFlags.Contains(TEXT("BlueprintVisible"))) {
				FBlueprintEditorUtils::SetBlueprintPropertyReadOnlyFlag(BP, NewVarName, false);
			}
			if (ChildrenPropertyFlags.Contains(TEXT("DisableEditOnInstance"))) {
				FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, NewVarName, true);
			}
		}*/
	}
}

FEdGraphPinType IBlueprintGeneratedClassImporter::GetPinType(UBlueprint* BP, const TSharedPtr<FJsonObject>& ChildrenObject)
{
	FEdGraphPinType PinType;

	if (!ChildrenObject.IsValid()) {
		return PinType;
	}

	const FString Name = ChildrenObject->GetStringField(TEXT("Name"));
	const FString Type = ChildrenObject->GetStringField(TEXT("Type"));

	if (Type == TEXT("ArrayProperty")) {
		FEdGraphPinType Inner = GetPinType(BP, ChildrenObject->GetObjectField(TEXT("Inner")));
		if (!Inner.PinCategory.IsNone()) {
			PinType.ContainerType = EPinContainerType::Array;
			PinType.PinCategory = Inner.PinCategory;
			PinType.PinSubCategory = Inner.PinSubCategory;
			PinType.PinSubCategoryObject = Inner.PinSubCategoryObject;
		}
	}
	else if (Type == TEXT("BoolProperty")) {
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Type == TEXT("ByteProperty")) {
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		const TSharedPtr<FJsonObject> EnumObject = ChildrenObject->GetObjectField(TEXT("Enum"));
		if (EnumObject.IsValid()) {
			TObjectPtr<UObject> EnumBP;
			LoadExport(&EnumObject, EnumBP);
			if (EnumBP) {
				PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
				PinType.PinSubCategoryObject = EnumBP.Get();
			}
		}
	}
	else if (Type == TEXT("EnumProperty")) {
		const TSharedPtr<FJsonObject> EnumAsObject = ChildrenObject->GetObjectField(TEXT("Enum"));
		if (EnumAsObject.IsValid()) {
			TObjectPtr<UObject> EnumBP;
			LoadExport(&EnumAsObject, EnumBP);
			if (EnumBP) {
				PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
				PinType.PinSubCategoryObject = EnumBP.Get();
			}
		}
	}
	else if (Type == TEXT("DoubleProperty")) {
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (Type == TEXT("DoubleProperty")) {
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (Type == TEXT("IntProperty")) {
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Type == TEXT("InterfaceProperty")) {
		const TSharedPtr<FJsonObject> InterfaceClassAsObject = ChildrenObject->GetObjectField(TEXT("InterfaceClass"));
		TObjectPtr<UObject> InterfaceObject;
		LoadExport(&InterfaceClassAsObject, InterfaceObject);
		if (InterfaceObject) {
			PinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
			PinType.PinSubCategoryObject = InterfaceObject.Get();
		}
	}
	else if (Type == TEXT("MapProperty")) {
		FEdGraphPinType Key = GetPinType(BP, ChildrenObject->GetObjectField(TEXT("KeyProp")));
		FEdGraphPinType Value = GetPinType(BP, ChildrenObject->GetObjectField(TEXT("ValueProp")));
		if (!Key.PinCategory.IsNone() && !Value.PinCategory.IsNone()) {
			PinType.ContainerType = EPinContainerType::Map;
			PinType.PinCategory = Key.PinCategory;
			PinType.PinSubCategory = Key.PinSubCategory;
			PinType.PinSubCategoryObject = Key.PinSubCategoryObject;
			PinType.PinValueType.TerminalCategory = Value.PinCategory;
			PinType.PinValueType.TerminalSubCategory = Value.PinSubCategory;
			PinType.PinValueType.TerminalSubCategoryObject = Value.PinSubCategoryObject;
		}
	}
	else if (Type == TEXT("MulticastInlineDelegateProperty")) {
		FUObjectExport FunctionExport = AssetContainer.GetExportByObjectPath(ChildrenObject->GetObjectField(TEXT("SignatureFunction")));
		UEdGraph* DelegateGraph = CreateFunction(BP, FunctionExport);
		if (DelegateGraph) {
			PinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
			PinType.PinSubCategoryObject = DelegateGraph;
		}
	}
	else if (Type == TEXT("NameProperty")) {
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Type == TEXT("ObjectProperty")) {
		const TSharedPtr<FJsonObject> PropertyClassObject = ChildrenObject->GetObjectField(TEXT("PropertyClass"));
		TObjectPtr<UObject> Object;
		LoadExport(&PropertyClassObject, Object);
		if (Object) {
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = Object.Get();
		}
	}
	else if (Type == TEXT("StrProperty")) {
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Type == TEXT("StructProperty") && !Name.Equals(TEXT("UberGraphFrame"))) {
		const TSharedPtr<FJsonObject> StructObject = ChildrenObject->GetObjectField(TEXT("Struct"));
		TObjectPtr<UObject> Object;
		LoadExport(&StructObject, Object);
		if (StructObject) {
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = Cast<UScriptStruct>(Object.Get());
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Unknown variable type: %s"), *Type);
	}

	return PinType;
}

UEdGraph* IBlueprintGeneratedClassImporter::CreateFunction(UBlueprint* BP, FUObjectExport FunctionExport) {
	if (!FunctionExport.IsJsonValid()) {
		return nullptr;
	}

	const FString FunctionName = FunctionExport.GetName().ToString();

	UEdGraph* ExistingGraph = FindObject<UEdGraph>(BP, *FunctionName);
	if (ExistingGraph) {
		return ExistingGraph;
	}

	const FString FunctionFlagsString = FunctionExport.JsonObject->GetStringField(TEXT("FunctionFlags"));
	TArray<FString> FunctionFlags;
	FunctionFlagsString.ParseIntoArray(FunctionFlags, TEXT(" | "), true);

	// Create function
	if (!FunctionFlags.Contains("FUNC_Event") && !FunctionFlags.Contains("FUNC_Delegate")) {
		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
			BP,
			*FunctionName,
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass()
		);

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		Schema->CreateDefaultNodesForGraph(*FunctionGraph);

		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(BP, FunctionGraph, true, nullptr);
		UE_LOG(LogTemp, Log, TEXT("Added Function Graph : '%s'."), *FunctionName);

		// Create the variables, input and output of the function
		if (FunctionExport.JsonObject->HasField(TEXT("ChildProperties"))) {
			const TArray<TSharedPtr<FJsonValue>> ChildPropertiesAsValueArray = FunctionExport.JsonObject->GetArrayField(TEXT("ChildProperties"));
			CreateVariables(BP, FunctionName, ChildPropertiesAsValueArray, FunctionGraph);
		}
		return FunctionGraph;
	}
	else if (FunctionFlags.Contains("FUNC_Delegate")) {
		FString DelegateName = FunctionName;
		if (DelegateName.EndsWith(TEXT("__DelegateSignature"))) {
			DelegateName = DelegateName.LeftChop(FString(TEXT("__DelegateSignature")).Len());
		}

		UEdGraph* DelegateGraph = FBlueprintEditorUtils::CreateNewGraph(
			BP,
			*DelegateName,
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass()
		);

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		Schema->CreateDefaultNodesForGraph(*DelegateGraph);
		Schema->CreateFunctionGraphTerminators(*DelegateGraph, (UClass*)nullptr);
		Schema->AddExtraFunctionFlags(DelegateGraph, (FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public));
		Schema->MarkFunctionEntryAsEditable(DelegateGraph, true);

		BP->DelegateSignatureGraphs.Add(DelegateGraph);
		UE_LOG(LogTemp, Log, TEXT("Added Delegate Signature Graph : '%s'."), *FunctionName);

		// Create the variables, input and output of the function
		if (FunctionExport.JsonObject->HasField(TEXT("ChildProperties"))) {
			const TArray<TSharedPtr<FJsonValue>> ChildPropertiesAsValueArray = FunctionExport.JsonObject->GetArrayField(TEXT("ChildProperties"));
			CreateVariables(BP, FunctionName, ChildPropertiesAsValueArray, DelegateGraph);
		}
		return DelegateGraph;
	}
	// Create event
	else {
		const TSharedPtr<FJsonObject> SuperStruct = FunctionExport.JsonObject->GetObjectField(TEXT("SuperStruct"));
		const FString ObjectName = SuperStruct->GetStringField(TEXT("ObjectName")).Replace(TEXT("Function'"), TEXT("")).Replace(TEXT("'"), TEXT(""));
		const FString ObjectPath = SuperStruct->GetStringField(TEXT("ObjectPath"));
		FString OuterName, EventName;
		ObjectName.Split(TEXT(":"), &OuterName, &EventName);

		UFunction* Function = BP->ParentClass->FindFunctionByName(FName(*EventName));
		UEdGraph* UberGraph = GetUberGraph(BP);
		if (Function && UberGraph) {
			UK2Node_Event* EventNode = NewObject<UK2Node_Event>(UberGraph);
			EventNode->EventReference.SetFromField<UFunction>(Function, false);
			EventNode->bOverrideFunction = false;
			EventNode->CreateNewGuid();
			EventNode->PostPlacedNewNode();
			EventNode->AllocateDefaultPins();
			UberGraph->AddNode(EventNode, true, false);
		}
		return UberGraph;
	}

	return nullptr;
}

UClass* IBlueprintGeneratedClassImporter::LoadParent(const TSharedPtr<FJsonObject>& ParentAsObject) {
	if (ParentAsObject.IsValid()) {
		UClass* ParentClass = LoadClass(ParentAsObject);
		if (!ParentClass) {
			TObjectPtr<UObject> Parent;
			LoadExport(&ParentAsObject, Parent);
			if (!Parent) {
				return nullptr;
			}
			ParentClass = LoadClass(ParentAsObject);
		}
		return ParentClass;
	}
	return nullptr;
}

void IBlueprintGeneratedClassImporter::ReadFuncMap(UBlueprint* BP) {
	// Get the ubergraph
	UEdGraph* UberGraph = GetUberGraph(BP);

	// Remove all the event nodes before creating the new ones
	RemoveEventNodes(UberGraph);

	const TSharedPtr<FJsonObject>* FuncMapObject;
	if (GetAssetData()->TryGetObjectField(TEXT("FuncMap"), FuncMapObject)) {
		for (const auto& Pair : (*FuncMapObject)->Values) {
			FUObjectExport FunctionExport = AssetContainer.GetExportByObjectPath(Pair.Value->AsObject());
			CreateFunction(BP, FunctionExport);
		}
	}
}

void IBlueprintGeneratedClassImporter::ReadInterfaces(UBlueprint* BP) {
	const TArray<TSharedPtr<FJsonValue>> InterfacesAsValueArray = GetAssetData()->GetArrayField(TEXT("Interfaces"));
	for (const TSharedPtr<FJsonValue>& InterfaceAsValue : InterfacesAsValueArray) {
		const TSharedPtr<FJsonObject> InterfaceAsObject = InterfaceAsValue->AsObject();
		const TSharedPtr<FJsonObject> ClassAsObject = InterfaceAsObject->GetObjectField(TEXT("Class"));
		UClass* ParentClass = LoadParent(InterfaceAsObject);
		if (ParentClass) {
			FBlueprintEditorUtils::ImplementNewInterface(BP, ParentClass->GetFName());
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Failed to add interface"));
		}
	}
}

void IBlueprintGeneratedClassImporter::HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot) {
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;

	for (const TSharedPtr<FJsonValue>& NodeObject : NodesObject) {
		FUObjectExport SCSNodeExport = AssetContainer.GetExportByObjectPath(NodeObject->AsObject());
		UClass* ComponentClass = LoadParent(SCSNodeExport.GetProperties()->GetObjectField(TEXT("ComponentClass")));
		FString InternalVariableName = SCSNodeExport.GetProperties()->GetStringField(TEXT("InternalVariableName"));
		if (InternalVariableName.Equals("DefaultSceneRoot")) { // Work-around if the Default Scene Root is in the RootNodes for some reasons
			continue;
		}
		USCS_Node* SCSNode = SCS->CreateNode(ComponentClass, *InternalVariableName);
		//SCSNode->Rename(*SCSNodeExport.GetName().ToString());

		FUObjectExport ComponentTemplateExport = AssetContainer.GetExportByObjectPath(SCSNodeExport.GetProperties()->GetObjectField(TEXT("ComponentTemplate")));
		ReadComponentTemplate(BP, SCSNode->ComponentTemplate, ComponentTemplateExport);

		GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(SCSNodeExport.GetProperties(), {
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
	
		if (SCSNodeExport.GetProperties()->HasField(TEXT("ChildNodes"))) {
			const TArray<TSharedPtr<FJsonValue>> ChildNodesObject = SCSNodeExport.GetProperties()->GetArrayField(TEXT("ChildNodes"));
			HandleSimpleConstructionScript(BP, SCSNode, ChildNodesObject, false);
		}
	}
}

void IBlueprintGeneratedClassImporter::HandleInheritableComponentHandler(UBlueprint* BP, FUObjectExport InheritableComponentHandlerExport) {
	TArray<USCS_Node*> AllNodes;
	GetAllSCSNodes(BP, AllNodes);

	const TArray<TSharedPtr<FJsonValue>> RecordsData = InheritableComponentHandlerExport.GetProperties()->GetArrayField(TEXT("Records"));
	for (const TSharedPtr<FJsonValue>& RecordData : RecordsData) {
		const TSharedPtr<FJsonObject> RecordDataObject = RecordData->AsObject();
		const TSharedPtr<FJsonObject> ComponentClassData = RecordDataObject->GetObjectField(TEXT("ComponentClass"));
		const TSharedPtr<FJsonObject> ComponentKeyData = RecordDataObject->GetObjectField(TEXT("ComponentKey"));

		USCS_Node* SCSNode = FindSCSNodeByName(AllNodes, *ComponentKeyData->GetStringField(TEXT("SCSVariableName")));
		if (!SCSNode) {
			UE_LOG(LogTemp, Error, TEXT("SCS Node of name '%s' not found"), *ComponentKeyData->GetStringField(TEXT("SCSVariableName")));
			continue;
		}

		UActorComponent* ComponentTemplate = SCSNode->ComponentTemplate;

		FUObjectExport ComponentTemplateExport = AssetContainer.GetExportByObjectPath(RecordDataObject->GetObjectField(TEXT("ComponentTemplate")));
		ReadComponentTemplate(BP, ComponentTemplate, ComponentTemplateExport);
	}
}

void IBlueprintGeneratedClassImporter::ReadComponentTemplate(UBlueprint* BP, UActorComponent* ComponentTemplate, FUObjectExport ComponentTemplateExport) {
	ComponentTemplate->Rename(*ComponentTemplateExport.GetName().ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	GetObjectSerializer()->DeserializeObjectProperties(ComponentTemplateExport.GetProperties(), ComponentTemplate);
}