// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_CreateBlueprintTimeline : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_blueprint_timeline"); }
	virtual FString GetDescription() const override { return TEXT("Create a Blueprint Timeline template and optional timeline node in a graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Timeline"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("timeline_name"), TEXT("string"), TEXT("Timeline variable name; if omitted UE generates a unique name"), false },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Graph to add the timeline node to"), false, TEXT("EventGraph") },
			{ TEXT("add_node"), TEXT("boolean"), TEXT("Add a UK2Node_Timeline to the graph"), false, TEXT("true") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position {x, y}"), false },
			{ TEXT("length"), TEXT("number"), TEXT("Timeline length in seconds"), false, TEXT("5.0") },
			{ TEXT("length_mode"), TEXT("string"), TEXT("timeline_length or last_keyframe"), false, TEXT("timeline_length") },
			{ TEXT("auto_play"), TEXT("boolean"), TEXT("Whether the timeline auto-plays"), false, TEXT("false") },
			{ TEXT("loop"), TEXT("boolean"), TEXT("Whether the timeline loops"), false, TEXT("false") },
			{ TEXT("replicated"), TEXT("boolean"), TEXT("Whether the timeline is replicated"), false, TEXT("false") },
			{ TEXT("ignore_time_dilation"), TEXT("boolean"), TEXT("Whether the timeline ignores global time dilation"), false, TEXT("false") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListBlueprintTimelines : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_blueprint_timelines"); }
	virtual FString GetDescription() const override { return TEXT("List Timeline templates and float tracks in a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Timeline"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return { { TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true } };
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetBlueprintTimelineSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_blueprint_timeline_settings"); }
	virtual FString GetDescription() const override { return TEXT("Set Timeline length, length mode, autoplay, loop, replication, and time dilation settings"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Timeline"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("timeline_name"), TEXT("string"), TEXT("Timeline variable name"), true },
			{ TEXT("length"), TEXT("number"), TEXT("Timeline length in seconds"), false },
			{ TEXT("length_mode"), TEXT("string"), TEXT("timeline_length or last_keyframe"), false },
			{ TEXT("auto_play"), TEXT("boolean"), TEXT("Whether the timeline auto-plays"), false },
			{ TEXT("loop"), TEXT("boolean"), TEXT("Whether the timeline loops"), false },
			{ TEXT("replicated"), TEXT("boolean"), TEXT("Whether the timeline is replicated"), false },
			{ TEXT("ignore_time_dilation"), TEXT("boolean"), TEXT("Whether the timeline ignores global time dilation"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_AddBlueprintTimelineFloatTrack : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_timeline_float_track"); }
	virtual FString GetDescription() const override { return TEXT("Add or replace a float track on a Blueprint Timeline. Keys are [{time,value}]."); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Timeline"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("timeline_name"), TEXT("string"), TEXT("Timeline variable name"), true },
			{ TEXT("track_name"), TEXT("string"), TEXT("Float track name / output pin name"), true },
			{ TEXT("keys"), TEXT("array"), TEXT("Array of key objects: {time, value}"), true },
			{ TEXT("replace_existing"), TEXT("boolean"), TEXT("Replace keys if the track already exists"), false, TEXT("false") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RemoveBlueprintTimeline : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_blueprint_timeline"); }
	virtual FString GetDescription() const override { return TEXT("Remove a Timeline template and its timeline node from a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Timeline"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("timeline_name"), TEXT("string"), TEXT("Timeline variable name"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};