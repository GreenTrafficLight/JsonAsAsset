/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Importers/Types/Materials/MaterialInstanceConstantImporter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Dom/JsonObject.h"
#include "RHIDefinitions.h"
#include "MaterialShared.h"

bool IMaterialInstanceConstantImporter::Import() {
	return true;
}

void IMaterialInstanceConstantImporter::ReadStaticParameters(const TSharedPtr<FJsonObject>& StaticParameters, TArray<TSharedPtr<FJsonValue>>& StaticSwitchParameters, TArray<TSharedPtr<FJsonValue>>& StaticComponentMaskParameters) {
	if (StaticParameters->HasField(TEXT("StaticSwitchParameters"))) {
		TArray<TSharedPtr<FJsonValue>> Params = StaticParameters->GetArrayField("StaticSwitchParameters");
		ConvertParameterNamesToInfos(Params);

		for (TSharedPtr<FJsonValue> Parameter : Params) {
			StaticSwitchParameters.Add(TSharedPtr<FJsonValue>(Parameter));
		}
	}

	if (StaticParameters->HasField(TEXT("StaticComponentMaskParameters"))) {
		TArray<TSharedPtr<FJsonValue>> Params = StaticParameters->GetArrayField("StaticComponentMaskParameters");
		ConvertParameterNamesToInfos(Params);

		for (TSharedPtr<FJsonValue> Parameter : Params) {
			StaticComponentMaskParameters.Add(TSharedPtr<FJsonValue>(Parameter));
		}
	}
}

void IMaterialInstanceConstantImporter::ConvertParameterNamesToInfos(TArray<TSharedPtr<FJsonValue>>& Input) {
	/* Convert ParameterName to be inside ParameterInfo */
	for (const TSharedPtr<FJsonValue>& Parameter : Input) {
		const TSharedPtr<FJsonObject>& ParameterObject = Parameter->AsObject();

		if (ParameterObject->HasField(TEXT("ParameterName"))) {
			TSharedPtr<FJsonObject> ParameterInfo = MakeShared<FJsonObject>();

			ParameterInfo->SetStringField(TEXT("Name"), ParameterObject->GetStringField(TEXT("ParameterName")));
			ParameterObject->SetObjectField("ParameterInfo", ParameterInfo);

			/* Cleanup */
			ParameterObject->RemoveField("ParameterName");
		}
	}
}