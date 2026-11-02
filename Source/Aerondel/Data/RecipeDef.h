#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/AerondelTypes.h"
#include "RecipeDef.generated.h"

UCLASS(BlueprintType)
class AERONDEL_API URecipeDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") EStationType Station = EStationType::Furnace;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") TObjectPtr<class USkillDef> Skill = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") int32 LevelRequired = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") float Xp = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") EToolType ToolRequired = EToolType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") TArray<FItemAmount> Inputs;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") TObjectPtr<class UItemDef> Output = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") int32 OutputAmount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe|Fail") TObjectPtr<class UItemDef> FailItem = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe|Fail") float FailChanceAtReq = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe|Fail") int32 StopFailLevel = 99;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe") float Seconds = 1.8f;

	UFUNCTION(BlueprintPure, Category="Recipe") float FailChance(int32 Level) const
	{
		if (FailChanceAtReq <= 0.f || Level >= StopFailLevel) return 0.f;
		const float T = float(Level - LevelRequired) / FMath::Max(1, StopFailLevel - LevelRequired);
		return FMath::Lerp(FailChanceAtReq, 0.f, FMath::Clamp(T, 0.f, 1.f));
	}

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Recipe"), GetFName()); }
};
