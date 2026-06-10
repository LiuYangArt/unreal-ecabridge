// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAAdvancedSystemsCommands.h"
#include "Commands/ECACommand.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"

// PCG
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "PCGEdge.h"

#if WITH_ECA_CONTROL_RIG
// ControlRig - in UE5.7 UControlRigBlueprint lives in ControlRigBlueprintLegacy.h
#include "ControlRigBlueprintLegacy.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Rigs/RigHierarchyElements.h"
#include "Rigs/RigHierarchyDefines.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "TransformNoScale.h"
#endif // WITH_ECA_CONTROL_RIG

#if WITH_ECA_GAMEPLAY_ABILITIES
// Gameplay Ability System
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#endif // WITH_ECA_GAMEPLAY_ABILITIES

// Register commands
REGISTER_ECA_COMMAND(FECACommand_DumpPCGGraph)
#if WITH_ECA_CONTROL_RIG
REGISTER_ECA_COMMAND(FECACommand_DumpControlRig)
REGISTER_ECA_COMMAND(FECACommand_AddControlRigNull)
REGISTER_ECA_COMMAND(FECACommand_AddControlRigControl)
REGISTER_ECA_COMMAND(FECACommand_SetControlRigElementTransform)
REGISTER_ECA_COMMAND(FECACommand_RenameControlRigElement)
REGISTER_ECA_COMMAND(FECACommand_RemoveControlRigElement)
#endif
#if WITH_ECA_GAMEPLAY_ABILITIES
REGISTER_ECA_COMMAND(FECACommand_DumpGameplayAbility)
#endif

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

namespace ECAAdvancedSystemsHelpers
{
	static FString SafeGetClassName(const UObject* Object)
	{
		if (!Object)
		{
			return TEXT("None");
		}
		const UClass* Class = Object->GetClass();
		return Class ? Class->GetName() : TEXT("None");
	}

	static FString TagContainerToString(const FGameplayTagContainer& Container)
	{
		return Container.ToStringSimple();
	}

	static TArray<TSharedPtr<FJsonValue>> TagContainerToJsonArray(const FGameplayTagContainer& Container)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		TArray<FGameplayTag> Tags;
		Container.GetGameplayTagArray(Tags);
		for (const FGameplayTag& Tag : Tags)
		{
			Out.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		return Out;
	}
}

//==============================================================================
// FECACommand_DumpPCGGraph
//==============================================================================

FECACommandResult FECACommand_DumpPCGGraph::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::Error(TEXT("graph_path is required"));
	}

	UPCGGraph* Graph = nullptr;
	{
		Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	}

	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at path: %s (make sure the PCG plugin is enabled)"), *GraphPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("graph_path"), GraphPath);
	Result->SetStringField(TEXT("graph_name"), Graph->GetName());

	// Nodes
	TArray<TSharedPtr<FJsonValue>> NodesArray;
	// Edges (all connections collected as we iterate pins)
	TArray<TSharedPtr<FJsonValue>> EdgesArray;

	// Helper: identify a node by its internal name (stable within the package)
	auto GetNodeId = [](const UPCGNode* Node) -> FString
	{
		if (!Node)
		{
			return TEXT("<null>");
		}
		return Node->GetName();
	};

	// Include input / output gateway nodes in addition to regular nodes
	TArray<const UPCGNode*> AllNodes;
	if (const UPCGNode* InNode = Graph->GetInputNode())
	{
		AllNodes.Add(InNode);
	}
	if (const UPCGNode* OutNode = Graph->GetOutputNode())
	{
		AllNodes.Add(OutNode);
	}
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		if (Node)
		{
			AllNodes.AddUnique(Node);
		}
	}

	for (const UPCGNode* Node : AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("node_id"), GetNodeId(Node));
		NodeObj->SetStringField(TEXT("node_title"), Node->GetAuthoredTitleName().ToString());

		const UPCGSettings* Settings = Node->GetSettings();
		NodeObj->SetStringField(TEXT("settings_class"), Settings ? Settings->GetClass()->GetName() : TEXT("None"));

#if WITH_EDITOR
		int32 PosX = 0;
		int32 PosY = 0;
		// GetNodePosition is editor-only
		Node->GetNodePosition(PosX, PosY);
		NodeObj->SetNumberField(TEXT("position_x"), PosX);
		NodeObj->SetNumberField(TEXT("position_y"), PosY);
#endif

		// Input pins
		TArray<TSharedPtr<FJsonValue>> InputPinsArray;
		for (const UPCGPin* Pin : Node->GetInputPins())
		{
			if (!Pin)
			{
				continue;
			}
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("label"), Pin->Properties.Label.ToString());
			PinObj->SetNumberField(TEXT("edge_count"), Pin->EdgeCount());
			InputPinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
		NodeObj->SetArrayField(TEXT("input_pins"), InputPinsArray);

		// Output pins (and harvest edges here since each edge stores Input/Output pin pointers)
		TArray<TSharedPtr<FJsonValue>> OutputPinsArray;
		for (const UPCGPin* Pin : Node->GetOutputPins())
		{
			if (!Pin)
			{
				continue;
			}
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("label"), Pin->Properties.Label.ToString());
			PinObj->SetNumberField(TEXT("edge_count"), Pin->EdgeCount());
			OutputPinsArray.Add(MakeShared<FJsonValueObject>(PinObj));

			// Walk each edge starting from this (output) pin.  In PCG, UPCGEdge->InputPin is the
			// upstream side and UPCGEdge->OutputPin is the downstream side.
			for (const UPCGEdge* Edge : Pin->Edges)
			{
				if (!Edge || !Edge->InputPin || !Edge->OutputPin)
				{
					continue;
				}
				// Only record edges where THIS pin is the upstream side, to avoid duplicates
				if (Edge->InputPin != Pin)
				{
					continue;
				}

				TSharedPtr<FJsonObject> EdgeObj = MakeShared<FJsonObject>();
				EdgeObj->SetStringField(TEXT("from_node_id"), GetNodeId(Edge->InputPin->Node));
				EdgeObj->SetStringField(TEXT("from_pin"), Edge->InputPin->Properties.Label.ToString());
				EdgeObj->SetStringField(TEXT("to_node_id"), GetNodeId(Edge->OutputPin->Node));
				EdgeObj->SetStringField(TEXT("to_pin"), Edge->OutputPin->Properties.Label.ToString());
				EdgesArray.Add(MakeShared<FJsonValueObject>(EdgeObj));
			}
		}
		NodeObj->SetArrayField(TEXT("output_pins"), OutputPinsArray);

		NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	Result->SetArrayField(TEXT("nodes"), NodesArray);
	Result->SetNumberField(TEXT("node_count"), NodesArray.Num());
	Result->SetArrayField(TEXT("edges"), EdgesArray);
	Result->SetNumberField(TEXT("edge_count"), EdgesArray.Num());

	// Parameters - FInstancedPropertyBag on the UPCGGraph
	TArray<TSharedPtr<FJsonValue>> ParametersArray;
	if (const FInstancedPropertyBag* UserParams = Graph->GetUserParametersStruct())
	{
		if (const UPropertyBag* BagStruct = UserParams->GetPropertyBagStruct())
		{
			for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
			{
				TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
				ParamObj->SetStringField(TEXT("name"), Desc.Name.ToString());

				FString TypeName;
				if (Desc.CachedProperty)
				{
					TypeName = Desc.CachedProperty->GetCPPType();
				}
				else if (const UEnum* PropTypeEnum = StaticEnum<EPropertyBagPropertyType>())
				{
					TypeName = PropTypeEnum->GetNameStringByValue(static_cast<int64>(Desc.ValueType));
				}
				else
				{
					TypeName = TEXT("Unknown");
				}
				ParamObj->SetStringField(TEXT("type"), TypeName);

				ParametersArray.Add(MakeShared<FJsonValueObject>(ParamObj));
			}
		}
	}
	Result->SetArrayField(TEXT("parameters"), ParametersArray);
	Result->SetNumberField(TEXT("parameter_count"), ParametersArray.Num());

	return FECACommandResult::Success(Result);
}

#if WITH_ECA_CONTROL_RIG

namespace ECAControlRigHelpers
{
	static UControlRigBlueprint* LoadRigBlueprint(const FString& RigPath, FString& OutError)
	{
		UControlRigBlueprint* RigBlueprint = LoadObject<UControlRigBlueprint>(nullptr, *RigPath);
		if (!RigBlueprint)
		{
			UBlueprint* GenericBP = LoadObject<UBlueprint>(nullptr, *RigPath);
			RigBlueprint = GenericBP ? Cast<UControlRigBlueprint>(GenericBP) : nullptr;
		}

		if (!RigBlueprint)
		{
			OutError = FString::Printf(
				TEXT("ControlRig blueprint not found at path: %s (make sure the ControlRig plugin is enabled and the asset is a Control Rig)"),
				*RigPath);
		}
		return RigBlueprint;
	}

	struct FRigEditContext
	{
		UControlRigBlueprint* RigBlueprint = nullptr;
		URigHierarchy* Hierarchy = nullptr;
		URigHierarchyController* Controller = nullptr;
	};

	static bool LoadRigEditContext(const FString& RigPath, bool bRequireController, FRigEditContext& OutContext, FString& OutError)
	{
		OutContext.RigBlueprint = LoadRigBlueprint(RigPath, OutError);
		if (!OutContext.RigBlueprint)
		{
			return false;
		}

		OutContext.Hierarchy = OutContext.RigBlueprint->GetHierarchy();
		OutContext.Controller = OutContext.RigBlueprint->GetHierarchyController();
		if (!OutContext.Hierarchy)
		{
			OutError = TEXT("ControlRig hierarchy is null");
			return false;
		}
		if (bRequireController && !OutContext.Controller)
		{
			OutError = TEXT("ControlRig hierarchy controller is null");
			return false;
		}
		return true;
	}

	static void ModifyRigForEdit(const FRigEditContext& Context)
	{
		static_cast<UBlueprint*>(Context.RigBlueprint)->Modify();
		Context.Hierarchy->Modify();
	}

	static bool ParseElementType(const FString& TypeText, ERigElementType& OutType)
	{
		const FString Lower = TypeText.ToLower();
		if (Lower == TEXT("bone")) { OutType = ERigElementType::Bone; return true; }
		if (Lower == TEXT("control")) { OutType = ERigElementType::Control; return true; }
		if (Lower == TEXT("null")) { OutType = ERigElementType::Null; return true; }
		if (Lower == TEXT("curve")) { OutType = ERigElementType::Curve; return true; }
		if (Lower == TEXT("reference")) { OutType = ERigElementType::Reference; return true; }
		if (Lower == TEXT("socket")) { OutType = ERigElementType::Socket; return true; }
		return false;
	}

	static FString ElementTypeToString(ERigElementType Type)
	{
		const UEnum* TypeEnum = StaticEnum<ERigElementType>();
		return TypeEnum ? TypeEnum->GetNameStringByValue(static_cast<int64>(Type)) : TEXT("Unknown");
	}

	static bool ResolveElementKey(URigHierarchy* Hierarchy, const FString& Name, const FString& TypeText, FRigElementKey& OutKey, FString& OutError)
	{
		if (!Hierarchy)
		{
			OutError = TEXT("ControlRig hierarchy is null");
			return false;
		}
		if (Name.IsEmpty())
		{
			OutKey = FRigElementKey();
			return true;
		}

		if (!TypeText.IsEmpty())
		{
			ERigElementType Type = ERigElementType::None;
			if (!ParseElementType(TypeText, Type))
			{
				OutError = FString::Printf(TEXT("Unsupported ControlRig element type: %s"), *TypeText);
				return false;
			}
			OutKey = FRigElementKey(*Name, Type);
			if (!Hierarchy->Contains(OutKey))
			{
				OutError = FString::Printf(TEXT("ControlRig element not found: %s (%s)"), *Name, *TypeText);
				return false;
			}
			return true;
		}

		TArray<FRigElementKey> Matches;
		for (const FRigElementKey& Key : Hierarchy->GetAllKeys(true, ERigElementType::All))
		{
			if (Key.Name.ToString() == Name)
			{
				Matches.Add(Key);
			}
		}

		if (Matches.Num() == 0)
		{
			OutError = FString::Printf(TEXT("ControlRig element not found: %s"), *Name);
			return false;
		}
		if (Matches.Num() > 1)
		{
			OutError = FString::Printf(TEXT("ControlRig element name is ambiguous: %s; pass element_type"), *Name);
			return false;
		}

		OutKey = Matches[0];
		return true;
	}

	static bool ReadVectorValue(const TSharedPtr<FJsonValue>& Value, FVector& OutVector, FString& OutError)
	{
		if (!Value.IsValid())
		{
			OutError = TEXT("Missing vector value");
			return false;
		}

		if (Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
			if (Arr.Num() < 2)
			{
				OutError = TEXT("Vector array must have at least 2 values");
				return false;
			}
			OutVector.X = Arr[0]->AsNumber();
			OutVector.Y = Arr[1]->AsNumber();
			OutVector.Z = Arr.Num() >= 3 ? Arr[2]->AsNumber() : 0.0;
			return true;
		}

		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			OutError = TEXT("Vector value must be an object or array");
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		if (!Obj->TryGetNumberField(TEXT("x"), X) || !Obj->TryGetNumberField(TEXT("y"), Y))
		{
			OutError = TEXT("Vector object must include x and y");
			return false;
		}
		Obj->TryGetNumberField(TEXT("z"), Z);
		OutVector = FVector(X, Y, Z);
		return true;
	}

	static bool ReadRotatorValue(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator, FString& OutError)
	{
		FVector Vector;
		if (Value.IsValid() && Value->Type == EJson::Array)
		{
			if (!ReadVectorValue(Value, Vector, OutError))
			{
				return false;
			}
			OutRotator = FRotator(Vector.X, Vector.Y, Vector.Z);
			return true;
		}

		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Obj.IsValid())
		{
			OutError = TEXT("Rotator value must be an object or array");
			return false;
		}

		double Pitch = 0.0;
		double Yaw = 0.0;
		double Roll = 0.0;
		if (Obj->TryGetNumberField(TEXT("pitch"), Pitch) || Obj->TryGetNumberField(TEXT("yaw"), Yaw) || Obj->TryGetNumberField(TEXT("roll"), Roll))
		{
			Obj->TryGetNumberField(TEXT("pitch"), Pitch);
			Obj->TryGetNumberField(TEXT("yaw"), Yaw);
			Obj->TryGetNumberField(TEXT("roll"), Roll);
			OutRotator = FRotator(Pitch, Yaw, Roll);
			return true;
		}

		if (!ReadVectorValue(Value, Vector, OutError))
		{
			return false;
		}
		OutRotator = FRotator(Vector.X, Vector.Y, Vector.Z);
		return true;
	}

	static bool ReadTransformObject(const TSharedPtr<FJsonObject>& Obj, const FTransform& DefaultTransform, FTransform& OutTransform, FString& OutError)
	{
		if (!Obj.IsValid())
		{
			OutError = TEXT("Transform must be an object");
			return false;
		}

		FVector Location = DefaultTransform.GetLocation();
		FRotator Rotation = DefaultTransform.Rotator();
		FVector Scale = DefaultTransform.GetScale3D();

		if (TSharedPtr<FJsonValue> LocationValue = Obj->TryGetField(TEXT("location")))
		{
			if (!ReadVectorValue(LocationValue, Location, OutError))
			{
				OutError = TEXT("Invalid transform.location: ") + OutError;
				return false;
			}
		}
		if (TSharedPtr<FJsonValue> TranslationValue = Obj->TryGetField(TEXT("translation")))
		{
			if (!ReadVectorValue(TranslationValue, Location, OutError))
			{
				OutError = TEXT("Invalid transform.translation: ") + OutError;
				return false;
			}
		}
		if (TSharedPtr<FJsonValue> RotationValue = Obj->TryGetField(TEXT("rotation")))
		{
			if (!ReadRotatorValue(RotationValue, Rotation, OutError))
			{
				OutError = TEXT("Invalid transform.rotation: ") + OutError;
				return false;
			}
		}
		if (TSharedPtr<FJsonValue> ScaleValue = Obj->TryGetField(TEXT("scale")))
		{
			if (!ReadVectorValue(ScaleValue, Scale, OutError))
			{
				OutError = TEXT("Invalid transform.scale: ") + OutError;
				return false;
			}
		}

		OutTransform = FTransform(Rotation, Location, Scale);
		return true;
	}

	static bool ReadTransformParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, const FTransform& DefaultTransform, FTransform& OutTransform, bool bRequired, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Params.IsValid() || !Params->TryGetObjectField(FieldName, Obj))
		{
			if (bRequired)
			{
				OutError = FieldName + TEXT(" is required and must be a transform object");
				return false;
			}
			OutTransform = DefaultTransform;
			return true;
		}
		return ReadTransformObject(*Obj, DefaultTransform, OutTransform, OutError);
	}

	static bool ParseControlType(const FString& TypeText, ERigControlType& OutType)
	{
		const FString Lower = TypeText.IsEmpty() ? TEXT("euler_transform") : TypeText.ToLower().Replace(TEXT("-"), TEXT("_"));
		if (Lower == TEXT("bool") || Lower == TEXT("boolean")) { OutType = ERigControlType::Bool; return true; }
		if (Lower == TEXT("float")) { OutType = ERigControlType::Float; return true; }
		if (Lower == TEXT("integer") || Lower == TEXT("int")) { OutType = ERigControlType::Integer; return true; }
		if (Lower == TEXT("vector2d")) { OutType = ERigControlType::Vector2D; return true; }
		if (Lower == TEXT("position") || Lower == TEXT("vector")) { OutType = ERigControlType::Position; return true; }
		if (Lower == TEXT("scale")) { OutType = ERigControlType::Scale; return true; }
		if (Lower == TEXT("rotator") || Lower == TEXT("rotation")) { OutType = ERigControlType::Rotator; return true; }
		if (Lower == TEXT("transform")) { OutType = ERigControlType::Transform; return true; }
		if (Lower == TEXT("transform_no_scale") || Lower == TEXT("transformnoscale")) { OutType = ERigControlType::TransformNoScale; return true; }
		if (Lower == TEXT("euler_transform") || Lower == TEXT("eulertransform")) { OutType = ERigControlType::EulerTransform; return true; }
		if (Lower == TEXT("scale_float") || Lower == TEXT("scalefloat")) { OutType = ERigControlType::ScaleFloat; return true; }
		return false;
	}

	static FRigControlValue DefaultControlValue(ERigControlType Type)
	{
		switch (Type)
		{
		case ERigControlType::Bool:
			return FRigControlValue::Make<bool>(false);
		case ERigControlType::Float:
			return FRigControlValue::Make<float>(0.0f);
		case ERigControlType::ScaleFloat:
			return FRigControlValue::Make<float>(1.0f);
		case ERigControlType::Integer:
			return FRigControlValue::Make<int32>(0);
		case ERigControlType::Vector2D:
			return FRigControlValue::Make(FVector2D::ZeroVector);
		case ERigControlType::Scale:
			return FRigControlValue::Make(FVector::OneVector);
		case ERigControlType::Rotator:
			return FRigControlValue::Make(FRotator::ZeroRotator);
		case ERigControlType::Transform:
			return FRigControlValue::Make(FTransform::Identity);
		case ERigControlType::TransformNoScale:
			return FRigControlValue::Make(FTransformNoScale(FTransform::Identity));
		case ERigControlType::EulerTransform:
			return FRigControlValue::Make(FEulerTransform(FTransform::Identity));
		case ERigControlType::Position:
		default:
			return FRigControlValue::Make(FVector::ZeroVector);
		}
	}

	static bool ReadControlValue(const TSharedPtr<FJsonObject>& Params, ERigControlType Type, FRigControlValue& OutValue, FString& OutError)
	{
		TSharedPtr<FJsonValue> Value = Params.IsValid() ? Params->TryGetField(TEXT("initial_value")) : nullptr;
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			OutValue = DefaultControlValue(Type);
			return true;
		}

		switch (Type)
		{
		case ERigControlType::Bool:
			OutValue = FRigControlValue::Make<bool>(Value->AsBool());
			return true;
		case ERigControlType::Float:
		case ERigControlType::ScaleFloat:
			OutValue = FRigControlValue::Make<float>(static_cast<float>(Value->AsNumber()));
			return true;
		case ERigControlType::Integer:
			OutValue = FRigControlValue::Make<int32>(static_cast<int32>(Value->AsNumber()));
			return true;
		case ERigControlType::Vector2D:
		{
			FVector Vec;
			if (!ReadVectorValue(Value, Vec, OutError)) { return false; }
			OutValue = FRigControlValue::Make(FVector2D(Vec.X, Vec.Y));
			return true;
		}
		case ERigControlType::Position:
		case ERigControlType::Scale:
		{
			FVector Vec;
			if (!ReadVectorValue(Value, Vec, OutError)) { return false; }
			OutValue = FRigControlValue::Make(Vec);
			return true;
		}
		case ERigControlType::Rotator:
		{
			FRotator Rot;
			if (!ReadRotatorValue(Value, Rot, OutError)) { return false; }
			OutValue = FRigControlValue::Make(Rot);
			return true;
		}
		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		case ERigControlType::EulerTransform:
		{
			FTransform Transform;
			if (!ReadTransformObject(Value->AsObject(), FTransform::Identity, Transform, OutError)) { return false; }
			if (Type == ERigControlType::TransformNoScale)
			{
				OutValue = FRigControlValue::Make(FTransformNoScale(Transform));
			}
			else if (Type == ERigControlType::EulerTransform)
			{
				OutValue = FRigControlValue::Make(FEulerTransform(Transform));
			}
			else
			{
				OutValue = FRigControlValue::Make(Transform);
			}
			return true;
		}
		default:
			OutError = TEXT("Unsupported control type for initial_value");
			return false;
		}
	}

	static bool IsTransformElement(ERigElementType Type)
	{
		return Type == ERigElementType::Bone ||
			Type == ERigElementType::Null ||
			Type == ERigElementType::Control ||
			Type == ERigElementType::Reference ||
			Type == ERigElementType::Socket;
	}

	static TSharedPtr<FJsonObject> VectorToJson(const FVector& Vec)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("x"), Vec.X);
		Obj->SetNumberField(TEXT("y"), Vec.Y);
		Obj->SetNumberField(TEXT("z"), Vec.Z);
		return Obj;
	}

	static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rot)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("pitch"), Rot.Pitch);
		Obj->SetNumberField(TEXT("yaw"), Rot.Yaw);
		Obj->SetNumberField(TEXT("roll"), Rot.Roll);
		return Obj;
	}

	static TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetObjectField(TEXT("location"), VectorToJson(Transform.GetLocation()));
		Obj->SetObjectField(TEXT("rotation"), RotatorToJson(Transform.Rotator()));
		Obj->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
		return Obj;
	}

	static TSharedPtr<FJsonObject> ElementToJson(URigHierarchy* Hierarchy, const FRigElementKey& Key)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Key.Name.ToString());
		Obj->SetStringField(TEXT("type"), ElementTypeToString(Key.Type));
		if (Hierarchy && IsTransformElement(Key.Type))
		{
			Obj->SetObjectField(TEXT("local_transform"), TransformToJson(Hierarchy->GetLocalTransform(Key, false)));
			Obj->SetObjectField(TEXT("global_transform"), TransformToJson(Hierarchy->GetGlobalTransform(Key, false)));
		}
		return Obj;
	}

	static void MarkRigModified(UControlRigBlueprint* RigBlueprint)
	{
		if (!RigBlueprint)
		{
			return;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(RigBlueprint);
		RigBlueprint->MarkPackageDirty();
	}
}

//==============================================================================
// FECACommand_DumpControlRig
//==============================================================================

FECACommandResult FECACommand_DumpControlRig::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString RigPath;
	if (!GetStringParam(Params, TEXT("rig_path"), RigPath, true))
	{
		return FECACommandResult::Error(TEXT("rig_path is required"));
	}

	// ControlRig assets are UControlRigBlueprint instances
	UControlRigBlueprint* RigBlueprint = LoadObject<UControlRigBlueprint>(nullptr, *RigPath);
	if (!RigBlueprint)
	{
		// Fall back: try to load as plain UBlueprint and see if parent is a ControlRig class
		UBlueprint* GenericBP = LoadObject<UBlueprint>(nullptr, *RigPath);
		if (GenericBP)
		{
			RigBlueprint = Cast<UControlRigBlueprint>(GenericBP);
		}
	}

	if (!RigBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("ControlRig blueprint not found at path: %s (make sure the ControlRig plugin is enabled and the asset is a Control Rig)"),
			*RigPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("rig_path"), RigPath);
	Result->SetStringField(TEXT("rig_name"), RigBlueprint->GetName());

	// --- Hierarchy elements, grouped by element type ---
	TMap<FString, TArray<TSharedPtr<FJsonValue>>> ElementsByType;

	URigHierarchy* Hierarchy = RigBlueprint->GetHierarchy();
	if (!Hierarchy)
	{
		Result->SetStringField(TEXT("hierarchy_error"), TEXT("ControlRigBlueprint->Hierarchy is null"));
	}
	else
	{
		// Snapshot all keys in one pass; traversing ensures parents come before children
		const TArray<FRigElementKey> AllKeys = Hierarchy->GetAllKeys(true /* bTraverse */, ERigElementType::All);

		for (const FRigElementKey& Key : AllKeys)
		{
			const UEnum* TypeEnum = StaticEnum<ERigElementType>();
			FString TypeName = TypeEnum ? TypeEnum->GetNameStringByValue(static_cast<int64>(Key.Type)) : TEXT("Unknown");

			TSharedPtr<FJsonObject> ElemObj = MakeShared<FJsonObject>();
			ElemObj->SetStringField(TEXT("name"), Key.Name.ToString());
			ElemObj->SetStringField(TEXT("type"), TypeName);

			// Parent
			const FRigElementKey ParentKey = Hierarchy->GetFirstParent(Key);
			if (ParentKey.IsValid())
			{
				TSharedPtr<FJsonObject> ParentObj = MakeShared<FJsonObject>();
				ParentObj->SetStringField(TEXT("name"), ParentKey.Name.ToString());
				const FString ParentTypeName = TypeEnum ? TypeEnum->GetNameStringByValue(static_cast<int64>(ParentKey.Type)) : TEXT("Unknown");
				ParentObj->SetStringField(TEXT("type"), ParentTypeName);
				ElemObj->SetObjectField(TEXT("parent"), ParentObj);
			}

			// Transform only for element types that actually carry one
			const bool bHasTransform =
				Key.Type == ERigElementType::Bone ||
				Key.Type == ERigElementType::Null ||
				Key.Type == ERigElementType::Control ||
				Key.Type == ERigElementType::Reference ||
				Key.Type == ERigElementType::Socket;

			if (bHasTransform)
			{
				const FTransform LocalXform = Hierarchy->GetLocalTransform(Key, false /* bInitial */);
				const FTransform GlobalXform = Hierarchy->GetGlobalTransform(Key, false);

				ElemObj->SetObjectField(TEXT("local_transform"), TransformToJson(LocalXform));
				ElemObj->SetObjectField(TEXT("global_transform"), TransformToJson(GlobalXform));
			}

			ElementsByType.FindOrAdd(TypeName).Add(MakeShared<FJsonValueObject>(ElemObj));
		}
	}

	TSharedPtr<FJsonObject> HierarchyObj = MakeShared<FJsonObject>();
	int32 TotalElements = 0;
	for (const TPair<FString, TArray<TSharedPtr<FJsonValue>>>& Pair : ElementsByType)
	{
		HierarchyObj->SetArrayField(Pair.Key, Pair.Value);
		TotalElements += Pair.Value.Num();
	}
	Result->SetObjectField(TEXT("hierarchy"), HierarchyObj);
	Result->SetNumberField(TEXT("element_count"), TotalElements);

	// --- Graphs summary ---
	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	auto AppendGraph = [&GraphsArray](const UEdGraph* G, const FString& Category)
	{
		if (!G)
		{
			return;
		}
		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), G->GetName());
		GraphObj->SetStringField(TEXT("category"), Category);
		GraphObj->SetNumberField(TEXT("node_count"), G->Nodes.Num());
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	};

	for (const UEdGraph* G : RigBlueprint->UbergraphPages)
	{
		AppendGraph(G, TEXT("Ubergraph"));
	}
	for (const UEdGraph* G : RigBlueprint->FunctionGraphs)
	{
		AppendGraph(G, TEXT("Function"));
	}

	Result->SetArrayField(TEXT("graphs"), GraphsArray);
	Result->SetNumberField(TEXT("graph_count"), GraphsArray.Num());

	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_AddControlRigNull::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAControlRigHelpers;

	FString RigPath;
	FString Name;
	if (!GetStringParam(Params, TEXT("rig_path"), RigPath, true))
	{
		return FECACommandResult::Error(TEXT("rig_path is required"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name, true))
	{
		return FECACommandResult::Error(TEXT("name is required"));
	}

	FString Error;
	FRigEditContext Context;
	if (!LoadRigEditContext(RigPath, true, Context, Error))
	{
		return FECACommandResult::Error(Error);
	}
	URigHierarchy* Hierarchy = Context.Hierarchy;
	URigHierarchyController* Controller = Context.Controller;

	FString ParentName;
	FString ParentType;
	GetStringParam(Params, TEXT("parent_name"), ParentName, false);
	GetStringParam(Params, TEXT("parent_type"), ParentType, false);

	FRigElementKey ParentKey;
	if (!ResolveElementKey(Hierarchy, ParentName, ParentType, ParentKey, Error))
	{
		return FECACommandResult::Error(Error);
	}

	FTransform Transform;
	if (!ReadTransformParam(Params, TEXT("transform"), FTransform::Identity, Transform, false, Error))
	{
		return FECACommandResult::Error(Error);
	}

	FString TransformSpace;
	GetStringParam(Params, TEXT("transform_space"), TransformSpace, false);
	const bool bTransformInGlobal = !TransformSpace.Equals(TEXT("local"), ESearchCase::IgnoreCase);

	ModifyRigForEdit(Context);
	const FRigElementKey NewKey = Controller->AddNull(*Name, ParentKey, Transform, bTransformInGlobal, true, false);
	if (!NewKey.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to add ControlRig null: %s"), *Name));
	}
	MarkRigModified(Context.RigBlueprint);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("rig_path"), RigPath);
	Result->SetObjectField(TEXT("element"), ElementToJson(Hierarchy, NewKey));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_AddControlRigControl::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAControlRigHelpers;

	FString RigPath;
	FString Name;
	if (!GetStringParam(Params, TEXT("rig_path"), RigPath, true))
	{
		return FECACommandResult::Error(TEXT("rig_path is required"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name, true))
	{
		return FECACommandResult::Error(TEXT("name is required"));
	}

	FString Error;
	FRigEditContext Context;
	if (!LoadRigEditContext(RigPath, true, Context, Error))
	{
		return FECACommandResult::Error(Error);
	}
	URigHierarchy* Hierarchy = Context.Hierarchy;
	URigHierarchyController* Controller = Context.Controller;

	FString ParentName;
	FString ParentType;
	GetStringParam(Params, TEXT("parent_name"), ParentName, false);
	GetStringParam(Params, TEXT("parent_type"), ParentType, false);

	FRigElementKey ParentKey;
	if (!ResolveElementKey(Hierarchy, ParentName, ParentType, ParentKey, Error))
	{
		return FECACommandResult::Error(Error);
	}

	FString ControlTypeText;
	GetStringParam(Params, TEXT("control_type"), ControlTypeText, false);
	ERigControlType ControlType = ERigControlType::EulerTransform;
	if (!ParseControlType(ControlTypeText, ControlType))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Unsupported ControlRig control_type: %s"), *ControlTypeText));
	}

	FRigControlValue InitialValue;
	if (!ReadControlValue(Params, ControlType, InitialValue, Error))
	{
		return FECACommandResult::Error(TEXT("Invalid initial_value: ") + Error);
	}

	FTransform OffsetTransform;
	if (!ReadTransformParam(Params, TEXT("transform"), FTransform::Identity, OffsetTransform, false, Error))
	{
		return FECACommandResult::Error(Error);
	}

	FRigControlSettings Settings;
	Settings.ControlType = ControlType;
	Settings.AnimationType = ERigControlAnimationType::AnimationControl;
	Settings.DisplayName = *Name;

	FString DisplayName;
	if (GetStringParam(Params, TEXT("display_name"), DisplayName, false) && !DisplayName.IsEmpty())
	{
		Settings.DisplayName = *DisplayName;
	}
	FString ShapeName;
	if (GetStringParam(Params, TEXT("shape_name"), ShapeName, false) && !ShapeName.IsEmpty())
	{
		Settings.ShapeName = *ShapeName;
	}
	bool bShapeVisible = true;
	GetBoolParam(Params, TEXT("shape_visible"), bShapeVisible, false);
	Settings.bShapeVisible = bShapeVisible;

	ModifyRigForEdit(Context);
	const FRigElementKey NewKey = Controller->AddControl(*Name, ParentKey, Settings, InitialValue, OffsetTransform, FTransform::Identity, true, false);
	if (!NewKey.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to add ControlRig control: %s"), *Name));
	}
	MarkRigModified(Context.RigBlueprint);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("rig_path"), RigPath);
	Result->SetObjectField(TEXT("element"), ElementToJson(Hierarchy, NewKey));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_SetControlRigElementTransform::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAControlRigHelpers;

	FString RigPath;
	FString ElementName;
	if (!GetStringParam(Params, TEXT("rig_path"), RigPath, true))
	{
		return FECACommandResult::Error(TEXT("rig_path is required"));
	}
	if (!GetStringParam(Params, TEXT("element_name"), ElementName, true))
	{
		return FECACommandResult::Error(TEXT("element_name is required"));
	}

	FString Error;
	FRigEditContext Context;
	if (!LoadRigEditContext(RigPath, false, Context, Error))
	{
		return FECACommandResult::Error(Error);
	}
	URigHierarchy* Hierarchy = Context.Hierarchy;

	FString ElementType;
	GetStringParam(Params, TEXT("element_type"), ElementType, false);
	FRigElementKey Key;
	if (!ResolveElementKey(Hierarchy, ElementName, ElementType, Key, Error))
	{
		return FECACommandResult::Error(Error);
	}
	if (!IsTransformElement(Key.Type))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("ControlRig element type does not support transforms: %s"), *ElementTypeToString(Key.Type)));
	}

	FTransform Transform;
	if (!ReadTransformParam(Params, TEXT("transform"), FTransform::Identity, Transform, true, Error))
	{
		return FECACommandResult::Error(Error);
	}

	FString TransformSpace;
	GetStringParam(Params, TEXT("transform_space"), TransformSpace, false);
	bool bAffectChildren = true;
	bool bSetInitial = false;
	GetBoolParam(Params, TEXT("affect_children"), bAffectChildren, false);
	GetBoolParam(Params, TEXT("set_initial"), bSetInitial, false);

	ModifyRigForEdit(Context);
	if (TransformSpace.Equals(TEXT("local"), ESearchCase::IgnoreCase))
	{
		Hierarchy->SetLocalTransform(Key, Transform, bSetInitial, bAffectChildren, true, false);
	}
	else
	{
		Hierarchy->SetGlobalTransform(Key, Transform, bSetInitial, bAffectChildren, true, false);
	}
	MarkRigModified(Context.RigBlueprint);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("rig_path"), RigPath);
	Result->SetObjectField(TEXT("element"), ElementToJson(Hierarchy, Key));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_RenameControlRigElement::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAControlRigHelpers;

	FString RigPath;
	FString ElementName;
	FString NewName;
	if (!GetStringParam(Params, TEXT("rig_path"), RigPath, true))
	{
		return FECACommandResult::Error(TEXT("rig_path is required"));
	}
	if (!GetStringParam(Params, TEXT("element_name"), ElementName, true))
	{
		return FECACommandResult::Error(TEXT("element_name is required"));
	}
	if (!GetStringParam(Params, TEXT("new_name"), NewName, true))
	{
		return FECACommandResult::Error(TEXT("new_name is required"));
	}

	FString Error;
	FRigEditContext Context;
	if (!LoadRigEditContext(RigPath, true, Context, Error))
	{
		return FECACommandResult::Error(Error);
	}
	URigHierarchy* Hierarchy = Context.Hierarchy;
	URigHierarchyController* Controller = Context.Controller;

	FString ElementType;
	GetStringParam(Params, TEXT("element_type"), ElementType, false);
	FRigElementKey Key;
	if (!ResolveElementKey(Hierarchy, ElementName, ElementType, Key, Error))
	{
		return FECACommandResult::Error(Error);
	}

	ModifyRigForEdit(Context);
	const FRigElementKey NewKey = Controller->RenameElement(Key, *NewName, true, false);
	if (!NewKey.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to rename ControlRig element: %s"), *ElementName));
	}
	MarkRigModified(Context.RigBlueprint);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("rig_path"), RigPath);
	Result->SetObjectField(TEXT("element"), ElementToJson(Hierarchy, NewKey));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_RemoveControlRigElement::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAControlRigHelpers;

	FString RigPath;
	FString ElementName;
	if (!GetStringParam(Params, TEXT("rig_path"), RigPath, true))
	{
		return FECACommandResult::Error(TEXT("rig_path is required"));
	}
	if (!GetStringParam(Params, TEXT("element_name"), ElementName, true))
	{
		return FECACommandResult::Error(TEXT("element_name is required"));
	}

	FString Error;
	FRigEditContext Context;
	if (!LoadRigEditContext(RigPath, true, Context, Error))
	{
		return FECACommandResult::Error(Error);
	}
	URigHierarchy* Hierarchy = Context.Hierarchy;
	URigHierarchyController* Controller = Context.Controller;

	FString ElementType;
	GetStringParam(Params, TEXT("element_type"), ElementType, false);
	FRigElementKey Key;
	if (!ResolveElementKey(Hierarchy, ElementName, ElementType, Key, Error))
	{
		return FECACommandResult::Error(Error);
	}

	ModifyRigForEdit(Context);
	const bool bRemoved = Controller->RemoveElement(Key, true, false);
	if (!bRemoved)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to remove ControlRig element: %s"), *ElementName));
	}
	MarkRigModified(Context.RigBlueprint);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("rig_path"), RigPath);
	Result->SetBoolField(TEXT("removed"), true);
	Result->SetStringField(TEXT("element_name"), ElementName);
	Result->SetStringField(TEXT("element_type"), ElementTypeToString(Key.Type));
	return FECACommandResult::Success(Result);
}

#endif // WITH_ECA_CONTROL_RIG

#if WITH_ECA_GAMEPLAY_ABILITIES

//==============================================================================
// FECACommand_DumpGameplayAbility
//==============================================================================

FECACommandResult FECACommand_DumpGameplayAbility::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath, true))
	{
		return FECACommandResult::Error(TEXT("asset_path is required"));
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found at path: %s"), *AssetPath));
	}

	UClass* ParentClass = Blueprint->ParentClass;
	UClass* GeneratedClass = Blueprint->GeneratedClass;

	if (!GeneratedClass)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Blueprint at %s has no GeneratedClass (not compiled?)"), *AssetPath));
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Blueprint at %s has no CDO"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	Result->SetStringField(TEXT("parent_class"), ParentClass ? ParentClass->GetName() : TEXT("None"));

	// Guard all subsystem access behind StaticClass checks so missing plugins don't crash us
	UClass* GameplayAbilityClass = UGameplayAbility::StaticClass();
	UClass* GameplayEffectClass = UGameplayEffect::StaticClass();
	UClass* AttributeSetClass = UAttributeSet::StaticClass();

	if (GameplayAbilityClass && GeneratedClass->IsChildOf(GameplayAbilityClass))
	{
		// --- GameplayAbility ---
		const UGameplayAbility* Ability = Cast<UGameplayAbility>(CDO);
		if (!Ability)
		{
			return FECACommandResult::Error(TEXT("CDO is not a UGameplayAbility"));
		}

		Result->SetStringField(TEXT("asset_type"), TEXT("GameplayAbility"));

		// Tags - use the publicly visible getters where available, otherwise reflection.
		// AbilityTags is protected on UGameplayAbility in some versions, but directly accessible on
		// the CDO when we read via the FProperty. We fall back to reading by property name.
		auto ReadTagContainerProp = [&](const TCHAR* PropertyName) -> FGameplayTagContainer
		{
			FGameplayTagContainer Empty;
			FProperty* Prop = GeneratedClass->FindPropertyByName(FName(PropertyName));
			if (!Prop)
			{
				return Empty;
			}
			FStructProperty* StructProp = CastField<FStructProperty>(Prop);
			if (!StructProp || StructProp->Struct != TBaseStructure<FGameplayTagContainer>::Get())
			{
				return Empty;
			}
			const FGameplayTagContainer* Ptr = StructProp->ContainerPtrToValuePtr<FGameplayTagContainer>(CDO);
			return Ptr ? *Ptr : Empty;
		};

		Result->SetArrayField(TEXT("ability_tags"),
			ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ReadTagContainerProp(TEXT("AbilityTags"))));

		Result->SetArrayField(TEXT("cancel_abilities_with_tag"),
			ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ReadTagContainerProp(TEXT("CancelAbilitiesWithTag"))));

		Result->SetArrayField(TEXT("block_abilities_with_tag"),
			ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ReadTagContainerProp(TEXT("BlockAbilitiesWithTag"))));

		Result->SetArrayField(TEXT("activation_owned_tags"),
			ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ReadTagContainerProp(TEXT("ActivationOwnedTags"))));

		Result->SetArrayField(TEXT("activation_required_tags"),
			ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ReadTagContainerProp(TEXT("ActivationRequiredTags"))));

		Result->SetArrayField(TEXT("activation_blocked_tags"),
			ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ReadTagContainerProp(TEXT("ActivationBlockedTags"))));

		// Cost / cooldown effect classes - read via reflection to avoid touching protected members
		auto ReadSubclassOfName = [&](const TCHAR* PropertyName) -> FString
		{
			FProperty* Prop = GeneratedClass->FindPropertyByName(FName(PropertyName));
			FClassProperty* ClassProp = CastField<FClassProperty>(Prop);
			if (!ClassProp)
			{
				return TEXT("None");
			}
			const UObject* const* RawPtr = ClassProp->ContainerPtrToValuePtr<const UObject*>(CDO);
			if (!RawPtr || !*RawPtr)
			{
				return TEXT("None");
			}
			// For TSubclassOf the storage is a UClass*
			const UClass* StoredClass = Cast<UClass>(*RawPtr);
			return StoredClass ? StoredClass->GetPathName() : TEXT("None");
		};

		Result->SetStringField(TEXT("cost_gameplay_effect_class"), ReadSubclassOfName(TEXT("CostGameplayEffectClass")));
		Result->SetStringField(TEXT("cooldown_gameplay_effect_class"), ReadSubclassOfName(TEXT("CooldownGameplayEffectClass")));
	}
	else if (GameplayEffectClass && GeneratedClass->IsChildOf(GameplayEffectClass))
	{
		// --- GameplayEffect ---
		const UGameplayEffect* Effect = Cast<UGameplayEffect>(CDO);
		if (!Effect)
		{
			return FECACommandResult::Error(TEXT("CDO is not a UGameplayEffect"));
		}

		Result->SetStringField(TEXT("asset_type"), TEXT("GameplayEffect"));

		// DurationPolicy via reflection to avoid accessing protected members
		{
			FProperty* Prop = GeneratedClass->FindPropertyByName(FName(TEXT("DurationPolicy")));
			FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop);
			if (EnumProp)
			{
				const void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(CDO);
				int64 EnumValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
				UEnum* Enum = EnumProp->GetEnum();
				Result->SetStringField(TEXT("duration_policy"), Enum ? Enum->GetNameStringByValue(EnumValue) : TEXT("Unknown"));
			}
			else if (Prop)
			{
				// Older ByteProperty-enum case
				if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
				{
					uint8 Value = 0;
					ByteProp->CopyCompleteValue(&Value, ByteProp->ContainerPtrToValuePtr<void>(CDO));
					if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
					{
						Result->SetStringField(TEXT("duration_policy"), Enum->GetNameStringByValue(Value));
					}
					else
					{
						Result->SetNumberField(TEXT("duration_policy"), Value);
					}
				}
			}
		}

		// Modifiers
		TArray<TSharedPtr<FJsonValue>> ModsArray;
		{
			FProperty* Prop = GeneratedClass->FindPropertyByName(FName(TEXT("Modifiers")));
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
			{
				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CDO));
				FStructProperty* ElemProp = CastField<FStructProperty>(ArrayProp->Inner);
				for (int32 i = 0; i < Helper.Num() && ElemProp; ++i)
				{
					const void* ModPtr = Helper.GetRawPtr(i);

					TSharedPtr<FJsonObject> ModObj = MakeShared<FJsonObject>();

					// Attribute (FGameplayAttribute)
					if (FProperty* AttrProp = ElemProp->Struct->FindPropertyByName(FName(TEXT("Attribute"))))
					{
						if (FStructProperty* AttrStruct = CastField<FStructProperty>(AttrProp))
						{
							const FGameplayAttribute* AttrVal = AttrStruct->ContainerPtrToValuePtr<FGameplayAttribute>(ModPtr);
							if (AttrVal)
							{
								ModObj->SetStringField(TEXT("attribute"), AttrVal->GetName());
							}
						}
					}

					// ModifierOp (TEnumAsByte<EGameplayModOp::Type>) - stored as FByteProperty
					if (FProperty* OpProp = ElemProp->Struct->FindPropertyByName(FName(TEXT("ModifierOp"))))
					{
						if (FByteProperty* ByteProp = CastField<FByteProperty>(OpProp))
						{
							uint8 Value = 0;
							ByteProp->CopyCompleteValue(&Value, ByteProp->ContainerPtrToValuePtr<void>(ModPtr));
							if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
							{
								ModObj->SetStringField(TEXT("modifier_op"), Enum->GetNameStringByValue(Value));
							}
							else
							{
								ModObj->SetNumberField(TEXT("modifier_op"), Value);
							}
						}
					}

					ModsArray.Add(MakeShared<FJsonValueObject>(ModObj));
				}
			}
		}
		Result->SetArrayField(TEXT("modifiers"), ModsArray);
		Result->SetNumberField(TEXT("modifier_count"), ModsArray.Num());

		// Granted tags (non-deprecated path): use GetGrantedTags() on the CDO
		Result->SetArrayField(TEXT("granted_tags"),
			ECAAdvancedSystemsHelpers::TagContainerToJsonArray(Effect->GetGrantedTags()));

		// Deprecated but still widely used: InheritableOwnedTagsContainer (FInheritedTagContainer)
		{
			FProperty* Prop = GeneratedClass->FindPropertyByName(FName(TEXT("InheritableOwnedTagsContainer")));
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				const FInheritedTagContainer* ContainerPtr =
					StructProp->ContainerPtrToValuePtr<FInheritedTagContainer>(CDO);
				if (ContainerPtr)
				{
					TSharedPtr<FJsonObject> InheritableObj = MakeShared<FJsonObject>();
					InheritableObj->SetArrayField(TEXT("combined"),
						ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ContainerPtr->CombinedTags));
					InheritableObj->SetArrayField(TEXT("added"),
						ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ContainerPtr->Added));
					InheritableObj->SetArrayField(TEXT("removed"),
						ECAAdvancedSystemsHelpers::TagContainerToJsonArray(ContainerPtr->Removed));
					Result->SetObjectField(TEXT("inheritable_owned_tags_container"), InheritableObj);
				}
			}
		}
	}
	else if (AttributeSetClass && GeneratedClass->IsChildOf(AttributeSetClass))
	{
		// --- AttributeSet ---
		Result->SetStringField(TEXT("asset_type"), TEXT("AttributeSet"));

		TArray<TSharedPtr<FJsonValue>> AttrsArray;
		for (TFieldIterator<FProperty> PropIt(GeneratedClass); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			if (!Prop)
			{
				continue;
			}

			FStructProperty* StructProp = CastField<FStructProperty>(Prop);
			if (!StructProp || !StructProp->Struct)
			{
				continue;
			}

			// Match FGameplayAttributeData or any subclass struct that stores FGameplayAttributeData-compatible data
			UScriptStruct* AttrDataStruct = FGameplayAttributeData::StaticStruct();
			if (!StructProp->Struct->IsChildOf(AttrDataStruct))
			{
				continue;
			}

			TSharedPtr<FJsonObject> AttrObj = MakeShared<FJsonObject>();
			AttrObj->SetStringField(TEXT("name"), Prop->GetName());
			AttrObj->SetStringField(TEXT("struct"), StructProp->Struct->GetName());

			const FGameplayAttributeData* DataPtr = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(CDO);
			if (DataPtr)
			{
				AttrObj->SetNumberField(TEXT("base_value"), DataPtr->GetBaseValue());
				AttrObj->SetNumberField(TEXT("current_value"), DataPtr->GetCurrentValue());
			}

			AttrsArray.Add(MakeShared<FJsonValueObject>(AttrObj));
		}

		Result->SetArrayField(TEXT("attributes"), AttrsArray);
		Result->SetNumberField(TEXT("attribute_count"), AttrsArray.Num());
	}
	else
	{
		Result->SetStringField(TEXT("asset_type"), TEXT("Unknown"));
		return FECACommandResult::Error(FString::Printf(
			TEXT("Blueprint at %s does not derive from UGameplayAbility, UGameplayEffect, or UAttributeSet (parent: %s)"),
			*AssetPath,
			*(ParentClass ? ParentClass->GetName() : FString(TEXT("None")))));
	}

	return FECACommandResult::Success(Result);
}

#endif // WITH_ECA_GAMEPLAY_ABILITIES
