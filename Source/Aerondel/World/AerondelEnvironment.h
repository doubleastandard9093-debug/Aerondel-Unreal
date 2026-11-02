#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/AerondelScalability.h"
#include "AerondelEnvironment.generated.h"

class UVolumetricCloudComponent;

/**
 * Tunes the level's existing sun / SkyAtmosphere / SkyLight / height fog / unbound PostProcessVolume per graphics tier,
 * live when the tier changes. High/Ultra: atmosphere-driven sun with light shafts and cloud shadows, real-time sky
 * capture, volumetric fog, volumetric clouds (engine cloud material, soft-loaded only here), richer post (Lumen
 * quality, AO, bloom, filmic grade). Quest/Low: clouds destroyed, volumetrics off, plain post; nothing High is loaded.
 * Spawned by AAerondelGameMode::BeginPlay.
 */
UCLASS()
class AERONDEL_API AAerondelEnvironment : public AActor
{
	GENERATED_BODY()
public:
	AAerondelEnvironment();
	void Apply(EAerondelTier Tier);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	FDelegateHandle TierHandle;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
};
