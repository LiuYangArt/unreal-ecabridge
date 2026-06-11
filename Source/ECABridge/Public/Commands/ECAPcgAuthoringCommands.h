// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new UPCGGraph asset at a content path.
 */
class FECACommand_CreatePCGGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_pcg_graph"); }
	virtual FString GetDescription() const override { return TEXT("Create a new PCG graph asset at the given /Game path. Returns the saved asset path."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Full content path, e.g. /Game/MyFolder/MyPCGGraph"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("asset_path"), TEXT("string"), TEXT("Saved PCGGraph asset path") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Asset name (last path component)") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a PCG node (settings class instance) to a graph.
 */
class FECACommand_AddPCGNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_pcg_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a new PCG node of the given settings class to a graph. Returns the new node_id (stable name within the graph). Use list_pcg_node_types to discover settings classes."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"),    TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("settings_class"), TEXT("string"), TEXT("Class name of the UPCGSettings subclass (e.g. 'PCGCreatePointsSettings')"), true },
			{ TEXT("position_x"),    TEXT("number"), TEXT("Editor X position; default 0"), false, TEXT("0") },
			{ TEXT("position_y"),    TEXT("number"), TEXT("Editor Y position; default 0"), false, TEXT("0") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"),     TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("node_id"),        TEXT("string"), TEXT("Stable name of the new node within the graph") },
			{ TEXT("settings_class"), TEXT("string"), TEXT("Resolved settings class name") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Connect two pins between two nodes inside a PCG graph.
 */
class FECACommand_ConnectPCGNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_pcg_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Add an edge from one PCG node's output pin to another node's input pin. Default pins are inferred when from_pin/to_pin are omitted."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"),   TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("from_node_id"), TEXT("string"), TEXT("Stable name of the upstream node"), true },
			{ TEXT("to_node_id"),   TEXT("string"), TEXT("Stable name of the downstream node"), true },
			{ TEXT("from_pin"),     TEXT("string"), TEXT("Output pin label on the upstream node; if omitted uses the first output pin"), false },
			{ TEXT("to_pin"),       TEXT("string"), TEXT("Input pin label on the downstream node; if omitted uses the first input pin"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"),   TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("from_node_id"), TEXT("string"), TEXT("Upstream node id") },
			{ TEXT("from_pin"),     TEXT("string"), TEXT("Resolved upstream pin label") },
			{ TEXT("to_node_id"),   TEXT("string"), TEXT("Downstream node id") },
			{ TEXT("to_pin"),       TEXT("string"), TEXT("Resolved downstream pin label") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Auto-layout a PCG graph with the shared layered graph layout engine.
 */
class FECACommand_AutoLayoutPCGGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("auto_layout_pcg_graph"); }
	virtual FString GetDescription() const override { return TEXT("Automatically arrange PCG graph nodes with the shared layered graph layout engine."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("spacing_x"), TEXT("number"), TEXT("Horizontal rank spacing between nodes (default: 320)"), false, TEXT("320") },
			{ TEXT("spacing_y"), TEXT("number"), TEXT("Vertical spacing within ranks (default: 160)"), false, TEXT("160") },
			{ TEXT("node_ids"), TEXT("array"), TEXT("Specific node IDs to layout (optional, layouts all if not specified)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a property by name on a PCG node's settings object using UE reflection.
 */
class FECACommand_SetPCGNodeProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_pcg_node_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property on a PCG node's settings object using UE reflection. Supports scalars (string/number/bool), FVector, FName. The value is passed as JSON of the appropriate shape."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"),    TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("node_id"),       TEXT("string"), TEXT("Stable name of the target node"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("FProperty name on the settings UObject"), true },
			{ TEXT("value"),         TEXT("object"), TEXT("Value to assign. JSON shape depends on property type."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"),    TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("node_id"),       TEXT("string"), TEXT("Target node id") },
			{ TEXT("property_name"), TEXT("string"), TEXT("Property modified") },
			{ TEXT("property_type"), TEXT("string"), TEXT("Resolved C++ type") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
/**
 * Add a user parameter to a PCG graph's UserParameters property bag.
 */
class FECACommand_AddPCGGraphParameter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_pcg_graph_parameter"); }
	virtual FString GetDescription() const override { return TEXT("Add or overwrite a PCG graph user parameter. Supports scalar, FVector/FRotator/FTransform, object/static_mesh, soft_object, class, and arrays."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("type"), TEXT("string"), TEXT("Parameter type: bool, int32, int64, float, double, name, string, text, vector, rotator, transform, object, static_mesh, soft_object, class"), true },
			{ TEXT("container"), TEXT("string"), TEXT("Container type: none or array"), false, TEXT("none") },
			{ TEXT("object_class"), TEXT("string"), TEXT("Optional UObject/UClass type for object, soft_object, and class parameters"), false },
			{ TEXT("overwrite"), TEXT("boolean"), TEXT("Whether to replace an existing parameter with the same name"), false, TEXT("false") },
			{ TEXT("value"), TEXT("object"), TEXT("Optional default value. Arrays expect a JSON array."), false },
			{ TEXT("tooltip"), TEXT("string"), TEXT("Developer tooltip shown for the parameter"), false },
			{ TEXT("description"), TEXT("string"), TEXT("Alias for tooltip"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter added") },
			{ TEXT("parameters"), TEXT("array"), TEXT("Updated parameter list"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a PCG graph user parameter tooltip.
 */
class FECACommand_SetPCGGraphParameterTooltip : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_pcg_graph_parameter_tooltip"); }
	virtual FString GetDescription() const override { return TEXT("Set a PCG graph user parameter tooltip. Accepts tooltip or description."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("tooltip"), TEXT("string"), TEXT("Developer tooltip shown for the parameter"), false },
			{ TEXT("description"), TEXT("string"), TEXT("Alias for tooltip"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter modified") },
			{ TEXT("tooltip"), TEXT("string"), TEXT("Tooltip applied") },
			{ TEXT("parameters"), TEXT("array"), TEXT("Updated parameter list"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Rename a user parameter in a PCG graph's UserParameters property bag.
 */
class FECACommand_RenamePCGGraphParameter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("rename_pcg_graph_parameter"); }
	virtual FString GetDescription() const override { return TEXT("Rename a PCG graph user parameter while preserving its type and value where the engine can migrate it."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Existing parameter name"), true },
			{ TEXT("new_name"), TEXT("string"), TEXT("New parameter name"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("name"), TEXT("string"), TEXT("Old parameter name") },
			{ TEXT("new_name"), TEXT("string"), TEXT("New parameter name") },
			{ TEXT("parameters"), TEXT("array"), TEXT("Updated parameter list"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a user parameter from a PCG graph's UserParameters property bag.
 */
class FECACommand_RemovePCGGraphParameter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_pcg_graph_parameter"); }
	virtual FString GetDescription() const override { return TEXT("Remove a PCG graph user parameter by name."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter name to remove"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter removed") },
			{ TEXT("parameters"), TEXT("array"), TEXT("Updated parameter list"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a default value on an existing PCG graph user parameter.
 */
class FECACommand_SetPCGGraphParameterDefault : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_pcg_graph_parameter_default"); }
	virtual FString GetDescription() const override { return TEXT("Set the default value for an existing PCG graph user parameter. Arrays expect a JSON array."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("value"), TEXT("object"), TEXT("Default value to assign. Arrays expect a JSON array."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("name"), TEXT("string"), TEXT("Parameter updated") },
			{ TEXT("parameters"), TEXT("array"), TEXT("Updated parameter list"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
