// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class GRIM_API FGridNode
{
public:
	FGridNode(const bool bIsWalkable, const FVector WorldCoord, const int GridX, const int GridY, const int GridZ);
	FGridNode() {} 

	bool IsWalkable() const { return bWalkable; }

	FVector GetWorldCoordinate() const { return WorldCoordinate; }

	// Will be used to keep track of the path, where the previous/next node is 
	FGridNode* Parent = nullptr; 

	// Grid index(es) (the array), can prob be made private and have getters 
	int GridX = -1;
	int GridY = -1;
	int GridZ = -1;

	// costs 
	int GCost = -1; // cost to get to the this node from start (or cost to start node)
	int HCost = -1; // estimated cost to get to target from this node 

	// F cost, sum of GCost and HCost 
	int GetFCost() const { return GCost + HCost; }
	
private:

	// If the node is walkable 
	bool bWalkable = false;

	FVector WorldCoordinate;
	
};