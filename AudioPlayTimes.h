// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioPlayTimes.generated.h"

/*
 * This class holds all audio components in the level and their play times so the sound propagation can start playing
 * at the correct time (in sync). Currently has a problem with multiple audio components playing the same sound and
 * the sync is not perfect either (maybe because of float round off error?), but good enough 
 * This class is based on a solution presented here: https://forums.unrealengine.com/t/how-to-get-current-playback-time-position-of-the-sound-playing-on-an-audio-component/388587/2
 *
 */

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GRIM_API UAudioPlayTimes : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAudioPlayTimes();

	// Sets up all audio components to keep track of their play times 
	void SetPlayTimes(TArray<UAudioComponent*>& AudioComponents);

	// Returns the current play time for the passed audio component or -1 if the audio component does not exist 
	float GetPlayTime(const UAudioComponent* AudioComp) const;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// Map holding all audio components and their sound's current playtime
	UPROPERTY()
	TMap<UAudioComponent*, float> PlayTimes; 

	UFUNCTION()
	void OnPlayBackChanged(const USoundWave* PlayingSoundWave, float PlayBackPercent);

	UFUNCTION()
	void ActorWithCompDestroyed(AActor* DestroyedActor);
};
