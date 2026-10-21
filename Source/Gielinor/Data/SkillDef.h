#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillDef.generated.h"

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

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Skill"), GetFName()); }
};

/** RuneScape XP curve: xp(L) = floor( sum_{n=1}^{L-1} floor(n + 300 * 2^(n/7)) / 4 ). L2 = 83, L99 = 13,034,431. */
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
