// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FLayeredLayoutConfig
{
	int32 StartX = 0;
	int32 StartY = 0;
	int32 PaddingX = 120;
	int32 PaddingY = 40;
	int32 RelaxationPasses = 3;
};

struct FLayeredLayoutVertex
{
	int32 Id = INDEX_NONE;
	int32 Width = 220;
	int32 Height = 100;
	int32 OriginalX = 0;
	int32 OriginalY = 0;
	int32 Rank = 0;
	int32 Order = 0;
	int32 X = 0;
	int32 Y = 0;
};

struct FLayeredLayoutEdge
{
	int32 SourceId = INDEX_NONE;
	int32 TargetId = INDEX_NONE;
	int32 SourcePinYOffset = 0;
	int32 TargetPinYOffset = 0;
	bool bExec = false;
};

struct FLayeredLayoutResult
{
	int32 VerticesPositioned = 0;
	int32 EdgeCount = 0;
	int32 RankCount = 0;
	TArray<FString> Warnings;
};

class FLayeredGraphLayout
{
public:
	void Reset();
	int32 AddVertex(int32 Id, int32 Width, int32 Height, int32 OriginalX, int32 OriginalY);
	void AddEdge(int32 SourceId, int32 TargetId, int32 SourcePinYOffset, int32 TargetPinYOffset, bool bExec);

	FLayeredLayoutResult Solve(const FLayeredLayoutConfig& Config);

	const FLayeredLayoutVertex* FindVertex(int32 Id) const;
	const TArray<FLayeredLayoutVertex>& GetVertices() const { return Vertices; }

private:
	TArray<FLayeredLayoutVertex> Vertices;
	TArray<FLayeredLayoutEdge> Edges;
	TMap<int32, int32> IdToIndex;

	void AssignRanks(TArray<FString>& Warnings);
	int32 CompactRanks();
	void AssignInitialOrder(TArray<TArray<int32>>& Ranks);
	void ReduceCrossings(TArray<TArray<int32>>& Ranks);
	void AssignCoordinates(const FLayeredLayoutConfig& Config, TArray<TArray<int32>>& Ranks);
	void ApplyRankY(const FLayeredLayoutConfig& Config, const TArray<int32>& Rank, const TArray<double>& DesiredY, const TArray<bool>& bHasDesiredY);
	void RebuildRanks(TArray<TArray<int32>>& Ranks, int32 RankCount) const;
};
