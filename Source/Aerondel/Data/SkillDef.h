#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillDef.generated.h"

/** A perk unlocked at a level (web build: every 5 levels, milestones at 25/50/75/99). */
USTRUCT(BlueprintType)
struct FSkillPerk
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Level = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bMilestone = false;
	/** Effect key -> value, e.g. acc=0.01, maxHit=1, double=0.08. Summed for all perks at or below the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Effects;
};

UCLASS(BlueprintType)
class AERONDEL_API USkillDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill") FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill") TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill") int32 StartLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill") int32 MaxLevel = 99;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill") TArray<FSkillPerk> Perks;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Skill"), GetFName()); }
};

/**
 * Aerondel Lite v13 XP curve (same as the web build): classic-shaped, scaled to 30%.
 * xp(L) = floor(sum_{l=1}^{L-1} floor(l + 300*2^(l/7)) / 4 * 0.3). L2 = 24, L10 = 346, L99 = 3,910,329.
 * Saves older than v4 used the old curve (L99 = 359,720) and are converted level-for-level by ConvertXpV1.
 */
UCLASS()
class AERONDEL_API UXpCurve : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxTableLevel = 120;
	static constexpr double XpScale = 0.3;
	UFUNCTION(BlueprintPure, Category="Aerondel|Skills") static int32 XpForLevel(int32 Level);
	UFUNCTION(BlueprintPure, Category="Aerondel|Skills") static int32 LevelForXp(float Xp, int32 MaxLevel = 99);
	/** Old-curve XP -> new-curve XP, keeping the level and the fraction through it. Above 99: extra XP x10. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Skills") static float ConvertXpV1(float Xp);
private:
	static const TArray<int32>& Table();
	static const TArray<int32>& TableV1();
};
