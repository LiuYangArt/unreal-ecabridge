// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── play_animation ───────────────────────────────────────────
// Play an animation asset (UAnimSequence or UAnimMontage) on an actor's
// SkeletalMeshComponent.
class FECACommand_PlayAnimation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("play_animation"); }
	virtual FString GetDescription() const override { return TEXT("Play an animation asset on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true },
			{ TEXT("animation_path"), TEXT("string"), TEXT("Asset path to a UAnimSequence or UAnimMontage (e.g. /Game/Animations/Walk)"), true },
			{ TEXT("loop"), TEXT("boolean"), TEXT("Whether to loop the animation (default false)"), false, TEXT("false") },
			{ TEXT("slot_name"), TEXT("string"), TEXT("Slot name for montage playback (optional, e.g. DefaultSlot)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── stop_animation ───────────────────────────────────────────
// Stop any playing animation on an actor's SkeletalMeshComponent.
class FECACommand_StopAnimation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("stop_animation"); }
	virtual FString GetDescription() const override { return TEXT("Stop any playing animation on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_actor_animations ─────────────────────────────────────
// List all animation assets compatible with an actor's skeleton.
class FECACommand_GetActorAnimations : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_actor_animations"); }
	virtual FString GetDescription() const override { return TEXT("List all animation assets compatible with an actor's skeleton"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_animation_blueprint ──────────────────────────────────
// Set the Animation Blueprint on an actor's SkeletalMeshComponent.
class FECACommand_SetAnimationBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_animation_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Set the Animation Blueprint on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true },
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint (e.g. /Game/Animations/ABP_Character)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_skeleton_info ────────────────────────────────────────
// Get bone hierarchy and morph target info for a skeletal mesh.
class FECACommand_GetSkeletonInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_skeleton_info"); }
	virtual FString GetDescription() const override { return TEXT("Get skeleton info: bone names, bone count, morph targets for a skeletal mesh"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of an actor with a SkeletalMeshComponent (optional if mesh_path provided)"), false },
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Asset path to a USkeletalMesh (optional if actor_name provided)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_skeletal_mesh ──────────────────────────────────────
// Set a skeletal mesh on a SkeletalMeshActor or any actor with a
// SkeletalMeshComponent.
class FECACommand_SetSkeletalMesh : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_skeletal_mesh"); }
	virtual FString GetDescription() const override { return TEXT("Set a skeletal mesh on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true },
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Asset path to a USkeletalMesh (e.g. /Game/Characters/SK_Mannequin)"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Specific SkeletalMeshComponent to target (optional, uses first found if omitted)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── dump_animation_blueprint ────────────────────────────────
// Serialize an Animation Blueprint's state machine, states, transitions, and blend spaces.
class FECACommand_DumpAnimationBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_animation_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Serialize an Animation Blueprint to JSON: state machines with states and transitions, blend spaces, variables, and the AnimGraph structure. Makes any AnimBP fully legible."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- list_anim_bp_state_machines ------------------------------------------
class FECACommand_ListAnimBPStateMachines : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_anim_bp_state_machines"); }
	virtual FString GetDescription() const override { return TEXT("List Animation Blueprint state machines, states, transitions, and entry state."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override { return { { TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true } }; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- add_anim_bp_state -----------------------------------------------------
class FECACommand_AddAnimBPState : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_anim_bp_state"); }
	virtual FString GetDescription() const override { return TEXT("Add a state to an Animation Blueprint state machine. Optionally seeds the state with an animation asset."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true },
			{ TEXT("state_machine_name"), TEXT("string"), TEXT("State machine graph name or state machine node title"), true },
			{ TEXT("state_name"), TEXT("string"), TEXT("New state name"), true },
			{ TEXT("x"), TEXT("number"), TEXT("Graph X position"), false, TEXT("0") },
			{ TEXT("y"), TEXT("number"), TEXT("Graph Y position"), false, TEXT("0") },
			{ TEXT("animation_asset_path"), TEXT("string"), TEXT("Optional UAnimationAsset path to place inside the new state's graph"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- delete_anim_bp_state --------------------------------------------------
class FECACommand_DeleteAnimBPState : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_anim_bp_state"); }
	virtual FString GetDescription() const override { return TEXT("Delete a state from an Animation Blueprint state machine."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true },
			{ TEXT("state_machine_name"), TEXT("string"), TEXT("State machine graph name or state machine node title"), true },
			{ TEXT("state_name"), TEXT("string"), TEXT("State to delete"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- set_anim_bp_state_position -------------------------------------------
class FECACommand_SetAnimBPStatePosition : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_anim_bp_state_position"); }
	virtual FString GetDescription() const override { return TEXT("Set a state node's position in an Animation Blueprint state machine graph."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true },
			{ TEXT("state_machine_name"), TEXT("string"), TEXT("State machine graph name or state machine node title"), true },
			{ TEXT("state_name"), TEXT("string"), TEXT("State to move"), true },
			{ TEXT("x"), TEXT("number"), TEXT("Graph X position"), true },
			{ TEXT("y"), TEXT("number"), TEXT("Graph Y position"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- set_anim_bp_entry_state ----------------------------------------------
class FECACommand_SetAnimBPEntryState : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_anim_bp_entry_state"); }
	virtual FString GetDescription() const override { return TEXT("Set the entry state for an Animation Blueprint state machine."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true },
			{ TEXT("state_machine_name"), TEXT("string"), TEXT("State machine graph name or state machine node title"), true },
			{ TEXT("state_name"), TEXT("string"), TEXT("State to use as entry"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- add_anim_bp_transition ------------------------------------------------
class FECACommand_AddAnimBPTransition : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_anim_bp_transition"); }
	virtual FString GetDescription() const override { return TEXT("Add a transition between two states in an Animation Blueprint state machine."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true },
			{ TEXT("state_machine_name"), TEXT("string"), TEXT("State machine graph name or state machine node title"), true },
			{ TEXT("source_state"), TEXT("string"), TEXT("Source state name"), true },
			{ TEXT("target_state"), TEXT("string"), TEXT("Target state name"), true },
			{ TEXT("crossfade_duration"), TEXT("number"), TEXT("Transition crossfade duration in seconds"), false },
			{ TEXT("priority_order"), TEXT("number"), TEXT("Transition priority order"), false },
			{ TEXT("bidirectional"), TEXT("boolean"), TEXT("Whether this transition can go both directions"), false },
			{ TEXT("disabled"), TEXT("boolean"), TEXT("Whether this transition is disabled"), false },
			{ TEXT("automatic_rule_based_on_sequence_player"), TEXT("boolean"), TEXT("Use automatic rule based on most relevant sequence player"), false },
			{ TEXT("automatic_rule_trigger_time"), TEXT("number"), TEXT("Automatic rule trigger time in seconds"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- set_anim_bp_transition ------------------------------------------------
class FECACommand_SetAnimBPTransition : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_anim_bp_transition"); }
	virtual FString GetDescription() const override { return TEXT("Update properties on an existing Animation Blueprint transition."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true },
			{ TEXT("state_machine_name"), TEXT("string"), TEXT("State machine graph name or state machine node title"), true },
			{ TEXT("source_state"), TEXT("string"), TEXT("Source state name"), true },
			{ TEXT("target_state"), TEXT("string"), TEXT("Target state name"), true },
			{ TEXT("transition_index"), TEXT("number"), TEXT("Zero-based match index when multiple transitions share the same endpoints"), false, TEXT("0") },
			{ TEXT("crossfade_duration"), TEXT("number"), TEXT("Transition crossfade duration in seconds"), false },
			{ TEXT("priority_order"), TEXT("number"), TEXT("Transition priority order"), false },
			{ TEXT("bidirectional"), TEXT("boolean"), TEXT("Whether this transition can go both directions"), false },
			{ TEXT("disabled"), TEXT("boolean"), TEXT("Whether this transition is disabled"), false },
			{ TEXT("automatic_rule_based_on_sequence_player"), TEXT("boolean"), TEXT("Use automatic rule based on most relevant sequence player"), false },
			{ TEXT("automatic_rule_trigger_time"), TEXT("number"), TEXT("Automatic rule trigger time in seconds"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// --- delete_anim_bp_transition --------------------------------------------
class FECACommand_DeleteAnimBPTransition : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_anim_bp_transition"); }
	virtual FString GetDescription() const override { return TEXT("Delete a transition between two states in an Animation Blueprint state machine."); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint"), true },
			{ TEXT("state_machine_name"), TEXT("string"), TEXT("State machine graph name or state machine node title"), true },
			{ TEXT("source_state"), TEXT("string"), TEXT("Source state name"), true },
			{ TEXT("target_state"), TEXT("string"), TEXT("Target state name"), true },
			{ TEXT("transition_index"), TEXT("number"), TEXT("Zero-based match index when multiple transitions share the same endpoints"), false, TEXT("0") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
// ─── create_animation_sequence ───────────────────────────────
// Create a new UAnimSequence asset with programmatic bone keyframes.
class FECACommand_CreateAnimationSequence : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_animation_sequence"); }
	virtual FString GetDescription() const override { return TEXT("Create a new UAnimSequence asset with programmatic bone keyframes"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("package_path"), TEXT("string"), TEXT("Package path for the new asset (e.g. /Game/Animations)"), true },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the new animation asset (e.g. Anim_Wave)"), true },
			{ TEXT("skeleton_path"), TEXT("string"), TEXT("Asset path to the USkeleton (from get_skeleton_info)"), true },
			{ TEXT("frame_count"), TEXT("number"), TEXT("Total number of frames in the animation"), true },
			{ TEXT("frame_rate"), TEXT("number"), TEXT("Frames per second (default 30)"), false, TEXT("30") },
			{ TEXT("bone_tracks"), TEXT("array"), TEXT("Array of bone track objects, each with: bone_name (string), keys (array of {frame, location?, rotation?, scale?})"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
