#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/AssetManager.h"
#include "Data/ContentDefs.h"
#include "GameDatabase.generated.h"

class UItemDef; class USkillDef; class UResourceNodeDef; class URecipeDef; class UQuestDef; class UNpcDef; class UMobDef;
class USpellDef; class UPrayerDef; class UPotionDef; class UAbilityDef; class UShopDef; class UAgilityCourseDef; class UThieveTargetDef; class URuneAltarDef;

/** Single registry of all content. Registered as PrimaryAssetType "GameDatabase" in DefaultGame.ini. */
UCLASS(BlueprintType)
class AERONDEL_API UGameDatabase : public UPrimaryDataAsset
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

	// ---- WebGPU-parity content (authored here or filled from Content/Data/*.json at startup) ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<TObjectPtr<USpellDef>> Spells;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<TObjectPtr<UPrayerDef>> Prayers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<TObjectPtr<UPotionDef>> Potions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<TObjectPtr<UAbilityDef>> Abilities;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<FCombatStyle> MeleeStyles;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<FCombatStyle> RangedStyles;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<FArmourSet> ArmourSets;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") TArray<FBountyDef> Bounties;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Activities") TArray<TObjectPtr<UShopDef>> Shops;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Activities") TArray<TObjectPtr<UAgilityCourseDef>> AgilityCourses;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Activities") TArray<TObjectPtr<UThieveTargetDef>> ThieveTargets;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Activities") TArray<TObjectPtr<URuneAltarDef>> RuneAltars;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Quests") TArray<FQuestPointMilestone> QuestPointMilestones;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|World") TArray<FTownInfo> Towns;
	/** Items every new character starts with on the toolbelt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Player") TArray<TObjectPtr<UItemDef>> StarterTools;
	/** Perk effect key -> cap (web PERK_CAPS). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Player") TMap<FName, float> PerkCaps;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Player") int32 InventorySize = 28;
	/** Hitpoints and food heal are x10 (web HEAL_SCALE). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Player") int32 HealScale = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") float DamageReductionCap = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Db|Combat") int32 SpellTicks = 5;

	UFUNCTION(BlueprintPure, Category="Db") UItemDef* FindItem(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") USkillDef* FindSkill(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UQuestDef* FindQuest(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UMobDef* FindMob(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UNpcDef* FindNpc(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") USpellDef* FindSpell(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UPrayerDef* FindPrayer(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UPotionDef* FindPotionForItem(const UItemDef* Item) const;
	UFUNCTION(BlueprintPure, Category="Db") UAbilityDef* FindAbility(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UShopDef* FindShop(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UAgilityCourseDef* FindAgilityCourse(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UThieveTargetDef* FindThieveTarget(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") URuneAltarDef* FindRuneAltar(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") UResourceNodeDef* FindResourceNode(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Db") float GetPerkCap(FName Key) const { const float* V = PerkCaps.Find(Key); return V ? *V : -1.f; }
	/** Total quest points available (161 with the web data). */
	UFUNCTION(BlueprintPure, Category="Db") int32 TotalQuestPoints() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("GameDatabase"), GetFName()); }

	/** Loads (synchronously, it is small) the first GameDatabase primary asset. */
	UFUNCTION(BlueprintPure, Category="Db", meta=(DisplayName="Get Aerondel Database")) static UGameDatabase* Get();
};

/** Custom asset manager (set in DefaultEngine.ini). Preloads the GameDatabase at startup. */
UCLASS()
class AERONDEL_API UAerondelAssetManager : public UAssetManager
{
	GENERATED_BODY()
public:
	virtual void StartInitialLoading() override;
	/** Loads the authored GameDatabase (or makes an empty transient one) and merges Content/Data/*.json into it. */
	UGameDatabase* LoadDatabase();
	UPROPERTY(Transient) TObjectPtr<UGameDatabase> Database = nullptr;
};
