// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/MaterialInstanceConstantImporter.h"

#include "Dom/JsonObject.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Utilities/JsonUtilities.h"
#include "RHIDefinitions.h"

#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "MaterialShared.h"

bool UMaterialInstanceConstantImporter::Import() {
	return true;
}
