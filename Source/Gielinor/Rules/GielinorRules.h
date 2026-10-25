#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/GielinorTypes.h"
#include "GielinorRules.generated.h"

class AGielinorPlayerState; class UItemDef; class UMobDef; class USpellDef; class UResourceNodeDef;
class UAgilityCourseDef; class UThieveTargetDef; class URuneAltarDef; class UPotionDef;
struct FCombatStyle; struct FArmourSet;

/** Extra multipliers for one swing (abilities, Berserk). Mirrors the web build's `mods`. */
USTRUCT(BlueprintType)
struct FSwingMods
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Accuracy = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bBigHit = false;
	UPROPERTY(BlueprintReadOnly) bool bCrit = false;
};

/** Unique-weapon procs after a hit lands. */
USTRUCT(BlueprintType)
struct FWeaponProcs
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bAgain = false;
	UPROPERTY(BlueprintReadOnly) int32 Heal = 0;
	UPROPERTY(BlueprintReadOnly) int32 Poison = 0;
	UPROPERTY(BlueprintReadOnly) bool bStun = false;
};

/** One gathering attempt. Item null + empty Error = failed roll (keep going). */
USTRUCT(BlueprintType)
struct FGatherResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(BlueprintReadOnly) int32 Count = 0;
	UPROPERTY(BlueprintReadOnly) float Xp = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bToBank = false;
	UPROPERTY(BlueprintReadOnly) FText Error;
};

/**
 * Every gameplay formula from the WebGPU build (js/gameplay.js, prayer.js, potions.js, abilities.js, skills.js),
 * ported 1:1 so numbers match. Pure functions of the player state; actors only apply the results.
 * Hitpoints use the web build's x10 scale (Hitpoints level 10 = 100 HP).
 */
UCLASS()
class GIELINOR_API UGielinorRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// ---- Bonus sources ----
	/** Sum of a skill's perk effect up to the player's level, x1.1 with that skill's cape, capped by PERK_CAPS. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float Perk(const AGielinorPlayerState* PS, FName SkillId, FName Key);
	/** Permanent quest bonuses plus quest-point milestones. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float QuestFx(const AGielinorPlayerState* PS, FName Key);
	/** Sum of unique-item effects over everything worn. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float GearFx(const AGielinorPlayerState* PS, FName Key);
	/** Active (complete) armour set effect. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float SetFx(const AGielinorPlayerState* PS, FName Key);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float PrayerFx(const AGielinorPlayerState* PS, FName Key);
	/** Largest timed potion extra (gather, xp). */
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float EffectFx(const AGielinorPlayerState* PS, FName Key);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static FItemBonuses EquipmentBonuses(const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 BoostedLevel(const AGielinorPlayerState* PS, FName SkillId);
	/** Closest armour set: returns false when none worn. */
	static bool SetStatus(const AGielinorPlayerState* PS, const FArmourSet*& OutSet, int32& OutHave, int32& OutNeed);
	/** 1 + quest xp bonuses + worn gear xp + Sage's brew. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float XpMultiplier(const AGielinorPlayerState* PS, FName SkillId);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 MaxHitpoints(const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 MaxPrayerPoints(const AGielinorPlayerState* PS);

	// ---- Toolbelt / gathering ----
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static UItemDef* BestTool(const AGielinorPlayerState* PS, EToolType Type);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static bool OwnsToolType(const AGielinorPlayerState* PS, EToolType Type);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float SuccessChance(int32 Level, int32 Required);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Rules") static FGatherResult GatherAttempt(const AGielinorPlayerState* PS, const UResourceNodeDef* Node, float ExtraBonus = 0.f);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float BurnChance(const AGielinorPlayerState* PS, int32 BurnStop, int32 RecipeLevel);

	// ---- Combat ----
	static const FCombatStyle* CurrentStyle(const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static bool IsRanged(const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 AttackSpeedTicks(const AGielinorPlayerState* PS);
	/** In web tiles (1 tile = 100 cm). */
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float AttackRangeTiles(const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float HitChance(float AttRoll, float DefRoll);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 PlayerMaxHit(const AGielinorPlayerState* PS, const FSwingMods& Mods);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float PlayerAccuracy(const AGielinorPlayerState* PS, const UMobDef* Mob, const FSwingMods& Mods);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float DamageMult(const AGielinorPlayerState* PS, const UMobDef* Mob, int32 MobHp, int32 MobMaxHp);
	/** 0 = miss. Capped at MobHp. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Rules") static int32 RollPlayerHit(const AGielinorPlayerState* PS, const UMobDef* Mob, int32 MobHp, int32 MobMaxHp, UPARAM(ref) FSwingMods& Mods);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Rules") static FWeaponProcs RollWeaponProcs(const AGielinorPlayerState* PS, int32 Damage);
	/** Skill id -> xp for damage dealt with the current style. */
	static TMap<FName, float> CombatXp(const AGielinorPlayerState* PS, int32 Damage);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float MonsterAccuracy(const UMobDef* Mob, const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Rules") static int32 RollMonsterHit(const UMobDef* Mob, const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 ReduceIncoming(const AGielinorPlayerState* PS, int32 Damage, bool bFortify, bool bRage);

	// ---- Magic ----
	/** Rune cost after the equipped staff's free rune. */
	static TArray<TPair<UItemDef*, int32>> RuneCost(const AGielinorPlayerState* PS, const USpellDef* Spell);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static bool CanCast(const AGielinorPlayerState* PS, const USpellDef* Spell, FText& OutReason);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static USpellDef* BestSpell(const AGielinorPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 MagicMaxHit(const AGielinorPlayerState* PS, const USpellDef* Spell);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float MagicAccuracy(const AGielinorPlayerState* PS, const UMobDef* Mob);

	// ---- Activities ----
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float AgilitySuccess(int32 Level, int32 Required, float FailLess);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float ThieveChance(int32 Level, int32 Required, float Bonus);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 RunesPerEssence(const URuneAltarDef* Altar, int32 Level);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 SellPrice(const AGielinorPlayerState* PS, const UItemDef* Item);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 BuyPrice(const AGielinorPlayerState* PS, int32 Price);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 HealAmount(const AGielinorPlayerState* PS, const UItemDef* Food);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float BuryXp(const AGielinorPlayerState* PS, const UItemDef* Bones);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static int32 PotionTicks(const AGielinorPlayerState* PS, int32 BaseTicks);
	UFUNCTION(BlueprintPure, Category="Gielinor|Rules") static float PrayerDrainPerTick(const AGielinorPlayerState* PS);

	/** Web tiles -> Unreal cm. */
	static constexpr float TileCm = 100.f;
	static constexpr float TickSeconds = 0.6f;
	static constexpr int32 PotionBaseTicks = 500;
	static constexpr int32 PoisonEvery = 10;
};
