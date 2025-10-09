// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/UMG/Blueprint/WidgetBlueprintGeneratedClassImporter.h"

#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "WidgetTree.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "Importers/Importer.h"

#include "Kismet2/KismetEditorUtilities.h"

// Shout-out to UEAssetToolkit
bool UWidgetBlueprintGeneratedClassImporter::ImportData() {
	try {
		const TSharedPtr<FJsonObject> SuperStruct = JsonObject->GetObjectField(TEXT("SuperStruct"));
		UClass* ParentClass = LoadClass(SuperStruct);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*FileName), BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint);

		const TSharedPtr<FJsonObject> WidgetTree = TSharedPtr<FJsonObject>(GetExportByObjectPath(JsonObject->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("WidgetTree")))->AsObject());
		
		// Get the Root Widget from the Widget Tree Json Object
		const TSharedPtr<FJsonObject> RootWidgetJsonObject = TSharedPtr<FJsonObject>(GetExportByObjectPath(WidgetTree->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("RootWidget")))->AsObject());
		// Create the root Widget and put it into the Widget Blueprint
		UCanvasPanel* RootWidget = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*RootWidgetJsonObject->GetStringField(TEXT("Name"))));
		WidgetBP->WidgetTree->RootWidget = RootWidget;

		// Handle the slots of a canvas panel
		HandleCanvasPanelSlots(WidgetBP, RootWidgetJsonObject, RootWidget);

		HandleAssetCreation(WidgetBP);

		UE_LOG(LogJson, Warning, TEXT("Hello World"));

		return true;


	} catch (const char* Exception) {
		UE_LOG(LogJson, Error, TEXT("%s"), *FString(Exception));
		return false;
	}


	return true;
}

void UWidgetBlueprintGeneratedClassImporter::HandleCanvasPanelSlots(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject> CanvasPanelJsonObject, UPanelWidget* Panel) {
	const TArray<TSharedPtr<FJsonValue>> Slots = CanvasPanelJsonObject->GetObjectField(TEXT("Properties"))->GetArrayField(TEXT("Slots"));
	for (const TSharedPtr<FJsonValue>& Slot : Slots) {
		const TSharedPtr<FJsonObject> PanelSlot = TSharedPtr<FJsonObject>(GetExportByObjectPath(Slot->AsObject())->AsObject());
		const TSharedPtr<FJsonObject> SlotContent = TSharedPtr<FJsonObject>(GetExportByObjectPath(PanelSlot->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("Content")))->AsObject());
		const TSharedPtr<FJsonObject> SlotContentProperties = SlotContent->GetObjectField(TEXT("Properties"));

		// Get the type of widget and create it
		UWidget* WidgetToPut = nullptr;
		const FString WidgetType = SlotContent->GetStringField(TEXT("Type"));
		if (WidgetType == "CanvasPanel") {
			WidgetToPut = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*SlotContent->GetStringField(TEXT("Name"))));
			HandleCanvasPanelSlots(WidgetBP, SlotContent, Cast<UCanvasPanel>(WidgetToPut));
		}
		else if (WidgetType == "HorizontalBox") {
			WidgetToPut = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox >(UHorizontalBox::StaticClass(), FName(*SlotContent->GetStringField(TEXT("Name"))));
			HandleCanvasPanelSlots(WidgetBP, SlotContent, Cast<UHorizontalBox>(WidgetToPut));
		}
		else if (WidgetType == "VerticalBox") {
			WidgetToPut = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*SlotContent->GetStringField(TEXT("Name"))));
			HandleCanvasPanelSlots(WidgetBP, SlotContent, Cast<UVerticalBox>(WidgetToPut));
		}
		else if (WidgetType == "Border") {
			WidgetToPut = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*SlotContent->GetStringField(TEXT("Name"))));
			GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(SlotContentProperties,
				{
					"Slot",
				}), WidgetToPut);
		}
		else if (WidgetType == "Image") {
			WidgetToPut = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*SlotContent->GetStringField(TEXT("Name"))));
			GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(SlotContentProperties,
				{
					"Slot",
				}), WidgetToPut);
		}
		else if (WidgetType == "ProgressBar") {
			// Import ResourceObject before ?
			WidgetToPut = WidgetBP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*SlotContent->GetStringField(TEXT("Name"))));
			GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(SlotContentProperties,
				{
					"Slot",
				}), WidgetToPut);
		}
		else if (WidgetType == "TextBlock") {
			// Import Font before ?
			WidgetToPut = WidgetBP->WidgetTree->ConstructWidget<UTextBlock >(UTextBlock::StaticClass(), FName(*SlotContent->GetStringField(TEXT("Name"))));
			GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(SlotContentProperties,
				{
					"Slot",
				}), WidgetToPut);
		}
		else {
			const TSharedPtr<FJsonObject>* TemplateObj;
			if (SlotContent->TryGetObjectField(TEXT("Template"), TemplateObj)) {
				
				/*FString Type, ObjectPath;
				// Split at the first `'` to separate type from path
				if (FullString.Split(TEXT("'"), &Type, &ObjectPath))
				{
					// Remove trailing quote if it exists
					ObjectPath = ObjectPath.Replace(TEXT("'"), TEXT(""));

					ObjectPath = ObjectPath.Replace(TEXT("Nimbus/Content"), TEXT("/Game"));

					// Optional: split off the class name if you need the name separately
					FString OuterPath, Name;
					if (ObjectPath.Split(TEXT("."), &OuterPath, &Name))
					{
						UE_LOG(LogTemp, Log, TEXT("Type: %s"), *Type);
						UE_LOG(LogTemp, Log, TEXT("Outer Path: %s"), *OuterPath);
						UE_LOG(LogTemp, Log, TEXT("Name: %s"), *Name);
					}
				}*/


				/*FString Type, Name, Path, Outer;
				IImporter* Importer = new IImporter();
				Importer->ParsePackageIndex(TemplateObj, Type, Name, Path, Outer);

				UObject* Object = NULL;
				Importer->DownloadWrapper(Object, TEXT("WidgetBlueprintGeneratedClass"), Type, Path);*/
			}
		}


		if (WidgetToPut) {
			
			// Get the type of slot and modify it
			UPanelSlot* SlotWidgetToPut = nullptr;
			const FString SlotType = PanelSlot->GetStringField(TEXT("Type"));
			if (SlotType == "CanvasPanelSlot") {
				if (UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(Panel))
				{
					UCanvasPanelSlot* CanvasPanelSlot = CanvasPanel->AddChildToCanvas(WidgetToPut);
					// Deserialize the CanvasPanelSlot properties
					const TSharedPtr<FJsonObject> PanelSlotProperties = PanelSlot->GetObjectField(TEXT("Properties"));
					GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(PanelSlotProperties,
						{
							"LayoutData",
							"ZOrder",
						}), CanvasPanelSlot);

					SlotWidgetToPut = CanvasPanelSlot;
				}
			}
			else if (SlotType == "HorizontalBoxSlot")
			{
				if (UHorizontalBox* HBox = Cast<UHorizontalBox>(Panel))
				{
					UHorizontalBoxSlot* HBoxSlot = HBox->AddChildToHorizontalBox(WidgetToPut);
					SlotWidgetToPut = HBoxSlot;
				}
			}
			else if (SlotType == "VerticalBoxSlot") 
			{
				if (UVerticalBox* VBox = Cast<UVerticalBox>(Panel))
				{
					UVerticalBoxSlot* VBoxSlot = VBox->AddChildToVerticalBox(WidgetToPut);
					SlotWidgetToPut = VBoxSlot;
				}
			}

			if (SlotWidgetToPut) {
				SlotWidgetToPut->Modify();
			}
		}
	}
}
