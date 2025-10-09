// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/MaterialImporter.h"

#include "Materials/Material.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Dom/JsonObject.h"
#include "Factories/MaterialFactoryNew.h"
#include "Utilities/MathUtilities.h"
#include "MaterialGraph/MaterialGraph.h"

#include "Settings/JsonAsAssetSettings.h"

bool UMaterialImporter::ImportData() {
	return true;
}

// Filter out Material Graph Nodes
// by checking their subgraph expression (composite)
TArray<TSharedPtr<FJsonValue>> UMaterialImporter::FilterGraphNodesBySubgraphExpression(const FString& Outer) {
	TArray<TSharedPtr<FJsonValue>> ReturnValue = TArray<TSharedPtr<FJsonValue>>();

	/*
	* How this works:
	* 1. Find a Material Graph Node
	* 2. Get the Material Expression
	* 3. Compare SubgraphExpression to the one provided
	*    to the function
	*/
	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = TSharedPtr<FJsonObject>(Value->AsObject());
		const TSharedPtr<FJsonObject> Properties = TSharedPtr<FJsonObject>(ValueObject->GetObjectField("Properties"));

		const TSharedPtr<FJsonObject>* MaterialExpression;
		if (Properties->TryGetObjectField("MaterialExpression", MaterialExpression)) {
			TSharedPtr<FJsonValue> ExpValue = GetExportByObjectPath(*MaterialExpression);
			TSharedPtr<FJsonObject> Expression = TSharedPtr<FJsonObject>(ExpValue->AsObject());
			const TSharedPtr<FJsonObject> _Properties = TSharedPtr<FJsonObject>(Expression->GetObjectField("Properties"));

			const TSharedPtr<FJsonObject>* _SubgraphExpression;
			if (_Properties->TryGetObjectField("SubgraphExpression", _SubgraphExpression)) {
				if (Outer == GetExportNameOfSubobject(_SubgraphExpression->Get()->GetStringField("ObjectName")).ToString()) {
					ReturnValue.Add(ExpValue);
				}
			}
		}
	}

	return ReturnValue;
}