// Fill out your copyright notice in the Description page of Project Settings.

#include "MapGrid.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values
AMapGrid::AMapGrid()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

AMapGrid::~AMapGrid()
{
	// does unreal handle this with its garbage collection? 
	delete [] Nodes; 
}

// Called when the game starts or when spawned
void AMapGrid::BeginPlay()
{
	Super::BeginPlay();

	NodeDiameter = NodeRadius * 2;
	
	CreateGrid();
	
	if(bDrawGridNodes && !bDrawOnlyBoxExtentOnTick) 
		DrawDebugStuff();

	// Only enable tick if debugging grid size 
	SetActorTickEnabled(bDrawOnlyBoxExtentOnTick); 
}

void AMapGrid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	DrawDebugBox(GetWorld(), GetActorLocation() + FVector::UpVector * (GridSize.Z / 2), GridSize / 2, FColor::Red, false, -1, 0, 10); 
}

void AMapGrid::CreateGrid()
{
	GridArrayLengthX = FMath::RoundToInt(GridSize.X / NodeDiameter); 
	GridArrayLengthY = FMath::RoundToInt(GridSize.Y / NodeDiameter); 
	GridArrayLengthZ = FMath::RoundToInt(GridSize.Z / NodeDiameter); 

	Nodes = new FGridNode[GridArrayLengthX * GridArrayLengthY * GridArrayLengthZ]; 

	// The grid's pivot is in the center, need its position as if pivot was in the bottom left corner 
	FVector GridBottomLeft = GetActorLocation();
	GridBottomLeft.X -= GridSize.X / 2;
	GridBottomLeft.Y -= GridSize.Y / 2;
	//GridBottomLeft.Z -= GridSize.Z / 2; // Is Z already correct? 

	GridBottomLeftLocation = GridBottomLeft; 

	TArray<AActor*> ActorsToIgnore; 
	
	for(int x = 0; x < GridArrayLengthX; x++)
	{
		for(int y = 0; y < GridArrayLengthY; y++)
		{
			for(int z = 0; z < GridArrayLengthZ; z++)
			{
				FVector NodePos = GridBottomLeft;
				NodePos.X += x * NodeDiameter + NodeRadius; 
				NodePos.Y += y * NodeDiameter + NodeRadius;
				NodePos.Z += z * NodeDiameter + NodeRadius; // Pos now in node center 

				// Check overlap to see if the node is un-walkable 
				TArray<AActor*> OverlappingActors; 
				UKismetSystemLibrary::SphereOverlapActors(this, NodePos, NodeRadius, AudioBlockingObjects, AActor::StaticClass(), ActorsToIgnore, OverlappingActors);

				AddToArray(x, y, z, FGridNode(OverlappingActors.IsEmpty(), NodePos, x, y, z));
			}
		}
	}
}

int AMapGrid::GetIndex (const int IndexX, const int IndexY, const int IndexZ) const
{
	// Source: https://stackoverflow.com/a/34363187 (reworked) 
	return IndexX * GridArrayLengthY * GridArrayLengthZ + IndexZ * GridArrayLengthY + IndexY; 
}

void AMapGrid::AddToArray(const int IndexX, const int IndexY, const int IndexZ, const FGridNode Node)
{
	Nodes[GetIndex(IndexX, IndexY, IndexZ)] = Node;
}

FGridNode* AMapGrid::GetNodeFromArray(const int IndexX, const int IndexY, const int IndexZ) const
{
	return &Nodes[GetIndex(IndexX, IndexY, IndexZ)]; 
}

FGridNode* AMapGrid::GetNodeFromWorldLocation(const FVector WorldLoc) const
{
	// Get coordinates relative to the grid's bottom left corner 
	FVector GridRelative = WorldLoc - GridBottomLeftLocation;
	
	// checks how many nodes "fit" in the relative position for array indexes 
	GridRelative = (GridRelative - NodeRadius) / NodeDiameter;

	// Clamp the result between array index bounds 
	const int x = FMath::Clamp(FMath::RoundToInt(GridRelative.X), 0, GridArrayLengthX - 1); 
	const int y = FMath::Clamp(FMath::RoundToInt(GridRelative.Y), 0, GridArrayLengthY - 1); 
	const int z = FMath::Clamp(FMath::RoundToInt(GridRelative.Z), 0, GridArrayLengthZ - 1); 

	return GetNodeFromArray(x, y, z); 
}

TArray<FGridNode*> AMapGrid::GetNeighbours(const FGridNode* Node) const
{
	TArray<FGridNode*> Neighbours;

	// -1 to plus 1 in each direction to get every neighbour node 
	for(int x = -1; x <= 1; x++)
	{
		for(int y = -1; y <= 1; y++)
		{
			for(int z = -1; z <= 1; z++)
			{
				if(x == 0 && y == 0 && z == 0) // itself 
					continue;

				const int GridX = Node->GridX + x; // Grid indexes 
				const int GridY = Node->GridY + y;
				const int GridZ = Node->GridZ + z; 

				// if any index is out of bounds 
				if(IsOutOfBounds(GridX, GridY, GridZ))
					continue;

				Neighbours.Add(GetNodeFromArray(GridX, GridY, GridZ));
			}
		}
	}

	return Neighbours; 
}

bool AMapGrid::IsOutOfBounds(const int GridX, const int GridY, const int GridZ) const
{
	return GridX < 0 || GridX > GridArrayLengthX - 1 || GridY < 0 || GridY > GridArrayLengthY - 1 || GridZ < 0 || GridZ > GridArrayLengthZ - 1; 
}

void AMapGrid::DrawDebugStuff() const
{
	// Draw border of grid 
	DrawDebugBox(GetWorld(), GetActorLocation() + FVector::UpVector * (GridSize.Z / 2), GridSize / 2, FColor::Red, false, -1, 0, 10); 

	// draw each node where un-walkable (audio blocking) nodes are red and walkable green 
	for(int x = 0; x < GridArrayLengthX; x++)
	{
		for(int y = 0; y < GridArrayLengthY; y++)
		{
			for(int z = 0; z < GridArrayLengthZ; z++)
			{
				const FGridNode* Node = GetNodeFromArray(x, y, z);
				FColor Color = Node->IsWalkable() ? FColor::Green : FColor::Red; 
				DrawDebugBox(GetWorld(), Node->GetWorldCoordinate(), FVector(NodeRadius, NodeRadius, 1), Color, true);
			}
		}
	}

	// prints some stuff 
	UE_LOG(LogTemp, Warning, TEXT("diameter: %f"), NodeDiameter)
	UE_LOG(LogTemp, Warning, TEXT("Grid Length: (X: %i, Y: %i, Z: %i)"), GridArrayLengthX, GridArrayLengthY, GridArrayLengthZ)
	UE_LOG(LogTemp, Warning, TEXT("GridSize: %s"), *GridSize.ToString())

	UE_LOG(LogTemp, Warning, TEXT("Number of nodes: %i"), GridArrayLengthX * GridArrayLengthY * GridArrayLengthZ)
}
