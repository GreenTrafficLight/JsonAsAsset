// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Blueprint/WidgetBlueprintGeneratedClassImporter.h"

#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

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

#include "WidgetTree.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "Importers/Constructor/Importer.h"

#include "Kismet2/KismetEditorUtilities.h"

// Shout-out to UEAssetToolkit
bool IWidgetBlueprintGeneratedClassImporter::Import() {
	try {
		const TSharedPtr<FJsonObject> SuperStruct = JsonObject->GetObjectField(TEXT("SuperStruct"));
		UClass* ParentClass = LoadClass(SuperStruct);
		
		UBlueprint* Blueprint = nullptr;
		Blueprint = FindObject<UBlueprint>(Package, *AssetName);
		if (!Blueprint) {
			Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*AssetName), BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
			UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint);

			const TSharedPtr<FJsonObject> WidgetTree = TSharedPtr<FJsonObject>(GetExportByObjectPath(JsonObject->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("WidgetTree")))->AsObject());

			// Get the Root Widget from the Widget Tree Json Object
			const TSharedPtr<FJsonObject> RootWidgetJsonObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(WidgetTree->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("RootWidget")))->AsObject());
			// Create the root Widget and put it into the Widget Blueprint
			UCanvasPanel* RootWidget = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*RootWidgetJsonObject->GetStringField(TEXT("Name"))));
			WidgetBP->WidgetTree->RootWidget = RootWidget;

			// Handle the slots of a canvas panel
			HandlePanelSlots(WidgetBP, RootWidgetJsonObject, RootWidget);

			HandleAssetCreation(WidgetBP);

			WidgetBP->MarkPackageDirty();

			SavePackage();
		}

		return true;

	} catch (const char* Exception) {
		UE_LOG(LogJson, Error, TEXT("%s"), *FString(Exception));
		return false;
	}


	return true;
}

void IWidgetBlueprintGeneratedClassImporter::HandlePanelSlots(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject> PanelJsonObject, UPanelWidget* Panel) {
	// Get the slots of the panel
	const TArray<TSharedPtr<FJsonValue>> Slots = PanelJsonObject->GetObjectField(TEXT("Properties"))->GetArrayField(TEXT("Slots"));
	// For each slot in the panel
	for (const TSharedPtr<FJsonValue>& Slot : Slots) {
		// Get the object data of the panel
		const TSharedPtr<FJsonObject> PanelSlot = TSharedPtr<FJsonObject>(GetExportByObjectPath(Slot->AsObject())->AsObject());
		// Get the object data of the slot
		const TSharedPtr<FJsonObject> SlotContent = TSharedPtr<FJsonObject>(GetExportByObjectPath(PanelSlot->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("Content")))->AsObject());
		const TSharedPtr<FJsonObject> SlotContentProperties = SlotContent->GetObjectField(TEXT("Properties"));

		UClass* WidgetClass = GetWidgetClass(SlotContent);
		UWidget* NewWidget = nullptr;
		if (WidgetClass) {
			NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*SlotContent->GetStringField(TEXT("Name"))));
			// If the widget is a panel one
			if (WidgetClass->IsChildOf(UPanelWidget::StaticClass()))
			{
				HandlePanelSlots(WidgetBP, SlotContent, Cast<UPanelWidget>(NewWidget));
			}
			else if (WidgetClass->IsChildOf(UUserWidget::StaticClass()))
			{
				UE_LOG(LogTemp, Log, TEXT("%s is a UserWidget (could be blueprint or native)"), *WidgetClass->GetName());
			}
			else
			{
				GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(SlotContentProperties,
					{
						"Slot",
					}), NewWidget);
			}
		}
		else {
			// Create the user created Widget BP
			const TSharedPtr<FJsonObject>* TemplateObj;
			if (SlotContent->TryGetObjectField(TEXT("Template"), TemplateObj)) {
				FString Type, Name, Path, Outer;
				IImporter* Importer = new IImporter();
				Importer->ParsePackageIndex(TemplateObj, Type, Name, Path, Outer);

				TObjectPtr<UObject> Object;
				Object = Importer->DownloadWrapper(Object, TEXT("WidgetBlueprintGeneratedClass"), Type, Path);

				if (UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Object.Get()))
				{
					NewWidget = WidgetBP->WidgetTree->ConstructWidget<UUserWidget>(WidgetClass, FName(*SlotContent->GetStringField(TEXT("Name"))));
				}
			}
		}

		if (NewWidget) {
			UPanelSlot* PanelSlotToPut = Panel->AddChild(NewWidget);

			// Get the type of slot and modify the properties
			const FString SlotType = PanelSlot->GetStringField(TEXT("Type"));
			const TSharedPtr<FJsonObject> SlotProperties = PanelSlot->GetObjectField(TEXT("Properties"));

			if (SlotType == "CanvasPanelSlot") {
				GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(SlotProperties,
					{
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

UClass* IWidgetBlueprintGeneratedClassImporter::GetWidgetClass(const TSharedPtr<FJsonObject>& ObjData) {
	UClass* WidgetClass = nullptr;

	FString ClassName = ObjData->GetStringField(TEXT("Class"));


	if (ClassName.StartsWith(TEXT("UScriptClass'"))) {
		ClassName = ClassName.Replace(TEXT("UScriptClass'"), TEXT("")).Replace(TEXT("'"), TEXT(""));

		WidgetClass = LoadClassFromPath(ClassName, TEXT("/Script/UMG"));
		if (WidgetClass == nullptr) {
			WidgetClass = LoadClassFromPath(ClassName, TEXT("/Script/Nimbus"));
		}
	}

	// Load the Widget BP Class
	/*if (ClassName.StartsWith(TEXT("WidgetBlueprintGeneratedClass'"))) {
		ClassName = ClassName.Replace(TEXT("WidgetBlueprintGeneratedClass'"), TEXT("")).Replace(TEXT("'"), TEXT(""));

		FString Path = ClassName.Replace(TEXT("Nimbus/Content"), TEXT("/Game"));
		FString Name;
		Path.Split(TEXT("."), nullptr, &Name);

		IImporter* Importer = new IImporter();

		UObject* Object = NULL;
		Object = Importer->DownloadWrapper(Object, TEXT("WidgetBlueprintGeneratedClass"), Name, Path);
	}*/
	
	return WidgetClass;
}
