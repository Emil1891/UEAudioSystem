// Fill out your copyright notice in the Description page of Project Settings.


#include "SoundPropagationComponent.h"

#include "AudioPlayTimes.h"
#include "MapGrid.h"
#include "Pathfinder.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GridNode.h"
#include "ParameterSettings.h"

// Sets default values for this component's properties
USoundPropagationComponent::USoundPropagationComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

}

// Called when the game starts
void USoundPropagationComponent::BeginPlay()
{
	Super::BeginPlay();

	bEnabled = UParameterSettings::GetParamSettings()->AudioSystemEnable; 
	
	// Dont set up component and dont tick if disabled 
	if(!bEnabled)
	{
		SetComponentTickEnabled(false); 
		return; 
	}
	
	const auto Grid = Cast<AMapGrid>(UGameplayStatics::GetActorOfClass(this, AMapGrid::StaticClass()));

	if(!Grid)
	{
		UE_LOG(LogTemp, Error, TEXT("There is no grid in the level. Sound propagation needs a grid added"))
		SetComponentTickEnabled(false); 
		return; 
	}

	GridNodeDiameter = Grid->GetNodeDiameter(); 
	
	Pathfinder = new FPathfinder(Grid, GetOwner(), this);

	SetAudioComponents(); 

	AudioPlayTimes = GetOwner()->FindComponentByClass<UAudioPlayTimes>();
	AudioPlayTimes->SetPlayTimes(AudioComponents);

	CameraComp = GetOwner()->FindComponentByClass<UCameraComponent>(); 
}

void USoundPropagationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	for(const auto AudioComp : AudioComponents)
	{
		if(IsValid(AudioComp))
			AudioComp->GetOwner()->OnDestroyed.RemoveDynamic(this, &USoundPropagationComponent::ActorWithCompDestroyed); 
	}
}

// Called every frame
void USoundPropagationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(!bEnabled)
		return;

	// SetAudioComponents(); 
	
	// Update each audio component's sound propagation 
	for(const auto& AudioComp : AudioComponents) 
	{
		if(!IsValid(AudioComp))
			continue; 
		
		const float DistanceToAudio = FVector::Dist(GetOwner()->GetActorLocation(), AudioComp->GetComponentLocation());

		// Only update the audio component if it is within fall off distance 
		if(AudioComp->AttenuationSettings->Attenuation.FalloffDistance > DistanceToAudio)
			UpdateSoundPropagation(AudioComp, DeltaTime); 
	}
}

void USoundPropagationComponent::SetAudioComponents()
{
	AudioComponents.Empty();
	
	// Find all actors of set class (default all actors)
	TArray<AActor*> AllFoundActors;
	UGameplayStatics::GetAllActorsOfClass(this, ActorClassToSearchFor, AllFoundActors);

	UE_LOG(LogTemp, Warning, TEXT("Found actors: %i"), AllFoundActors.Num())
	for(const auto Actor : AllFoundActors)
	{
		// TODO: ONLY FOR DEBUGGING TO REMOVE UNWANTED SOUNDS
		if(ActorClassesToIgnore.Contains(Actor->GetClass()) || bOnlyUseDebugSound && !Actor->GetActorNameOrLabel().Equals("TestSound"))
			continue;

		// Check if actor is of a class that should be ignored 
		if(ActorShouldBeIgnored(Actor))
			continue; 
		
		// If the actor has audio components 
		TArray<UActorComponent*> Comps;
		Actor->GetComponents(UAudioComponent::StaticClass(), Comps);
		for(const auto Comp : Comps)
		{
			if(auto AudioComp = Cast<UAudioComponent>(Comp))
			{
				// Only add it if it has attenuation (is not 2D) and has tag or all sounds should be occluded 
				if(AudioComp->AttenuationSettings && (bPropagateAllSounds || AudioComp->ComponentHasTag(PropagateCompTag)))
				{
					AudioComponents.Add(AudioComp); // Add it to the array
					UE_LOG(LogTemp, Warning, TEXT("Comp: %s"), *AudioComp->GetOwner()->GetActorNameOrLabel())
					// Bind function on destroyed to remove it from the array (NOTE: called when the Actor is removed)
					// And not the audio component 
					AudioComp->GetOwner()->OnDestroyed.AddDynamic(this, &USoundPropagationComponent::ActorWithCompDestroyed); 
				}
			}
		}
	}
}

bool USoundPropagationComponent::ActorShouldBeIgnored(const AActor* Actor)
{
	for(const auto UnwantedClass : ActorClassesToIgnore)
	{
		if(Actor->GetClass()->IsChildOf(UnwantedClass))
			return true; 
	}

	return false; 
}

void USoundPropagationComponent::UpdateSoundPropagation(UAudioComponent* AudioComp, const float DeltaTime)
{
	// const auto StartTime = FDateTime::Now().GetMillisecond(); // FOR DEBUGGING
	
	// Actors to ignore when doing line traces 
	const TArray<AActor*> ActorsToIgnore { GetOwner(), AudioComp->GetOwner() };

	// First do a line trace from the audio source to the player to see if there is direct line of sight
	// if so, then pathfinding is unnecessary because no propagation will occur 
	FHitResult HitResultToPlayer;
	DoLineTrace(HitResultToPlayer, AudioComp->GetComponentLocation(), ActorsToIgnore); 

	if(!HitResultToPlayer.bBlockingHit) // Nothing blocking the sound 
	{
		// Remove eventual propagated sound and return 
		RemovePropagatedSound(AudioComp, DeltaTime);
		return; 
	}

	bool bPlayerHasMoved = true;

	TArray<FGridNode*> Path; 
	
	if(!Pathfinder->FindPath(AudioComp->GetComponentLocation(), GetOwner()->GetActorLocation(), Path, bPlayerHasMoved))
	{
		// No path found, remove eventual propagated sound and return 
		RemovePropagatedSound(AudioComp, DeltaTime); 
		return; 
	}

	// If player has not moved then path is not calculated so FindPath does not set Path, so we can get the stored path 
	if(!bPlayerHasMoved && Paths.Contains(AudioComp)) 
		Path = Paths[AudioComp];
	else // Player has moved so there is a new path, store it 
		Paths.Add(AudioComp, Path); 
	
	// Iterate through path and find the last node with line of sight to player, that's the location to propagate the sound to 
	for(int i = 1; i < Path.Num(); i++)
	{
		FHitResult HitResult;
		DoLineTrace(HitResult, Path[i]->GetWorldCoordinate(), ActorsToIgnore); 
		
		// If nothing is blocking from the node to player, check next node 
		if(!HitResult.bBlockingHit)
			continue;
		
		UAudioComponent* PropAudioComp = nullptr; 
		
		// if we do not have a propagated sound for that audio comp in the world already 
		if(!PropagatedSounds.Contains(AudioComp))
		{
			PropAudioComp = SpawnPropagatedSound(AudioComp, Path[i - 1]->GetWorldCoordinate(), Path.Num()); 
		} else  // If we do have a propagated sound for that audio comp  
		{
			// Get the propagated audio component 
			PropAudioComp = PropagatedSounds[AudioComp];

			// if it's in the wrong location, lerp it to the correct location to prevent abrupt direction changes,
			// otherwise it's in the correct place already so we dont have to do anything 
			if(!PropAudioComp->GetComponentLocation().Equals(Path[i - 1]->GetWorldCoordinate()))
				MovePropagatedAudioComp(PropAudioComp, Path[i - 1], DeltaTime);
		}

		// Call volume change each update when it's not been removed to lerp the volume
		if(PropAudioComp)
			SetPropagatedSoundVolume(AudioComp, PropAudioComp, Path.Num(), DeltaTime); 
		
		break; // Found the node with block so no need to traverse the path any further 
	}
	
	// TODO: THIS IS ONLY FOR DEBUGGING! REMOVE WHEN DONE!
	// Draw the path (only drawn while there is line of sight), 
	if(Cast<AMapGrid>(UGameplayStatics::GetActorOfClass(this, AMapGrid::StaticClass()))->bDrawPath)
		for(auto Node : Path)
			DrawDebugSphere(GetWorld(), Node->GetWorldCoordinate(), 30, 10, FColor::Red); 

	// DEBUGGING 
	// const auto EndTime = FDateTime::Now().GetMillisecond();
	//if(EndTime - StartTime != 0)
	//	UE_LOG(LogTemp, Warning, TEXT("Update time: %i ms"), EndTime - StartTime)
}

bool USoundPropagationComponent::DoLineTrace(FHitResult& HitResultOut, const FVector& StartLoc, const TArray<AActor*>& ActorsToIgnore) const
{
	// Line trace from the node to player to see if there is line of sight  
	return UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), StartLoc,
		CameraComp->GetComponentLocation(), AudioBlockingTypes, false,
		ActorsToIgnore, EDrawDebugTrace::ForOneFrame, HitResultOut, true); 
}

void USoundPropagationComponent::RemovePropagatedSound(const UAudioComponent* AudioComp, const float DeltaTime)
{
	// if there is propagated sound in the level 
	if(PropagatedSounds.Contains(AudioComp))
	{
		// Volume is set to near zero because of UE optimizations which would lead to the original audio and the
		// propagated sound would be out of sync 
		UAudioComponent* PropAudio = PropagatedSounds[AudioComp]; 
		const float NewVolume = FMath::FInterpConstantTo(PropAudio->VolumeMultiplier, 0.01f, DeltaTime, PropVolumeLerpSpeed); 
		PropAudio->SetVolumeMultiplier(NewVolume); // Interpolates volume to zero

		// UE_LOG(LogTemp, Warning, TEXT("Prop vol: %f"), NewVolume)
	}
}

UAudioComponent* USoundPropagationComponent::SpawnPropagatedSound(UAudioComponent* AudioComp, const FVector& SpawnLocation, const int PathSize) 
{
	UAudioComponent* PropagatedAudioComp = DuplicateObject<UAudioComponent>(AudioComp, AudioComp->GetOwner(), FName(FString("PropagatedSound"))); 

	AudioComp->GetOwner()->AddInstanceComponent(PropagatedAudioComp);
	PropagatedAudioComp->RegisterComponent();
	PropagatedAudioComp->SetWorldLocation(SpawnLocation);
	
	PropagatedAudioComp->AttenuationSettings = PropagatedSoundAttenuation;
	PropagatedAudioComp->SetSourceEffectChain(PropagationSourceEffectChain); 

	// Plays the propagated audio source at the correct start time to keep it in sync with the original
	const float PlayTime = AudioPlayTimes->GetPlayTime(AudioComp);
	PropagatedAudioComp->Play(PlayTime); 
	
	PropagatedSounds.Add(AudioComp, PropagatedAudioComp);

	return PropagatedAudioComp; 
}

void USoundPropagationComponent::MovePropagatedAudioComp(UAudioComponent* PropAudioComp, const FGridNode* ToNode, const float DeltaTime) const
{
	// Moves the Propagated audio component to its correct location 
	const FVector CurrentLoc = PropAudioComp->GetComponentLocation();
	const FVector TargetLoc = ToNode->GetWorldCoordinate(); 
	const FVector InterpolatedLoc = UKismetMathLibrary::VInterpTo_Constant(CurrentLoc, TargetLoc, DeltaTime, PropagateLerpSpeed); 
	PropAudioComp->SetWorldLocation(InterpolatedLoc);
}

void USoundPropagationComponent::SetPropagatedSoundVolume(const UAudioComponent* AudioComp, UAudioComponent* PropAudioComp, const int PathSize, const float DeltaTime) const
{
	const float FalloffDistance = AudioComp->AttenuationSettings->Attenuation.GetMaxFalloffDistance(); 

	// This is an approximation that assumes each node traveled is the same length (diagonal travels are longer)
	const int DistanceFromPropToOriginal = PathSize * GridNodeDiameter; 

	// Calculates the volume by seeing how much percentage the distance from the source is of the max fall off distance,
	// giving a value close to 0 when it's close to the audio source and vice versa. That's why 1 - Value is needed 
	const float TargetVolume = 1 - FMath::Clamp(DistanceFromPropToOriginal / FalloffDistance, 0, 1);

	// Interpolates volume changes to it is not as abrupt 
	const float NewVolume = FMath::FInterpConstantTo(PropAudioComp->VolumeMultiplier, TargetVolume, DeltaTime, PropVolumeLerpSpeed); 

	// UE_LOG(LogTemp, Warning, TEXT("Prop vol: %f"), NewVolume)
	
	PropAudioComp->SetVolumeMultiplier(NewVolume); 
}

void USoundPropagationComponent::ActorWithCompDestroyed(AActor* DestroyedActor)
{
	// Remove the destroyed actor's audio components from the collections 
	TArray<UActorComponent*> Comps; 
	DestroyedActor->GetComponents(UAudioComponent::StaticClass(), Comps);
	for(const auto Comp : Comps)
	{
		if(auto AudioComp = Cast<UAudioComponent>(Comp))
		{
			AudioComponents.Remove(AudioComp);

			if(PropagatedSounds.Contains(AudioComp))
				PropagatedSounds.Remove(AudioComp);

			if(Paths.Contains(AudioComp))
				Paths.Remove(AudioComp); 
		}
	}
	
	DestroyedActor->OnDestroyed.RemoveDynamic(this, &USoundPropagationComponent::ActorWithCompDestroyed); 
}
