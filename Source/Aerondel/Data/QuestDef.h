#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/AerondelTypes.h"
#include "QuestDef.generated.h"

UCLASS(BlueprintType)
class AERONDEL_API UQuestDef : public UPrimaryDataAsset
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

	// ---- Web-build parity ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName Town;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FText StartText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FText ThanksLine;
	/** Skill id -> level needed to start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Requirements") TMap<FName, int32> RequiredLevels;
	/** Quest ids that must be complete first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Requirements") TArray<FName> RequiredQuests;
	/** Reward lines shown in the journal ("300 Cooking XP", ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") TArray<FText> RewardLines;
	/** Permanent bonus once complete. Keys: xp.<skill> / xp.*, dmgVs.<mob id> / dmgVs.*, maxHp, runRegen, sellBonus,
	 *  shopDiscount, dr, gather, rmaxHit, drainLess, potionTime, burnLess, ppRegen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") TMap<FName, float> PermanentBonus;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") FText PermanentBonusText;
	/** True for the web build's 22 hand-scripted quests whose steps still need authoring in Unreal (see docs/NEXT_STEPS.md). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") bool bNeedsStepAuthoring = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Quest"), GetFName()); }
};
