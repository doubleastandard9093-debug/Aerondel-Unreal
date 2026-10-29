#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/GielinorScalability.h"
#include "GielinorHighDetail.generated.h"

class UInstancedStaticMeshComponent; class UMaterialInterface; class UMeshComponent;

/**
 * High/Ultra-only world fidelity, fully reversible when the tier drops:
 *  - Materials: every surface using an M_Tint instance (ground, buildings, water, roads, props, mob and NPC bodies)
 *    gets a dynamic instance of M_TintHigh (procedural macro variation + animated water shimmer) with the same
 *    Color/Roughness/Metallic. Originals are kept and restored on Low.
 *  - Foliage: procedural grass tufts, flowers and pebbles scattered on grass surfaces around the player (ISMs,
 *    distance-culled, no shadows on grass; Ultra is denser).
 * Low/Quest never load M_TintHigh (soft path, resolved in Apply only when rich). Spawned by AGielinorGameMode.
 */
UCLASS()
class GIELINOR_API AGielinorHighDetail : public AActor
{
	GENERATED_BODY()
public:
	AGielinorHighDetail();
	void Apply(EGielinorTier Tier);
	virtual void Tick(float Dt) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void UpgradeMaterials();
	void RestoreMaterials();
	void Scatter(bool bUltra);
	void ClearFoliage();
	UInstancedStaticMeshComponent* MakeIsm(const TCHAR* Mesh, FLinearColor Color, float Rough, bool bShadow);

	FDelegateHandle TierHandle;
	bool bRich = false, bUltra = false;
	float RescanTimer = 0.f;
	UPROPERTY() TObjectPtr<UMaterialInterface> HighMaster;
	struct FSwap { TWeakObjectPtr<UMeshComponent> Comp; int32 Slot; TWeakObjectPtr<UMaterialInterface> Original; };
	TArray<FSwap> Swaps;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Foliage;
	FVector2D ScatterCentre = FVector2D(1e9, 1e9);
};
