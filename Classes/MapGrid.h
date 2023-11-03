// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GridNode.h"
#include "GameFramework/Actor.h"
#include "MapGrid.generated.h"

UCLASS()
class GRIM_API AMapGrid : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMapGrid();

	~AMapGrid() override; 

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

public:

	// Returns the node that the World Location is in 
	FGridNode* GetNodeFromWorldLocation(const FVector WorldLoc) const;
	
	FVector GetGridBottomLeftLocation() const { return GridBottomLeftLocation; }

	FVector GetGridSize() const { return GridSize; }
	
	TArray<FGridNode*> GetNeighbours(const FGridNode* Node) const;

	// Temporary bool to know if to draw path, will be removed 
	UPROPERTY(EditAnywhere)
	bool bDrawPath = true;

	float GetNodeDiameter() const { return NodeDiameter; }

private:

#pragma region DataMembers
	
	// 1D array (will be used as if it was 3D) keeping track of all nodes,
	// Unreal does not seem to like creating 3D arrays with Array[][][]
	// https://stackoverflow.com/a/34363187 (source to convert 3D array to 1D) 
	FGridNode* Nodes; 

	// Radius for each node, smaller radius means more accurate but more performance expensive 
	UPROPERTY(EditAnywhere)
	float NodeRadius = 50.f; 
	
	float NodeDiameter;  

	// The size of the grid 
	UPROPERTY(EditAnywhere) 
	FVector GridSize = FVector(100, 100, 100); 

	// Holds the array sizes 
	int GridArrayLengthX; 
	int GridArrayLengthY; 
	int GridArrayLengthZ; 

	FVector GridBottomLeftLocation; 

	// Object that should be considered to block audio, default: world static 
	UPROPERTY(EditAnywhere)
	TArray<TEnumAsByte<EObjectTypeQuery>> AudioBlockingObjects { TEnumAsByte<EObjectTypeQuery>::EnumType::ObjectTypeQuery1 };

#pragma endregion 

#pragma region Functions 

	void CreateGrid();

	void AddToArray(const int IndexX, const int IndexY, const int IndexZ, const FGridNode Node);

	FGridNode* GetNodeFromArray(const int IndexX, const int IndexY, const int IndexZ) const;

	int GetIndex(const int IndexX, const int IndexY, const int IndexZ) const;

	bool IsOutOfBounds(const int GridX, const int GridY, const int GridZ) const; 

#pragma endregion 

#pragma region Debugging

	// Draws the grid's nodes if true, red nodes block audio, green ones do not 
	UPROPERTY(EditAnywhere)
	bool bDrawGridNodes = true; 
	
	// Determines if the grid's extent should be drawn on tick for easier use in determining what size it should be 
	// Note: all other debug is disabled if this is active 
	UPROPERTY(EditAnywhere) 
	bool bDrawOnlyBoxExtentOnTick = false; 
	
	void DrawDebugStuff() const;

#pragma endregion
	
};