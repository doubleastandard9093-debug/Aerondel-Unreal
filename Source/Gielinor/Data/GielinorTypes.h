#pragma once
#include "CoreMinimal.h"
#include "GielinorTypes.generated.h"

class UItemDef;
class USkillDef;
class UNpcDef;
class UMobDef;

UENUM(BlueprintType)
// New values are only ever appended (saves and assets store the numeric value).
enum class EToolType : uint8 { None, Axe, Pickaxe, FishingNet, Hammer, Tinderbox, FishingRod, LobsterPot, Harpoon, Knife, Needle, Spade };

UENUM(BlueprintType)
enum class EEquipSlot : uint8 { None, Head, Body, Legs, Weapon, Shield, Hands, Feet, Cape, Neck, Ring, Ammo };

UENUM(BlueprintType)
enum class EResourceKind : uint8 { Tree, Rock, FishingSpot, HerbPatch };

UENUM(BlueprintType)
/** Hand = made from the inventory anywhere (Herbalism, Fletching, most Crafting). Fire = a lit fire (cooks like a Range). */
enum class EStationType : uint8 { Furnace, Anvil, Range, Hand, Fire };

UENUM(BlueprintType)
/** Deliver = the web build's 'bring' (all Items are handed over to the giver). Visit = enter an AQuestPlaceVolume. Xp = earn N xp in Skill. Bury = bury N bones. */
enum class EQuestStepType : uint8 { Talk, Gather, Deliver, Kill, Visit, Xp, Bury };

UENUM(BlueprintType)
enum class ECombatType : uint8 { Melee, Ranged, Magic };

UENUM(BlueprintType)
enum class EQuestState : uint8 { NotStarted, InProgress, Completed };

USTRUCT(BlueprintType)
struct FItemStack
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 0;
	bool IsEmpty() const { return Item == nullptr || Count <= 0; }
};

USTRUCT(BlueprintType)
struct FItemAmount
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Amount = 1;
};

USTRUCT(BlueprintType)
struct FDropEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Min = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Max = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, ClampMax=1)) float Chance = 1.f;
	/** If set, one of these is dropped instead of Item (the web build's random grimy herb pools). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<UItemDef>> OneOf;
};

/** The eight equipment bonuses of the web build (atk, str, ranged, rstr, def, rdef, prayer, mag). */
USTRUCT(BlueprintType)
struct FItemBonuses
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Attack = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Strength = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RangedAttack = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RangedStrength = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Defence = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RangedDefence = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Prayer = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Magic = 0;

	FItemBonuses& operator+=(const FItemBonuses& O)
	{
		Attack += O.Attack; Strength += O.Strength; RangedAttack += O.RangedAttack; RangedStrength += O.RangedStrength;
		Defence += O.Defence; RangedDefence += O.RangedDefence; Prayer += O.Prayer; Magic += O.Magic;
		return *this;
	}
};

USTRUCT(BlueprintType)
struct FXpReward
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkillDef> Skill = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Xp = 0.f;
};

USTRUCT(BlueprintType)
struct FQuestStep
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EQuestStepType Type = EQuestStepType::Talk;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UNpcDef> Npc = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UMobDef> Mob = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Amount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	/** Deliver steps with several items (Item/Amount above is the single-item shorthand). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FItemAmount> Items;
	/** Visit steps: matches AQuestPlaceVolume::PlaceId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PlaceId;
	/** Xp steps: skill id (e.g. "thieving"); Amount is the xp needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SkillId;
	/** Talk steps: what the Npc says when you reach them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FText> NpcLines;
};
