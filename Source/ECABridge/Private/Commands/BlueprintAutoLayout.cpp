// Copyright Epic Games, Inc. All Rights Reserved.
// BlueprintAutoLayout.cpp - Layered Blueprint graph layout adapter.

#include "BlueprintAutoLayout.h"
#include "BlueprintLayout/LayeredGraphLayout.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"

namespace
{
struct FCommentWrapInfo
{
	UEdGraphNode_Comment* Comment = nullptr;
	TArray<UEdGraphNode*> Members;
};

static int32 EstimateNodeWidth(UEdGraphNode* Node, int32 DefaultWidth)
{
	if (!Node)
	{
		return DefaultWidth;
	}
	if (Node->NodeWidth > 0)
	{
		return Node->NodeWidth;
	}
	const FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	return FMath::Max(DefaultWidth, Title.Len() * 8 + 60);
}

static int32 EstimateNodeHeight(UEdGraphNode* Node, int32 DefaultHeight, int32 PinHeight)
{
	if (!Node)
	{
		return DefaultHeight;
	}
	if (Node->NodeHeight > 0)
	{
		return Node->NodeHeight;
	}

	int32 Inputs = 0;
	int32 Outputs = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->bHidden)
		{
			continue;
		}
		if (Pin->Direction == EGPD_Input)
		{
			++Inputs;
		}
		else if (Pin->Direction == EGPD_Output)
		{
			++Outputs;
		}
	}
	return FMath::Max(DefaultHeight, 40 + FMath::Max(Inputs, Outputs) * PinHeight);
}

static bool IsInsideComment(UEdGraphNode_Comment* Comment, UEdGraphNode* Node, const FBlueprintLayoutConfig& Config)
{
	if (!Comment || !Node || Comment == Node)
	{
		return false;
	}

	const int32 CommentRight = Comment->NodePosX + FMath::Max(Comment->NodeWidth, Config.DefaultNodeWidth);
	const int32 CommentBottom = Comment->NodePosY + FMath::Max(Comment->NodeHeight, Config.DefaultNodeHeight);
	const int32 NodeRight = Node->NodePosX + EstimateNodeWidth(Node, Config.DefaultNodeWidth);
	const int32 NodeBottom = Node->NodePosY + EstimateNodeHeight(Node, Config.DefaultNodeHeight, Config.PinHeightEstimate);

	return Node->NodePosX >= Comment->NodePosX
		&& Node->NodePosY >= Comment->NodePosY
		&& NodeRight <= CommentRight
		&& NodeBottom <= CommentBottom;
}

static TArray<FCommentWrapInfo> CaptureCommentWraps(UEdGraph* Graph, const TArray<UEdGraphNode*>& LayoutNodes, const FBlueprintLayoutConfig& Config)
{
	TArray<FCommentWrapInfo> Wraps;
	if (!Graph || !Config.bAlignComments)
	{
		return Wraps;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
		if (!Comment)
		{
			continue;
		}

		FCommentWrapInfo Info;
		Info.Comment = Comment;
		for (UEdGraphNode* Candidate : LayoutNodes)
		{
			if (IsInsideComment(Comment, Candidate, Config))
			{
				Info.Members.Add(Candidate);
			}
		}
		if (Info.Members.Num() > 0)
		{
			Wraps.Add(MoveTemp(Info));
		}
	}
	return Wraps;
}

static int32 RewrapComments(const TArray<FCommentWrapInfo>& Wraps, const FBlueprintLayoutConfig& Config)
{
	int32 Wrapped = 0;
	const int32 Margin = 40;
	const int32 TitleHeight = 32;

	for (const FCommentWrapInfo& Wrap : Wraps)
	{
		if (!Wrap.Comment || Wrap.Members.Num() == 0)
		{
			continue;
		}

		int32 MinX = MAX_int32;
		int32 MinY = MAX_int32;
		int32 MaxX = MIN_int32;
		int32 MaxY = MIN_int32;

		for (UEdGraphNode* Node : Wrap.Members)
		{
			if (!Node)
			{
				continue;
			}
			const int32 Width = EstimateNodeWidth(Node, Config.DefaultNodeWidth);
			const int32 Height = EstimateNodeHeight(Node, Config.DefaultNodeHeight, Config.PinHeightEstimate);
			MinX = FMath::Min(MinX, Node->NodePosX);
			MinY = FMath::Min(MinY, Node->NodePosY);
			MaxX = FMath::Max(MaxX, Node->NodePosX + Width);
			MaxY = FMath::Max(MaxY, Node->NodePosY + Height);
		}

		if (MinX == MAX_int32)
		{
			continue;
		}

		Wrap.Comment->Modify();
		Wrap.Comment->NodePosX = MinX - Margin;
		Wrap.Comment->NodePosY = MinY - Margin - TitleHeight;
		Wrap.Comment->NodeWidth = (MaxX - MinX) + Margin * 2;
		Wrap.Comment->NodeHeight = (MaxY - MinY) + Margin * 2 + TitleHeight;
		++Wrapped;
	}
	return Wrapped;
}
}

int32 FBlueprintAutoLayout::LayoutGraph(UEdGraph* Graph, int32 StartX, int32 StartY)
{
	ResetState();
	return LayoutLayered(Graph, nullptr, StartX, StartY);
}

int32 FBlueprintAutoLayout::LayoutSubtree(UEdGraph* Graph, const TArray<UEdGraphNode*>& SpecificRoots, int32 StartX, int32 StartY)
{
	ResetState();
	if (!Graph || SpecificRoots.Num() == 0)
	{
		return 0;
	}

	TSet<UEdGraphNode*> SpecificNodes;
	for (UEdGraphNode* Node : SpecificRoots)
	{
		if (Node)
		{
			SpecificNodes.Add(Node);
		}
	}
	return LayoutLayered(Graph, &SpecificNodes, StartX, StartY);
}

void FBlueprintAutoLayout::ResetState()
{
	NodeInfoMap.Empty();
	LastCommentsWrapped = 0;
	LastAutoKnotsRemoved = 0;
	LastAutoKnotsCreated = 0;
	LastEdgeCount = 0;
	LastRankCount = 0;
	LastWarnings.Reset();
}

int32 FBlueprintAutoLayout::LayoutLayered(UEdGraph* Graph, const TSet<UEdGraphNode*>* SpecificNodes, int32 StartX, int32 StartY)
{
	if (!Graph)
	{
		LastWarnings.Add(TEXT("missing_graph"));
		return 0;
	}

	FLayeredGraphLayout Core;
	FLayeredLayoutConfig CoreConfig;
	CoreConfig.StartX = StartX;
	CoreConfig.StartY = StartY;
	CoreConfig.PaddingX = Config.NodePaddingX;
	CoreConfig.PaddingY = Config.NodePaddingY;

	TArray<UEdGraphNode*> LayoutNodes;
	TMap<UEdGraphNode*, int32> NodeToVertexId;
	TMap<int32, UEdGraphNode*> VertexIdToNode;
	int32 NextVertexId = 0;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || IsCommentNode(Node) || IsKnotNode(Node))
		{
			continue;
		}
		if (SpecificNodes && !SpecificNodes->Contains(Node))
		{
			continue;
		}

		FLayoutNodeInfo* Info = GetOrCreateNodeInfo(Node);
		ClassifyNode(Info);

		const int32 VertexId = NextVertexId++;
		NodeToVertexId.Add(Node, VertexId);
		VertexIdToNode.Add(VertexId, Node);
		LayoutNodes.Add(Node);
		Core.AddVertex(VertexId, Info->NodeWidth, Info->NodeHeight, Node->NodePosX, Node->NodePosY);
	}

	if (LayoutNodes.Num() == 0)
	{
		LastWarnings.Add(TEXT("no_layout_nodes"));
		return 0;
	}

	const TArray<FCommentWrapInfo> CommentWraps = CaptureCommentWraps(Graph, LayoutNodes, Config);
	TSet<FString> SeenEdges;

	for (UEdGraphNode* Node : LayoutNodes)
	{
		const int32 SourceId = NodeToVertexId.FindRef(Node);
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || Pin->bHidden)
			{
				continue;
			}

			TArray<UEdGraphPin*> DestPins;
			TSet<UEdGraphPin*> VisitedPins;
			TraceToRealInputPins(Pin, DestPins, VisitedPins);

			for (UEdGraphPin* DestPin : DestPins)
			{
				UEdGraphNode* DestNode = DestPin ? DestPin->GetOwningNode() : nullptr;
				const int32* TargetId = DestNode ? NodeToVertexId.Find(DestNode) : nullptr;
				if (!TargetId || DestNode == Node)
				{
					continue;
				}

				const FString EdgeKey = FString::Printf(TEXT("%d:%s>%d:%s"), SourceId, *Pin->PinName.ToString(), *TargetId, *DestPin->PinName.ToString());
				if (SeenEdges.Contains(EdgeKey))
				{
					continue;
				}
				SeenEdges.Add(EdgeKey);

				const bool bExec = IsExecPin(Pin) || IsExecPin(DestPin);
				Core.AddEdge(SourceId, *TargetId, EstimatePinYOffset(Node, Pin), EstimatePinYOffset(DestNode, DestPin), bExec);
			}
		}
	}

	FLayeredLayoutResult CoreResult = Core.Solve(CoreConfig);
	LastEdgeCount = CoreResult.EdgeCount;
	LastRankCount = CoreResult.RankCount;
	LastWarnings.Append(CoreResult.Warnings);

	for (const FLayeredLayoutVertex& Vertex : Core.GetVertices())
	{
		UEdGraphNode* Node = VertexIdToNode.FindRef(Vertex.Id);
		FLayoutNodeInfo* Info = Node ? NodeInfoMap.Find(Node) : nullptr;
		if (!Info)
		{
			continue;
		}
		Info->Depth = Vertex.Rank;
			Info->SubtreeHeight = Vertex.Height;
		Info->LayoutX = Vertex.X;
		Info->LayoutY = Vertex.Y;
		Info->bPositioned = true;
	}

	ApplyPositions(Graph);
	LastCommentsWrapped = RewrapComments(CommentWraps, Config);
	Graph->NotifyGraphChanged();

	if (Config.bMaterializeLongEdges)
	{
		LastWarnings.Add(TEXT("materialize_long_edges_not_implemented"));
	}

	return CoreResult.VerticesPositioned;
}

FLayoutNodeInfo* FBlueprintAutoLayout::GetOrCreateNodeInfo(UEdGraphNode* Node)
{
	if (FLayoutNodeInfo* Existing = NodeInfoMap.Find(Node))
	{
		return Existing;
	}

	FLayoutNodeInfo& Info = NodeInfoMap.Add(Node);
	Info.Node = Node;
	return &Info;
}

void FBlueprintAutoLayout::ClassifyNode(FLayoutNodeInfo* Info)
{
	if (!Info || !Info->Node)
	{
		return;
	}
	Info->bIsPureNode = IsPureNode(Info->Node);
	Info->bIsBranchNode = IsBranchNode(Info->Node);
	CalculateNodeDimensions(Info);
}

void FBlueprintAutoLayout::CalculateNodeDimensions(FLayoutNodeInfo* Info)
{
	if (!Info || !Info->Node)
	{
		return;
	}

	Info->NodeWidth = EstimateNodeWidth(Info->Node, Config.DefaultNodeWidth);
	Info->NodeHeight = EstimateNodeHeight(Info->Node, Config.DefaultNodeHeight, Config.PinHeightEstimate);
	if (Info->Node->IsA<UK2Node_CallFunction>())
	{
		Info->NodeWidth = FMath::Max(Info->NodeWidth, 250);
	}
}

void FBlueprintAutoLayout::ApplyPositions(UEdGraph* Graph)
{
	if (Graph)
	{
		Graph->Modify();
	}

	for (auto& Pair : NodeInfoMap)
	{
		FLayoutNodeInfo& Info = Pair.Value;
		if (!Info.Node || !Info.bPositioned)
		{
			continue;
		}
		Info.Node->Modify();
		Info.Node->NodePosX = Info.LayoutX;
		Info.Node->NodePosY = Info.LayoutY;
	}
}

bool FBlueprintAutoLayout::IsExecPin(UEdGraphPin* Pin) const
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

bool FBlueprintAutoLayout::IsPureNode(UEdGraphNode* Node) const
{
	if (!Node)
	{
		return false;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (IsExecPin(Pin))
		{
			return false;
		}
	}
	return true;
}

bool FBlueprintAutoLayout::IsBranchNode(UEdGraphNode* Node) const
{
	if (!Node)
	{
		return false;
	}
	if (Node->IsA<UK2Node_IfThenElse>())
	{
		return true;
	}
	if (Node->IsA<UK2Node_ExecutionSequence>())
	{
		return false;
	}
	return GetExecOutputPins(Node).Num() > 1;
}

bool FBlueprintAutoLayout::IsCommentNode(UEdGraphNode* Node) const
{
	return Node && Node->IsA<UEdGraphNode_Comment>();
}

bool FBlueprintAutoLayout::IsKnotNode(UEdGraphNode* Node) const
{
	return Node && Node->IsA<UK2Node_Knot>();
}

TArray<UEdGraphPin*> FBlueprintAutoLayout::GetExecOutputPins(UEdGraphNode* Node) const
{
	TArray<UEdGraphPin*> Result;
	if (!Node)
	{
		return Result;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && IsExecPin(Pin))
		{
			Result.Add(Pin);
		}
	}
	Result.Sort([](const UEdGraphPin& A, const UEdGraphPin& B)
	{
		if (A.PinName == TEXT("Then")) return true;
		if (B.PinName == TEXT("Then")) return false;
		return A.PinName.LexicalLess(B.PinName);
	});
	return Result;
}

void FBlueprintAutoLayout::TraceToRealInputPins(UEdGraphPin* OutputPin, TArray<UEdGraphPin*>& OutPins, TSet<UEdGraphPin*>& VisitedPins) const
{
	if (!OutputPin || VisitedPins.Contains(OutputPin))
	{
		return;
	}
	VisitedPins.Add(OutputPin);

	for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
	{
		if (!LinkedPin || VisitedPins.Contains(LinkedPin))
		{
			continue;
		}
		VisitedPins.Add(LinkedPin);

		UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
		if (IsKnotNode(LinkedNode))
		{
			for (UEdGraphPin* KnotPin : LinkedNode->Pins)
			{
				if (KnotPin && KnotPin != LinkedPin && KnotPin->Direction == EGPD_Output)
				{
					TraceToRealInputPins(KnotPin, OutPins, VisitedPins);
				}
			}
			continue;
		}

		if (LinkedPin->Direction == EGPD_Input)
		{
			OutPins.Add(LinkedPin);
		}
	}
}

int32 FBlueprintAutoLayout::EstimatePinYOffset(UEdGraphNode* Node, UEdGraphPin* Pin) const
{
	if (!Node || !Pin)
	{
		return Config.DefaultNodeHeight / 2;
	}

	int32 VisibleIndex = 0;
	int32 VisibleCount = 0;
	for (UEdGraphPin* Candidate : Node->Pins)
	{
		if (!Candidate || Candidate->bHidden || Candidate->Direction != Pin->Direction)
		{
			continue;
		}
		if (Candidate == Pin)
		{
			VisibleIndex = VisibleCount;
		}
		++VisibleCount;
	}

	const int32 NodeHeight = EstimateNodeHeight(Node, Config.DefaultNodeHeight, Config.PinHeightEstimate);
	const int32 Offset = 40 + VisibleIndex * Config.PinHeightEstimate;
	return FMath::Clamp(Offset, 12, FMath::Max(12, NodeHeight - 12));
}
