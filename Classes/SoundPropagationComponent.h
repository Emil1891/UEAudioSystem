// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"
#include "SoundPropagationComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GRIM_API USoundPropagationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USoundPropagationComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

#pragma region DataMembers 
	
	// Contains all audio components that should be propagated in the level 
	UPROPERTY()
	TArray<UAudioComponent*> AudioComponents;
	
	// The audio occlusion component that holds all audio comps in the level
	UPROPERTY()
	class UAudioOcclusionComponent* AudioOccComp; 

	class FPathfinder* Pathfinder = nullptr;

	// Map containing the original audio component and the spawned, propagated audio component
	UPROPERTY()
	TMap<UAudioComponent*, UAudioComponent*> PropagatedSounds; // TODO? Can this map replace the audio comp array? 

	// Which sound attenuation that the propagated sound should use 
	UPROPERTY(EditAnywhere)
	USoundAttenuation* PropagatedSoundAttenuation = nullptr;

	// How fast the propagated sound should move when its location changes, interpolates so the change is not instant
	// which would cause a too abrupt switch 
	UPROPERTY(EditAnywhere)
	float PropagateLerpSpeed = 3500.f;

	// Used to determine distance between propagated sound and the original sound source which will determine volume 
	float GridNodeDiameter;

	// If component should be used, used while testing it so components does not crash every level 
	UPROPERTY(EditAnywhere)
	bool bEnabled = true; 

	UPROPERTY()
	class UAudioPlayTimes* AudioPlayTimes;

	UPROPERTY(EditAnywhere)
	float PropagatedVolumeOffset = 0.f;

	// If set to true, all audio components will be propagated. Otherwise only audio components with set tag will be
	// handled 
	UPROPERTY(EditDefaultsOnly)
	bool bPropagateAllSounds = true;

	// The tag that will be looked for on audio components to see if it should be propagated
	// NOTE: only checks if bPropagateAllSounds is set to false 
	UPROPERTY(EditDefaultsOnly)
	FName PropagateCompTag = FName("Propagate");

	// Map containing every audio comp with a path so path does not need to be recalculated if player has not moved
	TMap<UAudioComponent*, TArray<class FGridNode*>> Paths; 

	UPROPERTY(EditAnywhere)
	USoundEffectSourcePresetChain* PropagationSourceEffectChain;

	// Speed of the interpolation of volume changes for the propagated sound 
	UPROPERTY(EditAnywhere) 
	float PropVolumeLerpSpeed = 0.5f; 

#pragma endregion

#pragma region Functions 

	// So Pathfinder class can use this class' data members when performing its line trace 
	friend class FPathfinder;
	
	// Called in begin play to fill the array with the audio comps in the level 
	void SetAudioComponents();
	
	void UpdateSoundPropagation(UAudioComponent* AudioComp, const float DeltaTime);

	bool DoLineTrace(FHitResult& HitResultOut, const FVector& StartLoc, const TArray<AActor*>& ActorsToIgnore) const;

	void RemovePropagatedSound(const UAudioComponent* AudioComp, const float DeltaTime);

	// Returns the created propagated audio component 
	UAudioComponent* SpawnPropagatedSound(UAudioComponent* AudioComp, const FVector& SpawnLocation, const int PathSize);

	// Returns the volume multiplier that the propagated audio source should have based on length from the original
	// source to the propagated audio source 
	void SetPropagatedSoundVolume(const UAudioComponent* AudioComp, UAudioComponent* PropAudioComp, int PathSize, const float DeltaTime) const;

	UFUNCTION()
	void ActorWithCompDestroyed(AActor* DestroyedActor);

	void MovePropagatedAudioComp(UAudioComponent* PropAudioComp, const class FGridNode* ToNode, const float DeltaTime) const;

#pragma endregion 

	// TODO: EVERYTHING BELOW IS SHARED BETWEEN AUDIO OCCLUSION AND THIS CLASS. Is that good or bad?
	// TODO: Allows for separate behaviour for occlusion and propagation but is also at least some duplication 
	
	// Only for debugging so only 1 sound is handled 
	UPROPERTY(EditAnywhere)
	bool bOnlyUseDebugSound = false;

	// The class that are checked to see if they have an audio component, default = all actors 
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> ActorClassToSearchFor = AActor::StaticClass();

	UPROPERTY()
	class UCameraComponent* CameraComp;

	// Which object types that should be considered to block audio, default: WorldStatic 
	UPROPERTY(EditAnywhere)
	TArray<TEnumAsByte<EObjectTypeQuery>> AudioBlockingTypes { TEnumAsByte<EObjectTypeQuery>::EnumType::ObjectTypeQuery1 };

	// These classes will not be searched to check if they have audio components 
	UPROPERTY(EditAnywhere)
	TSet<TSubclassOf<AActor>> ActorClassesToIgnore;
	
	bool ActorShouldBeIgnored(const AActor* Actor); 
};
