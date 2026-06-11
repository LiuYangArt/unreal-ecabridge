// Copyright Epic Games, Inc. All Rights Reserved.

#include "LayeredGraphLayout.h"

void FLayeredGraphLayout::Reset()
{
	Vertices.Reset();
	Edges.Reset();
	IdToIndex.Reset();
}

int32 FLayeredGraphLayout::AddVertex(int32 Id, int32 Width, int32 Height, int32 OriginalX, int32 OriginalY)
{
	if (const int32* Existing = IdToIndex.Find(Id))
	{
		return *Existing;
	}

	FLayeredLayoutVertex Vertex;
	Vertex.Id = Id;
	Vertex.Width = FMath::Max(Width, 1);
	Vertex.Height = FMath::Max(Height, 1);
	Vertex.OriginalX = OriginalX;
	Vertex.OriginalY = OriginalY;
	Vertex.X = OriginalX;
	Vertex.Y = OriginalY;

	const int32 Index = Vertices.Add(Vertex);
	IdToIndex.Add(Id, Index);
	return Index;
}

void FLayeredGraphLayout::AddEdge(int32 SourceId, int32 TargetId, int32 SourcePinYOffset, int32 TargetPinYOffset, bool bExec)
{
	if (SourceId == TargetId || !IdToIndex.Contains(SourceId) || !IdToIndex.Contains(TargetId))
	{
		return;
	}

	FLayeredLayoutEdge Edge;
	Edge.SourceId = SourceId;
	Edge.TargetId = TargetId;
	Edge.SourcePinYOffset = SourcePinYOffset;
	Edge.TargetPinYOffset = TargetPinYOffset;
	Edge.bExec = bExec;
	Edges.Add(Edge);
}

const FLayeredLayoutVertex* FLayeredGraphLayout::FindVertex(int32 Id) const
{
	if (const int32* Index = IdToIndex.Find(Id))
	{
		return Vertices.IsValidIndex(*Index) ? &Vertices[*Index] : nullptr;
	}
	return nullptr;
}

FLayeredLayoutResult FLayeredGraphLayout::Solve(const FLayeredLayoutConfig& Config)
{
	FLayeredLayoutResult Result;
	Result.EdgeCount = Edges.Num();
	if (Vertices.Num() == 0)
	{
		return Result;
	}

	AssignRanks(Result.Warnings);
	Result.RankCount = CompactRanks();

	TArray<TArray<int32>> Ranks;
	RebuildRanks(Ranks, Result.RankCount);
	AssignInitialOrder(Ranks);
	ReduceCrossings(Ranks);
	AssignCoordinates(Config, Ranks);

	Result.VerticesPositioned = Vertices.Num();
	return Result;
}

void FLayeredGraphLayout::AssignRanks(TArray<FString>& Warnings)
{
	TArray<TArray<int32>> Incoming;
	Incoming.SetNum(Vertices.Num());

	for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
	{
		const FLayeredLayoutEdge& Edge = Edges[EdgeIndex];
		const int32* TargetIndex = IdToIndex.Find(Edge.TargetId);
		if (TargetIndex && Incoming.IsValidIndex(*TargetIndex))
		{
			Incoming[*TargetIndex].Add(EdgeIndex);
		}
	}

	TSet<int32> Visiting;
	TSet<int32> Visited;
	bool bCycleWarningAdded = false;

	TFunction<int32(int32)> Visit = [&](int32 VertexIndex) -> int32
	{
		if (Visited.Contains(VertexIndex))
		{
			return Vertices[VertexIndex].Rank;
		}
		if (Visiting.Contains(VertexIndex))
		{
			if (!bCycleWarningAdded)
			{
				Warnings.Add(TEXT("cycle_detected_ignored_back_edges"));
				bCycleWarningAdded = true;
			}
			return Vertices[VertexIndex].Rank;
		}

		Visiting.Add(VertexIndex);
		int32 BestRank = 0;
		for (int32 EdgeIndex : Incoming[VertexIndex])
		{
			const FLayeredLayoutEdge& Edge = Edges[EdgeIndex];
			const int32* SourceIndex = IdToIndex.Find(Edge.SourceId);
			if (!SourceIndex || *SourceIndex == VertexIndex)
			{
				continue;
			}
			if (Visiting.Contains(*SourceIndex))
			{
				if (!bCycleWarningAdded)
				{
					Warnings.Add(TEXT("cycle_detected_ignored_back_edges"));
					bCycleWarningAdded = true;
				}
				continue;
			}
			BestRank = FMath::Max(BestRank, Visit(*SourceIndex) + 1);
		}
		Vertices[VertexIndex].Rank = BestRank;
		Visiting.Remove(VertexIndex);
		Visited.Add(VertexIndex);
		return BestRank;
	};

	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		Visit(Index);
	}
}

int32 FLayeredGraphLayout::CompactRanks()
{
	TArray<int32> RankValues;
	for (const FLayeredLayoutVertex& Vertex : Vertices)
	{
		RankValues.AddUnique(Vertex.Rank);
	}
	RankValues.Sort();

	for (FLayeredLayoutVertex& Vertex : Vertices)
	{
		Vertex.Rank = RankValues.IndexOfByKey(Vertex.Rank);
	}
	return RankValues.Num();
}

void FLayeredGraphLayout::RebuildRanks(TArray<TArray<int32>>& Ranks, int32 RankCount) const
{
	Ranks.Reset();
	Ranks.SetNum(RankCount);
	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		if (Ranks.IsValidIndex(Vertices[Index].Rank))
		{
			Ranks[Vertices[Index].Rank].Add(Index);
		}
	}
}

void FLayeredGraphLayout::AssignInitialOrder(TArray<TArray<int32>>& Ranks)
{
	for (TArray<int32>& Rank : Ranks)
	{
		Rank.Sort([this](int32 A, int32 B)
		{
			if (Vertices[A].OriginalY != Vertices[B].OriginalY)
			{
				return Vertices[A].OriginalY < Vertices[B].OriginalY;
			}
			return Vertices[A].OriginalX < Vertices[B].OriginalX;
		});

		for (int32 Order = 0; Order < Rank.Num(); ++Order)
		{
			Vertices[Rank[Order]].Order = Order;
		}
	}
}

void FLayeredGraphLayout::ReduceCrossings(TArray<TArray<int32>>& Ranks)
{
	struct FOrderEntry
	{
		int32 VertexIndex = INDEX_NONE;
		double Score = 0.0;
		bool bHasScore = false;
	};

	auto SortRank = [&](int32 RankIndex, bool bUseIncoming)
	{
		TArray<FOrderEntry> Entries;
		for (int32 VertexIndex : Ranks[RankIndex])
		{
			double Sum = 0.0;
			double Weight = 0.0;
			for (const FLayeredLayoutEdge& Edge : Edges)
			{
				const int32* SourceIndex = IdToIndex.Find(Edge.SourceId);
				const int32* TargetIndex = IdToIndex.Find(Edge.TargetId);
				if (!SourceIndex || !TargetIndex)
				{
					continue;
				}

				const bool bMatches = bUseIncoming ? (*TargetIndex == VertexIndex) : (*SourceIndex == VertexIndex);
				const int32 NeighborIndex = bUseIncoming ? *SourceIndex : *TargetIndex;
				if (!bMatches || Vertices[NeighborIndex].Rank == Vertices[VertexIndex].Rank)
				{
					continue;
				}

				const double EdgeWeight = Edge.bExec ? 3.0 : 1.0;
				const double PinBias = bUseIncoming ? Edge.SourcePinYOffset * 0.01 : Edge.TargetPinYOffset * 0.01;
				Sum += (Vertices[NeighborIndex].Order + PinBias) * EdgeWeight;
				Weight += EdgeWeight;
			}

			FOrderEntry Entry;
			Entry.VertexIndex = VertexIndex;
			if (Weight > 0.0)
			{
				Entry.Score = Sum / Weight;
				Entry.bHasScore = true;
			}
			Entries.Add(Entry);
		}

		Entries.Sort([this](const FOrderEntry& A, const FOrderEntry& B)
		{
			if (A.bHasScore != B.bHasScore)
			{
				return A.bHasScore;
			}
			if (A.bHasScore && !FMath::IsNearlyEqual(A.Score, B.Score))
			{
				return A.Score < B.Score;
			}
			if (Vertices[A.VertexIndex].OriginalY != Vertices[B.VertexIndex].OriginalY)
			{
				return Vertices[A.VertexIndex].OriginalY < Vertices[B.VertexIndex].OriginalY;
			}
			return Vertices[A.VertexIndex].OriginalX < Vertices[B.VertexIndex].OriginalX;
		});

		Ranks[RankIndex].Reset();
		for (int32 Order = 0; Order < Entries.Num(); ++Order)
		{
			Ranks[RankIndex].Add(Entries[Order].VertexIndex);
			Vertices[Entries[Order].VertexIndex].Order = Order;
		}
	};

	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		for (int32 RankIndex = 1; RankIndex < Ranks.Num(); ++RankIndex)
		{
			SortRank(RankIndex, true);
		}
		for (int32 RankIndex = Ranks.Num() - 2; RankIndex >= 0; --RankIndex)
		{
			SortRank(RankIndex, false);
			if (RankIndex == 0)
			{
				break;
			}
		}
	}
}

void FLayeredGraphLayout::AssignCoordinates(const FLayeredLayoutConfig& Config, TArray<TArray<int32>>& Ranks)
{
	TArray<int32> ColumnWidths;
	ColumnWidths.Init(0, Ranks.Num());
	for (int32 RankIndex = 0; RankIndex < Ranks.Num(); ++RankIndex)
	{
		for (int32 VertexIndex : Ranks[RankIndex])
		{
			ColumnWidths[RankIndex] = FMath::Max(ColumnWidths[RankIndex], Vertices[VertexIndex].Width);
		}
	}

	int32 X = Config.StartX;
	for (int32 RankIndex = 0; RankIndex < Ranks.Num(); ++RankIndex)
	{
		int32 Y = Config.StartY;
		for (int32 VertexIndex : Ranks[RankIndex])
		{
			Vertices[VertexIndex].X = X;
			Vertices[VertexIndex].Y = Y;
			Y += Vertices[VertexIndex].Height + Config.PaddingY;
		}
		X += ColumnWidths[RankIndex] + Config.PaddingX;
	}

	for (int32 Pass = 0; Pass < Config.RelaxationPasses; ++Pass)
	{
		for (int32 RankIndex = 1; RankIndex < Ranks.Num(); ++RankIndex)
		{
			TArray<double> DesiredY;
			TArray<bool> bHasDesiredY;
			DesiredY.Init(0.0, Vertices.Num());
			bHasDesiredY.Init(false, Vertices.Num());

			for (int32 VertexIndex : Ranks[RankIndex])
			{
				double Sum = 0.0;
				double Weight = 0.0;
				for (const FLayeredLayoutEdge& Edge : Edges)
				{
					const int32* SourceIndex = IdToIndex.Find(Edge.SourceId);
					const int32* TargetIndex = IdToIndex.Find(Edge.TargetId);
					if (!SourceIndex || !TargetIndex || *TargetIndex != VertexIndex || Vertices[*SourceIndex].Rank >= RankIndex)
					{
						continue;
					}
					const double EdgeWeight = Edge.bExec ? 3.0 : 1.0;
					Sum += (Vertices[*SourceIndex].Y + Edge.SourcePinYOffset - Edge.TargetPinYOffset) * EdgeWeight;
					Weight += EdgeWeight;
				}
				if (Weight > 0.0)
				{
					DesiredY[VertexIndex] = Sum / Weight;
					bHasDesiredY[VertexIndex] = true;
				}
			}
			ApplyRankY(Config, Ranks[RankIndex], DesiredY, bHasDesiredY);
		}

		for (int32 RankIndex = Ranks.Num() - 2; RankIndex >= 0; --RankIndex)
		{
			TArray<double> DesiredY;
			TArray<bool> bHasDesiredY;
			DesiredY.Init(0.0, Vertices.Num());
			bHasDesiredY.Init(false, Vertices.Num());

			for (int32 VertexIndex : Ranks[RankIndex])
			{
				double Sum = 0.0;
				double Weight = 0.0;
				for (const FLayeredLayoutEdge& Edge : Edges)
				{
					const int32* SourceIndex = IdToIndex.Find(Edge.SourceId);
					const int32* TargetIndex = IdToIndex.Find(Edge.TargetId);
					if (!SourceIndex || !TargetIndex || *SourceIndex != VertexIndex || Vertices[*TargetIndex].Rank <= RankIndex)
					{
						continue;
					}
					const double EdgeWeight = Edge.bExec ? 3.0 : 1.0;
					Sum += (Vertices[*TargetIndex].Y + Edge.TargetPinYOffset - Edge.SourcePinYOffset) * EdgeWeight;
					Weight += EdgeWeight;
				}
				if (Weight > 0.0)
				{
					DesiredY[VertexIndex] = Sum / Weight;
					bHasDesiredY[VertexIndex] = true;
				}
			}
			ApplyRankY(Config, Ranks[RankIndex], DesiredY, bHasDesiredY);
			if (RankIndex == 0)
			{
				break;
			}
		}
	}
}

void FLayeredGraphLayout::ApplyRankY(const FLayeredLayoutConfig& Config, const TArray<int32>& Rank, const TArray<double>& DesiredY, const TArray<bool>& bHasDesiredY)
{
	int32 Y = Config.StartY;
	for (int32 VertexIndex : Rank)
	{
		const int32 TargetY = bHasDesiredY.IsValidIndex(VertexIndex) && bHasDesiredY[VertexIndex]
			? FMath::RoundToInt(DesiredY[VertexIndex])
			: Vertices[VertexIndex].Y;
		Vertices[VertexIndex].Y = FMath::Max(TargetY, Y);
		Y = Vertices[VertexIndex].Y + Vertices[VertexIndex].Height + Config.PaddingY;
	}
}
