/* Copyright JsonAsAsset Contributors 2024-2026 */

#pragma once

#include "Importers/Constructor/Importer.h"

#include "WidgetBlueprint.h"
#include "Components/PanelWidget.h"

class IWidgetBlueprintGeneratedClassImporter : public IImporter {
public:
	virtual UObject* CreateAsset(UObject* CreatedAsset) override;
	virtual bool Import() override;
private:
	void HandlePanelSlots(UWidgetBlueprint* WidgetBP, FUObjectExport PanelExport, UPanelWidget* Panel);
};

REGISTER_IMPORTER(IWidgetBlueprintGeneratedClassImporter, {
	"WidgetBlueprintGeneratedClass"
}, "Blueprint Assets");