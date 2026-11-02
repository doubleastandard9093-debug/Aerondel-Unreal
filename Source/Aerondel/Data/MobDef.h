#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/AerondelTypes.h"
#include "MobDef.generated.h"

UCLASS(BlueprintType)
class AERONDEL_API UMobDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") int32 CombatLevel = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") int32 Hitpoints = 8;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") int32 MaxHit = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") int32 AttackLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") int32 DefenceLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") bool bAggressive = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") float AggroRadius = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") float LeashRadius = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") float AttackSeconds = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") float RespawnSeconds = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") FLinearColor Tint = FLinearColor::White;
	/** Placeholder body size in cm (until real meshes exist). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") FVector BodySize = FVector(90, 170, 110);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") TArray<FDropEntry> Drops;

	// ---- Web-build parity ----
	/** Optional title shown under bosses' names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") bool bBoss = false;
	/** Ranged attackers are stopped by ranged defence and attack from AttackRange. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") bool bRanged = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") float AttackRange = 170.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob", meta=(ClampMin=0, ClampMax=1)) float PoisonChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") int32 PoisonDamage = 0;
	/** humanoid (default), quad, spider, dragon: picks the placeholder body / future skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") FName Shape = TEXT("humanoid");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") TSoftObjectPtr<class USkeletalMesh> SkeletalMesh;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Mob"), GetFName()); }
};
