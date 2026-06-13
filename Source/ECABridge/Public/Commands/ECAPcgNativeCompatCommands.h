// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_GetPCGGraphStructure : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_graph_structure"); }
	virtual FString GetDescription() const override { return TEXT("Return a PCG graph structure: description, nodes, pins, edges, parameters, and comment boxes."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetPCGGraphSchema : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_graph_schema"); }
	virtual FString GetDescription() const override { return TEXT("Return PCG graph user-parameter schema and input/output pin schemas."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetPCGGraphDescription : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_graph_description"); }
	virtual FString GetDescription() const override { return TEXT("Return a PCG graph description text."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetPCGGraphDescription : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_pcg_graph_description"); }
	virtual FString GetDescription() const override { return TEXT("Set a PCG graph description text."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("description"), TEXT("string"), TEXT("New description"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListPCGNativeNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_pcg_native_nodes"); }
	virtual FString GetDescription() const override { return TEXT("List native UPCGSettings node classes, optionally limited to common node families."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("common_only"), TEXT("boolean"), TEXT("Filter to common node families; default true"), false, TEXT("true") }, { TEXT("name_filter"), TEXT("string"), TEXT("Optional class-name substring"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetPCGNativeNodeSchema : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_native_node_schema"); }
	virtual FString GetDescription() const override { return TEXT("Return a UPCGSettings class schema: editable properties and default pins."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("settings_class"), TEXT("string"), TEXT("UPCGSettings class name"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListPCGAvailableSubgraphs : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_pcg_available_subgraphs"); }
	virtual FString GetDescription() const override { return TEXT("List PCG graph assets usable as subgraphs."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("path_filter"), TEXT("string"), TEXT("Optional package path substring"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_AddPCGSubgraphNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_pcg_subgraph_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a subgraph node to a PCG graph."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("Parent PCGGraph path"), true }, { TEXT("subgraph_path"), TEXT("string"), TEXT("Subgraph PCGGraph path"), true }, { TEXT("node_id"), TEXT("string"), TEXT("Optional node object name"), false }, { TEXT("node_title"), TEXT("string"), TEXT("Optional title"), false }, { TEXT("node_comment"), TEXT("string"), TEXT("Optional comment"), false }, { TEXT("position_x"), TEXT("number"), TEXT("Editor X position"), false, TEXT("0") }, { TEXT("position_y"), TEXT("number"), TEXT("Editor Y position"), false, TEXT("0") }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_UpdatePCGNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("update_pcg_node"); }
	virtual FString GetDescription() const override { return TEXT("Update a PCG node title and/or multiple settings properties from JSON."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("node_id"), TEXT("string"), TEXT("Node object name"), true }, { TEXT("node_title"), TEXT("string"), TEXT("Optional node title"), false }, { TEXT("properties"), TEXT("object"), TEXT("Optional property map"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetPCGNodeInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_node_info"); }
	virtual FString GetDescription() const override { return TEXT("Return PCG node identity, position, pins, settings class, title, and comment."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("node_id"), TEXT("string"), TEXT("Node object name"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RepositionPCGNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("reposition_pcg_node"); }
	virtual FString GetDescription() const override { return TEXT("Move a PCG node in the graph editor layout."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("node_id"), TEXT("string"), TEXT("Node object name"), true }, { TEXT("position_x"), TEXT("number"), TEXT("Editor X position"), true }, { TEXT("position_y"), TEXT("number"), TEXT("Editor Y position"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetPCGNodeComment : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_pcg_node_comment"); }
	virtual FString GetDescription() const override { return TEXT("Set a PCG node comment bubble."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("node_id"), TEXT("string"), TEXT("Node object name"), true }, { TEXT("comment"), TEXT("string"), TEXT("Comment text; empty clears"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RemovePCGNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_pcg_node"); }
	virtual FString GetDescription() const override { return TEXT("Remove a PCG graph node and its edges."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("node_id"), TEXT("string"), TEXT("Node object name"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_DisconnectPCGNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("disconnect_pcg_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Remove an edge between two PCG node pins."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("from_node_id"), TEXT("string"), TEXT("Upstream node"), true }, { TEXT("to_node_id"), TEXT("string"), TEXT("Downstream node"), true }, { TEXT("from_pin"), TEXT("string"), TEXT("Output pin label"), false }, { TEXT("to_pin"), TEXT("string"), TEXT("Input pin label"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_AddPCGCommentBox : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_pcg_comment_box"); }
	virtual FString GetDescription() const override { return TEXT("Add a PCG graph comment box around nodes."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("node_ids"), TEXT("array"), TEXT("Node ids to enclose"), true }, { TEXT("comment"), TEXT("string"), TEXT("Comment text"), true }, { TEXT("color"), TEXT("object"), TEXT("Optional {r,g,b,a}"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_UpdatePCGCommentBox : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("update_pcg_comment_box"); }
	virtual FString GetDescription() const override { return TEXT("Update a PCG graph comment box text, color, or bounds from nodes."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("comment_id"), TEXT("string"), TEXT("Comment GUID"), true }, { TEXT("node_ids"), TEXT("array"), TEXT("Optional replacement node ids"), false }, { TEXT("comment"), TEXT("string"), TEXT("Optional text"), false }, { TEXT("color"), TEXT("object"), TEXT("Optional {r,g,b,a}"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RemovePCGCommentBox : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_pcg_comment_box"); }
	virtual FString GetDescription() const override { return TEXT("Remove a PCG graph comment box by GUID."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("comment_id"), TEXT("string"), TEXT("Comment GUID"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListPCGGraphInstances : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_pcg_graph_instances"); }
	virtual FString GetDescription() const override { return TEXT("List actors with PCG graph instances in the editor world."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SpawnPCGGraphInstance : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_pcg_graph_instance"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a PCGVolume actor and assign a PCG graph."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("actor_name"), TEXT("string"), TEXT("Actor label"), true }, { TEXT("location"), TEXT("object"), TEXT("Optional {x,y,z}"), false }, { TEXT("rotation"), TEXT("object"), TEXT("Optional {pitch,yaw,roll}"), false }, { TEXT("scale"), TEXT("object"), TEXT("Optional {x,y,z}"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetPCGGraphInstanceParams : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_graph_instance_params"); }
	virtual FString GetDescription() const override { return TEXT("Return graph parameter overrides on a PCG actor instance."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("actor_name"), TEXT("string"), TEXT("PCG actor label"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetPCGGraphInstanceParams : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_pcg_graph_instance_params"); }
	virtual FString GetDescription() const override { return TEXT("Set graph parameter overrides on a PCG actor instance."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("actor_name"), TEXT("string"), TEXT("PCG actor label"), true }, { TEXT("params"), TEXT("object"), TEXT("Parameter value map"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ResetPCGGraphInstanceParams : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("reset_pcg_graph_instance_params"); }
	virtual FString GetDescription() const override { return TEXT("Clear selected graph parameter overrides on a PCG actor instance."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("actor_name"), TEXT("string"), TEXT("PCG actor label"), true }, { TEXT("param_names"), TEXT("array"), TEXT("Parameter names to reset"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ExecutePCGGraphInstance : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("execute_pcg_graph_instance"); }
	virtual FString GetDescription() const override { return TEXT("Generate a PCG actor instance and report output data summary."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("actor_name"), TEXT("string"), TEXT("PCG actor label"), true }, { TEXT("force"), TEXT("boolean"), TEXT("Force regeneration"), false, TEXT("false") }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};


class FECACommand_RunPCGInstantGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("run_pcg_instant_graph"); }
	virtual FString GetDescription() const override { return TEXT("Run a PCG graph as a transient instant graph with optional parameter overrides. UE 5.8+ schedules through the native PCG execution source."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path"), true }, { TEXT("params"), TEXT("object"), TEXT("Optional graph parameter overrides"), false }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_DrawPCGSpline : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("draw_pcg_spline"); }
	virtual FString GetDescription() const override { return TEXT("Create or redraw a spline actor for PCG workflows from explicit points. This is the agent-native equivalent of the official interactive DrawSpline tool."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("actor_label"), TEXT("string"), TEXT("Actor label to create or redraw"), true }, { TEXT("actor_tag"), TEXT("string"), TEXT("Optional actor tag"), false }, { TEXT("redraw"), TEXT("boolean"), TEXT("Replace an existing actor's spline points"), false, TEXT("false") }, { TEXT("closed_spline"), TEXT("boolean"), TEXT("Whether the spline is closed"), false, TEXT("false") }, { TEXT("points"), TEXT("array"), TEXT("Spline points as {x,y,z} objects"), true }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetPCGNodeDataView : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_node_data_view"); }
	virtual FString GetDescription() const override { return TEXT("Return a compact data view for generated PCG output; supports point ranges and attribute/property names when available."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return {{ TEXT("actor_name"), TEXT("string"), TEXT("PCG actor label"), true }, { TEXT("pin"), TEXT("string"), TEXT("Optional output pin filter"), false }, { TEXT("attribute_name"), TEXT("string"), TEXT("Optional point property/attribute filter"), false }, { TEXT("start_index"), TEXT("number"), TEXT("Start index"), false, TEXT("0") }, { TEXT("end_index"), TEXT("number"), TEXT("Exclusive end; -1 means all"), false, TEXT("-1") }}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};