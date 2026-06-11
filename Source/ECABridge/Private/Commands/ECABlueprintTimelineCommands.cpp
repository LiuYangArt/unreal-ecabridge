// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECABlueprintTimelineCommands.h"

#include "Curves/CurveFloat.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/TimelineTemplate.h"
#include "K2Node_Timeline.h"
#include "Kismet2/BlueprintEditorUtils.h"

REGISTER_ECA_COMMAND(FECACommand_CreateBlueprintTimeline)
REGISTER_ECA_COMMAND(FECACommand_ListBlueprintTimelines)
REGISTER_ECA_COMMAND(FECACommand_SetBlueprintTimelineSettings)
REGISTER_ECA_COMMAND(FECACommand_AddBlueprintTimelineFloatTrack)
REGISTER_ECA_COMMAND(FECACommand_RemoveBlueprintTimeline)

static UEdGraph* FindTimelineGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	const bool bWantsEventGraph = GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase);

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && (Graph->GetName() == GraphName || bWantsEventGraph))
		{
			return Graph;
		}
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	return nullptr;
}

static FString LengthModeToString(ETimelineLengthMode Mode)
{
	return Mode == TL_LastKeyFrame ? TEXT("last_keyframe") : TEXT("timeline_length");
}

static bool ParseLengthMode(const FString& Text, ETimelineLengthMode& OutMode, FString& OutError)
{
	if (Text.IsEmpty() || Text.Equals(TEXT("timeline_length"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("length"), ESearchCase::IgnoreCase))
	{
		OutMode = TL_TimelineLength;
		return true;
	}
	if (Text.Equals(TEXT("last_keyframe"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("last_key"), ESearchCase::IgnoreCase))
	{
		OutMode = TL_LastKeyFrame;
		return true;
	}
	OutError = FString::Printf(TEXT("Invalid length_mode '%s'. Expected 'timeline_length' or 'last_keyframe'."), *Text);
	return false;
}

static bool ValidateSettings(const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
	FString Text;
	if (Params.IsValid() && Params->TryGetStringField(TEXT("length_mode"), Text))
	{
		ETimelineLengthMode Mode = TL_TimelineLength;
		return ParseLengthMode(Text, Mode, OutError);
	}
	return true;
}
static uint8 ResolveBoolSetting(
	const TSharedPtr<FJsonObject>& Params,
	const TCHAR* FieldName,
	bool bDefaults,
	uint8 CurrentValue)
{
	bool bValue = false;
	if (Params->TryGetBoolField(FieldName, bValue))
	{
		return bValue;
	}
	return bDefaults ? false : CurrentValue;
}
static FVector2D GetNodePosition(const TSharedPtr<FJsonObject>& Params)
{
	FVector2D Position(0.0, 0.0);
	const TSharedPtr<FJsonObject>* PosObj = nullptr;
	if (Params->TryGetObjectField(TEXT("node_position"), PosObj) && PosObj && PosObj->IsValid())
	{
		(*PosObj)->TryGetNumberField(TEXT("x"), Position.X);
		(*PosObj)->TryGetNumberField(TEXT("y"), Position.Y);
	}
	return Position;
}

static int32 FindFloatTrackIndex(const UTimelineTemplate* Timeline, FName TrackName)
{
	for (int32 Index = 0; Index < Timeline->FloatTracks.Num(); ++Index)
	{
		if (Timeline->FloatTracks[Index].GetTrackName() == TrackName)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

static void ApplySettings(UTimelineTemplate* Timeline, const TSharedPtr<FJsonObject>& Params, bool bDefaults)
{
	double Number = 5.0;
	if (Params->TryGetNumberField(TEXT("length"), Number) || (bDefaults && !Params->HasField(TEXT("length"))))
	{
		Timeline->TimelineLength = FMath::Max(0.0f, static_cast<float>(Number));
	}

	FString Text;
	if (Params->TryGetStringField(TEXT("length_mode"), Text) || bDefaults)
	{
		ETimelineLengthMode Mode = TL_TimelineLength;
		FString Error;
		if (ParseLengthMode(Text, Mode, Error))
		{
			Timeline->LengthMode = Mode;
		}
	}

	Timeline->bAutoPlay = ResolveBoolSetting(Params, TEXT("auto_play"), bDefaults, Timeline->bAutoPlay);
	Timeline->bLoop = ResolveBoolSetting(Params, TEXT("loop"), bDefaults, Timeline->bLoop);
	Timeline->bReplicated = ResolveBoolSetting(Params, TEXT("replicated"), bDefaults, Timeline->bReplicated);
	Timeline->bIgnoreTimeDilation = ResolveBoolSetting(
		Params,
		TEXT("ignore_time_dilation"),
		bDefaults,
		Timeline->bIgnoreTimeDilation);
}

static TSharedPtr<FJsonObject> CurveToJson(const UCurveFloat* Curve)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Keys;
	if (Curve)
	{
		for (const FRichCurveKey& Key : Curve->FloatCurve.GetConstRefOfKeys())
		{
			TSharedPtr<FJsonObject> KeyJson = MakeShared<FJsonObject>();
			KeyJson->SetNumberField(TEXT("time"), Key.Time);
			KeyJson->SetNumberField(TEXT("value"), Key.Value);
			Keys.Add(MakeShared<FJsonValueObject>(KeyJson));
		}
	}
	Json->SetArrayField(TEXT("keys"), Keys);
	Json->SetNumberField(TEXT("key_count"), Keys.Num());
	return Json;
}

static TSharedPtr<FJsonObject> TimelineToJson(UBlueprint* Blueprint, UTimelineTemplate* Timeline)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("timeline_name"), Timeline->GetVariableName().ToString());
	Json->SetStringField(TEXT("template_name"), Timeline->GetName());
	Json->SetStringField(TEXT("guid"), Timeline->TimelineGuid.ToString());
	Json->SetNumberField(TEXT("length"), Timeline->TimelineLength);
	Json->SetStringField(TEXT("length_mode"), LengthModeToString(Timeline->LengthMode));
	Json->SetBoolField(TEXT("auto_play"), Timeline->bAutoPlay != 0);
	Json->SetBoolField(TEXT("loop"), Timeline->bLoop != 0);
	Json->SetBoolField(TEXT("replicated"), Timeline->bReplicated != 0);
	Json->SetBoolField(TEXT("ignore_time_dilation"), Timeline->bIgnoreTimeDilation != 0);

	if (UK2Node_Timeline* Node = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Timeline))
	{
		Json->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
		Json->SetNumberField(TEXT("node_x"), Node->NodePosX);
		Json->SetNumberField(TEXT("node_y"), Node->NodePosY);
	}

	TArray<TSharedPtr<FJsonValue>> Tracks;
	for (const FTTFloatTrack& Track : Timeline->FloatTracks)
	{
		TSharedPtr<FJsonObject> TrackJson = MakeShared<FJsonObject>();
		TrackJson->SetStringField(TEXT("type"), TEXT("float"));
		TrackJson->SetStringField(TEXT("track_name"), Track.GetTrackName().ToString());
		TrackJson->SetObjectField(TEXT("curve"), CurveToJson(Track.CurveFloat));
		Tracks.Add(MakeShared<FJsonValueObject>(TrackJson));
	}
	Json->SetArrayField(TEXT("tracks"), Tracks);
	Json->SetNumberField(TEXT("float_track_count"), Timeline->FloatTracks.Num());
	return Json;
}

static bool ParseFloatKeys(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<TPair<float, float>>& OutKeys, FString& OutError)
{
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Values[Index].IsValid() || !Values[Index]->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			OutError = FString::Printf(TEXT("keys[%d] must be an object with numeric time and value."), Index);
			return false;
		}
		double Time = 0.0;
		double Value = 0.0;
		if (!(*Obj)->TryGetNumberField(TEXT("time"), Time) || !(*Obj)->TryGetNumberField(TEXT("value"), Value))
		{
			OutError = FString::Printf(TEXT("keys[%d] must include numeric time and value."), Index);
			return false;
		}
		OutKeys.Add(TPair<float, float>(static_cast<float>(Time), static_cast<float>(Value)));
	}
	return true;
}

static void ReconstructTimelineNode(UBlueprint* Blueprint, UTimelineTemplate* Timeline)
{
	if (UK2Node_Timeline* Node = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Timeline))
	{
		Node->Modify();
		Node->ReconstructNode();
	}
}

static void FinishMutation(UBlueprint* Blueprint, UTimelineTemplate* Timeline)
{
	ReconstructTimelineNode(Blueprint, Timeline);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->MarkPackageDirty();
}

FECACommandResult FECACommand_CreateBlueprintTimeline::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path"));
	}
	FString Error;
	if (!ValidateSettings(Params, Error))
	{
		return FECACommandResult::ValidationError(this, Error);
	}
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	FString TimelineNameText;
	GetStringParam(Params, TEXT("timeline_name"), TimelineNameText, false);
	FName TimelineName = TimelineNameText.IsEmpty() ? FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint) : FName(*TimelineNameText);
	if (Blueprint->FindTimelineTemplateByVariableName(TimelineName))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint already has a timeline named '%s'."), *TimelineName.ToString()));
	}

	UTimelineTemplate* Timeline = FBlueprintEditorUtils::AddNewTimeline(Blueprint, TimelineName);
	if (!Timeline)
	{
		return FECACommandResult::Error(TEXT("Failed to create timeline. The Blueprint class may not support timelines."));
	}
	Timeline->Modify();
	if (!Timeline->TimelineGuid.IsValid())
	{
		Timeline->TimelineGuid = FGuid::NewGuid();
	}
	ApplySettings(Timeline, Params, true);

	bool bAddNode = true;
	GetBoolParam(Params, TEXT("add_node"), bAddNode, false);
	UK2Node_Timeline* Node = nullptr;
	if (bAddNode)
	{
		FString GraphName = TEXT("EventGraph");
		GetStringParam(Params, TEXT("graph_name"), GraphName, false);
		UEdGraph* Graph = FindTimelineGraph(Blueprint, GraphName);
		if (!Graph)
		{
			FBlueprintEditorUtils::RemoveTimeline(Blueprint, Timeline, true);
			return FECACommandResult::Error(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
		}
		const FVector2D Position = GetNodePosition(Params);
		Node = NewObject<UK2Node_Timeline>(Graph);
		Node->TimelineName = Timeline->GetVariableName();
		Node->TimelineGuid = Timeline->TimelineGuid;
		Node->NodePosX = static_cast<int32>(Position.X);
		Node->NodePosY = static_cast<int32>(Position.Y);
		Graph->AddNode(Node, false, false);
		Node->AllocateDefaultPins();
		Node->CreateNewGuid();
		Node->ReconstructNode();
	}
	FinishMutation(Blueprint, Timeline);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Result->SetObjectField(TEXT("timeline"), TimelineToJson(Blueprint, Timeline));
	if (Node)
	{
		Result->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
	}
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_ListBlueprintTimelines::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path"));
	}
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	TArray<TSharedPtr<FJsonValue>> Timelines;
	for (UTimelineTemplate* Timeline : Blueprint->Timelines)
	{
		if (Timeline)
		{
			Timelines.Add(MakeShared<FJsonValueObject>(TimelineToJson(Blueprint, Timeline)));
		}
	}
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Result->SetArrayField(TEXT("timelines"), Timelines);
	Result->SetNumberField(TEXT("count"), Timelines.Num());
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_SetBlueprintTimelineSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	FString TimelineName;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath) || !GetStringParam(Params, TEXT("timeline_name"), TimelineName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path or timeline_name"));
	}
	FString Error;
	if (!ValidateSettings(Params, Error))
	{
		return FECACommandResult::ValidationError(this, Error);
	}
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(FName(*TimelineName));
	if (!Timeline)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Timeline not found: %s"), *TimelineName));
	}
	Timeline->Modify();
	ApplySettings(Timeline, Params, false);
	FinishMutation(Blueprint, Timeline);
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetObjectField(TEXT("timeline"), TimelineToJson(Blueprint, Timeline));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_AddBlueprintTimelineFloatTrack::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	FString TimelineName;
	FString TrackNameText;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath) || !GetStringParam(Params, TEXT("timeline_name"), TimelineName) || !GetStringParam(Params, TEXT("track_name"), TrackNameText))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path, timeline_name, or track_name"));
	}
	const TArray<TSharedPtr<FJsonValue>>* KeyValues = nullptr;
	if (!GetArrayParam(Params, TEXT("keys"), KeyValues) || !KeyValues)
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: keys"));
	}
	TArray<TPair<float, float>> Keys;
	FString KeyError;
	if (!ParseFloatKeys(*KeyValues, Keys, KeyError))
	{
		return FECACommandResult::ValidationError(this, KeyError);
	}
	bool bReplaceExisting = false;
	GetBoolParam(Params, TEXT("replace_existing"), bReplaceExisting, false);

	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(FName(*TimelineName));
	if (!Timeline)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Timeline not found: %s"), *TimelineName));
	}

	FName TrackName(*TrackNameText);
	int32 TrackIndex = FindFloatTrackIndex(Timeline, TrackName);
	if (TrackIndex != INDEX_NONE && !bReplaceExisting)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Timeline already has float track '%s'. Pass replace_existing=true to replace its keys."), *TrackNameText));
	}
	if (TrackIndex == INDEX_NONE && !Timeline->IsNewTrackNameValid(TrackName))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Timeline already has a track named '%s' with another type."), *TrackNameText));
	}

	Timeline->Modify();
	if (TrackIndex == INDEX_NONE)
	{
		FTTFloatTrack NewTrack;
		NewTrack.SetTrackName(TrackName, Timeline);
		NewTrack.CurveFloat = NewObject<UCurveFloat>(Blueprint->GeneratedClass, NAME_None, RF_Public | RF_Transactional);
		TrackIndex = Timeline->FloatTracks.Add(NewTrack);
		Timeline->AddDisplayTrack(FTTTrackId(FTTTrackBase::TT_FloatInterp, TrackIndex));
	}
	else if (!Timeline->FloatTracks[TrackIndex].CurveFloat)
	{
		Timeline->FloatTracks[TrackIndex].CurveFloat = NewObject<UCurveFloat>(Blueprint->GeneratedClass, NAME_None, RF_Public | RF_Transactional);
	}
	Timeline->FloatTracks[TrackIndex].CurveFloat->Modify();
	Timeline->FloatTracks[TrackIndex].CurveFloat->FloatCurve.Reset();
	for (const TPair<float, float>& Key : Keys)
	{
		Timeline->FloatTracks[TrackIndex].CurveFloat->FloatCurve.AddKey(Key.Key, Key.Value);
	}
	FinishMutation(Blueprint, Timeline);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("timeline_name"), Timeline->GetVariableName().ToString());
	Result->SetStringField(TEXT("track_name"), TrackNameText);
	Result->SetNumberField(TEXT("key_count"), Keys.Num());
	Result->SetObjectField(TEXT("timeline"), TimelineToJson(Blueprint, Timeline));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_RemoveBlueprintTimeline::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	FString TimelineName;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath) || !GetStringParam(Params, TEXT("timeline_name"), TimelineName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path or timeline_name"));
	}
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(FName(*TimelineName));
	if (!Timeline)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Timeline not found: %s"), *TimelineName));
	}
	FString TemplateName = Timeline->GetName();
	FString NodeId;
	if (UK2Node_Timeline* Node = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Timeline))
	{
		NodeId = Node->NodeGuid.ToString();
		Node->Modify();
		Node->DestroyNode();
	}
	FBlueprintEditorUtils::RemoveTimeline(Blueprint, Timeline, false);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Result->SetStringField(TEXT("timeline_name"), TimelineName);
	Result->SetStringField(TEXT("template_name"), TemplateName);
	if (!NodeId.IsEmpty())
	{
		Result->SetStringField(TEXT("node_id"), NodeId);
	}
	return FECACommandResult::Success(Result);
}
