// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioOcclusionComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GRIM_API UAudioOcclusionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAudioOcclusionComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* Function to manually add an audio component to also occlude, only necessary for components not present in level
	 * on begin play. Checks if it is already added to prevent duplicates */
	// TODO: This is not needed if we find added audio comps ourselves (preferable)
	void AddAudioComponentToOcclusion(UAudioComponent* AudioComponent); 

private: 

#pragma region DataMembers
	
	// Array holding all audio components in the level 
	TArray<UAudioComponent*> AudioComponents;

	// The class that are checked to see if they have an audio component, default = all actors 
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> ActorClassToSearchFor = AActor::StaticClass();

	// These classes will not be searched to check if they have audio components 
	UPROPERTY(EditAnywhere)
	TSet<TSubclassOf<AActor>> ActorClassesToIgnore; 
	
	// The camera's location will be the start location for all line traces as it is located in the player's "head" 
	UPROPERTY()
	class UCameraComponent* CameraComp = nullptr;
	
	// Which object types that should be considered to block audio, default: WorldStatic  
	UPROPERTY(EditAnywhere)
	TArray<TEnumAsByte<EObjectTypeQuery>> AudioBlockingTypes { TEnumAsByte<EObjectTypeQuery>::EnumType::ObjectTypeQuery1 };

	// How far audio can travel through meshes until it should be completely blocked/silenced 
	UPROPERTY(EditAnywhere)
	float MaxMeshDistanceToBlockAllAudio = 900.f;
	
	// When this distance away from the wall or more, low pass will be the same 
	UPROPERTY(EditAnywhere)
	float DistanceToWallToStopAddingLowPass = 700.f;

	// The maximum frequency to allow from audio sources when occluded 
	UPROPERTY(EditAnywhere)
	float MaxLowPassFrequency = 17000.f; 

	// Map containing materials with custom set occlusion multiplier. A higher value blocks more sound and vice versa 
	UPROPERTY(EditAnywhere)
	TMap<UMaterialInterface*, float> MaterialOcclusionMap;

	// Offset used for muffling sound when player is close to a wall. Higher value means muffling further from the wall 
	UPROPERTY(EditAnywhere)
	float DistanceToWallOffset = 60.f;

	// Timer to keep track of when to update low pass for audio sources 
	float LowPassTimer = 0; 

	// How often lowpass should be updated 
	UPROPERTY(EditAnywhere)
	float LowPassUpdateDelay = 0.1f;

	UPROPERTY(EditAnywhere)
	bool bOnlyUseDebugSound = false; 

	// If component should be used, used while testing it so components does not crash every level 
	UPROPERTY(EditAnywhere)
	bool bEnabled = true;

	// If set to true, all audio components will be occluded. Otherwise only audio components with set tag will be
	// handled 
	UPROPERTY(EditDefaultsOnly)
	bool bOccludeAllSounds = true;

	// The tag that will be looked for on audio components to see if it should be occluded
	// NOTE: only checks if bOccludeAllSounds is set to false 
	UPROPERTY(EditDefaultsOnly)
	FName OccludeCompTag = FName("Occlude");

#pragma endregion

#pragma region Functions 
	
	// Called in begin play to fill the array with the audio comps in the level 
	void SetAudioComponents();

	// Helper func to do line trace 
	bool DoLineTrace(TArray<FHitResult>& HitResultsOut, const FVector& StartLocation, const FVector& EndLocation, const TArray<AActor*>& ActorsToIgnore) const;
	
	void UpdateAudioComp(UAudioComponent* AudioComp, const float DeltaTime);

	// Gets the total occlusion value between 0 and 1 
	float GetOcclusionValue(const FHitResult& HitResultFromPlayer, const FHitResult& HitResultFromAudio);

	// Returns a value between zero and i based on player's distance to the blocking wall  
	float GetLowPassValueBasedOnDistanceToMesh(const FHitResult& HitResultFromPlayer) const;

	// Gets a value clamped between 0 and 1 based on the thickness of the meshes between the player and the audio source
	float GetThicknessValue(const FHitResult& HitResultFromPlayer, const FHitResult& HitResultFromAudio) const;

	float GetMaterialValue(const FHitResult& HitResult); 

	void ResetAudioComponentOnNoBlock(UAudioComponent* AudioComponent);

	void SetLowPassFilter(UAudioComponent* AudioComp, const TArray<FHitResult>& HitResultFromPlayer) const;

	UFUNCTION()
	void ActorWithCompDestroyed(AActor* DestroyedActor);

	bool ActorShouldBeIgnored(const AActor* Actor); 

#pragma endregion
	
};
