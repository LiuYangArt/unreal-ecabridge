// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAEditorNativeCompatCommands.h"
#include "Commands/ECAAssetCommands.h"
#include "Commands/ECAEditorCommands.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "IAssetViewport.h"
#include "IContentBrowserSingleton.h"
#include "Internationalization/Regex.h"
#include "LevelEditor.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"

REGISTER_ECA_COMMAND(FECACommand_CaptureAssetImage)
REGISTER_ECA_COMMAND(FECACommand_CaptureEditorImage)
REGISTER_ECA_COMMAND(FECACommand_CaptureViewport)
REGISTER_ECA_COMMAND(FECACommand_FocusOnActors)
REGISTER_ECA_COMMAND(FECACommand_GetCameraTransformNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_SetCameraTransformNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_GetContentBrowserPath)
REGISTER_ECA_COMMAND(FECACommand_SetContentBrowserPath)
REGISTER_ECA_COMMAND(FECACommand_GetSelectedAssets)
REGISTER_ECA_COMMAND(FECACommand_SelectAssets)
REGISTER_ECA_COMMAND(FECACommand_GetVisibleActors)
REGISTER_ECA_COMMAND(FECACommand_OpenEditorForAssetNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_GetOpenAssets)
REGISTER_ECA_COMMAND(FECACommand_SearchCVarsNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_WorldPosToScreenCoords)
REGISTER_ECA_COMMAND(FECACommand_ScreenCoordsToWorld)
REGISTER_ECA_COMMAND(FECACommand_StartPIENativeCompat)
REGISTER_ECA_COMMAND(FECACommand_StopPIENativeCompat)
REGISTER_ECA_COMMAND(FECACommand_IsPIERunningNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_GetLogEntriesNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_GetLogCategoriesNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_GetVerbosityNativeCompat)
REGISTER_ECA_COMMAND(FECACommand_SetVerbosityNativeCompat)

namespace
{
TSharedPtr<FJsonObject> VecJson(const FVector& V)
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("x"), V.X);
	O->SetNumberField(TEXT("y"), V.Y);
	O->SetNumberField(TEXT("z"), V.Z);
	return O;
}

TSharedPtr<FJsonObject> RotJson(const FRotator& R)
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("pitch"), R.Pitch);
	O->SetNumberField(TEXT("yaw"), R.Yaw);
	O->SetNumberField(TEXT("roll"), R.Roll);
	return O;
}

bool JsonVector(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, FVector& Out)
{
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (!Params.IsValid() || !Params->TryGetObjectField(Name, Obj) || !Obj || !Obj->IsValid())
		return false;
	double X = 0, Y = 0, Z = 0;
	if (!(*Obj)->TryGetNumberField(TEXT("x"), X) || !(*Obj)->TryGetNumberField(TEXT("y"), Y) || !(*Obj)->TryGetNumberField(TEXT("z"), Z))
		return false;
	Out = FVector(X, Y, Z);
	return true;
}

bool JsonRotator(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, FRotator& Out)
{
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (!Params.IsValid() || !Params->TryGetObjectField(Name, Obj) || !Obj || !Obj->IsValid())
		return false;
	double Pitch = 0, Yaw = 0, Roll = 0;
	if (!(*Obj)->TryGetNumberField(TEXT("pitch"), Pitch) || !(*Obj)->TryGetNumberField(TEXT("yaw"), Yaw))
		return false;
	(*Obj)->TryGetNumberField(TEXT("roll"), Roll);
	Out = FRotator(Pitch, Yaw, Roll);
	return true;
}

bool JsonVector2D(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, FVector2D& Out)
{
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (!Params.IsValid() || !Params->TryGetObjectField(Name, Obj) || !Obj || !Obj->IsValid())
		return false;
	double X = 0, Y = 0;
	if (!(*Obj)->TryGetNumberField(TEXT("x"), X) || !(*Obj)->TryGetNumberField(TEXT("y"), Y))
		return false;
	Out = FVector2D(X, Y);
	return true;
}

TArray<FString> JsonStringArray(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name)
{
	TArray<FString> Out;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Params.IsValid() && Params->TryGetArrayField(Name, Array) && Array)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Array)
			Out.Add(Value->AsString());
	}
	return Out;
}

FEditorViewportClient* ViewClient()
{
	FLevelEditorModule* M = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> V = M ? M->GetFirstActiveViewport() : nullptr;
	return V.IsValid() ? &V->GetAssetViewportClient() : nullptr;
}

FViewport* ActiveViewport()
{
	FLevelEditorModule* M = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> V = M ? M->GetFirstActiveViewport() : nullptr;
	return V.IsValid() ? V->GetActiveViewport() : nullptr;
}

UWorld* EditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

IContentBrowserSingleton* ContentBrowser()
{
	FContentBrowserModule* M = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
	return M ? &M->Get() : nullptr;
}

AActor* ActorByNameOrPath(const FString& Value)
{
	if (AActor* ByPath = FindObject<AActor>(nullptr, *Value))
		return ByPath;
	UWorld* World = EditorWorld();
	if (!World)
		return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && (A->GetActorLabel().Equals(Value, ESearchCase::IgnoreCase) || A->GetName().Equals(Value, ESearchCase::IgnoreCase)))
			return A;
	}
	return nullptr;
}

TSharedPtr<FJsonObject> ActorJson(AActor* A)
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("name"), A->GetActorLabel());
	O->SetStringField(TEXT("path"), A->GetPathName());
	O->SetStringField(TEXT("class"), A->GetClass()->GetName());
	O->SetObjectField(TEXT("location"), VecJson(A->GetActorLocation()));
	O->SetObjectField(TEXT("rotation"), RotJson(A->GetActorRotation()));
	O->SetObjectField(TEXT("scale"), VecJson(A->GetActorScale3D()));
	return O;
}

bool Project(FEditorViewportClient* VC, FViewport* VP, const FVector& P, FVector2D& Out, double& Depth)
{
	if (!VC || !VP)
		return false;
	const FIntPoint Size = VP->GetSizeXY();
	if (Size.X <= 0 || Size.Y <= 0)
		return false;
	const FRotator R = VC->GetViewRotation();
	const FVector D = P - VC->GetViewLocation();
	const FVector F = R.Vector();
	const FVector Right = FRotationMatrix(R).GetScaledAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(R).GetScaledAxis(EAxis::Z);
	Depth = FVector::DotProduct(D, F);
	if (Depth <= KINDA_SMALL_NUMBER)
		return false;
	const double HalfH = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.f, VC->ViewFOV) * 0.5f)) * Depth;
	const double HalfW = HalfH * (double(Size.X) / double(Size.Y));
	Out = FVector2D(0.5 + FVector::DotProduct(D, Right) / HalfW * 0.5, 0.5 - FVector::DotProduct(D, Up) / HalfH * 0.5);
	return true;
}

FVector ScreenRay(FEditorViewportClient* VC, FViewport* VP, const FVector2D& Coords)
{
	const FIntPoint Size = VP ? VP->GetSizeXY() : FIntPoint(1, 1);
	const double Aspect = Size.Y > 0 ? double(Size.X) / double(Size.Y) : 1.0;
	const FRotator R = VC->GetViewRotation();
	const double T = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.f, VC->ViewFOV) * 0.5f));
	return (R.Vector() + FRotationMatrix(R).GetScaledAxis(EAxis::Y) * ((Coords.X - 0.5) * 2.0 * T * Aspect) + FRotationMatrix(R).GetScaledAxis(EAxis::Z) * ((0.5 - Coords.Y) * 2.0 * T)).GetSafeNormal();
}
TArray<FString> ReadLog(FString& Path, FString& Error)
{
	Path = FPaths::ConvertRelativePathToFull(FPlatformOutputDevices::GetAbsoluteLogFilename());
	FString Text;
	if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Text, *Path, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
	{
		Error = FString::Printf(TEXT("Failed to read log file: %s"), *Path);
		return {};
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	Lines.RemoveAll([](const FString& L)
					{ return L.TrimStartAndEnd().IsEmpty(); });
	return Lines;
}

struct FCaptureOutput : public FOutputDevice
{
	TArray<FString> Lines;
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override { Lines.Add(FString(V)); }
};

TArray<FString> LogCategories(const FString& Filter)
{
	FCaptureOutput Out;
	FSelfRegisteringExec::StaticExec(nullptr, *(Filter.IsEmpty() ? FString(TEXT("log list")) : FString::Printf(TEXT("log list %s"), *Filter)), Out);
	TArray<FString> Categories;
	for (const FString& Line : Out.Lines)
	{
		TArray<FString> Parts;
		Line.TrimEnd().ParseIntoArrayWS(Parts);
		if (Parts.Num() > 0)
			Categories.Add(Parts[0]);
	}
	Categories.Sort();
	return Categories;
}

FString LogVerbosity(const FString& Category)
{
	FCaptureOutput Out;
	FSelfRegisteringExec::StaticExec(nullptr, *FString::Printf(TEXT("log list %s"), *Category), Out);
	for (const FString& Line : Out.Lines)
	{
		TArray<FString> Parts;
		Line.TrimEnd().ParseIntoArrayWS(Parts);
		if (Parts.Num() >= 2 && Parts[0].Equals(Category, ESearchCase::IgnoreCase))
			return Parts[1];
	}
	return FString();
}

TSharedPtr<FJsonObject> CVarJson(const FString& Name, IConsoleVariable* CVar)
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("name"), Name);
	O->SetStringField(TEXT("value"), CVar->GetString());
	O->SetStringField(TEXT("default_value"), CVar->GetDefaultValue());
	O->SetStringField(TEXT("type"), CVar->IsVariableInt() ? TEXT("int") : CVar->IsVariableBool() ? TEXT("bool")
																	  : CVar->IsVariableFloat()	 ? TEXT("float")
																								 : TEXT("string"));
	if (const TCHAR* Help = static_cast<IConsoleObject*>(CVar)->GetHelp())
		O->SetStringField(TEXT("help"), Help);
	return O;
}
} // namespace

TArray<FECACommandParam> FECACommand_CaptureAssetImage::GetParameters() const
{
	return { { TEXT("asset_path"), TEXT("string"), TEXT("Asset path"), true }, { TEXT("size"), TEXT("number"), TEXT("Thumbnail size"), false, TEXT("256") } };
}

FECACommandResult FECACommand_CaptureAssetImage::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: asset_path"));
	int32 Size = 256;
	GetIntParam(Params, TEXT("size"), Size, false);
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ECABridge"), TEXT("NativeCompat"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString File = FPaths::Combine(Dir, FPaths::MakeValidFileName(FPaths::GetBaseFilename(AssetPath) + TEXT("_") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S%f")) + TEXT(".png")));
	TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("asset_path"), AssetPath);
	P->SetStringField(TEXT("output_path"), File);
	P->SetNumberField(TEXT("size"), Size);
	FECACommand_GetAssetThumbnail Thumb;
	FECACommandResult R = Thumb.Execute(P);
	if (!R.bSuccess)
		return R;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *File))
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to read thumbnail: %s"), *File));
	TSharedPtr<FJsonObject> Result = R.ResultData.IsValid() ? R.ResultData : MakeResult();
	Result->SetStringField(TEXT("file_path"), File);
	FECACommandResult Out = FECACommandResult::Success(Result);
	Out.McpContent.Add(FECACommandResult::MakeImageContent(Bytes));
	return Out;
}

TArray<FECACommandParam> FECACommand_CaptureEditorImage::GetParameters() const
{
	return { { TEXT("file_path"), TEXT("string"), TEXT("Optional PNG output path"), false } };
}
FECACommandResult FECACommand_CaptureEditorImage::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("target"), TEXT("editor_window"));
	FString File;
	if (GetStringParam(Params, TEXT("file_path"), File, false) && !File.IsEmpty())
		P->SetStringField(TEXT("file_path"), File);
	FECACommand_TakeGameplayScreenshot Cmd;
	return Cmd.Execute(P);
}

TArray<FECACommandParam> FECACommand_CaptureViewport::GetParameters() const
{
	return { { TEXT("file_path"), TEXT("string"), TEXT("Optional PNG output path"), false }, { TEXT("show_ui"), TEXT("boolean"), TEXT("Accepted; ignored for render-target capture"), false, TEXT("false") } };
}
FECACommandResult FECACommand_CaptureViewport::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("target"), TEXT("editor_viewport"));
	FString File;
	if (GetStringParam(Params, TEXT("file_path"), File, false) && !File.IsEmpty())
		P->SetStringField(TEXT("file_path"), File);
	FECACommand_TakeGameplayScreenshot Cmd;
	return Cmd.Execute(P);
}
TArray<FECACommandParam> FECACommand_FocusOnActors::GetParameters() const
{
	return { { TEXT("actor_name"), TEXT("string"), TEXT("Actor label/name/path"), false }, { TEXT("actor_names"), TEXT("array"), TEXT("Actor labels/names/paths"), false }, { TEXT("actor_paths"), TEXT("array"), TEXT("Actor object paths"), false } };
}
FECACommandResult FECACommand_FocusOnActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (GEditor && GEditor->PlayWorld)
		return FECACommandResult::Error(TEXT("Cannot focus viewport while PIE is running."));
	TArray<AActor*> Actors;
	FString One;
	if (GetStringParam(Params, TEXT("actor_name"), One, false) && !One.IsEmpty())
		if (AActor* A = ActorByNameOrPath(One))
			Actors.Add(A);
	for (const FString& V : JsonStringArray(Params, TEXT("actor_names")))
		if (AActor* A = ActorByNameOrPath(V))
			Actors.AddUnique(A);
	for (const FString& V : JsonStringArray(Params, TEXT("actor_paths")))
		if (AActor* A = ActorByNameOrPath(V))
			Actors.AddUnique(A);
	if (Actors.IsEmpty())
		return FECACommandResult::Error(TEXT("No matching actors found."));
	GEditor->MoveViewportCamerasToActor(Actors, false);
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetNumberField(TEXT("count"), Actors.Num());
	return FECACommandResult::Success(R);
}

FECACommandResult FECACommand_GetCameraTransformNativeCompat::Execute(const TSharedPtr<FJsonObject>&)
{
	FEditorViewportClient* VC = ViewClient();
	if (!VC)
		return FECACommandResult::Error(TEXT("No active editor viewport."));
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetObjectField(TEXT("location"), VecJson(VC->GetViewLocation()));
	R->SetObjectField(TEXT("rotation"), RotJson(VC->GetViewRotation()));
	R->SetNumberField(TEXT("fov"), VC->ViewFOV);
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_SetCameraTransformNativeCompat::GetParameters() const
{
	return { { TEXT("location"), TEXT("object"), TEXT("{x,y,z}"), false }, { TEXT("rotation"), TEXT("object"), TEXT("{pitch,yaw,roll}"), false } };
}
FECACommandResult FECACommand_SetCameraTransformNativeCompat::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FEditorViewportClient* VC = ViewClient();
	if (!VC)
		return FECACommandResult::Error(TEXT("No active editor viewport."));
	FVector L;
	if (JsonVector(Params, TEXT("location"), L))
		VC->SetViewLocation(L);
	FRotator R;
	if (JsonRotator(Params, TEXT("rotation"), R))
		VC->SetViewRotation(R);
	VC->Invalidate();
	return FECACommandResult::Success();
}

FECACommandResult FECACommand_GetContentBrowserPath::Execute(const TSharedPtr<FJsonObject>&)
{
	IContentBrowserSingleton* CB = ContentBrowser();
	if (!CB)
		return FECACommandResult::Error(TEXT("ContentBrowser module not available."));
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FString Path = CB->GetCurrentPath(EContentBrowserPathType::Internal);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetStringField(TEXT("path"), Path);
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_SetContentBrowserPath::GetParameters() const
{
	return { { TEXT("path"), TEXT("string"), TEXT("Folder path"), true } };
}
FECACommandResult FECACommand_SetContentBrowserPath::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!GetStringParam(Params, TEXT("path"), Path))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: path"));
	IContentBrowserSingleton* CB = ContentBrowser();
	if (!CB)
		return FECACommandResult::Error(TEXT("ContentBrowser module not available."));
	CB->SyncBrowserToFolders({ Path });
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetStringField(TEXT("path"), Path);
	return FECACommandResult::Success(R);
}

FECACommandResult FECACommand_GetSelectedAssets::Execute(const TSharedPtr<FJsonObject>&)
{
	IContentBrowserSingleton* CB = ContentBrowser();
	if (!CB)
		return FECACommandResult::Error(TEXT("ContentBrowser module not available."));
	TArray<FAssetData> Selected;
	CB->GetSelectedAssets(Selected);
	TArray<TSharedPtr<FJsonValue>> A;
	for (const FAssetData& D : Selected)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("path"), D.PackageName.ToString());
		O->SetStringField(TEXT("object_path"), D.GetObjectPathString());
		O->SetStringField(TEXT("name"), D.AssetName.ToString());
		O->SetStringField(TEXT("class"), D.AssetClassPath.ToString());
		A.Add(MakeShared<FJsonValueObject>(O));
	}
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetArrayField(TEXT("assets"), A);
	R->SetNumberField(TEXT("count"), A.Num());
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_SelectAssets::GetParameters() const
{
	return { { TEXT("asset_paths"), TEXT("array"), TEXT("Asset paths"), true } };
}
FECACommandResult FECACommand_SelectAssets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	IContentBrowserSingleton* CB = ContentBrowser();
	if (!CB)
		return FECACommandResult::Error(TEXT("ContentBrowser module not available."));
	TArray<UObject*> Assets;
	for (const FString& Path : JsonStringArray(Params, TEXT("asset_paths")))
		if (UObject* Asset = LoadObject<UObject>(nullptr, *Path))
			Assets.Add(Asset);
	if (Assets.IsEmpty())
		return FECACommandResult::Error(TEXT("No assets loaded from asset_paths."));
	CB->SyncBrowserToAssets(Assets);
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetNumberField(TEXT("selected_count"), Assets.Num());
	return FECACommandResult::Success(R);
}

FECACommandResult FECACommand_GetVisibleActors::Execute(const TSharedPtr<FJsonObject>&)
{
	FEditorViewportClient* VC = ViewClient();
	FViewport* VP = ActiveViewport();
	UWorld* W = EditorWorld();
	if (!VC || !VP || !W)
		return FECACommandResult::Error(TEXT("No active editor viewport/world."));
	TArray<TSharedPtr<FJsonValue>> A;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsHiddenEd())
			continue;
		FVector2D C;
		double Depth = 0;
		if (Project(VC, VP, Actor->GetActorLocation(), C, Depth) && C.X >= 0 && C.X <= 1 && C.Y >= 0 && C.Y <= 1)
		{
			TSharedPtr<FJsonObject> O = ActorJson(Actor);
			O->SetObjectField(TEXT("screen_coords"), VecJson(FVector(C.X, C.Y, 0)));
			O->SetNumberField(TEXT("depth"), Depth);
			A.Add(MakeShared<FJsonValueObject>(O));
		}
	}
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetArrayField(TEXT("actors"), A);
	R->SetNumberField(TEXT("count"), A.Num());
	return FECACommandResult::Success(R);
}
TArray<FECACommandParam> FECACommand_OpenEditorForAssetNativeCompat::GetParameters() const
{
	return { { TEXT("asset_path"), TEXT("string"), TEXT("Asset path"), true } };
}
FECACommandResult FECACommand_OpenEditorForAssetNativeCompat::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!GetStringParam(Params, TEXT("asset_path"), Path))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: asset_path"));
	UObject* Asset = LoadObject<UObject>(nullptr, *Path);
	if (!Asset)
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *Path));
	UAssetEditorSubsystem* S = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!S)
		return FECACommandResult::Error(TEXT("AssetEditorSubsystem not available."));
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetBoolField(TEXT("opened"), S->OpenEditorForAsset(Asset));
	R->SetStringField(TEXT("asset_path"), Path);
	return FECACommandResult::Success(R);
}

FECACommandResult FECACommand_GetOpenAssets::Execute(const TSharedPtr<FJsonObject>&)
{
	UAssetEditorSubsystem* S = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!S)
		return FECACommandResult::Error(TEXT("AssetEditorSubsystem not available."));
	TArray<TSharedPtr<FJsonValue>> A;
	for (UObject* Asset : S->GetAllEditedAssets())
	{
		if (!Asset)
			continue;
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("path"), Asset->GetOutermost()->GetName());
		O->SetStringField(TEXT("object_path"), Asset->GetPathName());
		O->SetStringField(TEXT("name"), Asset->GetName());
		O->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
		A.Add(MakeShared<FJsonValueObject>(O));
	}
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetArrayField(TEXT("assets"), A);
	R->SetNumberField(TEXT("count"), A.Num());
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_SearchCVarsNativeCompat::GetParameters() const
{
	return { { TEXT("name"), TEXT("string"), TEXT("CVar substring"), true } };
}
FECACommandResult FECACommand_SearchCVarsNativeCompat::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	TArray<TSharedPtr<FJsonValue>> CVars;
	int32 Total = 0;
	IConsoleManager::Get().ForEachConsoleObjectThatContains(FConsoleObjectVisitor::CreateLambda([&](const TCHAR* CVarName, IConsoleObject* Obj)
																								{
		if (Obj && Obj->AsVariable()) { ++Total; CVars.Add(MakeShared<FJsonValueObject>(CVarJson(FString(CVarName), Obj->AsVariable()))); } }),
															*Name);
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetStringField(TEXT("query"), Name);
	R->SetArrayField(TEXT("cvars"), CVars);
	R->SetNumberField(TEXT("count"), CVars.Num());
	R->SetNumberField(TEXT("total_matched"), Total);
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_WorldPosToScreenCoords::GetParameters() const
{
	return { { TEXT("position"), TEXT("object"), TEXT("World position {x,y,z}"), true } };
}
FECACommandResult FECACommand_WorldPosToScreenCoords::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FVector P;
	if (!JsonVector(Params, TEXT("position"), P))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: position {x,y,z}"));
	FVector2D C;
	double Depth = 0;
	if (!Project(ViewClient(), ActiveViewport(), P, C, Depth))
		return FECACommandResult::Error(TEXT("Point is behind the camera or viewport is unavailable."));
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetNumberField(TEXT("x"), C.X);
	R->SetNumberField(TEXT("y"), C.Y);
	R->SetNumberField(TEXT("depth"), Depth);
	R->SetBoolField(TEXT("in_view"), C.X >= 0 && C.X <= 1 && C.Y >= 0 && C.Y <= 1);
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_ScreenCoordsToWorld::GetParameters() const
{
	return { { TEXT("coords"), TEXT("object"), TEXT("Normalized coords {x,y}"), true }, { TEXT("trace_distance"), TEXT("number"), TEXT("Trace distance"), false, TEXT("100000") } };
}
FECACommandResult FECACommand_ScreenCoordsToWorld::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FVector2D C;
	if (!JsonVector2D(Params, TEXT("coords"), C))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: coords {x,y}"));
	double Dist = 100000.0;
	GetFloatParam(Params, TEXT("trace_distance"), Dist, false);
	FEditorViewportClient* VC = ViewClient();
	FViewport* VP = ActiveViewport();
	UWorld* W = EditorWorld();
	if (!VC || !VP || !W)
		return FECACommandResult::Error(TEXT("No active editor viewport/world."));
	const FVector Start = VC->GetViewLocation();
	const FVector Dir = ScreenRay(VC, VP, C);
	const FVector End = Start + Dir * Dist;
	FHitResult Hit;
	const bool bHit = W->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility);
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetBoolField(TEXT("hit"), bHit);
	R->SetObjectField(TEXT("origin"), VecJson(Start));
	R->SetObjectField(TEXT("direction"), VecJson(Dir));
	R->SetObjectField(TEXT("position"), VecJson(bHit ? Hit.ImpactPoint : End));
	if (bHit && Hit.GetActor())
	{
		R->SetStringField(TEXT("actor"), Hit.GetActor()->GetActorLabel());
		R->SetStringField(TEXT("actor_path"), Hit.GetActor()->GetPathName());
	}
	return FECACommandResult::Success(R);
}

FECACommandResult FECACommand_StartPIENativeCompat::Execute(const TSharedPtr<FJsonObject>&)
{
	FECACommand_PlayInEditor C;
	return C.Execute(MakeShared<FJsonObject>());
}
FECACommandResult FECACommand_StopPIENativeCompat::Execute(const TSharedPtr<FJsonObject>&)
{
	FECACommand_StopPlayInEditor C;
	return C.Execute(MakeShared<FJsonObject>());
}
FECACommandResult FECACommand_IsPIERunningNativeCompat::Execute(const TSharedPtr<FJsonObject>&)
{
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetBoolField(TEXT("is_pie_running"), GEditor && GEditor->PlayWorld != nullptr);
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_GetLogEntriesNativeCompat::GetParameters() const
{
	return { { TEXT("category"), TEXT("string"), TEXT("Optional category"), false }, { TEXT("pattern"), TEXT("string"), TEXT("Optional regex"), false }, { TEXT("max_entries"), TEXT("number"), TEXT("Max lines"), false, TEXT("1000") } };
}
FECACommandResult FECACommand_GetLogEntriesNativeCompat::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Category, Pattern;
	GetStringParam(Params, TEXT("category"), Category, false);
	GetStringParam(Params, TEXT("pattern"), Pattern, false);
	int32 Max = 1000;
	GetIntParam(Params, TEXT("max_entries"), Max, false);
	FString Path, Error;
	TArray<FString> Lines = ReadLog(Path, Error);
	if (!Error.IsEmpty())
		return FECACommandResult::Error(Error);
	TOptional<FRegexPattern> Regex;
	if (!Pattern.IsEmpty())
		Regex.Emplace(Pattern, ERegexPatternFlags::CaseInsensitive);
	TArray<FString> Matches;
	const FString Prefix = Category.IsEmpty() ? FString() : FString::Printf(TEXT("]%s:"), *Category);
	for (const FString& L : Lines)
	{
		if (!Category.IsEmpty() && !L.Contains(Prefix, ESearchCase::IgnoreCase))
			continue;
		if (Regex.IsSet())
		{
			FRegexMatcher M(*Regex, L);
			if (!M.FindNext())
				continue;
		}
		Matches.Add(L);
	}
	if (Max > 0 && Matches.Num() > Max)
		Matches.RemoveAt(0, Matches.Num() - Max);
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FString& L : Matches)
		Out.Add(MakeShared<FJsonValueString>(L));
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetStringField(TEXT("log_file"), Path);
	R->SetArrayField(TEXT("entries"), Out);
	R->SetNumberField(TEXT("count"), Out.Num());
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_GetLogCategoriesNativeCompat::GetParameters() const
{
	return { { TEXT("filter"), TEXT("string"), TEXT("Optional filter"), false } };
}
FECACommandResult FECACommand_GetLogCategoriesNativeCompat::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	GetStringParam(Params, TEXT("filter"), Filter, false);
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FString& C : LogCategories(Filter))
		Out.Add(MakeShared<FJsonValueString>(C));
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetArrayField(TEXT("categories"), Out);
	R->SetNumberField(TEXT("count"), Out.Num());
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_GetVerbosityNativeCompat::GetParameters() const
{
	return { { TEXT("category"), TEXT("string"), TEXT("Log category"), true } };
}
FECACommandResult FECACommand_GetVerbosityNativeCompat::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Category;
	if (!GetStringParam(Params, TEXT("category"), Category) || Category.IsEmpty())
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: category"));
	const FString V = LogVerbosity(Category);
	if (V.IsEmpty())
		return FECACommandResult::Error(FString::Printf(TEXT("Log category not found: %s"), *Category));
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetStringField(TEXT("category"), Category);
	R->SetStringField(TEXT("verbosity"), V);
	return FECACommandResult::Success(R);
}

TArray<FECACommandParam> FECACommand_SetVerbosityNativeCompat::GetParameters() const
{
	return { { TEXT("category"), TEXT("string"), TEXT("Log category"), true }, { TEXT("verbosity"), TEXT("string"), TEXT("Verbosity"), true } };
}
FECACommandResult FECACommand_SetVerbosityNativeCompat::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Category, V;
	if (!GetStringParam(Params, TEXT("category"), Category) || Category.IsEmpty())
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: category"));
	if (!GetStringParam(Params, TEXT("verbosity"), V) || V.IsEmpty())
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: verbosity"));
	static const TArray<FString> Valid = { TEXT("NoLogging"), TEXT("Fatal"), TEXT("Error"), TEXT("Warning"), TEXT("Display"), TEXT("Log"), TEXT("Verbose"), TEXT("VeryVerbose") };
	if (!Valid.ContainsByPredicate([&](const FString& X)
								   { return X.Equals(V, ESearchCase::IgnoreCase); }))
		return FECACommandResult::Error(FString::Printf(TEXT("Invalid verbosity: %s"), *V));
	if (LogVerbosity(Category).IsEmpty())
		return FECACommandResult::Error(FString::Printf(TEXT("Log category not found: %s"), *Category));
	FOutputDeviceNull Null;
	FSelfRegisteringExec::StaticExec(nullptr, *FString::Printf(TEXT("log %s %s"), *Category, *V), Null);
	TSharedPtr<FJsonObject> R = MakeResult();
	R->SetStringField(TEXT("category"), Category);
	R->SetStringField(TEXT("verbosity"), LogVerbosity(Category));
	return FECACommandResult::Success(R);
}