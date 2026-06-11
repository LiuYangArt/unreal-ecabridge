// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPcgAuthoringCommands.h"
#include "Commands/ECACommand.h"
#include "BlueprintLayout/LayeredGraphLayout.h"

#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGInputOutputSettings.h"

#include "Engine/StaticMesh.h"
#include "StructUtils/PropertyBag.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_CreatePCGGraph)
REGISTER_ECA_COMMAND(FECACommand_AddPCGNode)
REGISTER_ECA_COMMAND(FECACommand_ConnectPCGNodes)
REGISTER_ECA_COMMAND(FECACommand_AutoLayoutPCGGraph)
REGISTER_ECA_COMMAND(FECACommand_SetPCGNodeProperty)
REGISTER_ECA_COMMAND(FECACommand_AddPCGGraphParameter)
REGISTER_ECA_COMMAND(FECACommand_SetPCGGraphParameterTooltip)
REGISTER_ECA_COMMAND(FECACommand_RenamePCGGraphParameter)
REGISTER_ECA_COMMAND(FECACommand_RemovePCGGraphParameter)
REGISTER_ECA_COMMAND(FECACommand_SetPCGGraphParameterDefault)

namespace ECAPcgHelpers
{
	static const FName TooltipMetaKey(TEXT("ToolTip"));

	// Find a settings subclass by short name (with or without leading 'U', case-insensitive).
	static UClass* FindPCGSettingsClass(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}
		const UClass* BaseClass = UPCGSettings::StaticClass();
		FString Stripped = Name;
		if (Stripped.StartsWith(TEXT("U")) && Stripped.Len() > 1 && FChar::IsUpper(Stripped[1]))
		{
			Stripped = Stripped.Mid(1);
		}
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class || !Class->IsChildOf(BaseClass)) continue;
			if (Class->HasAnyClassFlags(CLASS_Abstract)) continue;
			const FString ClassName = Class->GetName();
			if (ClassName.Equals(Name, ESearchCase::IgnoreCase) ||
				ClassName.Equals(Stripped, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}
		return nullptr;
	}

	static UPCGNode* FindNodeById(UPCGGraph* Graph, const FString& NodeId)
	{
		if (!Graph) return nullptr;
		if (UPCGNode* In = Graph->GetInputNode())
		{
			if (In->GetName() == NodeId) return In;
		}
		if (UPCGNode* Out = Graph->GetOutputNode())
		{
			if (Out->GetName() == NodeId) return Out;
		}
		for (UPCGNode* Node : Graph->GetNodes())
		{
			if (Node && Node->GetName() == NodeId) return Node;
		}
		return nullptr;
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
		const FString PackageName = Package->GetName();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
		if (!bSaved)
		{
			OutError = FString::Printf(TEXT("UPackage::SavePackage failed for '%s'"), *PackageFileName);
			return false;
		}
		return true;
	}
	static FString PropertyBagResultToString(EPropertyBagResult Result)
	{
		if (const UEnum* Enum = StaticEnum<EPropertyBagResult>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Result));
		}
		return FString::FromInt(static_cast<int32>(Result));
	}

	static FString PropertyBagAlterationResultToString(EPropertyBagAlterationResult Result)
	{
		if (const UEnum* Enum = StaticEnum<EPropertyBagAlterationResult>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Result));
		}
		return FString::FromInt(static_cast<int32>(Result));
	}

	static bool IsSuccess(EPropertyBagResult Result)
	{
		return Result == EPropertyBagResult::Success;
	}

	static bool IsSuccess(EPropertyBagAlterationResult Result)
	{
		return Result == EPropertyBagAlterationResult::Success;
	}

	static UClass* ResolveObjectClass(const FString& ClassName)
	{
		if (ClassName.IsEmpty())
		{
			return UObject::StaticClass();
		}

		if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *ClassName))
		{
			return LoadedClass;
		}

		FString Stripped = ClassName;
		if (Stripped.StartsWith(TEXT("U")) && Stripped.Len() > 1 && FChar::IsUpper(Stripped[1]))
		{
			Stripped = Stripped.Mid(1);
		}

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class)
			{
				continue;
			}
			if (Class->GetName().Equals(ClassName, ESearchCase::IgnoreCase) ||
				Class->GetName().Equals(Stripped, ESearchCase::IgnoreCase) ||
				Class->GetPathName().Equals(ClassName, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}

		return nullptr;
	}

	static bool ResolvePropertyBagType(const FString& TypeName, const FString& ObjectClassName, EPropertyBagPropertyType& OutType, const UObject*& OutTypeObject, FString& OutError)
	{
		const FString Type = TypeName.ToLower().Replace(TEXT("_"), TEXT(""));
		OutTypeObject = nullptr;

		if (Type == TEXT("bool") || Type == TEXT("boolean")) OutType = EPropertyBagPropertyType::Bool;
		else if (Type == TEXT("byte")) OutType = EPropertyBagPropertyType::Byte;
		else if (Type == TEXT("int") || Type == TEXT("int32")) OutType = EPropertyBagPropertyType::Int32;
		else if (Type == TEXT("int64")) OutType = EPropertyBagPropertyType::Int64;
		else if (Type == TEXT("uint32")) OutType = EPropertyBagPropertyType::UInt32;
		else if (Type == TEXT("uint64")) OutType = EPropertyBagPropertyType::UInt64;
		else if (Type == TEXT("float")) OutType = EPropertyBagPropertyType::Float;
		else if (Type == TEXT("double") || Type == TEXT("number")) OutType = EPropertyBagPropertyType::Double;
		else if (Type == TEXT("name")) OutType = EPropertyBagPropertyType::Name;
		else if (Type == TEXT("string")) OutType = EPropertyBagPropertyType::String;
		else if (Type == TEXT("text")) OutType = EPropertyBagPropertyType::Text;
		else if (Type == TEXT("vector") || Type == TEXT("fvector"))
		{
			OutType = EPropertyBagPropertyType::Struct;
			OutTypeObject = TBaseStructure<FVector>::Get();
		}
		else if (Type == TEXT("rotator") || Type == TEXT("frotator"))
		{
			OutType = EPropertyBagPropertyType::Struct;
			OutTypeObject = TBaseStructure<FRotator>::Get();
		}
		else if (Type == TEXT("transform") || Type == TEXT("ftransform"))
		{
			OutType = EPropertyBagPropertyType::Struct;
			OutTypeObject = TBaseStructure<FTransform>::Get();
		}
		else if (Type == TEXT("staticmesh"))
		{
			OutType = EPropertyBagPropertyType::Object;
			OutTypeObject = UStaticMesh::StaticClass();
		}
		else if (Type == TEXT("object"))
		{
			UClass* Class = ResolveObjectClass(ObjectClassName);
			if (!Class)
			{
				OutError = FString::Printf(TEXT("object_class '%s' could not be resolved"), *ObjectClassName);
				return false;
			}
			OutType = EPropertyBagPropertyType::Object;
			OutTypeObject = Class;
		}
		else if (Type == TEXT("softobject"))
		{
			UClass* Class = ResolveObjectClass(ObjectClassName);
			if (!Class)
			{
				OutError = FString::Printf(TEXT("object_class '%s' could not be resolved"), *ObjectClassName);
				return false;
			}
			OutType = EPropertyBagPropertyType::SoftObject;
			OutTypeObject = Class;
		}
		else if (Type == TEXT("class"))
		{
			UClass* Class = ResolveObjectClass(ObjectClassName);
			if (!Class)
			{
				OutError = FString::Printf(TEXT("object_class '%s' could not be resolved"), *ObjectClassName);
				return false;
			}
			OutType = EPropertyBagPropertyType::Class;
			OutTypeObject = Class;
		}
		else
		{
			OutError = FString::Printf(TEXT("Unsupported PCG graph parameter type '%s'"), *TypeName);
			return false;
		}

		return true;
	}

	static bool JsonToVector(const TSharedPtr<FJsonValue>& Json, FVector& OutValue)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Json.IsValid() || !Json->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			return false;
		}
		double X = 0.0, Y = 0.0, Z = 0.0;
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
		double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
		(*Obj)->TryGetNumberField(TEXT("pitch"), Pitch);
		(*Obj)->TryGetNumberField(TEXT("yaw"), Yaw);
		(*Obj)->TryGetNumberField(TEXT("roll"), Roll);
		OutValue = FRotator(Pitch, Yaw, Roll);
		return true;
	}

	static bool JsonToTransform(const TSharedPtr<FJsonValue>& Json, FTransform& OutValue)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Json.IsValid() || !Json->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			return false;
		}

		FVector Location = FVector::ZeroVector;
		FVector Scale = FVector::OneVector;
		FRotator Rotation = FRotator::ZeroRotator;
		if (const TSharedPtr<FJsonObject>* LocationObj = nullptr; (*Obj)->TryGetObjectField(TEXT("location"), LocationObj))
		{
			JsonToVector(MakeShared<FJsonValueObject>(*LocationObj), Location);
		}
		if (const TSharedPtr<FJsonObject>* ScaleObj = nullptr; (*Obj)->TryGetObjectField(TEXT("scale"), ScaleObj))
		{
			JsonToVector(MakeShared<FJsonValueObject>(*ScaleObj), Scale);
		}
		if (const TSharedPtr<FJsonObject>* RotationObj = nullptr; (*Obj)->TryGetObjectField(TEXT("rotation"), RotationObj))
		{
			JsonToRotator(MakeShared<FJsonValueObject>(*RotationObj), Rotation);
		}
		OutValue = FTransform(Rotation, Location, Scale);
		return true;
	}

	static UClass* GetDescObjectClass(const FPropertyBagPropertyDesc& Desc)
	{
		if (const UClass* Class = Cast<UClass>(Desc.ValueTypeObject.Get()))
		{
			return const_cast<UClass*>(Class);
		}
		return UObject::StaticClass();
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
			if (!ValueJson->TryGetBool(BoolValue)) break;
			Result = Bag.SetValueBool(Name, BoolValue);
			break;
		case EPropertyBagPropertyType::Byte:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = Bag.SetValueByte(Name, static_cast<uint8>(Number));
			break;
		case EPropertyBagPropertyType::Int32:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = Bag.SetValueInt32(Name, static_cast<int32>(Number));
			break;
		case EPropertyBagPropertyType::Int64:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = Bag.SetValueInt64(Name, static_cast<int64>(Number));
			break;
		case EPropertyBagPropertyType::UInt32:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = Bag.SetValueUInt32(Name, static_cast<uint32>(Number));
			break;
		case EPropertyBagPropertyType::UInt64:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = Bag.SetValueUInt64(Name, static_cast<uint64>(Number));
			break;
		case EPropertyBagPropertyType::Float:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = Bag.SetValueFloat(Name, static_cast<float>(Number));
			break;
		case EPropertyBagPropertyType::Double:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = Bag.SetValueDouble(Name, Number);
			break;
		case EPropertyBagPropertyType::Name:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = Bag.SetValueName(Name, FName(*StringValue));
			break;
		case EPropertyBagPropertyType::String:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = Bag.SetValueString(Name, StringValue);
			break;
		case EPropertyBagPropertyType::Text:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = Bag.SetValueText(Name, FText::FromString(StringValue));
			break;
		case EPropertyBagPropertyType::Struct:
			if (Desc.ValueTypeObject.Get() == TBaseStructure<FVector>::Get())
			{
				FVector VectorValue;
				if (JsonToVector(ValueJson, VectorValue)) Result = Bag.SetValueStruct(Name, VectorValue);
			}
			else if (Desc.ValueTypeObject.Get() == TBaseStructure<FRotator>::Get())
			{
				FRotator RotatorValue;
				if (JsonToRotator(ValueJson, RotatorValue)) Result = Bag.SetValueStruct(Name, RotatorValue);
			}
			else if (Desc.ValueTypeObject.Get() == TBaseStructure<FTransform>::Get())
			{
				FTransform TransformValue;
				if (JsonToTransform(ValueJson, TransformValue)) Result = Bag.SetValueStruct(Name, TransformValue);
			}
			break;
		case EPropertyBagPropertyType::Object:
			if (!ValueJson->TryGetString(StringValue)) break;
			{
				UObject* ObjectValue = StringValue.IsEmpty() ? nullptr : StaticLoadObject(UObject::StaticClass(), nullptr, *StringValue);
				UClass* ExpectedClass = GetDescObjectClass(Desc);
				if (ObjectValue && ExpectedClass && !ObjectValue->IsA(ExpectedClass))
				{
					OutError = FString::Printf(TEXT("Object '%s' is not a '%s'"), *StringValue, *ExpectedClass->GetName());
					return false;
				}
				Result = Bag.SetValueObject(Name, ObjectValue);
			}
			break;
		case EPropertyBagPropertyType::SoftObject:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = Bag.SetValueSoftPath(Name, FSoftObjectPath(StringValue));
			break;
		case EPropertyBagPropertyType::Class:
			if (!ValueJson->TryGetString(StringValue)) break;
			{
				UClass* ClassValue = StringValue.IsEmpty() ? nullptr : LoadObject<UClass>(nullptr, *StringValue);
				Result = Bag.SetValueClass(Name, ClassValue);
			}
			break;
		default:
			break;
		}

		if (!IsSuccess(Result))
		{
			OutError = FString::Printf(TEXT("Failed to set parameter '%s': %s"), *Name.ToString(), *PropertyBagResultToString(Result));
			return false;
		}
		return true;
	}

	static bool AddArrayValue(FPropertyBagArrayRef& ArrayRef, const FPropertyBagPropertyDesc& Desc, const TSharedPtr<FJsonValue>& ValueJson, FString& OutError)
	{
		const int32 Index = ArrayRef.AddValue();
		EPropertyBagResult Result = EPropertyBagResult::TypeMismatch;
		double Number = 0.0;
		FString StringValue;
		bool BoolValue = false;

		switch (Desc.ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			if (!ValueJson->TryGetBool(BoolValue)) break;
			Result = ArrayRef.SetValueBool(Index, BoolValue);
			break;
		case EPropertyBagPropertyType::Byte:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = ArrayRef.SetValueByte(Index, static_cast<uint8>(Number));
			break;
		case EPropertyBagPropertyType::Int32:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = ArrayRef.SetValueInt32(Index, static_cast<int32>(Number));
			break;
		case EPropertyBagPropertyType::Int64:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = ArrayRef.SetValueInt64(Index, static_cast<int64>(Number));
			break;
		case EPropertyBagPropertyType::UInt32:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = ArrayRef.SetValueUInt32(Index, static_cast<uint32>(Number));
			break;
		case EPropertyBagPropertyType::UInt64:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = ArrayRef.SetValueUInt64(Index, static_cast<uint64>(Number));
			break;
		case EPropertyBagPropertyType::Float:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = ArrayRef.SetValueFloat(Index, static_cast<float>(Number));
			break;
		case EPropertyBagPropertyType::Double:
			if (!ValueJson->TryGetNumber(Number)) break;
			Result = ArrayRef.SetValueDouble(Index, Number);
			break;
		case EPropertyBagPropertyType::Name:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = ArrayRef.SetValueName(Index, FName(*StringValue));
			break;
		case EPropertyBagPropertyType::String:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = ArrayRef.SetValueString(Index, StringValue);
			break;
		case EPropertyBagPropertyType::Text:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = ArrayRef.SetValueText(Index, FText::FromString(StringValue));
			break;
		case EPropertyBagPropertyType::Struct:
			if (Desc.ValueTypeObject.Get() == TBaseStructure<FVector>::Get())
			{
				FVector VectorValue;
				if (JsonToVector(ValueJson, VectorValue)) Result = ArrayRef.SetValueStruct(Index, VectorValue);
			}
			else if (Desc.ValueTypeObject.Get() == TBaseStructure<FRotator>::Get())
			{
				FRotator RotatorValue;
				if (JsonToRotator(ValueJson, RotatorValue)) Result = ArrayRef.SetValueStruct(Index, RotatorValue);
			}
			else if (Desc.ValueTypeObject.Get() == TBaseStructure<FTransform>::Get())
			{
				FTransform TransformValue;
				if (JsonToTransform(ValueJson, TransformValue)) Result = ArrayRef.SetValueStruct(Index, TransformValue);
			}
			break;
		case EPropertyBagPropertyType::Object:
			if (!ValueJson->TryGetString(StringValue)) break;
			{
				UObject* ObjectValue = StringValue.IsEmpty() ? nullptr : StaticLoadObject(UObject::StaticClass(), nullptr, *StringValue);
				UClass* ExpectedClass = GetDescObjectClass(Desc);
				if (ObjectValue && ExpectedClass && !ObjectValue->IsA(ExpectedClass))
				{
					OutError = FString::Printf(TEXT("Object '%s' is not a '%s'"), *StringValue, *ExpectedClass->GetName());
					return false;
				}
				Result = ArrayRef.SetValueObject(Index, ObjectValue);
			}
			break;
		case EPropertyBagPropertyType::SoftObject:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = ArrayRef.SetValueSoftPath(Index, FSoftObjectPath(StringValue));
			break;
		case EPropertyBagPropertyType::Class:
			if (!ValueJson->TryGetString(StringValue)) break;
			Result = ArrayRef.SetValueClass(Index, StringValue.IsEmpty() ? nullptr : LoadObject<UClass>(nullptr, *StringValue));
			break;
		default:
			break;
		}

		if (!IsSuccess(Result))
		{
			OutError = FString::Printf(TEXT("Failed to set array value at index %d: %s"), Index, *PropertyBagResultToString(Result));
			return false;
		}
		return true;
	}

	static const FPropertyBagPropertyDesc* FindPropertyDesc(const FInstancedPropertyBag& Bag, const FName Name)
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

	static bool SetBagValue(FInstancedPropertyBag& Bag, const FName Name, const TSharedPtr<FJsonValue>& ValueJson, FString& OutError)
	{
		if (!ValueJson.IsValid())
		{
			OutError = TEXT("value is required");
			return false;
		}

		const FPropertyBagPropertyDesc* Desc = FindPropertyDesc(Bag, Name);
		if (!Desc)
		{
			OutError = FString::Printf(TEXT("Parameter '%s' was not found"), *Name.ToString());
			return false;
		}

		if (Desc->ContainerTypes.GetFirstContainerType() == EPropertyBagContainerType::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!ValueJson->TryGetArray(Values) || !Values)
			{
				OutError = FString::Printf(TEXT("Parameter '%s' is an array; value must be a JSON array"), *Name.ToString());
				return false;
			}

			TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> ArrayResult = Bag.GetMutableArrayRef(*Desc);
			if (!ArrayResult.IsValid())
			{
				OutError = FString::Printf(TEXT("Failed to access array parameter '%s': %s"), *Name.ToString(), *PropertyBagResultToString(ArrayResult.GetError()));
				return false;
			}

			FPropertyBagArrayRef& ArrayRef = ArrayResult.GetValue();
			ArrayRef.EmptyValues(Values->Num());
			for (const TSharedPtr<FJsonValue>& Item : *Values)
			{
				if (!AddArrayValue(ArrayRef, *Desc, Item, OutError))
				{
					return false;
				}
			}
			return true;
		}

		if (!Desc->ContainerTypes.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Parameter '%s' uses an unsupported container type"), *Name.ToString());
			return false;
		}

		return SetBagScalarValue(Bag, Name, *Desc, ValueJson, OutError);
	}

	static TArray<TSharedPtr<FJsonValue>> BuildParametersJson(const FInstancedPropertyBag* UserParams)
	{
		TArray<TSharedPtr<FJsonValue>> ParametersArray;
		if (!UserParams)
		{
			return ParametersArray;
		}
		const UPropertyBag* BagStruct = UserParams->GetPropertyBagStruct();
		if (!BagStruct)
		{
			return ParametersArray;
		}

		for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), Desc.Name.ToString());
			if (Desc.CachedProperty)
			{
				ParamObj->SetStringField(TEXT("cpp_type"), Desc.CachedProperty->GetCPPType());
			}
			if (const UEnum* PropTypeEnum = StaticEnum<EPropertyBagPropertyType>())
			{
				ParamObj->SetStringField(TEXT("type"), PropTypeEnum->GetNameStringByValue(static_cast<int64>(Desc.ValueType)));
			}
			if (const UEnum* ContainerTypeEnum = StaticEnum<EPropertyBagContainerType>())
			{
				ParamObj->SetStringField(TEXT("container"), ContainerTypeEnum->GetNameStringByValue(static_cast<int64>(Desc.ContainerTypes.GetFirstContainerType())));
			}
			if (Desc.ValueTypeObject)
			{
				ParamObj->SetStringField(TEXT("type_object"), Desc.ValueTypeObject->GetPathName());
			}
#if WITH_EDITOR
			const FString Tooltip = Desc.GetMetaData(TooltipMetaKey);
			if (!Tooltip.IsEmpty())
			{
				ParamObj->SetStringField(TEXT("tooltip"), Tooltip);
				ParamObj->SetStringField(TEXT("description"), Tooltip);
			}
#endif
			ParametersArray.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		return ParametersArray;
	}

	static bool ResolvePropertyBagContainerType(const FString& ContainerName, EPropertyBagContainerType& OutContainerType, FString& OutError)
	{
		const FString NormalizedContainer = ContainerName.ToLower();
		OutContainerType = EPropertyBagContainerType::None;
		if (NormalizedContainer == TEXT("array"))
		{
			OutContainerType = EPropertyBagContainerType::Array;
			return true;
		}
		if (NormalizedContainer.IsEmpty() || NormalizedContainer == TEXT("none"))
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Unsupported container '%s'. Supported: none, array"), *ContainerName);
		return false;
	}

	static TSharedPtr<FJsonObject> BuildGraphParameterResult(UPCGGraph* Graph, const FString& GraphPath, const FString& Name, const FString* NewName = nullptr)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("graph_path"), GraphPath);
		Out->SetStringField(TEXT("name"), Name);
		if (NewName)
		{
			Out->SetStringField(TEXT("new_name"), *NewName);
		}
		Out->SetArrayField(TEXT("parameters"), BuildParametersJson(Graph ? Graph->GetUserParametersStruct() : nullptr));
		return Out;
	}
}

//==============================================================================
// create_pcg_graph
//==============================================================================
FECACommandResult FECACommand_CreatePCGGraph::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("asset_path is required"));
	}

	if (!AssetPath.StartsWith(TEXT("/")))
	{
		return FECACommandResult::Error(TEXT("asset_path must be a full content path starting with '/' (e.g. /Game/MyFolder/MyGraph)"));
	}

	if (StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

	FString PackageName, AssetName;
	if (!AssetPath.Split(TEXT("."), &PackageName, &AssetName))
	{
		PackageName = AssetPath;
		AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	}
	if (AssetName.IsEmpty())
	{
		AssetName = FPackageName::GetLongPackageAssetName(PackageName);
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package '%s'"), *PackageName));
	}

	UPCGGraph* Graph = NewObject<UPCGGraph>(Package, UPCGGraph::StaticClass(), *AssetName, RF_Public | RF_Standalone);
	if (!Graph)
	{
		return FECACommandResult::Error(TEXT("NewObject<UPCGGraph> returned null"));
	}
	FAssetRegistryModule::AssetCreated(Graph);

	FString SaveError;
	if (!ECAPcgHelpers::SaveAssetPackage(Graph, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), Graph->GetPathName());
	Result->SetStringField(TEXT("graph_name"), Graph->GetName());
	return FECACommandResult::Success(Result);
}

//==============================================================================
// add_pcg_node
//==============================================================================
FECACommandResult FECACommand_AddPCGNode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, SettingsClassName;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("settings_class"), SettingsClassName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("settings_class is required"));
	}
	double PosX = 0.0, PosY = 0.0;
	GetFloatParam(Params, TEXT("position_x"), PosX, false);
	GetFloatParam(Params, TEXT("position_y"), PosY, false);

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	UClass* SettingsClass = ECAPcgHelpers::FindPCGSettingsClass(SettingsClassName);
	if (!SettingsClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("UPCGSettings subclass '%s' not found. Use list_pcg_node_types to discover available classes."), *SettingsClassName));
	}

	UPCGSettings* DefaultSettings = nullptr;
	UPCGNode* NewNode = Graph->AddNodeOfType(TSubclassOf<UPCGSettings>(SettingsClass), DefaultSettings);
	if (!NewNode)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("UPCGGraph::AddNodeOfType returned null for '%s'"), *SettingsClass->GetName()));
	}

#if WITH_EDITOR
	NewNode->SetNodePosition(static_cast<int32>(PosX), static_cast<int32>(PosY));
#endif

	FString SaveError;
	ECAPcgHelpers::SaveAssetPackage(Graph, SaveError); // best-effort save

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("graph_path"), GraphPath);
	Result->SetStringField(TEXT("node_id"), NewNode->GetName());
	Result->SetStringField(TEXT("settings_class"), SettingsClass->GetName());
	return FECACommandResult::Success(Result);
}

//==============================================================================
// connect_pcg_nodes
//==============================================================================
FECACommandResult FECACommand_ConnectPCGNodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, FromId, ToId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("from_node_id"), FromId, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("from_node_id is required"));
	}
	if (!GetStringParam(Params, TEXT("to_node_id"), ToId, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("to_node_id is required"));
	}
	FString FromPinName, ToPinName;
	GetStringParam(Params, TEXT("from_pin"), FromPinName, false);
	GetStringParam(Params, TEXT("to_pin"), ToPinName, false);

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	UPCGNode* From = ECAPcgHelpers::FindNodeById(Graph, FromId);
	UPCGNode* To = ECAPcgHelpers::FindNodeById(Graph, ToId);
	if (!From) return FECACommandResult::Error(FString::Printf(TEXT("from_node_id '%s' not found in graph"), *FromId));
	if (!To)   return FECACommandResult::Error(FString::Printf(TEXT("to_node_id '%s' not found in graph"), *ToId));

	// Resolve default pins if labels omitted
	if (FromPinName.IsEmpty())
	{
		const TArray<TObjectPtr<UPCGPin>>& Pins = From->GetOutputPins();
		if (Pins.Num() == 0 || !Pins[0])
		{
			return FECACommandResult::Error(FString::Printf(TEXT("from node '%s' has no output pins"), *FromId));
		}
		FromPinName = Pins[0]->Properties.Label.ToString();
	}
	if (ToPinName.IsEmpty())
	{
		const TArray<TObjectPtr<UPCGPin>>& Pins = To->GetInputPins();
		if (Pins.Num() == 0 || !Pins[0])
		{
			return FECACommandResult::Error(FString::Printf(TEXT("to node '%s' has no input pins"), *ToId));
		}
		ToPinName = Pins[0]->Properties.Label.ToString();
	}

	UPCGNode* Result = Graph->AddEdge(From, FName(*FromPinName), To, FName(*ToPinName));
	if (!Result)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("UPCGGraph::AddEdge failed (from '%s'.%s -> '%s'.%s; check pin labels)"),
			*FromId, *FromPinName, *ToId, *ToPinName));
	}

	FString SaveError;
	ECAPcgHelpers::SaveAssetPackage(Graph, SaveError);

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), GraphPath);
	Out->SetStringField(TEXT("from_node_id"), FromId);
	Out->SetStringField(TEXT("from_pin"), FromPinName);
	Out->SetStringField(TEXT("to_node_id"), ToId);
	Out->SetStringField(TEXT("to_pin"), ToPinName);
	return FECACommandResult::Success(Out);
}

//==============================================================================
// auto_layout_pcg_graph
//==============================================================================
FECACommandResult FECACommand_AutoLayoutPCGGraph::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
	FString GraphPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	int32 SpacingX = 320;
	int32 SpacingY = 160;
	if (Params->HasField(TEXT("spacing_x")))
	{
		SpacingX = FMath::Max(1, static_cast<int32>(Params->GetNumberField(TEXT("spacing_x"))));
	}
	if (Params->HasField(TEXT("spacing_y")))
	{
		SpacingY = FMath::Max(1, static_cast<int32>(Params->GetNumberField(TEXT("spacing_y"))));
	}

	TSet<FString> SpecificNodeIds;
	const TArray<TSharedPtr<FJsonValue>>* NodeIdsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("node_ids"), NodeIdsArray) && NodeIdsArray)
	{
		for (const TSharedPtr<FJsonValue>& IdValue : *NodeIdsArray)
		{
			FString NodeId;
			if (IdValue->TryGetString(NodeId) && !NodeId.IsEmpty())
			{
				SpecificNodeIds.Add(NodeId);
			}
		}
	}

	TArray<UPCGNode*> NodesToLayout;
	if (UPCGNode* InputNode = Graph->GetInputNode())
	{
		NodesToLayout.Add(InputNode);
	}
	if (UPCGNode* OutputNode = Graph->GetOutputNode())
	{
		NodesToLayout.AddUnique(OutputNode);
	}
	for (UPCGNode* Node : Graph->GetNodes())
	{
		if (Node)
		{
			NodesToLayout.AddUnique(Node);
		}
	}

	for (int32 Index = NodesToLayout.Num() - 1; Index >= 0; --Index)
	{
		UPCGNode* Node = NodesToLayout[Index];
		if (!Node || (SpecificNodeIds.Num() > 0 && !SpecificNodeIds.Contains(Node->GetName())))
		{
			NodesToLayout.RemoveAt(Index);
		}
	}

	if (NodesToLayout.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("No PCG nodes to layout"));
	}

	FLayeredGraphLayout Core;
	FLayeredLayoutConfig LayoutConfig;
	LayoutConfig.PaddingX = SpacingX;
	LayoutConfig.PaddingY = SpacingY;

	TMap<UPCGNode*, int32> NodeToVertexId;
	TMap<int32, UPCGNode*> VertexIdToNode;
	int32 NextVertexId = 0;
	bool bHasAnchor = false;
	for (UPCGNode* Node : NodesToLayout)
	{
		int32 PosX = 0;
		int32 PosY = 0;
		Node->GetNodePosition(PosX, PosY);
		if (!bHasAnchor)
		{
			LayoutConfig.StartX = PosX;
			LayoutConfig.StartY = PosY;
			bHasAnchor = true;
		}
		else
		{
			LayoutConfig.StartX = FMath::Min(LayoutConfig.StartX, PosX);
			LayoutConfig.StartY = FMath::Min(LayoutConfig.StartY, PosY);
		}

		const int32 VertexId = NextVertexId++;
		NodeToVertexId.Add(Node, VertexId);
		VertexIdToNode.Add(VertexId, Node);
		Core.AddVertex(VertexId, 260, 120, PosX, PosY);
	}

	TSet<FString> SeenEdges;
	int32 EdgeCount = 0;
	for (UPCGNode* SourceNode : NodesToLayout)
	{
		const int32 SourceId = NodeToVertexId.FindRef(SourceNode);
		int32 SourcePinIndex = 0;
		for (const UPCGPin* Pin : SourceNode->GetOutputPins())
		{
			if (!Pin)
			{
				++SourcePinIndex;
				continue;
			}

			for (const UPCGEdge* Edge : Pin->Edges)
			{
				if (!Edge || Edge->InputPin != Pin || !Edge->OutputPin || !Edge->OutputPin->Node)
				{
					continue;
				}

				UPCGNode* TargetNode = Edge->OutputPin->Node;
				const int32* TargetId = NodeToVertexId.Find(TargetNode);
				if (!TargetId)
				{
					continue;
				}

				const FString EdgeKey = FString::Printf(TEXT("%d:%d:%s:%s"), SourceId, *TargetId,
					*Pin->Properties.Label.ToString(), *Edge->OutputPin->Properties.Label.ToString());
				if (SeenEdges.Contains(EdgeKey))
				{
					continue;
				}
				SeenEdges.Add(EdgeKey);
				Core.AddEdge(SourceId, *TargetId, 36 + SourcePinIndex * 24, 60, false);
				++EdgeCount;
			}
			++SourcePinIndex;
		}
	}

	FLayeredLayoutResult LayoutResult = Core.Solve(LayoutConfig);

	Graph->Modify();
	int32 NodesPositioned = 0;
	for (const FLayeredLayoutVertex& Vertex : Core.GetVertices())
	{
		UPCGNode* Node = VertexIdToNode.FindRef(Vertex.Id);
		if (!Node)
		{
			continue;
		}
		Node->Modify();
		Node->SetNodePosition(Vertex.X, Vertex.Y);
		++NodesPositioned;
	}
	Graph->PostEditChange();
	Graph->MarkPackageDirty();

	TArray<TSharedPtr<FJsonValue>> WarningsArray;
	for (const FString& Warning : LayoutResult.Warnings)
	{
		WarningsArray.Add(MakeShared<FJsonValueString>(Warning));
	}

	TArray<TSharedPtr<FJsonValue>> PositionedNodesArray;
	for (const FLayeredLayoutVertex& Vertex : Core.GetVertices())
	{
		UPCGNode* Node = VertexIdToNode.FindRef(Vertex.Id);
		if (!Node)
		{
			continue;
		}

		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("node_id"), Node->GetName());
		NodeObj->SetStringField(TEXT("node_title"), Node->GetAuthoredTitleName().ToString());
		NodeObj->SetNumberField(TEXT("x"), Vertex.X);
		NodeObj->SetNumberField(TEXT("y"), Vertex.Y);
		NodeObj->SetNumberField(TEXT("rank"), Vertex.Rank);
		PositionedNodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), GraphPath);
	Result->SetStringField(TEXT("graph_path"), GraphPath);
	Result->SetStringField(TEXT("graph_name"), Graph->GetName());
	Result->SetStringField(TEXT("layout_engine"), TEXT("layered"));
	Result->SetNumberField(TEXT("nodes_positioned"), NodesPositioned);
	Result->SetNumberField(TEXT("edge_count"), EdgeCount);
	Result->SetNumberField(TEXT("rank_count"), LayoutResult.RankCount);
	Result->SetNumberField(TEXT("comments_wrapped"), 0);
	Result->SetNumberField(TEXT("auto_knots_removed"), 0);
	Result->SetNumberField(TEXT("auto_knots_created"), 0);
	Result->SetNumberField(TEXT("spacing_x"), SpacingX);
	Result->SetNumberField(TEXT("spacing_y"), SpacingY);
	Result->SetArrayField(TEXT("warnings"), WarningsArray);
	Result->SetArrayField(TEXT("positioned_nodes"), PositionedNodesArray);
	return FECACommandResult::Success(Result);
#else
	return FECACommandResult::Error(TEXT("auto_layout_pcg_graph requires an editor build"));
#endif
}

//==============================================================================
// set_pcg_node_property
//==============================================================================
FECACommandResult FECACommand_SetPCGNodeProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, NodeId, PropertyName;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	}
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("property_name is required"));
	}

	const TSharedPtr<FJsonValue> ValueJson = Params.IsValid() ? Params->TryGetField(TEXT("value")) : nullptr;
	if (!ValueJson.IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("value is required"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}
	UPCGNode* Node = ECAPcgHelpers::FindNodeById(Graph, NodeId);
	if (!Node)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found in graph"), *NodeId));
	}
	UPCGSettings* Settings = Node->GetSettings();
	if (!Settings)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' has no settings object"), *NodeId));
	}

	FProperty* Property = Settings->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Property '%s' not found on settings class '%s'"), *PropertyName, *Settings->GetClass()->GetName()));
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Settings);
	bool bApplied = false;
	FString TypeName = Property->GetCPPType();

	// Note: UPCGSettings overrides PreEditChange / PostEditChangeProperty as
	// protected, so we can't drive the change broadcast through the standard
	// UObject path. Saving the graph package below dirties the asset; the
	// editor refreshes on reload.

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (ValueJson->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			bApplied = true;
		}
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(NumVal));
			bApplied = true;
		}
	}
	else if (FInt64Property* I64Prop = CastField<FInt64Property>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			I64Prop->SetPropertyValue(ValuePtr, static_cast<int64>(NumVal));
			bApplied = true;
		}
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(NumVal));
			bApplied = true;
		}
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			DoubleProp->SetPropertyValue(ValuePtr, NumVal);
			bApplied = true;
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (ValueJson->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			bApplied = true;
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (ValueJson->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			bApplied = true;
		}
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		FString StrVal;
		double NumVal = 0.0;
		if (ValueJson->TryGetString(StrVal))
		{
			UEnum* Enum = EnumProp->GetEnum();
			int64 EnumValue = Enum ? Enum->GetValueByNameString(StrVal) : INDEX_NONE;
			if (EnumValue == INDEX_NONE && Enum)
			{
				EnumValue = Enum->GetValueByName(FName(*StrVal));
			}
			if (EnumValue != INDEX_NONE)
			{
				EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
				bApplied = true;
			}
		}
		else if (ValueJson->TryGetNumber(NumVal))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumVal));
			bApplied = true;
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			const TSharedPtr<FJsonObject>* Obj;
			if (ValueJson->TryGetObject(Obj) && Obj && (*Obj).IsValid())
			{
				double X = 0, Y = 0, Z = 0;
				(*Obj)->TryGetNumberField(TEXT("x"), X);
				(*Obj)->TryGetNumberField(TEXT("y"), Y);
				(*Obj)->TryGetNumberField(TEXT("z"), Z);
				FVector V(X, Y, Z);
				StructProp->CopyCompleteValue(ValuePtr, &V);
				bApplied = true;
			}
		}
	}

	if (bApplied)
	{
		Settings->Modify();
	}

	if (!bApplied)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Property '%s' of type '%s' could not be assigned from the provided value JSON. Supported: bool, int, float/double, string, name, enum (by name or number), FVector ({x,y,z})."),
			*PropertyName, *TypeName));
	}

	FString SaveError;
	ECAPcgHelpers::SaveAssetPackage(Graph, SaveError);

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), GraphPath);
	Out->SetStringField(TEXT("node_id"), NodeId);
	Out->SetStringField(TEXT("property_name"), PropertyName);
	Out->SetStringField(TEXT("property_type"), TypeName);
	return FECACommandResult::Success(Out);
}

//==============================================================================
// add_pcg_graph_parameter
//==============================================================================
FECACommandResult FECACommand_AddPCGGraphParameter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, Name, TypeName;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("name is required"));
	}
	if (!GetStringParam(Params, TEXT("type"), TypeName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("type is required"));
	}

	FString ContainerName = TEXT("none");
	FString ObjectClassName;
	bool bOverwrite = false;
	GetStringParam(Params, TEXT("container"), ContainerName, false);
	GetStringParam(Params, TEXT("object_class"), ObjectClassName, false);
	GetBoolParam(Params, TEXT("overwrite"), bOverwrite, false);
	FString Tooltip;
	GetStringParam(Params, TEXT("tooltip"), Tooltip, false);
	if (Tooltip.IsEmpty())
	{
		GetStringParam(Params, TEXT("description"), Tooltip, false);
	}
	const TSharedPtr<FJsonValue> ValueJson = Params.IsValid() ? Params->TryGetField(TEXT("value")) : nullptr;

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	EPropertyBagPropertyType ValueType = EPropertyBagPropertyType::None;
	const UObject* ValueTypeObject = nullptr;
	FString Error;
	if (!ECAPcgHelpers::ResolvePropertyBagType(TypeName, ObjectClassName, ValueType, ValueTypeObject, Error))
	{
		return FECACommandResult::Error(Error);
	}

	EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None;
	if (!ECAPcgHelpers::ResolvePropertyBagContainerType(ContainerName, ContainerType, Error))
	{
		return FECACommandResult::Error(Error);
	}

	Graph->Modify();
	FString EditError;
	Graph->UpdateUserParametersStruct([&](FInstancedPropertyBag& Bag)
	{
		const FName ParamName(*Name);
		FPropertyBagPropertyDesc Desc = ContainerType == EPropertyBagContainerType::Array
			? FPropertyBagPropertyDesc(ParamName, ContainerType, ValueType, ValueTypeObject)
			: FPropertyBagPropertyDesc(ParamName, ValueType, ValueTypeObject);
#if WITH_EDITOR
		if (!Tooltip.IsEmpty())
		{
			Desc.SetMetaData(ECAPcgHelpers::TooltipMetaKey, Tooltip);
		}
#endif
		TArray<FPropertyBagPropertyDesc> Descs;
		Descs.Add(Desc);
		const EPropertyBagAlterationResult AddResult = Bag.AddProperties(Descs, bOverwrite);

		if (!ECAPcgHelpers::IsSuccess(AddResult))
		{
			EditError = FString::Printf(TEXT("Failed to add parameter '%s': %s"), *Name, *ECAPcgHelpers::PropertyBagAlterationResultToString(AddResult));
			return;
		}

		if (ValueJson.IsValid())
		{
			ECAPcgHelpers::SetBagValue(Bag, ParamName, ValueJson, EditError);
		}
	});

	if (!EditError.IsEmpty())
	{
		return FECACommandResult::Error(EditError);
	}

	FString SaveError;
	if (!ECAPcgHelpers::SaveAssetPackage(Graph, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	return FECACommandResult::Success(ECAPcgHelpers::BuildGraphParameterResult(Graph, GraphPath, Name));
}

//==============================================================================
// set_pcg_graph_parameter_tooltip
//==============================================================================
FECACommandResult FECACommand_SetPCGGraphParameterTooltip::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, Name, Tooltip;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("name is required"));
	}
	GetStringParam(Params, TEXT("tooltip"), Tooltip, false);
	if (Tooltip.IsEmpty())
	{
		GetStringParam(Params, TEXT("description"), Tooltip, false);
	}
	if (Tooltip.IsEmpty())
	{
		return FECACommandResult::ValidationError(this, TEXT("tooltip or description is required"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	Graph->Modify();
	FString EditError;
	Graph->UpdateUserParametersStruct([&](FInstancedPropertyBag& Bag)
	{
		const FName ParamName(*Name);
		const FPropertyBagPropertyDesc* ExistingDesc = Bag.FindPropertyDescByName(ParamName);
		if (!ExistingDesc)
		{
			EditError = FString::Printf(TEXT("Parameter not found: %s"), *Name);
			return;
		}

		FPropertyBagPropertyDesc UpdatedDesc = *ExistingDesc;
#if WITH_EDITOR
		UpdatedDesc.SetMetaData(ECAPcgHelpers::TooltipMetaKey, Tooltip);
#endif
		TArray<FPropertyBagPropertyDesc> Descs;
		Descs.Add(UpdatedDesc);
		const EPropertyBagAlterationResult Result = Bag.AddProperties(Descs, true);
		if (!ECAPcgHelpers::IsSuccess(Result))
		{
			EditError = FString::Printf(TEXT("Failed to set tooltip for parameter '%s': %s"), *Name, *ECAPcgHelpers::PropertyBagAlterationResultToString(Result));
		}
	});

	if (!EditError.IsEmpty())
	{
		return FECACommandResult::Error(EditError);
	}

	FString SaveError;
	if (!ECAPcgHelpers::SaveAssetPackage(Graph, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> Result = ECAPcgHelpers::BuildGraphParameterResult(Graph, GraphPath, Name);
	Result->SetStringField(TEXT("tooltip"), Tooltip);
	Result->SetStringField(TEXT("description"), Tooltip);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// rename_pcg_graph_parameter
//==============================================================================
FECACommandResult FECACommand_RenamePCGGraphParameter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, Name, NewName;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("name is required"));
	}
	if (!GetStringParam(Params, TEXT("new_name"), NewName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("new_name is required"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	Graph->Modify();
	FString EditError;
	Graph->UpdateUserParametersStruct([&](FInstancedPropertyBag& Bag)
	{
		const EPropertyBagAlterationResult Result = Bag.RenameProperty(FName(*Name), FName(*NewName));
		if (!ECAPcgHelpers::IsSuccess(Result))
		{
			EditError = FString::Printf(TEXT("Failed to rename parameter '%s' to '%s': %s"), *Name, *NewName, *ECAPcgHelpers::PropertyBagAlterationResultToString(Result));
		}
	});

	if (!EditError.IsEmpty())
	{
		return FECACommandResult::Error(EditError);
	}

	FString SaveError;
	if (!ECAPcgHelpers::SaveAssetPackage(Graph, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	return FECACommandResult::Success(ECAPcgHelpers::BuildGraphParameterResult(Graph, GraphPath, Name, &NewName));
}

//==============================================================================
// remove_pcg_graph_parameter
//==============================================================================
FECACommandResult FECACommand_RemovePCGGraphParameter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, Name;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("name is required"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	Graph->Modify();
	FString EditError;
	Graph->UpdateUserParametersStruct([&](FInstancedPropertyBag& Bag)
	{
		const EPropertyBagAlterationResult Result = Bag.RemovePropertyByName(FName(*Name));
		if (!ECAPcgHelpers::IsSuccess(Result))
		{
			EditError = FString::Printf(TEXT("Failed to remove parameter '%s': %s"), *Name, *ECAPcgHelpers::PropertyBagAlterationResultToString(Result));
		}
	});

	if (!EditError.IsEmpty())
	{
		return FECACommandResult::Error(EditError);
	}

	FString SaveError;
	if (!ECAPcgHelpers::SaveAssetPackage(Graph, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	return FECACommandResult::Success(ECAPcgHelpers::BuildGraphParameterResult(Graph, GraphPath, Name));
}

//==============================================================================
// set_pcg_graph_parameter_default
//==============================================================================
FECACommandResult FECACommand_SetPCGGraphParameterDefault::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, Name;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("name is required"));
	}
	const TSharedPtr<FJsonValue> ValueJson = Params.IsValid() ? Params->TryGetField(TEXT("value")) : nullptr;
	if (!ValueJson.IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("value is required"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	Graph->Modify();
	FString EditError;
	Graph->UpdateUserParametersStruct([&](FInstancedPropertyBag& Bag)
	{
		ECAPcgHelpers::SetBagValue(Bag, FName(*Name), ValueJson, EditError);
	});

	if (!EditError.IsEmpty())
	{
		return FECACommandResult::Error(EditError);
	}

	FString SaveError;
	if (!ECAPcgHelpers::SaveAssetPackage(Graph, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	return FECACommandResult::Success(ECAPcgHelpers::BuildGraphParameterResult(Graph, GraphPath, Name));
}
