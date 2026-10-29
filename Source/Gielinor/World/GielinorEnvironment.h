#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/GielinorScalability.h"
#include "GielinorEnvironment.generated.h"

class UVolumetricCloudComponent;

/**
 * Tunes the level's existing sun / SkyAtmosphere / SkyLight / height fog / unbound PostProcessVolume per graphics tier,
 * live when the tier changes. High/Ultra: atmosphere-driven sun with light shafts and cloud shadows, real-time sky
 * capture, volumetric fog, volumetric clouds (engine cloud material, soft-loaded only here), richer post (Lumen
 * quality, AO, bloom, filmic grade). Quest/Low: clouds destroyed, volumetrics off, plain post; nothing High is loaded.
 * Spawned by AGielinorGameMode::BeginPlay.
 */
UCLASS()
class GIELINOR_API AGielinorEnvironment : public AActor
{
	GENERATED_BODY()
public:
	AGielinorEnvironment();
	void Apply(EGielinorTier Tier);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	FDelegateHandle TierHandle;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
};
