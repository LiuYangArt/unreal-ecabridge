// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Dump a PCG (Procedural Content Generation) graph: all nodes, edges, parameters.
 */
class FECACommand_DumpPCGGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_pcg_graph"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a PCG graph to JSON: all nodes with class, title, inputs/outputs, edges, and graph parameters. Requires PCG plugin."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Content path to the PCGGraph asset"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path of the PCGGraph asset") },
			{ TEXT("nodes"),      TEXT("array"),  TEXT("Graph nodes: {id, class, title, inputs, outputs}"), TEXT("object") },
			{ TEXT("edges"),      TEXT("array"),  TEXT("Edges between node pins"), TEXT("object") },
			{ TEXT("parameters"), TEXT("array"),  TEXT("Graph-level parameters"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#if WITH_ECA_CONTROL_RIG
/**
 * Dump a Control Rig: hierarchy elements (bones, controls, nulls), rig graph, and settings.
 */
class FECACommand_DumpControlRig : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_control_rig"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a Control Rig asset: rig hierarchy (bones, controls, nulls, curves), graph nodes, variables. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"),  TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("hierarchy"), TEXT("array"),  TEXT("Rig elements: bones, controls, nulls, curves"), TEXT("object") },
			{ TEXT("graph"),     TEXT("object"), TEXT("Rig graph: nodes, links, variables") },
			{ TEXT("variables"), TEXT("array"),  TEXT("Rig variables: {name, type, default}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a null to a Control Rig hierarchy.
 */
class FECACommand_AddControlRigNull : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_control_rig_null"); }
	virtual FString GetDescription() const override { return TEXT("Add a null element to a Control Rig hierarchy. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("name"), TEXT("string"), TEXT("New null name"), true },
			{ TEXT("parent_name"), TEXT("string"), TEXT("Optional parent element name"), false },
			{ TEXT("parent_type"), TEXT("string"), TEXT("Optional parent type: bone, control, null, curve, socket"), false },
			{ TEXT("transform"), TEXT("object"), TEXT("Optional transform {location, rotation, scale}"), false },
			{ TEXT("transform_space"), TEXT("string"), TEXT("global (default) or local"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("element"), TEXT("object"), TEXT("Created element") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a control to a Control Rig hierarchy.
 */
class FECACommand_AddControlRigControl : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_control_rig_control"); }
	virtual FString GetDescription() const override { return TEXT("Add a control element to a Control Rig hierarchy. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("name"), TEXT("string"), TEXT("New control name"), true },
			{ TEXT("parent_name"), TEXT("string"), TEXT("Optional parent element name"), false },
			{ TEXT("parent_type"), TEXT("string"), TEXT("Optional parent type: bone, control, null, curve, socket"), false },
			{ TEXT("control_type"), TEXT("string"), TEXT("bool, float, integer, vector2d, position, scale, rotator, euler_transform"), false },
			{ TEXT("transform"), TEXT("object"), TEXT("Optional control offset transform {location, rotation, scale}"), false },
			{ TEXT("initial_value"), TEXT("object"), TEXT("Optional initial value matching control_type"), false },
			{ TEXT("display_name"), TEXT("string"), TEXT("Optional display name"), false },
			{ TEXT("shape_name"), TEXT("string"), TEXT("Optional shape name"), false },
			{ TEXT("shape_visible"), TEXT("boolean"), TEXT("Whether the shape is visible"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("element"), TEXT("object"), TEXT("Created element") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set an existing Control Rig hierarchy element transform.
 */
class FECACommand_SetControlRigElementTransform : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_control_rig_element_transform"); }
	virtual FString GetDescription() const override { return TEXT("Set local or global transform on a Control Rig hierarchy element. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("element_name"), TEXT("string"), TEXT("Element name"), true },
			{ TEXT("element_type"), TEXT("string"), TEXT("Optional type: bone, control, null, curve, socket"), false },
			{ TEXT("transform"), TEXT("object"), TEXT("Transform {location, rotation, scale}"), true },
			{ TEXT("transform_space"), TEXT("string"), TEXT("global (default) or local"), false },
			{ TEXT("affect_children"), TEXT("boolean"), TEXT("Whether children keep their relative transform"), false },
			{ TEXT("set_initial"), TEXT("boolean"), TEXT("Set initial transform instead of current"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("element"), TEXT("object"), TEXT("Updated element") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Rename a Control Rig hierarchy element.
 */
class FECACommand_RenameControlRigElement : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("rename_control_rig_element"); }
	virtual FString GetDescription() const override { return TEXT("Rename a Control Rig hierarchy element. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("element_name"), TEXT("string"), TEXT("Current element name"), true },
			{ TEXT("new_name"), TEXT("string"), TEXT("New element name"), true },
			{ TEXT("element_type"), TEXT("string"), TEXT("Optional type: bone, control, null, curve, socket"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("element"), TEXT("object"), TEXT("Renamed element") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a Control Rig hierarchy element.
 */
class FECACommand_RemoveControlRigElement : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_control_rig_element"); }
	virtual FString GetDescription() const override { return TEXT("Remove a Control Rig hierarchy element. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("element_name"), TEXT("string"), TEXT("Element name"), true },
			{ TEXT("element_type"), TEXT("string"), TEXT("Optional type: bone, control, null, curve, socket"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("removed"), TEXT("boolean"), TEXT("Whether the element was removed") },
			{ TEXT("element_name"), TEXT("string"), TEXT("Removed element name") },
			{ TEXT("element_type"), TEXT("string"), TEXT("Removed element type") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a RigVM unit node to a Control Rig graph.
 */
class FECACommand_AddControlRigUnitNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_control_rig_unit_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a RigVM unit node to a Control Rig graph. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("script_struct"), TEXT("string"), TEXT("RigVM unit script struct path or name"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Optional RigVM graph name; defaults to the main graph"), false },
			{ TEXT("method_name"), TEXT("string"), TEXT("RigVM method name; defaults to Execute"), false },
			{ TEXT("node_name"), TEXT("string"), TEXT("Optional node name"), false },
			{ TEXT("position"), TEXT("object"), TEXT("Optional graph position {x,y}"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Edited RigVM graph") },
			{ TEXT("node"), TEXT("object"), TEXT("Created RigVM node") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a RigVM node from a Control Rig graph.
 */
class FECACommand_RemoveControlRigGraphNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_control_rig_graph_node"); }
	virtual FString GetDescription() const override { return TEXT("Remove a RigVM node from a Control Rig graph. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("node_name"), TEXT("string"), TEXT("RigVM node name"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Optional RigVM graph name; defaults to the main graph"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Edited RigVM graph") },
			{ TEXT("removed"), TEXT("boolean"), TEXT("Whether the node was removed") },
			{ TEXT("node_name"), TEXT("string"), TEXT("Removed node name") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a RigVM pin default value in a Control Rig graph.
 */
class FECACommand_SetControlRigPinDefault : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_control_rig_pin_default"); }
	virtual FString GetDescription() const override { return TEXT("Set a RigVM pin default value in a Control Rig graph. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("pin_path"), TEXT("string"), TEXT("RigVM pin path, for example Node.Pin.SubPin"), true },
			{ TEXT("default_value"), TEXT("string"), TEXT("RigVM pin default value string"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Optional RigVM graph name; defaults to the main graph"), false },
			{ TEXT("resize_arrays"), TEXT("boolean"), TEXT("Whether array defaults may resize arrays"), false },
			{ TEXT("set_linked_pins"), TEXT("boolean"), TEXT("Whether linked pins receive the value too"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Edited RigVM graph") },
			{ TEXT("pin"), TEXT("object"), TEXT("Updated RigVM pin") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Connect two RigVM pins in a Control Rig graph.
 */
class FECACommand_ConnectControlRigPins : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_control_rig_pins"); }
	virtual FString GetDescription() const override { return TEXT("Connect two RigVM pins in a Control Rig graph. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("output_pin_path"), TEXT("string"), TEXT("Source/output RigVM pin path"), true },
			{ TEXT("input_pin_path"), TEXT("string"), TEXT("Target/input RigVM pin path"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Optional RigVM graph name; defaults to the main graph"), false },
			{ TEXT("create_cast_node"), TEXT("boolean"), TEXT("Whether RigVM may create a cast node"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Edited RigVM graph") },
			{ TEXT("link"), TEXT("object"), TEXT("Created RigVM link") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Disconnect two RigVM pins in a Control Rig graph.
 */
class FECACommand_DisconnectControlRigPins : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("disconnect_control_rig_pins"); }
	virtual FString GetDescription() const override { return TEXT("Disconnect two RigVM pins in a Control Rig graph. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true },
			{ TEXT("output_pin_path"), TEXT("string"), TEXT("Source/output RigVM pin path"), true },
			{ TEXT("input_pin_path"), TEXT("string"), TEXT("Target/input RigVM pin path"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Optional RigVM graph name; defaults to the main graph"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"), TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Edited RigVM graph") },
			{ TEXT("disconnected"), TEXT("boolean"), TEXT("Whether the link was disconnected") },
			{ TEXT("output_pin_path"), TEXT("string"), TEXT("Disconnected source pin") },
			{ TEXT("input_pin_path"), TEXT("string"), TEXT("Disconnected target pin") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
#endif // WITH_ECA_CONTROL_RIG

#if WITH_ECA_GAMEPLAY_ABILITIES
/**
 * Dump Gameplay Ability System data: abilities, effects, attribute sets, tags.
 */
class FECACommand_DumpGameplayAbility : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_gameplay_ability"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a GameplayAbility, GameplayEffect, or AttributeSet blueprint: tags, cooldowns, costs, modifiers, granted abilities. Requires GameplayAbilities plugin."); }
	virtual FString GetCategory() const override { return TEXT("GAS"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to a GameplayAbility, GameplayEffect, or AttributeSet blueprint"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("asset_path"),  TEXT("string"), TEXT("Path of the GAS asset") },
			{ TEXT("asset_class"), TEXT("string"), TEXT("Specific class (GameplayAbility, GameplayEffect, AttributeSet)") },
			{ TEXT("tags"),        TEXT("array"),  TEXT("Gameplay tags relevant to this asset"), TEXT("string") },
			{ TEXT("modifiers"),   TEXT("array"),  TEXT("Effect modifiers (for GameplayEffect)"), TEXT("object") },
			{ TEXT("attributes"),  TEXT("array"),  TEXT("Attribute definitions (for AttributeSet)"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
#endif // WITH_ECA_GAMEPLAY_ABILITIES
