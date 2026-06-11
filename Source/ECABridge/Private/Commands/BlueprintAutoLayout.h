// Copyright Epic Games, Inc. All Rights Reserved.
// BlueprintAutoLayout.h - Blueprint graph auto layout adapter.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

struct FBlueprintLayoutConfig
{
	int32 NodePaddingX = 120;
	int32 NodePaddingY = 40;
	int32 DefaultNodeWidth = 220;
	int32 DefaultNodeHeight = 100;
	int32 PinHeightEstimate = 26;
	bool bAlignComments = true;
	bool bMaterializeLongEdges = false;
};

struct FLayoutNodeInfo
{
	UEdGraphNode* Node = nullptr;
	int32 NodeWidth = 0;
	int32 NodeHeight = 0;
	int32 Depth = 0;
	int32 SubtreeHeight = 0;
	int32 LayoutX = 0;
	int32 LayoutY = 0;
	bool bIsBranchNode = false;
	bool bIsPureNode = false;
	bool bPositioned = false;
};

class FBlueprintAutoLayout
{
public:
	FBlueprintAutoLayout(const FBlueprintLayoutConfig& InConfig = FBlueprintLayoutConfig())
		: Config(InConfig)
	{}

	int32 LayoutGraph(UEdGraph* Graph, int32 StartX = 0, int32 StartY = 0);
	int32 LayoutSubtree(UEdGraph* Graph, const TArray<UEdGraphNode*>& RootNodes, int32 StartX = 0, int32 StartY = 0);

	const TMap<UEdGraphNode*, FLayoutNodeInfo>& GetLayoutInfo() const { return NodeInfoMap; }
	int32 GetCommentsWrapped() const { return LastCommentsWrapped; }
	int32 GetAutoKnotsRemoved() const { return LastAutoKnotsRemoved; }
	int32 GetAutoKnotsCreated() const { return LastAutoKnotsCreated; }
	int32 GetEdgeCount() const { return LastEdgeCount; }
	int32 GetRankCount() const { return LastRankCount; }
	const TArray<FString>& GetWarnings() const { return LastWarnings; }

private:
	FBlueprintLayoutConfig Config;
	TMap<UEdGraphNode*, FLayoutNodeInfo> NodeInfoMap;
	int32 LastCommentsWrapped = 0;
	int32 LastAutoKnotsRemoved = 0;
	int32 LastAutoKnotsCreated = 0;
	int32 LastEdgeCount = 0;
	int32 LastRankCount = 0;
	TArray<FString> LastWarnings;

	void ResetState();
	int32 LayoutLayered(UEdGraph* Graph, const TSet<UEdGraphNode*>* SpecificNodes, int32 StartX, int32 StartY);
	FLayoutNodeInfo* GetOrCreateNodeInfo(UEdGraphNode* Node);
	void ClassifyNode(FLayoutNodeInfo* Info);
	void CalculateNodeDimensions(FLayoutNodeInfo* Info);
	void ApplyPositions(UEdGraph* Graph);

	bool IsExecPin(UEdGraphPin* Pin) const;
	bool IsPureNode(UEdGraphNode* Node) const;
	bool IsBranchNode(UEdGraphNode* Node) const;
	bool IsCommentNode(UEdGraphNode* Node) const;
	bool IsKnotNode(UEdGraphNode* Node) const;
	TArray<UEdGraphPin*> GetExecOutputPins(UEdGraphNode* Node) const;
	void TraceToRealInputPins(UEdGraphPin* OutputPin, TArray<UEdGraphPin*>& OutPins, TSet<UEdGraphPin*>& VisitedPins) const;
	int32 EstimatePinYOffset(UEdGraphNode* Node, UEdGraphPin* Pin) const;
};

inline int32 AutoLayoutBlueprintGraph(UEdGraph* Graph, int32 StartX = 0, int32 StartY = 0)
{
	FBlueprintAutoLayout Layout;
	return Layout.LayoutGraph(Graph, StartX, StartY);
}