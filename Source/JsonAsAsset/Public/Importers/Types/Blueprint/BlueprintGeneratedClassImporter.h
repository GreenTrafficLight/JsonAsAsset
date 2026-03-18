/* Copyright JsonAsAsset Contributors 2024-2026 */

#pragma once

#include "Importers/Constructor/Importer.h"

#include "EdGraph/EdGraph.h"

class IBlueprintGeneratedClassImporter : public IImporter {
public:
	virtual UObject* CreateAsset(UObject* CreatedAsset) override;
	virtual bool Import() override;
private:
	void CreateVariables(UBlueprint* BP, FString OuterName, const TArray<TSharedPtr<FJsonValue>> ChildrensObjectPath, UEdGraph* FunctionGraph = nullptr);

	FEdGraphPinType GetPinType(UBlueprint* BP, const TSharedPtr<FJsonObject>& ChildrenObject);

	UEdGraph* CreateFunction(UBlueprint* BP, FUObjectExport FunctionExport);

	UClass* LoadParent(const TSharedPtr<FJsonObject>& ParentAsObject);

	void ReadFuncMap(UBlueprint* BP);

	void ReadInterfaces(UBlueprint* BP);

	void HandleSimpleConstructionScript(UBlueprint* BP, USCS_Node* Node, const TArray<TSharedPtr<FJsonValue>> NodesObject, bool bIsRoot);

	void HandleInheritableComponentHandler(UBlueprint* BP, FUObjectExport InheritableComponentHandlerExport);

	void ReadComponentTemplate(UBlueprint* BP, UActorComponent* ComponentTemplate, FUObjectExport ComponentTemplateExport);
};

REGISTER_IMPORTER(IBlueprintGeneratedClassImporter, {
	"BlueprintGeneratedClass"
}, "Blueprint Assets");