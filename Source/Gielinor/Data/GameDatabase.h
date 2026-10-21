#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/AssetManager.h"
#include "GameDatabase.generated.h"

class UItemDef; class USkillDef; class UResourceNodeDef; class URecipeDef; class UQuestDef; class UNpcDef; class UMobDef;

/** Single registry of all content. Registered as PrimaryAssetType "GameDatabase" in DefaultGame.ini. */
UCLASS(BlueprintType)
class GIELINOR_API UGameDatabase : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db") TArray<TObjectPtr<UItemDef>> Items;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db") TArray<TObjectPtr<USkillDef>> Skills;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db") TArray<TObjectPtr<UResourceNodeDef>> ResourceNodes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db") TArray<TObjectPtr<URecipeDef>> Recipes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db") TArray<TObjectPtr<UQuestDef>> Quests;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db") TArray<TObjectPtr<UNpcDef>> Npcs;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db") TArray<TObjectPtr<UMobDef>> Mobs;

	UFUNCTION(BlueprintPure, Category="Db") UItemDef* FindItem(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") USkillDef* FindSkill(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UQuestDef* FindQuest(FName Id) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("GameDatabase"), GetFName()); }

	/** Loads (synchronously, it is small) the first GameDatabase primary asset. */
	UFUNCTION(BlueprintPure, Category="Db", meta=(DisplayName="Get Gielinor Database")) static UGameDatabase* Get();
};

/** Custom asset manager (set in DefaultEngine.ini). Preloads the GameDatabase at startup. */
UCLASS()
class GIELINOR_API UGielinorAssetManager : public UAssetManager
{
	GENERATED_BODY()
public:
	virtual void StartInitialLoading() override;
	UGameDatabase* LoadDatabase();
	UPROPERTY(Transient) TObjectPtr<UGameDatabase> Database = nullptr;
};
