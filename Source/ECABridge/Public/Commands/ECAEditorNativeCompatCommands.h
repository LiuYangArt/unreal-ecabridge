// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_CaptureAssetImage : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("capture_asset_image"); }
	virtual FString GetDescription() const override { return TEXT("Render an asset thumbnail and return it as an MCP image content block. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_CaptureEditorImage : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("capture_editor_image"); }
	virtual FString GetDescription() const override { return TEXT("Capture the full editor window as an MCP image content block. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_CaptureViewport : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("capture_viewport"); }
	virtual FString GetDescription() const override { return TEXT("Capture the active level editor viewport as an MCP image content block. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_FocusOnActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("focus_on_actors"); }
	virtual FString GetDescription() const override { return TEXT("Move the editor viewport camera to focus on one or more actors. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetCameraTransformNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_camera_transform"); }
	virtual FString GetDescription() const override { return TEXT("Return the active level editor viewport camera transform. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetCameraTransformNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_camera_transform"); }
	virtual FString GetDescription() const override { return TEXT("Set the active level editor viewport camera location and rotation. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetContentBrowserPath : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_content_browser_path"); }
	virtual FString GetDescription() const override { return TEXT("Return the active Content Browser path. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetContentBrowserPath : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_content_browser_path"); }
	virtual FString GetDescription() const override { return TEXT("Navigate the Content Browser to a folder path. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetSelectedAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_selected_assets"); }
	virtual FString GetDescription() const override { return TEXT("Return assets currently selected in the Content Browser. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SelectAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("select_assets"); }
	virtual FString GetDescription() const override { return TEXT("Select assets in the Content Browser by package path. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetVisibleActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_visible_actors"); }
	virtual FString GetDescription() const override { return TEXT("Return actors projected inside the active level viewport. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_OpenEditorForAssetNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("open_editor_for_asset"); }
	virtual FString GetDescription() const override { return TEXT("Open the asset editor for an asset path. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetOpenAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_open_assets"); }
	virtual FString GetDescription() const override { return TEXT("Return package paths for assets currently open in asset editors. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SearchCVarsNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("search_cvars"); }
	virtual FString GetDescription() const override { return TEXT("Search console variables by substring. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_WorldPosToScreenCoords : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("world_pos_to_screen_coords"); }
	virtual FString GetDescription() const override { return TEXT("Project a world location into normalized active viewport coordinates. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ScreenCoordsToWorld : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("screen_coords_to_world"); }
	virtual FString GetDescription() const override { return TEXT("Trace from normalized active viewport coordinates into the world. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_StartPIENativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("start_pie"); }
	virtual FString GetDescription() const override { return TEXT("Start a Play In Editor session. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_StopPIENativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("stop_pie"); }
	virtual FString GetDescription() const override { return TEXT("Stop the active Play In Editor session. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_IsPIERunningNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("is_pie_running"); }
	virtual FString GetDescription() const override { return TEXT("Return whether Play In Editor is currently running. Native EditorToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetLogEntriesNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_log_entries"); }
	virtual FString GetDescription() const override { return TEXT("Read matching lines from the current editor log. Native LogsToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetLogCategoriesNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_log_categories"); }
	virtual FString GetDescription() const override { return TEXT("List registered UE log categories. Native LogsToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetVerbosityNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_verbosity"); }
	virtual FString GetDescription() const override { return TEXT("Get the current verbosity for a UE log category. Native LogsToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetVerbosityNativeCompat : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_verbosity"); }
	virtual FString GetDescription() const override { return TEXT("Set the verbosity for a UE log category. Native LogsToolset compatibility command."); }
	virtual FString GetCategory() const override { return TEXT("EditorNativeCompat"); }
	virtual TArray<FECACommandParam> GetParameters() const override;
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};