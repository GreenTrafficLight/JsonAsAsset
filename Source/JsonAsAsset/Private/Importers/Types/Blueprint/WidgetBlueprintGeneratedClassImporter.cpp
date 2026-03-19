
/* Copyright JsonAsAsset Contributors 2024-2026 */

#include "Importers/Types/Blueprint/WidgetBlueprintGeneratedClassImporter.h"

#include "Importers/Constructor/ImportReader.h"

#include "Utilities/BlueprintUtilities.h"
#include "Utilities/JsonUtilities.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#endif

#include "Blueprint/WidgetTree.h"



UObject* IWidgetBlueprintGeneratedClassImporter::CreateAsset(UObject* CreatedAsset) {
	const TSharedPtr<FJsonObject> SuperStruct = GetAssetData()->GetObjectField(TEXT("SuperStruct"));
	UClass* ParentClass = LoadClass(SuperStruct);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, GetPackage(), *GetAssetName(), BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

	return IImporter::CreateAsset(Blueprint);
}

bool IWidgetBlueprintGeneratedClassImporter::Import() {
	UBlueprint* Blueprint = nullptr;
	Blueprint = FindObject<UBlueprint>(GetPackage(), *GetAssetName());
	if (!Blueprint) {
		Blueprint = Create<UBlueprint>();
		UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint);

		FUObjectExport WidgetTreeExport = AssetContainer.GetExportByObjectPath(GetAssetData()->GetObjectField(TEXT("WidgetTree")));

		// Get the Root Widget from the Widget Tree Json Object
		FUObjectExport RootWidgetExport = AssetContainer.GetExportByObjectPath(WidgetTreeExport.GetProperties()->GetObjectField(TEXT("RootWidget")));
		// Create the root Widget and put it into the Widget Blueprint
		UCanvasPanel* RootWidget = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), RootWidgetExport.GetName());
		WidgetBP->WidgetTree->RootWidget = RootWidget;

		// Handle the slots of a canvas panel
		HandlePanelSlots(WidgetBP, RootWidgetExport, RootWidget);

		FUObjectExport ClassDefaultObjectExport = AssetContainer.GetExportByObjectPath(GetAssetData()->GetObjectField(TEXT("ClassDefaultObject")));
		if (ClassDefaultObjectExport.GetProperties().IsValid()) {
			UObject* ClassDefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
			GetObjectSerializer()->Parent = Blueprint->GeneratedClass->GetDefaultObject();
			GetObjectSerializer()->SetupExports(AssetContainer.JsonObjects);
			GetObjectSerializer()->DeserializeObjectProperties(ClassDefaultObjectExport.GetProperties(), ClassDefaultObject);
		}
	}

	return OnAssetCreation(Blueprint);
}

void IWidgetBlueprintGeneratedClassImporter::HandlePanelSlots(UWidgetBlueprint* WidgetBP, FUObjectExport PanelExport, UPanelWidget* Panel) {
	// Get the slots of the panel
	const TArray<TSharedPtr<FJsonValue>> Slots = PanelExport.GetProperties()->GetArrayField(TEXT("Slots"));
	// For each slot in the panel
	for (const TSharedPtr<FJsonValue>& Slot : Slots) {
		// Get the object data of the panel
		FUObjectExport PanelSlotExport = AssetContainer.GetExportByObjectPath(Slot->AsObject());
		// Get the object data of the slot
		FUObjectExport SlotContentExport = AssetContainer.GetExportByObjectPath(PanelSlotExport.GetProperties()->GetObjectField(TEXT("Content")));

		UClass* WidgetClass = nullptr;
		FString ClassName = SlotContentExport.JsonObject->GetStringField(TEXT("Class"));
		if (ClassName.StartsWith(TEXT("UScriptClass'"))) {
			ClassName = ClassName.Replace(TEXT("UScriptClass'"), TEXT("")).Replace(TEXT("'"), TEXT(""));

			WidgetClass = LoadClassFromPath(ClassName, TEXT("/Script/UMG"));
			const UJsonAsAssetSettings* Settings = GetSettings();
			if (WidgetClass == nullptr && !Settings->AssetSettings.ProjectName.IsEmpty()) {
				WidgetClass = LoadClassFromPath(ClassName, TEXT("/Script/") + Settings->AssetSettings.ProjectName);
			}
		}

		UWidget* NewWidget = nullptr;
		if (WidgetClass) {
			NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, SlotContentExport.GetName());
			// If the widget is a panel one
			if (WidgetClass->IsChildOf(UPanelWidget::StaticClass()))
			{
				HandlePanelSlots(WidgetBP, SlotContentExport, Cast<UPanelWidget>(NewWidget));
			}
			else if (WidgetClass->IsChildOf(UUserWidget::StaticClass()))
			{
				UE_LOG(LogTemp, Log, TEXT("%s is a UserWidget (could be blueprint or native)"), *WidgetClass->GetName());
			}
			else
			{
				GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(SlotContentExport.GetProperties(), {
					"Slot",
				}), NewWidget);
			}
		}
		else {
			// Create the user created Widget BP
			const TSharedPtr<FJsonObject>* TemplateObject;
			if (SlotContentExport.JsonObject->TryGetObjectField(TEXT("Template"), TemplateObject)) {
				FString Type, Name, Path, Outer;
				IImportReader::ParsePackageIndex(TemplateObject, Type, Name, Path, Outer);

				TSharedPtr<FJsonObject> SuperStruct = MakeShared<FJsonObject>();
				SuperStruct->SetStringField(TEXT("ObjectName"), TEXT("WidgetBlueprintGeneratedClass'") + Type + "'");
				SuperStruct->SetStringField(TEXT("ObjectPath"), Path);
				
				TObjectPtr<UObject> Parent;
				LoadExport(&SuperStruct, Parent);

				if (UWidgetBlueprintGeneratedClass* WidgetBPChild = Cast<UWidgetBlueprintGeneratedClass>(Parent.Get()))
				{
					NewWidget = WidgetBP->WidgetTree->ConstructWidget<UUserWidget>(WidgetBPChild, SlotContentExport.GetName());
				}
			}
		}

		if (NewWidget) {
			UPanelSlot* PanelSlotToPut = Panel->AddChild(NewWidget);

			// Get the type of slot and modify the properties
			const FString SlotType = PanelSlotExport.JsonObject->GetStringField(TEXT("Type"));

			if (SlotType == "CanvasPanelSlot") {
				GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(PanelSlotExport.GetProperties(), {
					"LayoutData",
					"ZOrder",
				}), Cast<UCanvasPanelSlot>(PanelSlotToPut));
			}
			else if (SlotType == "HorizontalBoxSlot")
			{
			}
			else if (SlotType == "OverlaySlot")
			{
			}
			else if (SlotType == "VerticalBoxSlot")
			{
			}

			if (PanelSlotToPut) {
				PanelSlotToPut->Modify();
			}
		}
	}
}