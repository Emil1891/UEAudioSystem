// Fill out your copyright notice in the Description page of Project Settings.

#include "GridNode.h"

FGridNode::FGridNode(const bool bIsWalkable, const FVector WorldCoord, const int GridX, const int GridY, const int GridZ) :
	GridX(GridX), GridY(GridY), GridZ(GridZ), bWalkable(bIsWalkable), WorldCoordinate(WorldCoord)
{
}
