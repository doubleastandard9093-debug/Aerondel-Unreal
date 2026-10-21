#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/GielinorTypes.h"
#include "QuestDef.generated.h"

UCLASS(BlueprintType)
class GIELINOR_API UQuestDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UNpcDef> Giver = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FText Difficulty;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") int32 QuestPoints = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") TArray<FText> IntroLines;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FText EndLine;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") TArray<FQuestStep> Steps;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") TArray<FXpReward> XpRewards;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") TArray<FItemAmount> ItemRewards;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") int32 Coins = 0;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Quest"), GetFName()); }
};
