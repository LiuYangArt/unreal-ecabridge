// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPcgNativeCompatCommands.h"
#include "Commands/ECACommand.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"
#include "PCGVolume.h"
#include "Data/PCGPointData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Runtime/Launch/Resources/Version.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

REGISTER_ECA_COMMAND(FECACommand_GetPCGGraphStructure)
REGISTER_ECA_COMMAND(FECACommand_GetPCGGraphSchema)
REGISTER_ECA_COMMAND(FECACommand_GetPCGGraphDescription)
REGISTER_ECA_COMMAND(FECACommand_SetPCGGraphDescription)
REGISTER_ECA_COMMAND(FECACommand_ListPCGNativeNodes)
REGISTER_ECA_COMMAND(FECACommand_GetPCGNativeNodeSchema)
REGISTER_ECA_COMMAND(FECACommand_ListPCGAvailableSubgraphs)
REGISTER_ECA_COMMAND(FECACommand_AddPCGSubgraphNode)
REGISTER_ECA_COMMAND(FECACommand_UpdatePCGNode)
REGISTER_ECA_COMMAND(FECACommand_GetPCGNodeInfo)
REGISTER_ECA_COMMAND(FECACommand_RepositionPCGNode)
REGISTER_ECA_COMMAND(FECACommand_SetPCGNodeComment)
REGISTER_ECA_COMMAND(FECACommand_RemovePCGNode)
REGISTER_ECA_COMMAND(FECACommand_DisconnectPCGNodes)
REGISTER_ECA_COMMAND(FECACommand_AddPCGCommentBox)
REGISTER_ECA_COMMAND(FECACommand_UpdatePCGCommentBox)
REGISTER_ECA_COMMAND(FECACommand_RemovePCGCommentBox)
REGISTER_ECA_COMMAND(FECACommand_ListPCGGraphInstances)
REGISTER_ECA_COMMAND(FECACommand_SpawnPCGGraphInstance)
REGISTER_ECA_COMMAND(FECACommand_GetPCGGraphInstanceParams)
REGISTER_ECA_COMMAND(FECACommand_SetPCGGraphInstanceParams)
REGISTER_ECA_COMMAND(FECACommand_ResetPCGGraphInstanceParams)
REGISTER_ECA_COMMAND(FECACommand_ExecutePCGGraphInstance)
REGISTER_ECA_COMMAND(FECACommand_GetPCGNodeDataView)

namespace ECAPcgNativeCompat
{
	static UPCGGraph* LoadGraph(const FString& GraphPath)
	{
		return GraphPath.IsEmpty() ? nullptr : LoadObject<UPCGGraph>(nullptr, *GraphPath);
	}

	static bool SaveAssetPackage(UObject* Asset, FString& OutError)
	{
		UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
		if (!Package)
		{
			OutError = TEXT("Asset has no outer package");
			return false;
		}

		Package->MarkPackageDirty();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs))
		{
			OutError = FString::Printf(TEXT("UPackage::SavePackage failed for '%s'"), *PackageFileName);
			return false;
		}
		return true;
	}

	static UPCGNode* FindNodeById(UPCGGraph* Graph, const FString& NodeId)
	{
		if (!Graph)
		{
			return nullptr;
		}
		if (UPCGNode* Input = Graph->GetInputNode())
		{
			if (Input->GetName() == NodeId || Input->GetFName().ToString() == NodeId)
			{
				return Input;
			}
		}
		if (UPCGNode* Output = Graph->GetOutputNode())
		{
			if (Output->GetName() == NodeId || Output->GetFName().ToString() == NodeId)
			{
				return Output;
			}
		}
		for (UPCGNode* Node : Graph->GetNodes())
		{
			if (Node && (Node->GetName() == NodeId || Node->GetFName().ToString() == NodeId))
			{
				return Node;
			}
		}
		return nullptr;
	}

	static UClass* FindPCGSettingsClass(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}

		FString Stripped = Name;
		if (Stripped.StartsWith(TEXT("U")) && Stripped.Len() > 1 && FChar::IsUpper(Stripped[1]))
		{
			Stripped = Stripped.Mid(1);
		}

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class || !Class->IsChildOf(UPCGSettings::StaticClass()))
			{
				continue;
			}
			if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden))
			{
				continue;
			}

			const FString ClassName = Class->GetName();
			if (ClassName.Equals(Name, ESearchCase::IgnoreCase) ||
				ClassName.Equals(Stripped, ESearchCase::IgnoreCase))
			{
				return Class;
			}

			if (const UPCGSettings* CDO = Cast<UPCGSettings>(Class->GetDefaultObject(false)))
			{
				const FString Title = CDO->GetDefaultNodeTitle().ToString();
				if (Title.Equals(Name, ESearchCase::IgnoreCase))
				{
					return Class;
				}
			}
		}
		return nullptr;
	}

	static FString BagResultToString(EPropertyBagResult Result)
	{
		if (const UEnum* Enum = StaticEnum<EPropertyBagResult>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Result));
		}
		return FString::FromInt(static_cast<int32>(Result));
	}

	static const FPropertyBagPropertyDesc* FindPropertyDesc(const FInstancedPropertyBag& Bag, FName Name)
	{
		const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
		if (!BagStruct)
		{
			return nullptr;
		}
		for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
		{
			if (Desc.Name == Name)
			{
				return &Desc;
			}
		}
		return nullptr;
	}

	static bool JsonToVector(const TSharedPtr<FJsonValue>& Json, FVector& OutValue)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Json.IsValid() || !Json->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			return false;
		}
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		(*Obj)->TryGetNumberField(TEXT("x"), X);
		(*Obj)->TryGetNumberField(TEXT("y"), Y);
		(*Obj)->TryGetNumberField(TEXT("z"), Z);
		OutValue = FVector(X, Y, Z);
		return true;
	}

	static bool JsonToRotator(const TSharedPtr<FJsonValue>& Json, FRotator& OutValue)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Json.IsValid() || !Json->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			return false;
		}
		double Pitch = 0.0;
		double Yaw = 0.0;
		double Roll = 0.0;
		(*Obj)->TryGetNumberField(TEXT("pitch"), Pitch);
		(*Obj)->TryGetNumberField(TEXT("yaw"), Yaw);
		(*Obj)->TryGetNumberField(TEXT("roll"), Roll);
		OutValue = FRotator(Pitch, Yaw, Roll);
		return true;
	}

	static bool JsonToColor(const TSharedPtr<FJsonObject>& Obj, FLinearColor& OutColor)
	{
		if (!Obj.IsValid())
		{
			return false;
		}
		double R = 1.0;
		double G = 1.0;
		double B = 1.0;
		double A = 1.0;
		Obj->TryGetNumberField(TEXT("r"), R);
		Obj->TryGetNumberField(TEXT("g"), G);
		Obj->TryGetNumberField(TEXT("b"), B);
		Obj->TryGetNumberField(TEXT("a"), A);
		OutColor = FLinearColor(R, G, B, A);
		return true;
	}

	static TSharedPtr<FJsonObject> ColorToJson(const FLinearColor& Color)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("r"), Color.R);
		Obj->SetNumberField(TEXT("g"), Color.G);
		Obj->SetNumberField(TEXT("b"), Color.B);
		Obj->SetNumberField(TEXT("a"), Color.A);
		return Obj;
	}

	static TSharedPtr<FJsonObject> PinPropsToJson(const FPCGPinProperties& Pin)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("label"), Pin.Label.ToString());
		Obj->SetBoolField(TEXT("allow_multiple_data"), Pin.bAllowMultipleData);
		Obj->SetBoolField(TEXT("allow_multiple_connections"), Pin.AllowsMultipleConnections());
		Obj->SetNumberField(TEXT("allowed_types_mask"), static_cast<uint32>(static_cast<EPCGDataType>(Pin.AllowedTypes)));
		Obj->SetStringField(TEXT("tooltip"), Pin.Tooltip.ToString());
		return Obj;
	}

	static TArray<TSharedPtr<FJsonValue>> PinsToJson(const TArray<TObjectPtr<UPCGPin>>& Pins)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const TObjectPtr<UPCGPin>& Pin : Pins)
		{
			if (Pin)
			{
				Result.Add(MakeShared<FJsonValueObject>(PinPropsToJson(Pin->Properties)));
			}
		}
		return Result;
	}

	static TArray<TSharedPtr<FJsonValue>> DefaultPinsToJson(const TArray<FPCGPinProperties>& Pins)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FPCGPinProperties& Pin : Pins)
		{
			Result.Add(MakeShared<FJsonValueObject>(PinPropsToJson(Pin)));
		}
		return Result;
	}

	static TSharedPtr<FJsonObject> NodeToJson(const UPCGNode* Node)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Node)
		{
			return Obj;
		}

		Obj->SetStringField(TEXT("node_id"), Node->GetName());
		Obj->SetStringField(TEXT("title"), Node->NodeTitle.ToString());
		Obj->SetStringField(TEXT("comment"), Node->NodeComment);
		Obj->SetBoolField(TEXT("comment_visible"), Node->bCommentBubbleVisible);
		Obj->SetBoolField(TEXT("enabled"), true);
#if WITH_EDITOR
		int32 PositionX = 0;
		int32 PositionY = 0;
		Node->GetNodePosition(PositionX, PositionY);
		Obj->SetNumberField(TEXT("position_x"), PositionX);
		Obj->SetNumberField(TEXT("position_y"), PositionY);
#endif
		if (const UPCGSettings* Settings = Node->GetSettings())
		{
			Obj->SetStringField(TEXT("settings_class"), Settings->GetClass()->GetName());
			Obj->SetStringField(TEXT("default_title"), Settings->GetDefaultNodeTitle().ToString());
		}
		Obj->SetArrayField(TEXT("input_pins"), PinsToJson(Node->GetInputPins()));
		Obj->SetArrayField(TEXT("output_pins"), PinsToJson(Node->GetOutputPins()));
		return Obj;
	}

	static TArray<TSharedPtr<FJsonValue>> GraphEdgesToJson(const UPCGGraph* Graph)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		if (!Graph)
		{
			return Result;
		}

		TArray<const UPCGNode*> Nodes;
		if (const UPCGNode* Input = Graph->GetInputNode()) Nodes.Add(Input);
		for (const UPCGNode* Node : Graph->GetNodes()) if (Node) Nodes.Add(Node);
		if (const UPCGNode* Output = Graph->GetOutputNode()) Nodes.Add(Output);

		TSet<const UPCGEdge*> SeenEdges;
		for (const UPCGNode* Node : Nodes)
		{
			for (const TObjectPtr<UPCGPin>& Pin : Node->GetOutputPins())
			{
				if (!Pin) continue;
				for (const TObjectPtr<UPCGEdge>& EdgePtr : Pin->Edges)
				{
					const UPCGEdge* Edge = EdgePtr.Get();
					if (!Edge || SeenEdges.Contains(Edge) || !Edge->InputPin || !Edge->OutputPin)
					{
						continue;
					}
					SeenEdges.Add(Edge);
					TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
					Obj->SetStringField(TEXT("from_node_id"), Edge->InputPin->Node ? Edge->InputPin->Node->GetName() : FString());
					Obj->SetStringField(TEXT("from_pin"), Edge->InputPin->Properties.Label.ToString());
					Obj->SetStringField(TEXT("to_node_id"), Edge->OutputPin->Node ? Edge->OutputPin->Node->GetName() : FString());
					Obj->SetStringField(TEXT("to_pin"), Edge->OutputPin->Properties.Label.ToString());
					Result.Add(MakeShared<FJsonValueObject>(Obj));
				}
			}
		}
		return Result;
	}

	static TArray<TSharedPtr<FJsonValue>> BagSchemaToJson(const FInstancedPropertyBag* Bag)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		const UPropertyBag* BagStruct = Bag ? Bag->GetPropertyBagStruct() : nullptr;
		if (!BagStruct)
		{
			return Result;
		}
		for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), Desc.Name.ToString());
			if (const UEnum* TypeEnum = StaticEnum<EPropertyBagPropertyType>())
			{
				Obj->SetStringField(TEXT("type"), TypeEnum->GetNameStringByValue(static_cast<int64>(Desc.ValueType)));
			}
			if (const UEnum* ContainerEnum = StaticEnum<EPropertyBagContainerType>())
			{
				Obj->SetStringField(TEXT("container"), ContainerEnum->GetNameStringByValue(static_cast<int64>(Desc.ContainerTypes.GetFirstContainerType())));
			}
			if (Desc.CachedProperty)
			{
				Obj->SetStringField(TEXT("cpp_type"), Desc.CachedProperty->GetCPPType());
			}
			if (Desc.ValueTypeObject)
			{
				Obj->SetStringField(TEXT("type_object"), Desc.ValueTypeObject->GetPathName());
			}
			Result.Add(MakeShared<FJsonValueObject>(Obj));
		}
		return Result;
	}

// END CHUNK 1
	static TSharedPtr<FJsonValue> ExportPropertyValue(const UObject* Owner, const FProperty* Property)
	{
		if (!Owner || !Property)
		{
			return MakeShared<FJsonValueNull>();
		}
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);
		FString Text;
		Property->ExportTextItem_Direct(Text, ValuePtr, nullptr, const_cast<UObject*>(Owner), PPF_None);
		return MakeShared<FJsonValueString>(Text);
	}

	static TArray<TSharedPtr<FJsonValue>> EditablePropertiesToJson(const UObject* Object)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		if (!Object)
		{
			return Result;
		}
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance | CPF_Deprecated))
			{
				continue;
			}
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), Property->GetName());
			Obj->SetStringField(TEXT("cpp_type"), Property->GetCPPType());
			Obj->SetField(TEXT("value"), ExportPropertyValue(Object, Property));
			Result.Add(MakeShared<FJsonValueObject>(Obj));
		}
		return Result;
	}

	static bool SetUObjectPropertyFromJson(UObject* Object, const FString& PropertyName, const TSharedPtr<FJsonValue>& ValueJson, FString& OutError)
	{
		if (!Object || !ValueJson.IsValid())
		{
			OutError = TEXT("object and value are required");
			return false;
		}
		FProperty* Property = Object->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			OutError = FString::Printf(TEXT("Property '%s' not found on '%s'"), *PropertyName, *Object->GetClass()->GetName());
			return false;
		}
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		bool bApplied = false;
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			bool BoolValue = false;
			if (ValueJson->TryGetBool(BoolValue))
			{
				BoolProp->SetPropertyValue(ValuePtr, BoolValue);
				bApplied = true;
			}
		}
		else if (FNumericProperty* NumberProp = CastField<FNumericProperty>(Property))
		{
			double Number = 0.0;
			if (ValueJson->TryGetNumber(Number))
			{
				if (NumberProp->IsInteger())
				{
					NumberProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(Number));
				}
				else
				{
					NumberProp->SetFloatingPointPropertyValue(ValuePtr, Number);
				}
				bApplied = true;
			}
		}
		else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
		{
			FString StringValue;
			if (ValueJson->TryGetString(StringValue))
			{
				StrProp->SetPropertyValue(ValuePtr, StringValue);
				bApplied = true;
			}
		}
		else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			FString StringValue;
			if (ValueJson->TryGetString(StringValue))
			{
				NameProp->SetPropertyValue(ValuePtr, FName(*StringValue));
				bApplied = true;
			}
		}
		else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			FString StringValue;
			if (ValueJson->TryGetString(StringValue))
			{
				TextProp->SetPropertyValue(ValuePtr, FText::FromString(StringValue));
				bApplied = true;
			}
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			FString StringValue;
			double Number = 0.0;
			if (ValueJson->TryGetString(StringValue))
			{
				UEnum* Enum = EnumProp->GetEnum();
				const int64 EnumValue = Enum ? Enum->GetValueByNameString(StringValue) : INDEX_NONE;
				if (EnumValue != INDEX_NONE)
				{
					EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
					bApplied = true;
				}
			}
			else if (ValueJson->TryGetNumber(Number))
			{
				EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(Number));
				bApplied = true;
			}
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct == TBaseStructure<FVector>::Get())
			{
				FVector Vector;
				if (JsonToVector(ValueJson, Vector))
				{
					StructProp->CopyCompleteValue(ValuePtr, &Vector);
					bApplied = true;
				}
			}
			else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
			{
				FRotator Rotator;
				if (JsonToRotator(ValueJson, Rotator))
				{
					StructProp->CopyCompleteValue(ValuePtr, &Rotator);
					bApplied = true;
				}
			}
		}

		if (!bApplied)
		{
			OutError = FString::Printf(TEXT("Property '%s' of type '%s' could not be assigned"), *PropertyName, *Property->GetCPPType());
			return false;
		}
		Object->Modify();
		return true;
	}

	static bool SetObjectPropertiesFromJson(UObject* Object, const TSharedPtr<FJsonObject>& Properties, FString& OutError)
	{
		if (!Properties.IsValid())
		{
			return true;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
		{
			if (!SetUObjectPropertyFromJson(Object, Pair.Key, Pair.Value, OutError))
			{
				return false;
			}
		}
		return true;
	}

	static bool SetBagScalarValue(FInstancedPropertyBag& Bag, const FName Name, const FPropertyBagPropertyDesc& Desc, const TSharedPtr<FJsonValue>& ValueJson, FString& OutError)
	{
		EPropertyBagResult Result = EPropertyBagResult::TypeMismatch;
		double Number = 0.0;
		FString StringValue;
		bool BoolValue = false;
		switch (Desc.ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			if (ValueJson->TryGetBool(BoolValue)) Result = Bag.SetValueBool(Name, BoolValue);
			break;
		case EPropertyBagPropertyType::Int32:
			if (ValueJson->TryGetNumber(Number)) Result = Bag.SetValueInt32(Name, static_cast<int32>(Number));
			break;
		case EPropertyBagPropertyType::Int64:
			if (ValueJson->TryGetNumber(Number)) Result = Bag.SetValueInt64(Name, static_cast<int64>(Number));
			break;
		case EPropertyBagPropertyType::Float:
			if (ValueJson->TryGetNumber(Number)) Result = Bag.SetValueFloat(Name, static_cast<float>(Number));
			break;
		case EPropertyBagPropertyType::Double:
			if (ValueJson->TryGetNumber(Number)) Result = Bag.SetValueDouble(Name, Number);
			break;
		case EPropertyBagPropertyType::Name:
			if (ValueJson->TryGetString(StringValue)) Result = Bag.SetValueName(Name, FName(*StringValue));
			break;
		case EPropertyBagPropertyType::String:
			if (ValueJson->TryGetString(StringValue)) Result = Bag.SetValueString(Name, StringValue);
			break;
		case EPropertyBagPropertyType::Text:
			if (ValueJson->TryGetString(StringValue)) Result = Bag.SetValueText(Name, FText::FromString(StringValue));
			break;
		case EPropertyBagPropertyType::Struct:
			if (Desc.ValueTypeObject.Get() == TBaseStructure<FVector>::Get())
			{
				FVector Vector;
				if (JsonToVector(ValueJson, Vector)) Result = Bag.SetValueStruct(Name, Vector);
			}
			else if (Desc.ValueTypeObject.Get() == TBaseStructure<FRotator>::Get())
			{
				FRotator Rotator;
				if (JsonToRotator(ValueJson, Rotator)) Result = Bag.SetValueStruct(Name, Rotator);
			}
			break;
		case EPropertyBagPropertyType::SoftObject:
			if (ValueJson->TryGetString(StringValue)) Result = Bag.SetValueSoftPath(Name, FSoftObjectPath(StringValue));
			break;
		default:
			break;
		}
		if (Result != EPropertyBagResult::Success)
		{
			OutError = FString::Printf(TEXT("Failed to set parameter '%s': %s"), *Name.ToString(), *BagResultToString(Result));
			return false;
		}
		return true;
	}

	static bool SetBagValue(FInstancedPropertyBag& Bag, const FName Name, const TSharedPtr<FJsonValue>& ValueJson, FString& OutError)
	{
		const FPropertyBagPropertyDesc* Desc = FindPropertyDesc(Bag, Name);
		if (!Desc)
		{
			OutError = FString::Printf(TEXT("Parameter '%s' was not found"), *Name.ToString());
			return false;
		}
		if (!Desc->ContainerTypes.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Parameter '%s' uses an unsupported container type"), *Name.ToString());
			return false;
		}
		return SetBagScalarValue(Bag, Name, *Desc, ValueJson, OutError);
	}

	static AActor* FindActorByLabel(UWorld* World, const FString& ActorName)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && (Actor->GetName() == ActorName || Actor->GetActorNameOrLabel() == ActorName))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	static UPCGComponent* FindPCGComponent(AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<UPCGComponent>() : nullptr;
	}

	static TSharedPtr<FJsonObject> ComponentToJson(AActor* Actor, UPCGComponent* Component)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("actor_name"), Actor ? Actor->GetActorNameOrLabel() : FString());
		Obj->SetStringField(TEXT("actor_path"), Actor ? Actor->GetPathName() : FString());
		if (Component)
		{
			Obj->SetStringField(TEXT("component_name"), Component->GetName());
			if (UPCGGraphInterface* GraphInterface = Component->GetGraphInstance())
			{
				Obj->SetStringField(TEXT("graph_instance_path"), GraphInterface->GetPathName());
				if (const UPCGGraph* Graph = GraphInterface->GetGraph())
				{
					Obj->SetStringField(TEXT("graph_path"), Graph->GetPathName());
				}
			}
		}
		return Obj;
	}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	static FIntRect GetNodeBounds(const TArray<UPCGNode*>& Nodes)
	{
		FIntRect Bounds(0, 0, 300, 180);
		bool bInitialized = false;
		for (const UPCGNode* Node : Nodes)
		{
			if (!Node)
			{
				continue;
			}
			int32 PositionX = 0;
			int32 PositionY = 0;
			Node->GetNodePosition(PositionX, PositionY);
			FIntRect NodeRect(PositionX - 40, PositionY - 40, PositionX + 300, PositionY + 180);
			if (!bInitialized)
			{
				Bounds = NodeRect;
				bInitialized = true;
			}
			else
			{
				Bounds.Union(NodeRect);
			}
		}
		return Bounds;
	}

	static TArray<UPCGNode*> ResolveNodes(UPCGGraph* Graph, const TArray<TSharedPtr<FJsonValue>>& NodeIds, FString& OutError)
	{
		TArray<UPCGNode*> Nodes;
		for (const TSharedPtr<FJsonValue>& Value : NodeIds)
		{
			FString NodeId;
			if (!Value.IsValid() || !Value->TryGetString(NodeId))
			{
				OutError = TEXT("node_ids must contain strings");
				return {};
			}
			UPCGNode* Node = FindNodeById(Graph, NodeId);
			if (!Node)
			{
				OutError = FString::Printf(TEXT("node_id '%s' not found"), *NodeId);
				return {};
			}
			Nodes.Add(Node);
		}
		return Nodes;
	}
#endif
}

using namespace ECAPcgNativeCompat;

FECACommandResult FECACommand_GetPCGGraphStructure::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	TArray<TSharedPtr<FJsonValue>> Nodes;
	if (UPCGNode* Input = Graph->GetInputNode()) Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Input)));
	for (UPCGNode* Node : Graph->GetNodes()) if (Node) Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
	if (UPCGNode* Output = Graph->GetOutputNode()) Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Output)));

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Out->SetStringField(TEXT("graph_name"), Graph->GetName());
	Out->SetStringField(TEXT("description"), Graph->Description.ToString());
	Out->SetArrayField(TEXT("nodes"), Nodes);
	Out->SetArrayField(TEXT("edges"), GraphEdgesToJson(Graph));
	Out->SetArrayField(TEXT("parameters"), BagSchemaToJson(Graph->GetUserParametersStruct()));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	TArray<TSharedPtr<FJsonValue>> Comments;
	for (const FPCGGraphCommentNodeData& Comment : Graph->GetCommentNodes())
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("comment_id"), Comment.GUID.ToString());
		Obj->SetStringField(TEXT("comment"), Comment.NodeComment);
		Obj->SetNumberField(TEXT("position_x"), Comment.NodePosX);
		Obj->SetNumberField(TEXT("position_y"), Comment.NodePosY);
		Obj->SetNumberField(TEXT("width"), Comment.NodeWidth);
		Obj->SetNumberField(TEXT("height"), Comment.NodeHeight);
		Obj->SetObjectField(TEXT("color"), ColorToJson(Comment.CommentColor));
		Comments.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Out->SetArrayField(TEXT("comment_boxes"), Comments);
#else
	Out->SetArrayField(TEXT("comment_boxes"), {});
#endif
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetPCGGraphSchema::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Out->SetArrayField(TEXT("parameters"), BagSchemaToJson(Graph->GetUserParametersStruct()));
	Out->SetArrayField(TEXT("input_pins"), Graph->GetInputNode() ? PinsToJson(Graph->GetInputNode()->GetOutputPins()) : TArray<TSharedPtr<FJsonValue>>{});
	Out->SetArrayField(TEXT("output_pins"), Graph->GetOutputNode() ? PinsToJson(Graph->GetOutputNode()->GetInputPins()) : TArray<TSharedPtr<FJsonValue>>{});
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetPCGGraphDescription::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Out->SetStringField(TEXT("description"), Graph->Description.ToString());
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_SetPCGGraphDescription::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString Description;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("description"), Description, true)) return FECACommandResult::ValidationError(this, TEXT("description is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	Graph->Modify();
	Graph->Description = FText::FromString(Description);
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Out->SetStringField(TEXT("description"), Description);
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_ListPCGNativeNodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	bool bCommonOnly = true;
	GetBoolParam(Params, TEXT("common_only"), bCommonOnly, false);
	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);
	UPackage* PCGPackage = FindPackage(nullptr, TEXT("/Script/PCG"));
	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->IsChildOf(UPCGSettings::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden)) continue;
		if (bCommonOnly && PCGPackage && !Class->IsInPackage(PCGPackage)) continue;
		const UPCGSettings* CDO = Cast<UPCGSettings>(Class->GetDefaultObject(false));
		if (bCommonOnly && CDO && !CDO->bExposeToLibrary) continue;
		const FString ClassName = Class->GetName();
		const FString Title = CDO ? CDO->GetDefaultNodeTitle().ToString() : ClassName;
		if (!NameFilter.IsEmpty() && !ClassName.Contains(NameFilter, ESearchCase::IgnoreCase) && !Title.Contains(NameFilter, ESearchCase::IgnoreCase)) continue;
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("settings_class"), ClassName);
		Obj->SetStringField(TEXT("node_name"), Title);
		Obj->SetStringField(TEXT("default_node_name"), CDO ? CDO->GetDefaultNodeName().ToString() : ClassName);
		Nodes.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Nodes.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("node_name")).Compare(B->AsObject()->GetStringField(TEXT("node_name")), ESearchCase::IgnoreCase) < 0;
	});
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetArrayField(TEXT("nodes"), Nodes);
	Out->SetNumberField(TEXT("count"), Nodes.Num());
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetPCGNativeNodeSchema::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!GetStringParam(Params, TEXT("settings_class"), ClassName, true)) return FECACommandResult::ValidationError(this, TEXT("settings_class is required"));
	UClass* Class = FindPCGSettingsClass(ClassName);
	if (!Class) return FECACommandResult::Error(FString::Printf(TEXT("UPCGSettings subclass '%s' not found"), *ClassName));
	UPCGSettings* CDO = Cast<UPCGSettings>(Class->GetDefaultObject(true));
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("settings_class"), Class->GetName());
	Out->SetStringField(TEXT("node_name"), CDO ? CDO->GetDefaultNodeTitle().ToString() : Class->GetName());
	Out->SetStringField(TEXT("description"), Class->GetToolTipText().ToString());
	Out->SetArrayField(TEXT("properties"), EditablePropertiesToJson(CDO));
	Out->SetArrayField(TEXT("input_pins"), CDO ? DefaultPinsToJson(CDO->DefaultInputPinProperties()) : TArray<TSharedPtr<FJsonValue>>{});
	Out->SetArrayField(TEXT("output_pins"), CDO ? DefaultPinsToJson(CDO->DefaultOutputPinProperties()) : TArray<TSharedPtr<FJsonValue>>{});
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_ListPCGAvailableSubgraphs::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter;
	GetStringParam(Params, TEXT("path_filter"), PathFilter, false);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UPCGGraph::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);
	TArray<TSharedPtr<FJsonValue>> Graphs;
	for (const FAssetData& Asset : Assets)
	{
		const FString Path = Asset.GetObjectPathString();
		if (!PathFilter.IsEmpty() && !Path.Contains(PathFilter, ESearchCase::IgnoreCase)) continue;
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("graph_path"), Path);
		Obj->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
		Graphs.Add(MakeShared<FJsonValueObject>(Obj));
	}
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetArrayField(TEXT("graphs"), Graphs);
	Out->SetNumberField(TEXT("count"), Graphs.Num());
	return FECACommandResult::Success(Out);
}

// END CHUNK 2
FECACommandResult FECACommand_AddPCGSubgraphNode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString SubgraphPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("subgraph_path"), SubgraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("subgraph_path is required"));
	FString NodeId;
	FString NodeTitle;
	FString NodeComment;
	GetStringParam(Params, TEXT("node_id"), NodeId, false);
	GetStringParam(Params, TEXT("node_title"), NodeTitle, false);
	GetStringParam(Params, TEXT("node_comment"), NodeComment, false);
	double X = 0.0;
	double Y = 0.0;
	GetFloatParam(Params, TEXT("position_x"), X, false);
	GetFloatParam(Params, TEXT("position_y"), Y, false);

	UPCGGraph* Graph = LoadGraph(GraphPath);
	UPCGGraph* Subgraph = LoadGraph(SubgraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	if (!Subgraph) return FECACommandResult::Error(FString::Printf(TEXT("Subgraph PCGGraph not found at '%s'"), *SubgraphPath));

	Graph->Modify();
	UPCGSubgraphSettings* SubgraphSettings = nullptr;
	UPCGNode* NewNode = Graph->AddNodeOfType(SubgraphSettings);
	if (!NewNode || !SubgraphSettings)
	{
		return FECACommandResult::Error(TEXT("Failed to create PCG subgraph node"));
	}
	SubgraphSettings->SetSubgraph(Subgraph);
	if (!NodeId.IsEmpty())
	{
		NewNode->Rename(*NodeId, NewNode->GetOuter());
	}
	NewNode->NodeTitle = FName(*NodeTitle);
	NewNode->NodeComment = NodeComment;
	NewNode->bCommentBubbleVisible = !NodeComment.IsEmpty();
#if WITH_EDITOR
	NewNode->SetNodePosition(static_cast<int32>(X), static_cast<int32>(Y));
#endif
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Out->SetStringField(TEXT("node_id"), NewNode->GetName());
	Out->SetStringField(TEXT("subgraph_path"), Subgraph->GetPathName());
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_UpdatePCGNode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString NodeId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true)) return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	FString NodeTitle;
	GetStringParam(Params, TEXT("node_title"), NodeTitle, false);
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	GetObjectParam(Params, TEXT("properties"), Properties, false);
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	UPCGNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found"), *NodeId));
	Graph->Modify();
	Node->Modify();
	if (!NodeTitle.IsEmpty())
	{
		Node->NodeTitle = FName(*NodeTitle);
	}
	if (Properties && Properties->IsValid())
	{
		UPCGSettings* Settings = Node->GetSettings();
		if (!Settings) return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' has no settings"), *NodeId));
		FString Error;
		if (!SetObjectPropertiesFromJson(Settings, *Properties, Error))
		{
			return FECACommandResult::Error(Error);
		}
	}
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetObjectField(TEXT("node"), NodeToJson(Node));
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetPCGNodeInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString NodeId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true)) return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	UPCGNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found"), *NodeId));
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetObjectField(TEXT("node"), NodeToJson(Node));
	if (const UPCGSettings* Settings = Node->GetSettings())
	{
		Out->SetArrayField(TEXT("properties"), EditablePropertiesToJson(Settings));
	}
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_RepositionPCGNode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString NodeId;
	double X = 0.0;
	double Y = 0.0;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true)) return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	if (!GetFloatParam(Params, TEXT("position_x"), X, true)) return FECACommandResult::ValidationError(this, TEXT("position_x is required"));
	if (!GetFloatParam(Params, TEXT("position_y"), Y, true)) return FECACommandResult::ValidationError(this, TEXT("position_y is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	UPCGNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found"), *NodeId));
	Node->Modify();
#if WITH_EDITOR
	Node->SetNodePosition(static_cast<int32>(X), static_cast<int32>(Y));
#endif
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	return FECACommand_GetPCGNodeInfo().Execute(Params);
}

FECACommandResult FECACommand_SetPCGNodeComment::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString NodeId;
	FString Comment;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true)) return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	if (!GetStringParam(Params, TEXT("comment"), Comment, true)) return FECACommandResult::ValidationError(this, TEXT("comment is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	UPCGNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found"), *NodeId));
	Node->Modify();
	Node->NodeComment = Comment;
	Node->bCommentBubbleVisible = !Comment.IsEmpty();
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	return FECACommand_GetPCGNodeInfo().Execute(Params);
}

FECACommandResult FECACommand_RemovePCGNode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString NodeId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true)) return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	UPCGNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found"), *NodeId));
	Graph->Modify();
	Graph->RemoveNode(Node);
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Out->SetStringField(TEXT("removed_node_id"), NodeId);
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_DisconnectPCGNodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString FromId;
	FString ToId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("from_node_id"), FromId, true)) return FECACommandResult::ValidationError(this, TEXT("from_node_id is required"));
	if (!GetStringParam(Params, TEXT("to_node_id"), ToId, true)) return FECACommandResult::ValidationError(this, TEXT("to_node_id is required"));
	FString FromPin;
	FString ToPin;
	GetStringParam(Params, TEXT("from_pin"), FromPin, false);
	GetStringParam(Params, TEXT("to_pin"), ToPin, false);
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	UPCGNode* From = FindNodeById(Graph, FromId);
	UPCGNode* To = FindNodeById(Graph, ToId);
	if (!From) return FECACommandResult::Error(FString::Printf(TEXT("from_node_id '%s' not found"), *FromId));
	if (!To) return FECACommandResult::Error(FString::Printf(TEXT("to_node_id '%s' not found"), *ToId));
	if (FromPin.IsEmpty() && From->GetOutputPins().Num() > 0 && From->GetOutputPins()[0]) FromPin = From->GetOutputPins()[0]->Properties.Label.ToString();
	if (ToPin.IsEmpty() && To->GetInputPins().Num() > 0 && To->GetInputPins()[0]) ToPin = To->GetInputPins()[0]->Properties.Label.ToString();
	if (FromPin.IsEmpty() || ToPin.IsEmpty()) return FECACommandResult::Error(TEXT("from_pin and to_pin are required when default pins cannot be inferred"));
	Graph->Modify();
	if (!Graph->RemoveEdge(From, FName(*FromPin), To, FName(*ToPin)))
	{
		return FECACommandResult::Error(TEXT("UPCGGraph::RemoveEdge failed; check node ids and pin labels"));
	}
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetBoolField(TEXT("disconnected"), true);
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_AddPCGCommentBox::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	FString GraphPath;
	FString Comment;
	const TArray<TSharedPtr<FJsonValue>>* NodeIds = nullptr;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetArrayParam(Params, TEXT("node_ids"), NodeIds, true)) return FECACommandResult::ValidationError(this, TEXT("node_ids is required"));
	if (!GetStringParam(Params, TEXT("comment"), Comment, true)) return FECACommandResult::ValidationError(this, TEXT("comment is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	FString Error;
	TArray<UPCGNode*> Nodes = ResolveNodes(Graph, *NodeIds, Error);
	if (!Error.IsEmpty()) return FECACommandResult::Error(Error);
	FLinearColor Color = FLinearColor::White;
	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (GetObjectParam(Params, TEXT("color"), ColorObj, false) && ColorObj) JsonToColor(*ColorObj, Color);
	const FIntRect Bounds = GetNodeBounds(Nodes);
	TArray<FPCGGraphCommentNodeData> Comments = Graph->GetCommentNodes();
	FPCGGraphCommentNodeData& NewComment = Comments.Emplace_GetRef(Bounds.Min.X, Bounds.Min.Y, Bounds.Width(), Bounds.Height(), Comment, Color);
	NewComment.GUID = FGuid::NewGuid();
	Graph->Modify();
	Graph->SetCommentNodes(Comments);
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("comment_id"), NewComment.GUID.ToString());
	Out->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	return FECACommandResult::Success(Out);
#else
	return FECACommandResult::Error(TEXT("PCG graph comment boxes require UE 5.8 or newer"));
#endif
}

FECACommandResult FECACommand_UpdatePCGCommentBox::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	FString GraphPath;
	FString CommentId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("comment_id"), CommentId, true)) return FECACommandResult::ValidationError(this, TEXT("comment_id is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	TArray<FPCGGraphCommentNodeData> Comments = Graph->GetCommentNodes();
	FPCGGraphCommentNodeData* Found = Comments.FindByPredicate([&](const FPCGGraphCommentNodeData& Item) { return Item.GUID.ToString() == CommentId; });
	if (!Found) return FECACommandResult::Error(FString::Printf(TEXT("comment_id '%s' not found"), *CommentId));
	FString Comment;
	if (GetStringParam(Params, TEXT("comment"), Comment, false)) Found->NodeComment = Comment;
	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (GetObjectParam(Params, TEXT("color"), ColorObj, false) && ColorObj) JsonToColor(*ColorObj, Found->CommentColor);
	const TArray<TSharedPtr<FJsonValue>>* NodeIds = nullptr;
	if (GetArrayParam(Params, TEXT("node_ids"), NodeIds, false) && NodeIds)
	{
		FString Error;
		const TArray<UPCGNode*> Nodes = ResolveNodes(Graph, *NodeIds, Error);
		if (!Error.IsEmpty()) return FECACommandResult::Error(Error);
		const FIntRect Bounds = GetNodeBounds(Nodes);
		Found->NodePosX = Bounds.Min.X;
		Found->NodePosY = Bounds.Min.Y;
		Found->NodeWidth = Bounds.Width();
		Found->NodeHeight = Bounds.Height();
	}
	Graph->Modify();
	Graph->SetCommentNodes(Comments);
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetBoolField(TEXT("updated"), true);
	Out->SetStringField(TEXT("comment_id"), CommentId);
	return FECACommandResult::Success(Out);
#else
	return FECACommandResult::Error(TEXT("PCG graph comment boxes require UE 5.8 or newer"));
#endif
}

FECACommandResult FECACommand_RemovePCGCommentBox::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	FString GraphPath;
	FString CommentId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("comment_id"), CommentId, true)) return FECACommandResult::ValidationError(this, TEXT("comment_id is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	TArray<FPCGGraphCommentNodeData> Comments = Graph->GetCommentNodes();
	const int32 Removed = Comments.RemoveAll([&](const FPCGGraphCommentNodeData& Item) { return Item.GUID.ToString() == CommentId; });
	if (Removed == 0) return FECACommandResult::Error(FString::Printf(TEXT("comment_id '%s' not found"), *CommentId));
	Graph->Modify();
	Graph->SetCommentNodes(Comments);
	FString SaveError;
	SaveAssetPackage(Graph, SaveError);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetBoolField(TEXT("removed"), true);
	Out->SetStringField(TEXT("comment_id"), CommentId);
	return FECACommandResult::Success(Out);
#else
	return FECACommandResult::Error(TEXT("PCG graph comment boxes require UE 5.8 or newer"));
#endif
}

// END CHUNK 3
FECACommandResult FECACommand_ListPCGGraphInstances::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("Editor world not available"));
	TArray<TSharedPtr<FJsonValue>> Instances;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		UPCGComponent* Component = FindPCGComponent(Actor);
		if (Component && Component->GetGraphInstance())
		{
			Instances.Add(MakeShared<FJsonValueObject>(ComponentToJson(Actor, Component)));
		}
	}
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetArrayField(TEXT("instances"), Instances);
	Out->SetNumberField(TEXT("count"), Instances.Num());
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_SpawnPCGGraphInstance::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	FString ActorName;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true)) return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true)) return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	UPCGGraph* Graph = LoadGraph(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("Editor world not available"));

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector(25.0, 25.0, 10.0);
	const TSharedPtr<FJsonObject>* LocationObj = nullptr;
	if (GetObjectParam(Params, TEXT("location"), LocationObj, false) && LocationObj)
	{
		double X = 0.0; double Y = 0.0; double Z = 0.0;
		(*LocationObj)->TryGetNumberField(TEXT("x"), X);
		(*LocationObj)->TryGetNumberField(TEXT("y"), Y);
		(*LocationObj)->TryGetNumberField(TEXT("z"), Z);
		Location = FVector(X, Y, Z);
	}
	const TSharedPtr<FJsonObject>* RotationObj = nullptr;
	if (GetObjectParam(Params, TEXT("rotation"), RotationObj, false) && RotationObj)
	{
		double Pitch = 0.0; double Yaw = 0.0; double Roll = 0.0;
		(*RotationObj)->TryGetNumberField(TEXT("pitch"), Pitch);
		(*RotationObj)->TryGetNumberField(TEXT("yaw"), Yaw);
		(*RotationObj)->TryGetNumberField(TEXT("roll"), Roll);
		Rotation = FRotator(Pitch, Yaw, Roll);
	}
	const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
	if (GetObjectParam(Params, TEXT("scale"), ScaleObj, false) && ScaleObj)
	{
		double X = 25.0; double Y = 25.0; double Z = 10.0;
		(*ScaleObj)->TryGetNumberField(TEXT("x"), X);
		(*ScaleObj)->TryGetNumberField(TEXT("y"), Y);
		(*ScaleObj)->TryGetNumberField(TEXT("z"), Z);
		Scale = FVector(X, Y, Z);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, APCGVolume::StaticClass(), FName(*ActorName));
	APCGVolume* Volume = World->SpawnActor<APCGVolume>(APCGVolume::StaticClass(), Location, Rotation, SpawnParams);
	if (!Volume) return FECACommandResult::Error(TEXT("Failed to spawn APCGVolume"));
	Volume->SetActorLabel(ActorName);
	Volume->SetActorScale3D(Scale);
	UPCGComponent* Component = FindPCGComponent(Volume);
	if (!Component)
	{
		Volume->Destroy();
		return FECACommandResult::Error(TEXT("Spawned PCGVolume has no PCGComponent"));
	}
	Component->SetGraph(Graph);
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetObjectField(TEXT("instance"), ComponentToJson(Volume, Component));
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetPCGGraphInstanceParams::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true)) return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	AActor* Actor = FindActorByLabel(GetEditorWorld(), ActorName);
	if (!Actor) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	UPCGComponent* Component = FindPCGComponent(Actor);
	if (!Component || !Component->GetGraphInstance()) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no PCG graph instance"), *ActorName));
	UPCGGraphInstance* Instance = Component->GetGraphInstance();
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetObjectField(TEXT("instance"), ComponentToJson(Actor, Component));
	Out->SetArrayField(TEXT("parameters"), BagSchemaToJson(&Instance->ParametersOverrides.Parameters));
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_SetPCGGraphInstanceParams::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true)) return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!GetObjectParam(Params, TEXT("params"), ParamsObj, true)) return FECACommandResult::ValidationError(this, TEXT("params is required"));
	AActor* Actor = FindActorByLabel(GetEditorWorld(), ActorName);
	if (!Actor) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	UPCGComponent* Component = FindPCGComponent(Actor);
	if (!Component || !Component->GetGraphInstance()) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no PCG graph instance"), *ActorName));
	UPCGGraphInstance* Instance = Component->GetGraphInstance();
	FInstancedPropertyBag& Bag = Instance->ParametersOverrides.Parameters;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ParamsObj)->Values)
	{
		FString Error;
		if (!SetBagValue(Bag, FName(*Pair.Key), Pair.Value, Error))
		{
			return FECACommandResult::Error(Error);
		}
		if (const FPropertyBagPropertyDesc* Desc = FindPropertyDesc(Bag, FName(*Pair.Key)))
		{
			if (Desc->CachedProperty)
			{
				Instance->UpdatePropertyOverride(Desc->CachedProperty, true);
			}
		}
	}
	Actor->Modify();
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("actor_name"), Actor->GetActorNameOrLabel());
	Out->SetArrayField(TEXT("parameters"), BagSchemaToJson(&Bag));
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_ResetPCGGraphInstanceParams::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true)) return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
	if (!GetArrayParam(Params, TEXT("param_names"), Names, true)) return FECACommandResult::ValidationError(this, TEXT("param_names is required"));
	AActor* Actor = FindActorByLabel(GetEditorWorld(), ActorName);
	if (!Actor) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	UPCGComponent* Component = FindPCGComponent(Actor);
	if (!Component || !Component->GetGraphInstance()) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no PCG graph instance"), *ActorName));
	UPCGGraphInstance* Instance = Component->GetGraphInstance();
	FInstancedPropertyBag& Bag = Instance->ParametersOverrides.Parameters;
	TArray<TSharedPtr<FJsonValue>> Reset;
	for (const TSharedPtr<FJsonValue>& NameValue : *Names)
	{
		FString Name;
		if (!NameValue.IsValid() || !NameValue->TryGetString(Name)) return FECACommandResult::Error(TEXT("param_names must contain strings"));
		const FPropertyBagPropertyDesc* Desc = FindPropertyDesc(Bag, FName(*Name));
		if (!Desc || !Desc->CachedProperty) return FECACommandResult::Error(FString::Printf(TEXT("Parameter '%s' was not found"), *Name));
		Instance->UpdatePropertyOverride(Desc->CachedProperty, false);
		Reset.Add(MakeShared<FJsonValueString>(Name));
	}
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("actor_name"), Actor->GetActorNameOrLabel());
	Out->SetArrayField(TEXT("reset"), Reset);
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_ExecutePCGGraphInstance::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true)) return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	bool bForce = false;
	GetBoolParam(Params, TEXT("force"), bForce, false);
	AActor* Actor = FindActorByLabel(GetEditorWorld(), ActorName);
	if (!Actor) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	UPCGComponent* Component = FindPCGComponent(Actor);
	if (!Component || !Component->GetGraphInstance()) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no PCG graph instance"), *ActorName));
	Component->GenerateLocal(bForce);
	const FPCGDataCollection& Collection = Component->GetGeneratedGraphOutput();
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetObjectField(TEXT("instance"), ComponentToJson(Actor, Component));
	Out->SetNumberField(TEXT("output_count"), Collection.GetAllInputs().Num());
	TArray<TSharedPtr<FJsonValue>> Data;
	for (const FPCGTaggedData& Entry : Collection.GetAllInputs())
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("pin"), Entry.Pin.ToString());
		Obj->SetStringField(TEXT("class"), Entry.Data ? Entry.Data->GetClass()->GetName() : TEXT("None"));
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(Entry.Data))
		{
			Obj->SetNumberField(TEXT("point_count"), PointData->GetPoints().Num());
		}
		Data.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Out->SetArrayField(TEXT("outputs"), Data);
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetPCGNodeDataView::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true)) return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	FString PinFilter;
	FString AttributeName;
	GetStringParam(Params, TEXT("pin"), PinFilter, false);
	GetStringParam(Params, TEXT("attribute_name"), AttributeName, false);
	int32 StartIndex = 0;
	int32 EndIndex = -1;
	GetIntParam(Params, TEXT("start_index"), StartIndex, false);
	GetIntParam(Params, TEXT("end_index"), EndIndex, false);
	AActor* Actor = FindActorByLabel(GetEditorWorld(), ActorName);
	if (!Actor) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	UPCGComponent* Component = FindPCGComponent(Actor);
	if (!Component) return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no PCGComponent"), *ActorName));
	const FPCGDataCollection& Collection = Component->GetGeneratedGraphOutput();
	TArray<TSharedPtr<FJsonValue>> Data;
	for (const FPCGTaggedData& Entry : Collection.GetAllInputs())
	{
		if (!PinFilter.IsEmpty() && Entry.Pin.ToString() != PinFilter) continue;
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("pin"), Entry.Pin.ToString());
		Obj->SetStringField(TEXT("class"), Entry.Data ? Entry.Data->GetClass()->GetName() : TEXT("None"));
		TArray<TSharedPtr<FJsonValue>> Tags;
		for (const FString& Tag : Entry.Tags) Tags.Add(MakeShared<FJsonValueString>(Tag));
		Obj->SetArrayField(TEXT("tags"), Tags);
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(Entry.Data))
		{
			const TArray<FPCGPoint>& Points = PointData->GetPoints();
			const int32 ClampedStart = FMath::Clamp(StartIndex, 0, Points.Num());
			const int32 ClampedEnd = EndIndex < 0 ? Points.Num() : FMath::Clamp(EndIndex, ClampedStart, Points.Num());
			Obj->SetNumberField(TEXT("point_count"), Points.Num());
			TArray<TSharedPtr<FJsonValue>> PointRows;
			for (int32 Index = ClampedStart; Index < ClampedEnd; ++Index)
			{
				const FPCGPoint& Point = Points[Index];
				TSharedPtr<FJsonObject> PointObj = MakeShared<FJsonObject>();
				PointObj->SetNumberField(TEXT("index"), Index);
				PointObj->SetObjectField(TEXT("position"), IECACommand::VectorToJson(Point.Transform.GetLocation()));
				PointObj->SetNumberField(TEXT("density"), Point.Density);
				PointRows.Add(MakeShared<FJsonValueObject>(PointObj));
			}
			Obj->SetArrayField(TEXT("points"), PointRows);
			if (!AttributeName.IsEmpty()) Obj->SetStringField(TEXT("attribute_name"), AttributeName);
		}
		Data.Add(MakeShared<FJsonValueObject>(Obj));
	}
	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("actor_name"), Actor->GetActorNameOrLabel());
	Out->SetArrayField(TEXT("data"), Data);
	Out->SetNumberField(TEXT("data_count"), Data.Num());
	return FECACommandResult::Success(Out);
}

// END CHUNK 4