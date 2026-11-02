#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/AerondelTypes.h"
#include "ContentDefs.generated.h"

class UItemDef; class USkillDef;

/**
 * Content types added for parity with the Aerondel Lite WebGPU build.
 * Every one is a UPrimaryDataAsset (author them in the editor) and is also created at runtime from Content/Data/*.json
 * by FJsonContentLoader, so the web build's tables can be re-exported with Tools/export_webgpu_data.mjs.
 */

USTRUCT(BlueprintType)
struct FRuneCost
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UItemDef> Rune = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Amount = 1;
};

/** Combat spell (Magic). Damage = 1 + rand(MaxHit + magic bonus). Xp = Xp*2 + dmg*0.6 Magic, dmg*0.4 Hitpoints. */
UCLASS(BlueprintType)
class AERONDEL_API USpellDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") int32 MaxHit = 20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") float Xp = 5.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") TArray<FRuneCost> Runes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") FLinearColor Color = FLinearColor::White;
	/** Hexcraft: rot | bind | wither | leech (None for elemental spells). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell") FName Hex;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Spell"), GetFName()); }
};

/** Prayer. Only one prayer per Group may be active. Effects: att, str, def, rng, protect, regen, saveMat, gather. */
UCLASS(BlueprintType)
class AERONDEL_API UPrayerDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") int32 Level = 1;
	/** Prayer points drained per 0.6 s tick, divided by (1 + prayer bonus / 30). Points are x10 (max = Prayer level * 10). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") float DrainPerTick = 0.17f;
	/** Quest id that must be complete to use it (Ward against Melee: fallen). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") FName RequiredQuest;
	/** def, str, att, rng, heal, hands, gather, protect. One per group; rng also clashes with att and str. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") FName Group;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prayer") TMap<FName, float> Effects;
	UFUNCTION(BlueprintPure, Category="Prayer") float GetEffect(FName Key) const { const float* V = Effects.Find(Key); return V ? *V : 0.f; }
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Prayer"), GetFName()); }
};

/** Potion. Boosts are Flat + Pct * level and do not stack (largest wins). Instant kinds: antipoison, energy, prayer, restore. */
UCLASS(BlueprintType)
class AERONDEL_API UPotionDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") FText Description;
	/** Skill id -> flat boost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") TMap<FName, int32> BoostFlat;
	/** Skill id -> fraction of level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") TMap<FName, float> BoostPct;
	/** Timed extras while active (gather, xp). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") TMap<FName, float> Fx;
	/** Empty for boost potions, otherwise antipoison / energy / prayer / restore. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") FName Instant;
	/** Antipoison: ticks of immunity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") int32 ImmuneTicks = 0;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Potion"), GetFName()); }
};

UENUM(BlueprintType)
enum class EAbilityKind : uint8 { Basic, Threshold, Ultimate };

/** Combat ability. Basics build adrenaline (+9), thresholds cost 50, ultimates 100. */
UCLASS(BlueprintType)
class AERONDEL_API UAbilityDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") EAbilityKind Kind = EAbilityKind::Basic;
	/** Ticks (reduced by the Attack abilityCd perk, min 3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") int32 Cooldown = 10;
	/** Unlocked at Level in Skill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FName Skill;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") int32 Level = 1;
	/** any, melee or ranged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FName Style = TEXT("any");
	/** power, puncture, bash, snipe, volley, fortify, rage, rejuv; magic kit windb/waterb/earthb/fireb/surge/storm; paths challenge/wall/mend/ward/rend/flurry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FName Special;
	/** Magic kit: wind | water | earth | fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FName Element;
	/** Runes spent when a magic-kit ability rides on a cast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") TArray<FRuneCost> Runes;
	/** Combat Path that grants it (bulwark | mender | striker), else None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FName Path;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Ability"), GetFName()); }
};

USTRUCT(BlueprintType)
struct FShopStock
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UItemDef> Item = nullptr;
	/** 0 = use the item's value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Price = 0;
	/** Only stocked once this quest is complete. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName UnlockQuest;
	/** Skill capes: only stocked once this skill is 99. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RequiresSkill99;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRequiresQuestCape = false;
};

/** Shop. Sell price = floor(value * 0.6) * (1 + sellBonus); buy price = ceil(price * (1 - shopDiscount)). */
UCLASS(BlueprintType)
class AERONDEL_API UShopDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop") TArray<FShopStock> Stock;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Shop"), GetFName()); }
};

USTRUCT(BlueprintType)
struct FAgilityObstacle
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	/** "Walk-across", "Climb-over"... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Verb;
	/** log, net, rope, pipe, wall, gap... (picks the placeholder mesh / animation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Kind;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Xp = 7.f;
	/** Fail damage (x10 hitpoint scale); actual = FailDamage * (0.5..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 FailDamage = 10;
};

/** Agility course. Obstacles must be done in order; finishing the last gives LapXp. */
UCLASS(BlueprintType)
class AERONDEL_API UAgilityCourseDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility") int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility") float LapXp = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility") TArray<FAgilityObstacle> Obstacles;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("AgilityCourse"), GetFName()); }
};

/** Thieving target (stall or NPC). Chance = clamp(0.6 + (lvl-req)*0.012 + bonus, 0.1, 0.95). Fail = stun + damage. */
UCLASS(BlueprintType)
class AERONDEL_API UThieveTargetDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") float Xp = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") int32 StunTicks = 5;
	/** Fail damage (x10 hitpoint scale); actual = FailDamage * (0.5..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") int32 FailDamage = 10;
	/** Every entry rolls on its own (Chance 1 = always). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") TArray<FDropEntry> Loot;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("ThieveTarget"), GetFName()); }
};

/** Glyphcraft altar. Crafts all essence at once. Runes per essence = 1 + floor(level / MultiplierEvery). */
UCLASS(BlueprintType)
class AERONDEL_API URuneAltarDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Glyphcraft") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Glyphcraft") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Glyphcraft") TObjectPtr<UItemDef> Rune = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Glyphcraft") int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Glyphcraft") float XpPerEssence = 5.f;
	/** 0 = always one rune per essence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Glyphcraft") int32 MultiplierEvery = 11;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Glyphcraft") bool bPureEssenceOnly = false;
	UFUNCTION(BlueprintPure, Category="Glyphcraft") int32 RunesPerEssence(int32 InLevel) const { return MultiplierEvery > 0 ? 1 + InLevel / MultiplierEvery : 1; }
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("RuneAltar"), GetFName()); }
};

/** Armour set: wearing every piece grants Effects (dr, acc, racc, dmgPct). */
USTRUCT(BlueprintType)
struct FArmourSet
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Name;
	/** Slots that must all hold an item of this set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<EEquipSlot> Pieces;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Effects;
};

/** Quest point milestone: unlocks a reward the first time QP >= Points. */
USTRUCT(BlueprintType)
struct FQuestPointMilestone
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Points = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
	/** Same keys as UQuestDef::PermanentBonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Bonus;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UItemDef> Item = nullptr;
};

/** Combat style (melee/ranged). Bonus levels and which skills earn XP. */
USTRUCT(BlueprintType)
struct FCombatStyle
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ECombatType Type = ECombatType::Melee;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 AttackBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 StrengthBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DefenceBonus = 0;
	/** Ranged rapid: -1 tick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SpeedTicks = 0;
	/** Ranged longrange: +2 tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RangeTiles = 0.f;
	/** Skill ids sharing the 1.2 xp/damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> XpSkills;
};

/** A town / named place (web-build world units). */
USTRUCT(BlueprintType)
struct FTownInfo
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D Position = FVector2D::ZeroVector;
	/** Name banner over the town gate ("Royal City of Highcrest"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Banner;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Motto;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Blurb;
	/** Standing faction id (watch, mariners, circle, lantern) or None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Faction;
	/** Waystone spot: fast travel lands here (web units). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D TravelPoint = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHasBank = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 StoreCount = 0;
	/** Names this town had before the v5 rename (Data/TownRenames.h). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FString> LegacyNames;
};

/** Repeatable bounty: kill Count of Mob for Coins and XP. */
USTRUCT(BlueprintType)
struct FBountyDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MobId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Count = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Coins = 0;
	/** Skill id -> xp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Xp;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Ask;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Giver;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Where;
};
