// Fill out your copyright notice in the Description page of Project Settings.


#include "AudioOcclusionComponent.h"

#include "ParameterSettings.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values for this component's properties
UAudioOcclusionComponent::UAudioOcclusionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame. You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
}

// Called when the game starts
void UAudioOcclusionComponent::BeginPlay()
{
	Super::BeginPlay();

	bEnabled = UParameterSettings::GetParamSettings()->AudioSystemEnable;
	
	// Dont set up component and dont tick if disabled 
	if(!bEnabled)
	{
		PrimaryComponentTick.bCanEverTick = false;
		return; 
	}
	
	SetAudioComponents();

	CameraComp = GetOwner()->FindComponentByClass<UCameraComponent>();
}

void UAudioOcclusionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	for(const auto AudioComp : AudioComponents)
	{
		if(IsValid(AudioComp))
			AudioComp->GetOwner()->OnDestroyed.RemoveDynamic(this, &UAudioOcclusionComponent::ActorWithCompDestroyed); 
	}
}

// Called every frame
void UAudioOcclusionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(!bEnabled)
		return;
	
	// Gets all audio components in the level, now every tick in case of spawned audio 
	// SetAudioComponents();

	// Add to timer 
	LowPassTimer += DeltaTime;

	// Update all audio components 
	for(UAudioComponent* AudioComp : AudioComponents)
	{
		if(!IsValid(AudioComp))
		{
			//AudioComponents.Remove(AudioComp); 
			continue; 
		}
		
		const float DistanceToAudio = FVector::Dist(GetOwner()->GetActorLocation(), AudioComp->GetComponentLocation());

		// Only update the audio component if it is within fall off distance 
		if(AudioComp->AttenuationSettings->Attenuation.FalloffDistance > DistanceToAudio)
			UpdateAudioComp(AudioComp, DeltaTime);
	}

	// Check if timer exceeded delay after updating all audio comps. If so reset it. Audio Comps have already updated
	// their low pass by now 
	if(LowPassTimer > LowPassUpdateDelay)
		LowPassTimer = 0;
}

void UAudioOcclusionComponent::AddAudioComponentToOcclusion(UAudioComponent* AudioComponent)
{
	// Add if it does not already exist in array 
	if(!AudioComponents.Contains(AudioComponent)) 
		AudioComponents.Add(AudioComponent); 
}

void UAudioOcclusionComponent::SetAudioComponents()
{
	AudioComponents.Empty(); 
	// Find all actors of set class (default all actors)
	TArray<AActor*> AllFoundActors;
	UGameplayStatics::GetAllActorsOfClass(this, ActorClassToSearchFor, AllFoundActors);

	for(const auto Actor : AllFoundActors)
	{
		// TODO: ONLY FOR DEBUGGING TO REMOVE UNWANTED SOUNDS
		 if(bOnlyUseDebugSound && !Actor->GetActorNameOrLabel().Equals("TestSound")) 
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
				if(AudioComp->AttenuationSettings && (bOccludeAllSounds || AudioComp->ComponentHasTag(OccludeCompTag)))
				{
					AudioComponents.Add(AudioComp); // Add it to the array
					// Bind function on destroyed to remove it from the array (NOTE: called when the Actor is removed)
					// And not the audio component 
					AudioComp->GetOwner()->OnDestroyed.AddDynamic(this, &UAudioOcclusionComponent::ActorWithCompDestroyed); 
				}
			}
		}
	}
}

bool UAudioOcclusionComponent::ActorShouldBeIgnored(const AActor* Actor)
{
	for(const auto UnwantedClass : ActorClassesToIgnore)
	{
		if(Actor->GetClass()->IsChildOf(UnwantedClass))
			return true; 
	}

	return false; 
}

bool UAudioOcclusionComponent::DoLineTrace(TArray<FHitResult>& HitResultsOut, const FVector& StartLocation, const FVector& EndLocation, const TArray<AActor*>& ActorsToIgnore) const
{
	return UKismetSystemLibrary::LineTraceMultiForObjects(GetWorld(), StartLocation, EndLocation, AudioBlockingTypes, false, ActorsToIgnore, EDrawDebugTrace::ForOneFrame, HitResultsOut, true); 
}

void UAudioOcclusionComponent::UpdateAudioComp(UAudioComponent* AudioComp, const float DeltaTime)
{
	const TArray<AActor*> ActorsToIgnoreInLineTrace { GetOwner(), AudioComp->GetOwner() }; 
	
	TArray<FHitResult> HitResultsFromPlayer; 
	// No blocking objects 
	if(!DoLineTrace(HitResultsFromPlayer, CameraComp->GetComponentLocation(), AudioComp->GetComponentLocation(), ActorsToIgnoreInLineTrace))
	{
		// Reset values when not blocking 
		ResetAudioComponentOnNoBlock(AudioComp); 
		return;
	}

	// Used to calculate distances that rays travel within objects by also doing a line trace from the audio source
	// resulting in a hit on both sides of the object 
	TArray<FHitResult> HitResultsFromAudio;
	DoLineTrace(HitResultsFromAudio, AudioComp->GetComponentLocation(), CameraComp->GetComponentLocation(), ActorsToIgnoreInLineTrace);
	
	if(HitResultsFromAudio.Num() != HitResultsFromPlayer.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("Ray trace hits not equal for player and audio!, Audio: %i - Player: %i"), HitResultsFromAudio.Num(), HitResultsFromPlayer.Num())
		return; 
	}

	// Reverse the hit results from the audio's perspective so they are in the same order as the player's for easier use 
	Algo::Reverse(HitResultsFromAudio); 

	// Adds all blocking meshes' occlusion values to determine how much the sound should be occluded 
	float TotalOccValue = 0; 
	for(int i = 0; i < HitResultsFromAudio.Num(); i++)
		TotalOccValue += GetOcclusionValue(HitResultsFromPlayer[i], HitResultsFromAudio[i]); 
	
	TotalOccValue = FMath::Clamp(TotalOccValue, 0, 1); 

	// Subtract the volume multiplier with the total occlusion value to determine how low the sound should be, higher
	// occlusion means lower volume 
	AudioComp->SetVolumeMultiplier(FMath::Clamp(1 - TotalOccValue, 0.01f, 1)); 

	// Update LowPass only at set interval for optimization 
	if(LowPassTimer > LowPassUpdateDelay)
		SetLowPassFilter(AudioComp, HitResultsFromPlayer);
}

float UAudioOcclusionComponent::GetOcclusionValue(const FHitResult& HitResultFromPlayer, const FHitResult& HitResultFromAudio) 
{
	const float ThicknessValue = GetThicknessValue(HitResultFromPlayer, HitResultFromAudio);

	const float MaterialValue = GetMaterialValue(HitResultFromPlayer); 

	const float OccValue = ThicknessValue * MaterialValue; 
	
	return FMath::Clamp(OccValue, 0, 1); 
}

float UAudioOcclusionComponent::GetMaterialValue(const FHitResult& HitResult)
{
	// Get all materials from hit component 
	TArray<UMaterialInterface*> Materials; 
	HitResult.GetComponent()->GetUsedMaterials(Materials);

	float MaterialValue = 1; 
	for(const auto& Material : Materials)
	{
		// Get the material value if it has one 
		if(MaterialOcclusionMap.Contains(Material))
		{
			MaterialValue = MaterialOcclusionMap[Material];
			//UE_LOG(LogTemp, Warning, TEXT("Material value: %f"), MaterialValue)
			break; // stop iterating, right material found 
		}
	}

	return MaterialValue; 
}

float UAudioOcclusionComponent::GetLowPassValueBasedOnDistanceToMesh(const FHitResult& HitResultFromPlayer) const
{
	FVector ClosestPointOnMeshToPlayer; // In world coordinates 
	HitResultFromPlayer.GetComponent()->GetClosestPointOnCollision(CameraComp->GetComponentLocation(), ClosestPointOnMeshToPlayer);

	const float DistanceFromPlayerToMeshPoint = FVector::Dist(ClosestPointOnMeshToPlayer, CameraComp->GetComponentLocation());

	// Clamps Low Pass Value between 0 and the max distance, then divides by max distance to give a value between 0 and 1
	const float LowPassValue = FMath::Clamp(DistanceFromPlayerToMeshPoint - DistanceToWallOffset, 0, DistanceToWallToStopAddingLowPass) / DistanceToWallToStopAddingLowPass;
	
	//UE_LOG(LogTemp, Warning, TEXT("LowPassValue based on distance to wall: %f"), LowPassValue);
	
	return LowPassValue;
}

float UAudioOcclusionComponent::GetThicknessValue(const FHitResult& HitResultFromPlayer, const FHitResult& HitResultFromAudio) const
{
	// Get how far the ray traveled through the blocking mesh 
	float RayTravelDistance = FVector::Dist(HitResultFromPlayer.ImpactPoint, HitResultFromAudio.ImpactPoint); 

	// Divide by the max distance to travel before blocking all audio and clamp to get a value between 0 and 1 
	RayTravelDistance /= MaxMeshDistanceToBlockAllAudio; 
	RayTravelDistance = FMath::Clamp(RayTravelDistance, 0, 1); 

	//UE_LOG(LogTemp, Warning, TEXT("RayTravelDistance: %f"), FMath::Clamp(RayTravelDistance, 0, 1)) 

	return RayTravelDistance; 
}

void UAudioOcclusionComponent::ResetAudioComponentOnNoBlock(UAudioComponent* AudioComponent)
{
	//UE_LOG(LogTemp, Warning, TEXT("Volume: 1, Low Pass: Disabled"))
	
	if(AudioComponent->VolumeMultiplier != 1)
		AudioComponent->SetVolumeMultiplier(1);

	// UE seems to not update the low pass filter correctly so if checks are not correct, reset every frame thus needed 
	// if(AudioComponent->bEnableLowPassFilter)
		AudioComponent->SetLowPassFilterEnabled(false);
}

void UAudioOcclusionComponent::SetLowPassFilter(UAudioComponent* AudioComp, const TArray<FHitResult>& HitResultFromPlayer) const
{
	// Enable low pass filter 
	AudioComp->SetLowPassFilterEnabled(true); 

	// Calculate what frequency to filter by, using the distance to the blocking wall 
	float Frequency = MaxLowPassFrequency * GetLowPassValueBasedOnDistanceToMesh(HitResultFromPlayer[0]); 
	// Clamp to ensure a min frequency of 200 and max of set variable 
	Frequency = FMath::Clamp(Frequency, 200, MaxLowPassFrequency); 

	AudioComp->SetLowPassFilterFrequency(Frequency);
	
	// UE_LOG(LogTemp, Warning, TEXT("Volume: %f Frequency: %f"), AudioComp->VolumeMultiplier, Frequency); 
}

void UAudioOcclusionComponent::ActorWithCompDestroyed(AActor* DestroyedActor)
{
	TArray<UActorComponent*> Comps; 
	DestroyedActor->GetComponents(UAudioComponent::StaticClass(), Comps);
	for(const auto Comp : Comps)
	{
		if(auto AudioComp = Cast<UAudioComponent>(Comp)) 
			AudioComponents.Remove(AudioComp); 
	}

	DestroyedActor->OnDestroyed.RemoveDynamic(this, &UAudioOcclusionComponent::ActorWithCompDestroyed); 
}
