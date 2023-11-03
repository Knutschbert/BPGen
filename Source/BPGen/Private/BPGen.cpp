// Copyright Epic Games, Inc. All Rights Reserved.

#include "BPGen.h"
#include "AssetToolsModule.h"
#include "BPGenStyle.h"
#include "BPGenCommands.h"
#include "EdGraph/EdGraph.h"
#include "Factories/BlueprintFactory.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "LevelEditor.h"
#include "Serialization/JsonWriter.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Utils.h"
#include "Misc/MessageDialog.h"
#include "Animation/AnimInstance.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EditorAssetLibrary.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include <Runtime/JsonUtilities/Public/JsonObjectConverter.h>
//#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Internationalization/Regex.h"



static const FName BPGenTabName("BPGen");

#define LOCTEXT_NAMESPACE "FBPGenModule"
DEFINE_LOG_CATEGORY(LogBPGen)

void FBPGenModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FBPGenStyle::Initialize();
	FBPGenStyle::ReloadTextures();

	FBPGenCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBPGenCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FBPGenModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBPGenModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BPGenTabName, FOnSpawnTab::CreateRaw(this, &FBPGenModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FBPGenTabTitle", "BPGen"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FBPGenModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBPGenStyle::Shutdown();

	FBPGenCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BPGenTabName);
}

TSharedRef<SDockTab> FBPGenModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FBPGenModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("BPGen.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(WidgetText)
			]
		];
}

static void Generate();

void FBPGenModule::PluginButtonClicked()
{
	
	Generate();

}

void FBPGenModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FBPGenCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FBPGenCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

static UK2Node_CallFunction* CreateFunctionNode(UEdGraph* EventGraph, UFunction* Function) {
	UK2Node_CallFunction* GetOwnerNode = NewObject<UK2Node_CallFunction>(EventGraph);
	GetOwnerNode->CreateNewGuid();
	GetOwnerNode->PostPlacedNewNode();
	GetOwnerNode->SetFromFunction(Function);
	GetOwnerNode->SetFlags(RF_Transactional);
	GetOwnerNode->AllocateDefaultPins();
	UEdGraphSchema_K2::SetNodeMetaData(GetOwnerNode, FNodeMetadata::DefaultGraphNode);
	GetOwnerNode->SetEnabledState(ENodeEnabledState::Enabled);
	EventGraph->AddNode(GetOwnerNode);
	return GetOwnerNode;
}

static UK2Node_IfThenElse* CreateBranchNode(UEdGraph* EventGraph) {
	UK2Node_IfThenElse* GetOwnerNode = NewObject<UK2Node_IfThenElse>(EventGraph);
	GetOwnerNode->CreateNewGuid();
	GetOwnerNode->PostPlacedNewNode();
	GetOwnerNode->SetFlags(RF_Transactional);
	GetOwnerNode->AllocateDefaultPins();
	UEdGraphSchema_K2::SetNodeMetaData(GetOwnerNode, FNodeMetadata::DefaultGraphNode);
	GetOwnerNode->SetEnabledState(ENodeEnabledState::Enabled);
	EventGraph->AddNode(GetOwnerNode);
	return GetOwnerNode;
}


static void CreateNodes() {
	// FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString("asdf"));
	// Utils::GetSelectedButtonFromDialog("asdf", EAppMsgType::YesNo, EAppReturnType::Yes);
	// if (Utils::GetSelectedButtonFromDialog("Do you wish to run asset generator first?", EAppMsgType::YesNo,	EAppReturnType::Yes)) {
	// 	UE_LOG(LogTemp, Display, TEXT("FileManipulation: File %s loaded."), *SelectedFileNames[0]);
	// }
	// FGlobalTabmanager::Get()->TryInvokeTab(BPGenTabName);

	FString Path = "/Game/_AssemblyStorm/TestMod/Gen/";
	FString Name = "GenCpp";

	NewObject<UBlueprintFactory>();

	// ObjectIndex = i;
	// FString Path = FString(UTF8_TO_TCHAR("/")) + Objects[i].Path;
	UE_LOG(LogTemp, Display, TEXT("AssetGenOperation: Asset path: %s"), *Path);
	// FString Name = Objects[i].Name;
	UClass* AssetClassType = UBlueprint::StaticClass();
	UFactory* AssetFactoryType = NewObject<UBlueprintFactory>();
	if (AssetClassType != nullptr && AssetFactoryType != nullptr)
	{
		// UEditorAssetLibrary::DeleteAsset(Path + Name);
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, Path, AssetClassType, AssetFactoryType);
		UE_LOG(LogTemp, Display, TEXT("AssetGenOperation: Created asset: %s"), *Name);

		// Save the asset
		if (NewAsset != nullptr)
		{
			UBlueprint* InBlueprint = Cast<UBlueprint>(NewAsset);

			if (ensure(InBlueprint->UbergraphPages.Num() > 0))
			{
				UEdGraph* EventGraph = InBlueprint->UbergraphPages[0];

				int32 SafeXPosition = 0;
				int32 SafeYPosition = 0;

				if (EventGraph->Nodes.Num() != 0)
				{
					SafeXPosition = EventGraph->Nodes[0]->NodePosX;
					SafeYPosition = EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodePosY + EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodeHeight + 100;
				}

				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				UK2Node_CallFunction* Node1 = CreateFunctionNode(EventGraph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
				Node1->NodePosX = SafeXPosition;
				Node1->NodePosY = SafeYPosition;
				EventGraph->AddNode(Node1);


				UK2Node_CallFunction* NodeThen = CreateFunctionNode(EventGraph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
				NodeThen->NodePosX = SafeXPosition + 600;
				NodeThen->NodePosY = SafeYPosition;
				EventGraph->AddNode(NodeThen);

				UK2Node_CallFunction* NodeElse = CreateFunctionNode(EventGraph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
				NodeElse->NodePosX = SafeXPosition + 600;
				NodeElse->NodePosY = SafeYPosition + 300;
				EventGraph->AddNode(NodeElse);

				UK2Node_IfThenElse* NodeBranch = CreateBranchNode(EventGraph);
				NodeBranch->NodePosX = SafeXPosition + 300;
				NodeBranch->NodePosY = SafeYPosition;
				EventGraph->AddNode(NodeBranch);


				// UK2Node_CallFunction* Node2 = CreateFunctionNode(EventGraph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, RetriggerableDelay)));
				// Node2->NodePosX = SafeXPosition + 300;
				// Node2->NodePosY = SafeYPosition;
				// EventGraph->AddNode(Node2);


				// UK2Node_CallFunction* NodeElse = CreateBranchNode(EventGraph);

				//UFunction* MakeNodeFunction = UAnimInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAnimInstance, TryGetPawnOwner));
				// UFunction* MakeNodeFunction = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay));

				K2Schema->FindExecutionPin(*Node1, EEdGraphPinDirection::EGPD_Output)
					->MakeLinkTo(K2Schema->FindExecutionPin(*NodeBranch, EEdGraphPinDirection::EGPD_Input));
				NodeBranch->GetThenPin()
					->MakeLinkTo(K2Schema->FindExecutionPin(*NodeThen, EEdGraphPinDirection::EGPD_Input));
				NodeBranch->GetElsePin()
					->MakeLinkTo(K2Schema->FindExecutionPin(*NodeElse, EEdGraphPinDirection::EGPD_Input));

				// check(EventExecPin);
			}




			UPackage* Package = NewAsset->GetOutermost();
			Package->MarkPackageDirty();
			FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FString PackageFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + Path);
			FString PackageFile = PackageFilePath + "/" + PackageFileName;
			if (FPackageName::DoesPackageExist(Package->GetName()))
			{
				UPackage::SavePackage(Package, nullptr, RF_Standalone, *PackageFile);
				UE_LOG(LogTemp, Display, TEXT("AssetGenOperation: Saved package: %s"), *PackageFile);
			}
			UEditorAssetLibrary::SaveLoadedAsset(InBlueprint, true);
		}

	} else { UE_LOG(LogTemp, Error, TEXT("AssetGenOperation: Failed to create asset: %s"), *Name); }
}

static void ParseFunctionDescription(const FString& FunctionDescription, TMap<FString, FString>& ParamMap)
{
	// Regular expression pattern to match main description, @param tags, and their descriptions
	FRegexPattern ParamPattern(TEXT("@param\\s+(\\w+)\\s+([^@\\n]+)(?:\\s*@return\\s+([^@\\n]+))?"));
	FRegexMatcher ParamMatcher(ParamPattern, FunctionDescription);
	//FRegexPattern ParamPattern(TEXT("((?:(?!@param).)*)@param\\s+(\\S+)\\s+(.+)"));

	// If there are no @param tags, the whole string is the main description
	if (!ParamMatcher.FindNext())
	{
		ParamMap.Add(TEXT("MainDescription"), FunctionDescription.TrimStartAndEnd());
		return;
	}

	TArray<FString> Substrings;
	FunctionDescription.ParseIntoArray(Substrings, TEXT("@"), true);
	if (Substrings.Num())
		ParamMap.Add(TEXT("MainDescription"), Substrings[0]);

	// Capture parameter names and descriptions
	do
	{
		FString ParamName = ParamMatcher.GetCaptureGroup(1);
		FString ParamDescription = ParamMatcher.GetCaptureGroup(2);
		ParamMap.Add(ParamName, ParamDescription.TrimStartAndEnd());
	} while (ParamMatcher.FindNext());
}

static TArray<UProperty*> GetPropertiesFromClass(UClass* Class)
{
	TArray<UProperty*> Properties;

	// Iterate through the class fields
	for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;

		// Check if the field is a property (variable)
		if (Property->IsA(UProperty::StaticClass()))
		{
			Properties.Add(Property);
		}
	}

	return Properties;
}

static FString GetStrippedClassName(UClass* Class)
{
	FString ClassName = Class->GetName();

	// Check if the class is a Blueprint-generated class
	if (Class && Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		// The class is a Blueprint-generated class, remove both U and A prefixes
		ClassName.RemoveFromStart(TEXT("U"));
		ClassName.RemoveFromStart(TEXT("A"));
	}
	else
	{
		// The class is not a Blueprint-generated class, remove only the U prefix
		ClassName.RemoveFromStart(TEXT("U"));
	}

	return ClassName;
}

struct FTypeInfo
{
	FString OuterType;
	bool bIsPointer;

	FTypeInfo(const FString& Outer, bool IsPtr)
		: OuterType(Outer), bIsPointer(IsPtr) {}
};

static TSharedPtr<FJsonObject> ParseCPPName(const FString& PropertyType)
{
	FRegexPattern TypePattern(TEXT("([A-Za-z_]+)\\s*(?:<([^<>]+)>\\s*)?(\\*?)"));
	FRegexMatcher TypeMatcher(TypePattern, PropertyType);

	FString OuterType;
	FString InnerType;
	bool bIsPointer = false;

	// Extract outer type, inner type, and check for pointers using regular expressions
	if (TypeMatcher.FindNext())
	{
		OuterType = TypeMatcher.GetCaptureGroup(1);
		InnerType = TypeMatcher.GetCaptureGroup(2);
		FString PointerMatch = TypeMatcher.GetCaptureGroup(3);

		// If a pointer (*) is found, set bIsPointer to true
		if (!PointerMatch.IsEmpty() && PointerMatch == TEXT("*"))
		{
			bIsPointer = true;
		}
	}

	// Create a JSON object
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	// Add properties to the JSON object
	JsonObject->SetStringField(TEXT("OuterType"), OuterType);
	JsonObject->SetStringField(TEXT("InnerType"), InnerType);
	JsonObject->SetBoolField(TEXT("IsPointer"), bIsPointer);

	return JsonObject;
}


static void ExportFunctions() {
	TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject);

	TSharedRef<FJsonObject> Classes = MakeShareable(new FJsonObject);

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
		UClass* const Class = (*ClassIt);
		// UE_LOG(LogTemp, Display, TEXT("Found class %s"), *Class->GetPathName());

		TSharedRef<FJsonObject> ClassObject = MakeShareable(new FJsonObject);

		TSharedRef<FJsonObject> PropertiesObject = MakeShareable(new FJsonObject);
		TSharedRef<FJsonObject> FunctionsObject = MakeShareable(new FJsonObject);
		TArray<UProperty*> properties = GetPropertiesFromClass(Class);
		//Class->GetNativePropertyValues(tempPMap);
		for (auto itr : properties)
		{
			FString PName = itr->GetName();
			FString PType = itr->GetCPPType();
			PropertiesObject->SetStringField(PName, PType);
			//UE_LOG(LogBPGen, Log, TEXT("Name: %s, Type: %s"), *PName, *PType);
		}

		for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt) {
			UFunction* Function = *FunctionIt;
			FString tooltip = Function->GetMetaData("ToolTip");
			TMap<FString, FString> tempMap;
			ParseFunctionDescription(tooltip, tempMap);
			TSharedRef<FJsonObject> FunctionObject = MakeShareable(new FJsonObject);

			FunctionObject->SetBoolField("pure", Function->HasAnyFunctionFlags(FUNC_BlueprintPure));

			TArray< TSharedPtr<FJsonValue> > Pins;
			// TSharedRef<FJsonObject> PinsObject = MakeShareable(new FJsonObject);

			// UE_LOG(LogTemp, Display, TEXT("Found %s function %s"), pure ? TEXT("pure") : TEXT("impure"), *Function->GetName());

			for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt) {
				FProperty* Param = *PropIt;

				TSharedRef<FJsonObject> PinObject = MakeShareable(new FJsonObject);
				// UE_LOG(LogTemp, Display, TEXT("Found param %s"), *Param->GetName());

				const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_ReturnParm) && (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm));
				const bool bIsRefParam = Param->HasAnyPropertyFlags(CPF_ReferenceParm) && bIsFunctionInput;

				const EEdGraphPinDirection Direction = bIsFunctionInput ? EGPD_Input : EGPD_Output;

				PinObject->SetStringField("name", *Param->GetName().TrimStartAndEnd());
				FString typeStr = *Param->GetCPPType().TrimStartAndEnd();
				PinObject->SetStringField("type", typeStr);
				PinObject->SetObjectField("type_parsed", ParseCPPName(typeStr));

				
				//PinObject->SetStringField("GetCPPTypeForwardDeclaration", *Param->GetMetaDataMap());
				PinObject->SetStringField("direction", bIsFunctionInput ? "input" : "output");
				PinObject->SetBoolField("isRef", bIsRefParam);
				if (FString* tt = tempMap.Find(*Param->GetName()))
					PinObject->SetStringField("tooltip", (*tt).TrimStartAndEnd());
				const TMap<FName, FString>* metaData = Param->GetMetaDataMap();
				if (metaData != nullptr)
					for (auto itr : *metaData)
						if (!itr.Value.IsEmpty())
							PinObject->SetStringField(FString("PMeta_") + itr.Key.ToString(), itr.Value);
				
				FString ToolTip = Param->GetMetaData("ToolTip");
				FString DisplayName = Param->GetMetaData("DisplayName");
				FString Category = Param->GetMetaData("Category");
				FString EditCondition = Param->GetMetaData("EditCondition");
				FString SaveGame = Param->GetMetaData("SaveGame");
				FString AssetRegistrySearchable = Param->GetMetaData("AssetRegistrySearchable");

				// PinsObject->SetObjectField(*Param->GetName(), PinObject);
				Pins.Add(MakeShareable(new FJsonValueObject(PinObject)));

				// UEdGraphNode::FCreatePinParams PinParams;
				// PinParams.bIsReference = bIsRefParam;
				// UEdGraphPin* Pin = CreatePin(Direction, NAME_None, Param->GetFName(), PinParams);

				/*
				const bool bPinGood = (Pin && K2Schema->ConvertPropertyToPinType(Param, Pin->PinType));

				if (bPinGood)
				{
					// Check for a display name override
					const FString& PinDisplayName = Param->GetMetaData(FBlueprintMetadata::MD_DisplayName);
					if (!PinDisplayName.IsEmpty())
					{
						Pin->PinFriendlyName = FText::FromString(PinDisplayName);
					}
					else if (Function->GetReturnProperty() == Param && Function->HasMetaData(FBlueprintMetadata::MD_ReturnDisplayName))
					{
						Pin->PinFriendlyName = Function->GetMetaDataText(FBlueprintMetadata::MD_ReturnDisplayName);
					}

					//Flag pin as read only for const reference property
					Pin->bDefaultValueIsIgnored = Param->HasAllPropertyFlags(CPF_ConstParm | CPF_ReferenceParm) && (!Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) || Pin->PinType.IsContainer());

					const bool bAdvancedPin = Param->HasAllPropertyFlags(CPF_AdvancedDisplay);
					Pin->bAdvancedView = bAdvancedPin;
					if(bAdvancedPin && (ENodeAdvancedPins::NoPins == AdvancedPinDisplay))
					{
						AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
					}

					FString ParamValue;
					if (K2Schema->FindFunctionParameterDefaultValue(Function, Param, ParamValue))
					{
						K2Schema->SetPinAutogeneratedDefaultValue(Pin, ParamValue);
					}
					else
					{
						K2Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
					}
					
					if (PinsToHide.Contains(Pin->PinName))
					{
						const FString PinNameStr = Pin->PinName.ToString();
						const FString& DefaultToSelfMetaValue = Function->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);
						const FString& WorldContextMetaValue  = Function->GetMetaData(FBlueprintMetadata::MD_WorldContext);
						bool bIsSelfPin = ((PinNameStr == DefaultToSelfMetaValue) || (PinNameStr == WorldContextMetaValue));

						if (!bShowWorldContextPin || !bIsSelfPin)
						{
							Pin->bHidden = true;
							Pin->bNotConnectable = InternalPins.Contains(Pin->PinName);
						}
					}

					PostParameterPinCreated(Pin);
				}

				bAllPinsGood = bAllPinsGood && bPinGood;
				*/
			}
			
			FunctionObject->SetStringField("tooltip", *tempMap.Find(L"MainDescription"));

			static const FString validMData[] = {
				L"CommutativeAssociativeBinaryOperator",
				L"CompactNodeTitle",
				L"ShortToolTip",
				L"DisplayName",
				L"HidePin",
				L"KeyWords"
			};

			for (auto itr : validMData)
			{
				FString val = Function->GetMetaData(*itr);
				if (!val.IsEmpty())
					FunctionObject->SetStringField(FString(L"FMeta_") + itr, *val);
			}
			// Only if there was something to parse
			FString val = Function->GetMetaData("ToolTip");
			if (!val.IsEmpty() && val.Len() > tempMap.Find(L"MainDescription")->Len())
				FunctionObject->SetStringField(FString(L"FMeta_Tooltip"), *val);

			FunctionObject->SetArrayField("pins", Pins);
			FunctionsObject->SetObjectField(Function->GetName(), FunctionObject);
		}

		if (FunctionsObject->Values.Num())
		{
			
			//ClassObject->SetStringField("GetAuthoredName", Class->GetAuthoredName());
			//ClassObject->SetStringField("StrippedName", GetStrippedClassName(Class));
			//ClassObject->SetStringField("GetFName", Class->GetFName().ToString());
			ClassObject->SetStringField("GetDisplayNameText", Class->GetDisplayNameText().ToString());
			//ClassObject->SetStringField("GetFullGroupName", Class->GetFullGroupName(true));
			ClassObject->SetStringField("GetDefaultObjectName", Class->GetDefaultObjectName().ToString());
			//ClassObject->SetStringField("GetFullName", Class->GetFullName());
			ClassObject->SetObjectField("properties", PropertiesObject);
			ClassObject->SetObjectField("functions", FunctionsObject);
			/*Classes->GetObjectField*/

			TArray<FString> Substrings;
			Class->GetPathName().ParseIntoArray(Substrings, TEXT("."), true);
			if (Substrings.Num() == 2)
			{
				
				//if (field.IsValid())
				if (Classes->HasField(Substrings[0]))
				{
					auto field = Classes->GetObjectField(Substrings[0]);
					field->SetObjectField(Substrings[1], ClassObject);
				}
				else
				{
					TSharedRef<FJsonObject> field = MakeShareable(new FJsonObject);
					field->SetObjectField(Substrings[1], ClassObject);
					Classes->SetObjectField(Substrings[0], field);
				}
				/*if (field.IsValid() && field->Values.Num())
					field->SetObjectField(Substrings[1], ClassObject);
				else
				{
					field = MakeShareable(new FJsonObject);
					field->SetObjectField(Substrings[1], ClassObject);
					Classes->SetObjectField(Substrings[0], field);
				}*/
			}
				//Classes->SetObjectField(*Class->GetPathName(), ClassObject);
			else
				Classes->SetObjectField(*Class->GetPathName(), ClassObject);
		}
		//Classes.Add(MakeShareable(new FJsonValueObject(ClassObject)));
	}

	RootObject->SetObjectField("classes", Classes);

	FString OutputString;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);

	FString Path = FPaths::Combine(FPaths::ProjectDir(), FString("kismet.json"));

	// IFileHandle * pFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite( *Path );

	FFileHelper::SaveStringToFile(OutputString, *Path);
}

static void Generate() {
	ExportFunctions();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBPGenModule, BPGen)