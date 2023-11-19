// Fill out your copyright notice in the Description page of Project Settings.


#include "Pathfinder.h"
#include "MapGrid.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SoundPropagationComponent.h"

// Tried overloading comparison operator (<) in GridNode class but did not seem to work,
// using a predicate does seem to work even though it is somewhat clunkier
namespace
{
	auto HeapPred = [](const FGridNode& Left, const FGridNode& Right)
	{
		if(Left.GetFCost() == Right.GetFCost()) // If FCost is the same, check HCost 
			return Left.HCost < Right.HCost; 

		// Otherwise, FCost decides priority 
		return Left.GetFCost() < Right.GetFCost(); 
	};
}

FPathfinder::FPathfinder(AMapGrid* Grid, AActor* Player, USoundPropagationComponent* PropComp) : Grid(Grid), Player(Player), PropComp(PropComp)
{
}

bool FPathfinder::FindPath(const FVector& From, const FVector& To, TArray<FGridNode*>& Path, bool& bOutPlayerHasMoved)
{
	FGridNode* StartNode = Grid->GetNodeFromWorldLocation(From); 
	FGridNode* EndNode = GetTargetNode(To);
	
	// Surely heap is most effective? TArray seems to have support functions for a heap 
	TArray<FGridNode*> ToBeChecked; 
	TSet<FGridNode*> Visited; // Only needs to keep track of which nodes have been visited, set should be effective 

	// Target has not moved, simply return 
	if(EndNode == OldEndNode)
	{
		bOutPlayerHasMoved = false; 
		return true;
	}

	bOutPlayerHasMoved = true; 
	
	// TODO: remove if, bad way of forcing path draw each frame by always updating the path 
	if(!Grid->bDrawPath) 
		OldEndNode = EndNode; 

	// Reset the start node 
	StartNode->GCost = 0;
	StartNode->HCost = 0;

	// Add it to be checked 
	ToBeChecked.HeapPush(StartNode, HeapPred);

	// While there are still nodes to check 
	while(!ToBeChecked.IsEmpty())
	{
		// Remove the node with highest priority (most promising path)
		FGridNode* Current;
		ToBeChecked.HeapPop(Current, HeapPred);

		Visited.Add(Current);

		// If we have reached the end node, a path has been found 
		if(Current == EndNode)
		{
			// Build the path and return 
			Path = GetPath(StartNode, EndNode);
			return true; 
		}

		// For each neighbouring node 
		for(const auto Neighbour : Grid->GetNeighbours(Current))
		{
			// Check if it's walkable or has already been visited 
			if(!Neighbour->IsWalkable() || Visited.Contains(Neighbour))
				continue; // if so, skip it

			// otherwise, calculate the GCost to neighbour
			const int NewGCostToNeighbour = Current->GCost + GetCostToNode(Current, Neighbour); 

			// If new GCost is lower or it has not been added to be checked 
			if(NewGCostToNeighbour < Neighbour->GCost || !ToBeChecked.Contains(Neighbour))
			{
				// Update its G- and HCost (and thus FCost as well)
				Neighbour->GCost = NewGCostToNeighbour;
				Neighbour->HCost = GetCostToNode(Neighbour, EndNode);

				// Set its parent to current to keep track of where we came from (shortest path to the node)
				Neighbour->Parent = Current;

				// Add neighbour to be checked if not already present 
				if(!ToBeChecked.Contains(Neighbour))
					ToBeChecked.HeapPush(Neighbour, HeapPred); 
			}
		}
	}

	// No path found, clear path and return 
	Path.Empty(); 
	return false; 
}

FGridNode* FPathfinder::GetTargetNode(const FVector& TargetLocation) const
{
	FGridNode* TargetNode = Grid->GetNodeFromWorldLocation(TargetLocation);

	const TArray<AActor*> ActorsToIgnore { Player }; 

	// If player resides in an un-walkable node, check its neighbours for a walkable node with line of sight to player
	// The player's node can become a node on other side of walls if it was not for the line trace 
	if(!TargetNode->IsWalkable())
	{
		for(const auto Neighbour : Grid->GetNeighbours(TargetNode))
		{
			if(Neighbour->IsWalkable())
			{
				// Neighbour is valid if no hit occured for the line trace, i.e. has line of sight to player 
				FHitResult HitResult; 
				if(!UKismetSystemLibrary::LineTraceSingleForObjects(PropComp, Neighbour->GetWorldCoordinate(), Player->GetActorLocation(), PropComp->AudioBlockingTypes, false, ActorsToIgnore, EDrawDebugTrace::ForOneFrame, HitResult, true)) 
					return Neighbour;
			}
		}
	}
	
	return TargetNode; 
}

TArray<FGridNode*> FPathfinder::GetPath(const FGridNode* StartNode, FGridNode* EndNode)
{
	// Construct the path by building it using the nodes' parents
	TArray<FGridNode*> Path;

	// Set current to EndNode 
	FGridNode* Current = EndNode;

	// While not reached start 
	while(Current != StartNode)
	{
		// Add the current node to the path and set current to its target 
		Path.Add(Current);
		Current = Current->Parent; 
	}

	// Reverse the path since we constructed it "backwards". Note below: 
	// Since we want to search the path from the audio source but later handle it as if it is from the player.
	// It works out perfectly for us with not reversing it 
	//Algo::Reverse(Path);

	return Path; 
}

int FPathfinder::GetCostToNode(const FGridNode* From, const FGridNode* To) const
{
	// Euclidean distance (The dot product of a vector with itself is its magnitude squared) 
	const FVector DirectionalVector = To->GetWorldCoordinate() - From->GetWorldCoordinate(); 
	// Should return sqrt of Dot Product (or just DirVec.Size) but I think it gives better result without it,
	// (although I think it punishes diagonal movement?) 
	return DirectionalVector.Dot(DirectionalVector); // Seems to be the best (and fastest) approach 
}
