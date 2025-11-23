// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Utilities/Serializers/PropertyUtilities.h"
#include "Windows/WindowsPlatformApplicationMisc.h"
#include "Utilities/Serializers/ObjectUtilities.h"
#include "Settings/JsonAsAssetSettings.h"
#include "Interfaces/IMainFrameModule.h"

#include "Windows/WindowsHWrapper.h"
#include "Interfaces/IHttpRequest.h"
#include "DesktopPlatformModule.h"
#include "ContentBrowserModule.h"
#include "IDesktopPlatform.h"
#include "RemoteUtilities.h"
#include "AssetUtilities.h"
#if !UE4_18_BELOW
#include "PluginUtils.h"
#endif
#include "HttpModule.h"
#include "IMessageLogListing.h"
#include "ISettingsModule.h"
#include "TlHelp32.h"

#include "MessageLogModule.h"
#include "Logging/MessageLog.h"
#include "Modules/LogCategory.h"

#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "Engine/SCS_Node.h"

/**
 * Get the asset currently selected in the Content Browser.
 *
 * @return Selected Asset
 */
template <typename T>
T* GetSelectedAsset(const bool SuppressErrors = false, FString OptionalAssetNameCheck = "") {
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() == 0) {
		if (SuppressErrors == true) {
			return nullptr;
		}

		GLog->Log("JsonAsAsset: [GetSelectedAsset] None selected, returning nullptr.");

		const FText DialogText = FText::Format(
			FText::FromString(TEXT("Importing an asset of type '{0}' requires a base asset selected to modify. Select one in your content browser.")),
			FText::FromString(T::StaticClass()->GetName())
		);

		FMessageDialog::Open(EAppMsgType::Ok, DialogText);

		return nullptr;
	}

	UObject* SelectedAsset = SelectedAssets[0].GetAsset();
	T* CastedAsset = Cast<T>(SelectedAsset);

	if (!CastedAsset) {
		if (SuppressErrors == true) {
			return nullptr;
		}

		GLog->Log("JsonAsAsset: [GetSelectedAsset] Selected asset is not of the required class, returning nullptr.");

		const FText DialogText = FText::Format(
			FText::FromString(TEXT("The selected asset is not of type '{0}'. Please select a valid asset.")),
			FText::FromString(T::StaticClass()->GetName())
		);

		FMessageDialog::Open(EAppMsgType::Ok, DialogText);

		return nullptr;
	}

	if (CastedAsset && OptionalAssetNameCheck != "" && !CastedAsset->GetName().Equals(OptionalAssetNameCheck)) {
		CastedAsset = nullptr;
	}

	return CastedAsset;
}

inline void SpawnPrompt(const FString& Title, const FString& Text) {
	FText DialogTitle = FText::FromString(Title);
	const FText DialogMessage = FText::FromString(Text);

	FMessageDialog::Open(EAppMsgType::Ok, DialogMessage);
}

inline bool IsProcessRunning(const FString& ProcessName) {
	bool bIsRunning = false;

	/* Convert FString to WCHAR */
	const TCHAR* ProcessNameChar = *ProcessName;

	const HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (Snapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 ProcessEntry;
		ProcessEntry.dwSize = sizeof(ProcessEntry);

		if (Process32First(Snapshot, &ProcessEntry)) {
			do {
				if (_wcsicmp(ProcessEntry.szExeFile, ProcessNameChar) == 0) {
					bIsRunning = true;
					break;
				}
			} while (Process32Next(Snapshot, &ProcessEntry));
		}

		CloseHandle(Snapshot);
	}

	return bIsRunning;
}

inline TSharedPtr<FJsonObject> GetExport(const FString& Type, TArray<TSharedPtr<FJsonValue>> AllJsonObjects, const bool bGetProperties = false) {
	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = Value->AsObject();

		if (ValueObject->GetStringField(TEXT("Type")) == Type) {
			if (bGetProperties) {
				return ValueObject->GetObjectField(TEXT("Properties"));
			}

			return ValueObject;
		}
	}

	return nullptr;
}

inline TSharedPtr<FJsonObject> GetExport(const FJsonObject* PackageIndex, TArray<TSharedPtr<FJsonValue>> AllJsonObjects) {
	FString ObjectName = PackageIndex->GetStringField(TEXT("ObjectName")); /* Class'Asset:ExportName' */
	FString ObjectPath = PackageIndex->GetStringField(TEXT("ObjectPath")); /* Path/Asset.Index */
	FString Outer;

	/* Clean up ObjectName (Class'Asset:ExportName' --> Asset:ExportName --> ExportName) */
	ObjectName.Split("'", nullptr, &ObjectName);
	ObjectName.Split("'", &ObjectName, nullptr);

	if (ObjectName.Contains(":")) {
		ObjectName.Split(":", nullptr, &ObjectName); /* Asset:ExportName --> ExportName */
	}

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", nullptr, &ObjectName);
	}

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", &Outer, &ObjectName);
	}

	int Index = 0;

	/* Search for the object in the AllJsonObjects array */
	for (const TSharedPtr<FJsonValue>& Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = Value->AsObject();

		FString Name;
		if (ValueObject->TryGetStringField(TEXT("Name"), Name) && Name == ObjectName) {
			if (ValueObject->HasField(TEXT("Outer")) && !Outer.IsEmpty()) {
				FString OuterName = ValueObject->GetStringField(TEXT("Outer"));

				if (OuterName == Outer) {
					return AllJsonObjects[Index]->AsObject();
				}
			}
			else {
				return ValueObject;
			}
		}

		Index++;
	}

	return nullptr;
}

/* ReSharper disable once CppParameterNeverUsed */
inline void SetNotificationSubText(FNotificationInfo& Notification, const FText& SubText) {
#if ENGINE_UE5
	Notification.SubText = SubText;
#endif
}

/* Show the user a Notification */
inline auto AppendNotification(const FText& Text, const FText& SubText, const float ExpireDuration,
	const SNotificationItem::ECompletionState CompletionState, const bool bUseSuccessFailIcons,
	const float WidthOverride) -> void
{
	FNotificationInfo Info = FNotificationInfo(Text);
	Info.ExpireDuration = ExpireDuration;
	Info.bUseLargeFont = true;
	Info.bUseSuccessFailIcons = bUseSuccessFailIcons;
	Info.WidthOverride = FOptionalSize(WidthOverride);

	SetNotificationSubText(Info, SubText);

	const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(CompletionState);
}

/* Show the user a Notification with Subtext */
inline auto AppendNotification(const FText& Text, const FText& SubText, float ExpireDuration,
	const FSlateBrush* SlateBrush, SNotificationItem::ECompletionState CompletionState,
	const bool bUseSuccessFailIcons, const float WidthOverride) -> void
{
	FNotificationInfo Info = FNotificationInfo(Text);
	Info.ExpireDuration = ExpireDuration;
	Info.bUseLargeFont = true;
	Info.bUseSuccessFailIcons = bUseSuccessFailIcons;
	Info.WidthOverride = FOptionalSize(WidthOverride);
	Info.Image = SlateBrush;

	SetNotificationSubText(Info, SubText);

	const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(CompletionState);
}

inline int32 ConvertVersionStringToInt(const FString& VersionStr) {
	return FCString::Atoi(*VersionStr.Replace(TEXT("."), TEXT("")));
}

inline FString ReadPathFromObject(const TSharedPtr<FJsonObject>* PackageIndex) {
	FString ObjectType, ObjectName, ObjectPath, Outer;
	PackageIndex->Get()->GetStringField(TEXT("ObjectName")).Split("'", &ObjectType, &ObjectName);

	ObjectPath = PackageIndex->Get()->GetStringField(TEXT("ObjectPath"));
	ObjectPath.Split(".", &ObjectPath, nullptr);

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	ObjectPath = ObjectPath.Replace(TEXT("Nimbus/Content"), TEXT("/Game"));

	ObjectPath = ObjectPath.Replace(TEXT("Engine/Content"), TEXT("/Engine"));
	ObjectName = ObjectName.Replace(TEXT("'"), TEXT(""));

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", nullptr, &ObjectName);
	}

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", &Outer, &ObjectName);
	}

	return ObjectPath + "." + ObjectName;
}

/* Creates a plugin in the name (may result in bugs if inputted wrong) */
static void CreatePlugin(FString PluginName) {
	/* Plugin creation is different between UE5 and UE4 */
#if ENGINE_UE5
	FPluginUtils::FNewPluginParamsWithDescriptor CreationParams;
	CreationParams.Descriptor.bCanContainContent = true;

	CreationParams.Descriptor.FriendlyName = PluginName;
	CreationParams.Descriptor.Version = 1;
	CreationParams.Descriptor.VersionName = TEXT("1.0");
	CreationParams.Descriptor.Category = TEXT("Other");

	FText FailReason;
	FPluginUtils::FLoadPluginParams LoadParams;
	LoadParams.bEnablePluginInProject = true;
	LoadParams.bUpdateProjectPluginSearchPath = true;
	LoadParams.bSelectInContentBrowser = false;

	FPluginUtils::CreateAndLoadNewPlugin(PluginName, FPaths::ProjectPluginsDir(), CreationParams, LoadParams);
#elif UE4_18_BELOW

#else
	FPluginUtils::FNewPluginParams CreationParams;
	CreationParams.bCanContainContent = true;

	FText FailReason;
	FPluginUtils::FMountPluginParams LoadParams;
	LoadParams.bEnablePluginInProject = true;
	LoadParams.bUpdateProjectPluginSearchPath = true;
	LoadParams.bSelectInContentBrowser = false;

	FPluginUtils::CreateAndMountNewPlugin(PluginName, FPaths::ProjectPluginsDir(), CreationParams, LoadParams, FailReason);
#endif

#define LOCTEXT_NAMESPACE "UMG"
#if WITH_EDITOR
	/* Setup notification's arguments */
	FFormatNamedArguments Args;
	Args.Add(TEXT("PluginName"), FText::FromString(PluginName));

	/* Create notification */
	FNotificationInfo Info(FText::Format(LOCTEXT("PluginCreated", "Plugin Created: {PluginName}"), Args));
	Info.ExpireDuration = 10.0f;
	Info.bUseLargeFont = true;
	Info.bUseSuccessFailIcons = false;
	Info.WidthOverride = FOptionalSize(350);
	SetNotificationSubText(Info, FText::FromString(FString("Created successfully")));

	const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
#endif
#undef LOCTEXT_NAMESPACE
}

inline bool DeserializeJSONObject(const FString& String, TSharedPtr<FJsonObject>& JsonParsed) {
	FString Content = FString(TEXT("{\"data\": "));
	Content.Append(String);
	Content.Append(FString("}"));

	const TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Content);

	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject)) {
		JsonParsed = JsonObject->GetObjectField(TEXT("data"));

		return true;
	}

	return false;
}

inline TArray<FString> OpenFileDialog(const FString& Title, const FString& Type) {
	TArray<FString> ReturnValue;

	/* Window Handler for Windows */
	const void* ParentWindowHandle = nullptr;
	const IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
	const TSharedPtr<SWindow> MainWindow = MainFrameModule.GetParentWindow();

	if (MainWindow.IsValid() && MainWindow->GetNativeWindow().IsValid()) {
		ParentWindowHandle = MainWindow->GetNativeWindow()->GetOSWindowHandle();
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	FString DefaultPath = FString("");

	if (!ClipboardContent.IsEmpty()) {
		if (FPaths::FileExists(ClipboardContent)) {
			DefaultPath = FPaths::GetPath(ClipboardContent);
		}
		else if (FPaths::DirectoryExists(ClipboardContent)) {
			DefaultPath = ClipboardContent;
		}
	}

	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get()) {
		constexpr uint32 SelectionFlag = 1;

		DesktopPlatform->OpenFileDialog(ParentWindowHandle, Title, DefaultPath, FString(""), Type, SelectionFlag, ReturnValue);
	}

	return ReturnValue;
}

inline TSharedPtr<FJsonObject> RemovePropertiesShared(const TSharedPtr<FJsonObject>& Input, const TArray<FString>& RemovedProperties) {
	TSharedPtr<FJsonObject> ClonedJsonObject = MakeShareable(new FJsonObject(*Input));

	for (const FString& Property : RemovedProperties) {
		if (ClonedJsonObject->HasField(Property)) {
			ClonedJsonObject->RemoveField(Property);
		}
	}

	return ClonedJsonObject;
}

/* Filter to whitelist */
inline TSharedPtr<FJsonObject> KeepPropertiesShared(const TSharedPtr<FJsonObject>& Input, TArray<FString> WhitelistProperties) {
	const TSharedPtr<FJsonObject> RawSharedPtrData = MakeShared<FJsonObject>();

	for (const FString& Property : WhitelistProperties) {
		if (Input->HasField(Property)) {
			RawSharedPtrData->SetField(Property, Input->TryGetField(Property));
		}
	}

	return RawSharedPtrData;
}

inline void SavePluginConfig(UDeveloperSettings* EditorSettings) {
	EditorSettings->SaveConfig();

#if ENGINE_UE5
	EditorSettings->TryUpdateDefaultConfigFile();
	EditorSettings->ReloadConfig(nullptr, nullptr, UE::LCPF_PropagateToInstances);
#else
	EditorSettings->UpdateDefaultConfigFile();
	EditorSettings->ReloadConfig(nullptr, nullptr, UE4::LCPF_PropagateToInstances);
#endif

	EditorSettings->LoadConfig();
}

inline void OpenPluginSettings() {
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Editor", "Plugins", "JsonAsAsset");
}

/* Simple handler for JsonArray */
inline auto ProcessJsonArrayField(const TSharedPtr<FJsonObject>& ObjectField, const FString& ArrayFieldName,
	const TFunction<void(const TSharedPtr<FJsonObject>&)>& ProcessObjectFunction) -> void
{
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;

	if (ObjectField->TryGetArrayField(ArrayFieldName, JsonArray)) {
		for (const auto& JsonValue : *JsonArray) {
			const TSharedPtr<FJsonObject> JsonItem = JsonValue->AsObject();
			if (JsonItem.IsValid()) {
				ProcessObjectFunction(JsonItem);
			}
		}
	}
}

inline TSharedPtr<FJsonObject> RequestObjectURL(const FString& URL) {
	FHttpModule* HttpModule = &FHttpModule::Get();

	const auto Request = HttpModule->CreateRequest();

	Request->SetURL(URL);
	Request->SetVerb(TEXT("GET"));

	const auto Response = FRemoteUtilities::ExecuteRequestSync(Request);
	if (!Response.IsValid()) return TSharedPtr<FJsonObject>();

	TSharedPtr<FJsonObject> DeserializedJSON;

	if (!DeserializeJSONObject(Response->GetContentAsString(), DeserializedJSON)) return TSharedPtr<FJsonObject>();
	return DeserializedJSON;
};


inline TSubclassOf<UObject> LoadClassFromPath(const FString& ObjectName, const FString& ObjectPath) {
	const FString FullPath = ObjectPath + TEXT(".") + ObjectName;
	UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);

	if (LoadedObject) {
		UClass* LoadedClass = Cast<UClass>(LoadedObject);
		if (LoadedClass) {
			return LoadedClass;
		}
	}

	return nullptr;
}


inline TSubclassOf<UObject> LoadBlueprintClass(FString& ObjectPath) {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	if (!Settings->AssetSettings.GameName.IsEmpty()) {
		ObjectPath = ObjectPath.Replace(*(Settings->AssetSettings.GameName + "/Content"), TEXT("/Game"));
	}

	FString FullPath = ObjectPath;
	if (FullPath.EndsWith(TEXT(".1"))) {
		FullPath = FullPath.LeftChop(2);
	}

#if UE4_18_BELOW
	UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);
	if (LoadedObject)
#else
	if (UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath))
#endif
	{
		const UBlueprint* LoadedBlueprint = Cast<UBlueprint>(LoadedObject);

		if (LoadedBlueprint && LoadedBlueprint->GeneratedClass) {
			return LoadedBlueprint->GeneratedClass;
		}
	}

	return nullptr;
}

inline UClass* LoadClass(const TSharedPtr<FJsonObject>& SuperStruct) {
	const FString ObjectName = SuperStruct->GetStringField(TEXT("ObjectName")).Replace(TEXT("Class'"), TEXT("")).Replace(TEXT("'"), TEXT(""));
	FString ObjectPath = SuperStruct->GetStringField(TEXT("ObjectPath"));

	/* It's a C++ class if it has Script in it */
	if (ObjectPath.Contains("/Script/")) {
		return LoadClassFromPath(ObjectName, ObjectPath);
	}

	ObjectPath.Split(".", &ObjectPath, nullptr);

	return LoadBlueprintClass(ObjectPath);
}


inline UObject* LoadStruct(const TSharedPtr<FJsonObject>& Struct) {
	const FString ObjectName = Struct->GetStringField(TEXT("ObjectName")).Replace(TEXT("ScriptStruct'"), TEXT("")).Replace(TEXT("'"), TEXT(""));

	if (ObjectName.Equals(TEXT("PointerToUberGraphFrame"))) {
		return nullptr;
	}

	FString ObjectPath = Struct->GetStringField(TEXT("ObjectPath"));

	const FString FullPath = ObjectPath + TEXT(".") + ObjectName;
	UObject* LoadedObject = StaticLoadObject(UScriptStruct::StaticClass(), nullptr, *FullPath);

	return LoadedObject;
}

inline TSharedRef<IMessageLogListing> GetMessageLogListing() {
	FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
	const TSharedRef<IMessageLogListing> LogListing = MessageLogModule.GetLogListing("JsonAsAsset");

	return LogListing;
}

inline FMessageLog GetMessageLog() {
	return FMessageLog(FName("JsonAsAsset"));
}

inline void OpenMessageLog() {
	GetMessageLog().Open(EMessageSeverity::Info, true);
}

inline void EmptyMessageLog() {
	GetMessageLogListing()->ClearMessages();
}

inline void RemoveNotification(TWeakPtr<SNotificationItem> Notification) {
	const TSharedPtr<SNotificationItem> Item = Notification.Pin();

	if (Item.IsValid()) {
		Item->SetFadeOutDuration(0.001);
		Item->Fadeout();
		Notification.Reset();
	}
}

inline UJsonAsAssetSettings* GetSettings() {
	return GetMutableDefault<UJsonAsAssetSettings>();
}

inline FJsonObject* EnsureObjectField(FJsonObject* Parent, const FString& FieldName) {
	if (!Parent->HasField(FieldName)) {
		Parent->SetObjectField(FieldName, MakeShareable(new FJsonObject()));
	}

	return Parent->GetObjectField(FieldName).Get();
}

inline FJsonObject* EnsureObjectField(const TSharedPtr<FJsonObject>& Parent, const FString& FieldName) {
	if (!Parent->HasField(FieldName)) {
		Parent->SetObjectField(FieldName, MakeShareable(new FJsonObject()));
	}

	return Parent->GetObjectField(FieldName).Get();
}

inline TArray<TSharedPtr<FJsonValue>> EnsureArrayField(const TSharedPtr<FJsonObject>& Parent, const FString& FieldName) {
	if (!Parent->HasField(FieldName)) {
		Parent->SetArrayField(FieldName, TArray<TSharedPtr<FJsonValue>>());
	}

	return Parent->GetArrayField(FieldName);
}

inline FName GetExportNameOfSubobject(const FString& PackageIndex) {
	FString Name; {
		PackageIndex.Split("'", nullptr, &Name);
		Name.Split(":", nullptr, &Name);
		Name = Name.Replace(TEXT("'"), TEXT(""));
	}

#if UE4_18_BELOW
	return FName(*Name);
#else
	return FName(Name);
#endif
}

inline TSharedPtr<FJsonValue> GetExportByObjectPath(const TSharedPtr<FJsonObject>& Object, TArray<TSharedPtr<FJsonValue>> AllJsonObjects) {
	const TSharedPtr<FJsonObject> ValueObject = TSharedPtr<FJsonObject>(Object);

	FString StringIndex; {
		ValueObject->GetStringField("ObjectPath").Split(".", nullptr, &StringIndex);
	}

	return AllJsonObjects[FCString::Atod(*StringIndex)];
}

/**
 * Get the ubergraph of a blueprint
 *
 *
 */
inline UEdGraph* GetUberGraph(UBlueprint* Blueprint) {
	if (Blueprint->UbergraphPages.Num() > 0) {
		return Blueprint->UbergraphPages[0];
	}
	return nullptr;
}

/**
 * Remove the event nodes from a graph
 */
inline void RemoveEventNodes(UEdGraph* EdGraph) {
	TArray<UK2Node_Event*> EventNodes;
	for (UEdGraphNode* Node : EdGraph->Nodes) {
		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node)) {
			EventNodes.Add(EventNode);
		}
	}

	for (UK2Node_Event* EventNode : EventNodes) {
		EdGraph->RemoveNode(EventNode);
	}
}

inline void GetAllSCSNodes(UBlueprint* Blueprint, TArray<USCS_Node*>& OutNodes)
{
	if (!Blueprint)
		return;

	// Add current Blueprint’s nodes
	if (Blueprint->SimpleConstructionScript)
	{
		OutNodes.Append(Blueprint->SimpleConstructionScript->GetAllNodes());
	}

	// Walk up the inheritance chain to include parent Blueprints
	for (UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass ? Blueprint->ParentClass->ClassGeneratedBy : nullptr);
		ParentBP;
		ParentBP = Cast<UBlueprint>(ParentBP->ParentClass ? ParentBP->ParentClass->ClassGeneratedBy : nullptr))
	{
		if (ParentBP->SimpleConstructionScript)
		{
			OutNodes.Append(ParentBP->SimpleConstructionScript->GetAllNodes());
		}
	}
}

inline USCS_Node* FindSCSNodeByName(const TArray<USCS_Node*>& AllNodes, const FName& ComponentName)
{
	for (USCS_Node* Node : AllNodes)
	{
		if (!Node) {
			continue;
		}
			
		if (Node->GetVariableName() == ComponentName) {
			return Node;
		}
	}
	return nullptr;
}