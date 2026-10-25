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
class GIELINOR_API USkillDef : public UPrimaryDataAsset
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
 * Gielinor Lite XP curve (same as the web/WebGPU build so saves and balance match):
 * xp(L) = sum_{l=1}^{L-1} floor(50*l + 2*l^1.6). L2 = 52, L10 = 2,514, L99 = 359,720.
 */
UCLASS()
class GIELINOR_API UXpCurve : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxTableLevel = 120;
	UFUNCTION(BlueprintPure, Category="Gielinor|Skills") static int32 XpForLevel(int32 Level);
	UFUNCTION(BlueprintPure, Category="Gielinor|Skills") static int32 LevelForXp(float Xp, int32 MaxLevel = 99);
private:
	static const TArray<int32>& Table();
};
