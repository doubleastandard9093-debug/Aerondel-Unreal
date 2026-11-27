#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/AerondelTypes.h"
#include "AerondelRules.generated.h"

class AAerondelPlayerState; class UItemDef; class UMobDef; class USpellDef; class UResourceNodeDef;
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
class AERONDEL_API UAerondelRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// ---- Bonus sources ----
	/** Sum of a skill's perk effect up to the player's level, x1.1 with that skill's cape, capped by PERK_CAPS. */
	/** Skill synergy web: every 10 levels in one skill adds a small passive to a related skill (added inside Perk). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float Synergy(const AAerondelPlayerState* PS, FName SkillId, FName Key);
	/** Text lines for the skill guide: links into and out of SkillId. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static TArray<FString> SynergyLines(const AAerondelPlayerState* PS, FName SkillId);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float Perk(const AAerondelPlayerState* PS, FName SkillId, FName Key);
	/** Permanent quest bonuses plus quest-point milestones. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float QuestFx(const AAerondelPlayerState* PS, FName Key);
	/** Sum of unique-item effects over everything worn. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float GearFx(const AAerondelPlayerState* PS, FName Key);
	/** Active (complete) armour set effect. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float SetFx(const AAerondelPlayerState* PS, FName Key);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float PrayerFx(const AAerondelPlayerState* PS, FName Key);
	/** Largest timed potion extra (gather, xp). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float EffectFx(const AAerondelPlayerState* PS, FName Key);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static FItemBonuses EquipmentBonuses(const AAerondelPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 BoostedLevel(const AAerondelPlayerState* PS, FName SkillId);
	/** Closest armour set: returns false when none worn. */
	static bool SetStatus(const AAerondelPlayerState* PS, const FArmourSet*& OutSet, int32& OutHave, int32& OutNeed);
	/** 1 + quest xp bonuses + worn gear xp + Sage's brew. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float XpMultiplier(const AAerondelPlayerState* PS, FName SkillId);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 MaxHitpoints(const AAerondelPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 MaxPrayerPoints(const AAerondelPlayerState* PS);

	// ---- Toolbelt / gathering ----
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static UItemDef* BestTool(const AAerondelPlayerState* PS, EToolType Type);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static bool OwnsToolType(const AAerondelPlayerState* PS, EToolType Type);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float SuccessChance(int32 Level, int32 Required);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Rules") static FGatherResult GatherAttempt(const AAerondelPlayerState* PS, const UResourceNodeDef* Node, float ExtraBonus = 0.f);
	/** Occasional Mining extras beside the ore (web gameplay.js mineSideDrop). Never from essence rocks. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static UItemDef* MineSideDrop(const UItemDef* OreItem);

	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float BurnChance(const AAerondelPlayerState* PS, int32 BurnStop, int32 RecipeLevel);

	// ---- Combat ----
	static const FCombatStyle* CurrentStyle(const AAerondelPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static bool IsRanged(const AAerondelPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 AttackSpeedTicks(const AAerondelPlayerState* PS);
	/** In web tiles (1 tile = 100 cm). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float AttackRangeTiles(const AAerondelPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float HitChance(float AttRoll, float DefRoll);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 PlayerMaxHit(const AAerondelPlayerState* PS, const FSwingMods& Mods);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float PlayerAccuracy(const AAerondelPlayerState* PS, const UMobDef* Mob, const FSwingMods& Mods);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float DamageMult(const AAerondelPlayerState* PS, const UMobDef* Mob, int32 MobHp, int32 MobMaxHp);
	/** 0 = miss. Capped at MobHp. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Rules") static int32 RollPlayerHit(const AAerondelPlayerState* PS, const UMobDef* Mob, int32 MobHp, int32 MobMaxHp, UPARAM(ref) FSwingMods& Mods);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Rules") static FWeaponProcs RollWeaponProcs(const AAerondelPlayerState* PS, int32 Damage);
	/** Skill id -> xp for damage dealt with the current style. */
	static TMap<FName, float> CombatXp(const AAerondelPlayerState* PS, int32 Damage);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float MonsterAccuracy(const UMobDef* Mob, const AAerondelPlayerState* PS);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Rules") static int32 RollMonsterHit(const UMobDef* Mob, const AAerondelPlayerState* PS);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 ReduceIncoming(const AAerondelPlayerState* PS, int32 Damage, bool bFortify, bool bRage);

	// ---- Magic ----
	/** Rune cost after the equipped staff's free rune. */
	static TArray<TPair<UItemDef*, int32>> RuneCost(const AAerondelPlayerState* PS, const USpellDef* Spell);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static bool CanCast(const AAerondelPlayerState* PS, const USpellDef* Spell, FText& OutReason);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static USpellDef* BestSpell(const AAerondelPlayerState* PS);
	/** Web gameplay.js WEAKNESS: spells/kit hits of the mob's weak element deal x1.3. */
	static constexpr float WeakMult = 1.3f;
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static bool WeakTo(const UMobDef* Mob, FName Element);
	/** wind/water/earth/fire from a spell id prefix ("fire_lance" -> fire), else None. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static FName SpellElement(const USpellDef* Spell);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 MagicMaxHit(const AAerondelPlayerState* PS, const USpellDef* Spell);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float MagicAccuracy(const AAerondelPlayerState* PS, const UMobDef* Mob);

	// ---- Activities ----
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float AgilitySuccess(int32 Level, int32 Required, float FailLess);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float ThieveChance(int32 Level, int32 Required, float Bonus);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 RunesPerEssence(const URuneAltarDef* Altar, int32 Level);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 SellPrice(const AAerondelPlayerState* PS, const UItemDef* Item);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 BuyPrice(const AAerondelPlayerState* PS, int32 Price);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 HealAmount(const AAerondelPlayerState* PS, const UItemDef* Food);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float BuryXp(const AAerondelPlayerState* PS, const UItemDef* Bones);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static int32 PotionTicks(const AAerondelPlayerState* PS, int32 BaseTicks);
	UFUNCTION(BlueprintPure, Category="Aerondel|Rules") static float PrayerDrainPerTick(const AAerondelPlayerState* PS);

	/** Web tiles -> Unreal cm. */
	static constexpr float TileCm = 100.f;
	static constexpr float TickSeconds = 0.6f;
	static constexpr int32 PotionBaseTicks = 500;
	static constexpr int32 PoisonEvery = 10;
};
