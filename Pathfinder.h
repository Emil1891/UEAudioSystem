// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class USoundPropagationComponent;

/**
 * 
 */
class GRIM_API FPathfinder
{
public:
	FPathfinder(class AMapGrid* Grid, AActor* Player, USoundPropagationComponent* PropComp); 

	bool FindPath(const FVector& From, const FVector& To, TArray<class FGridNode*>& Path, bool& bOutPlayerHasMoved);

private:
	AMapGrid* Grid;

	FGridNode* OldEndNode = nullptr;

	TArray<FGridNode*> GetPath(const FGridNode* StartNode, FGridNode* EndNode);

	// Returns an approximate cost to travel between nodes (ignoring obstacles)
	int GetCostToNode(const FGridNode* From, const FGridNode* To) const;

	FGridNode* GetTargetNode(const FVector& TargetLocation) const;

	AActor* Player; 

	USoundPropagationComponent* PropComp; 
	
};
