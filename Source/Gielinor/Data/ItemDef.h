#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Data/GielinorTypes.h"
#include "ItemDef.generated.h"

UCLASS(BlueprintType)
class GIELINOR_API UItemDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FText Examine;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FLinearColor Tint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") TSoftObjectPtr<UStaticMesh> WorldMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") bool bStackable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") int32 Value = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") int32 HealAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool") EToolType ToolType = EToolType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool") int32 ToolTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") EEquipSlot EquipSlot = EEquipSlot::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 AttackBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 StrengthBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 DefenceBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 RequiredLevel = 1;

	// ---- Web-build parity (filled by the JSON content loader or by hand) ----
	/** resource, processed, food, weapon, head, body, legs, shield, cape, neck, ring, ammo, tool, rune, herb, grimy, secondary, vial, potion, bones, junk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FName ItemType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") bool bUnique = false;
	/** uncommon / rare / epic / legendary (empty = common). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FName Rarity;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FText SpecialText;
	/** Metal tier 0 (Bronze) .. 6 (Dragon); -1 when not a metal item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") int32 MetalTier = -1;

	/** All eight bonuses. The legacy Attack/Strength/DefenceBonus fields above are added on top. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") FItemBonuses Bonuses;
	/** Skill id -> level needed to wear/use, e.g. defence=40. RequiredLevel above is the legacy Attack/Defence shorthand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") TMap<FName, int32> SkillRequirements;
	/** Game ticks (0.6 s) between attacks for weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 AttackSpeedTicks = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") bool bRangedWeapon = false;
	/** Thrown weapons are their own ammo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") bool bThrown = false;
	/** Armour set name (see UGameDatabase::ArmourSets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") FName SetName;
	/** Skill cape: boosts this skill's perks by 10% while worn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") FName CapeSkill;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") bool bQuestCape = false;
	/** Elemental staff: item id of the rune it supplies for free. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") FName RuneSource;
	/**
	 * Numeric effects summed over worn gear: crit, dmgPct, lifesteal, doubleHit, stun, bigHit, multiArrow, reflect,
	 * potionTime, poison (chance) + poisonDmg, regen, gather.<skill>, xp.<skill>.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") TMap<FName, float> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable") float PrayerXp = 0.f;
	/** Grimy herb -> clean herb id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable") FName CleansInto;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable") int32 HerbLevel = 0;
	/** Food buffs: prayer points restored (x10 like the web build). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable") int32 BuffPrayer = 0;
	/** Food buffs: skill id -> flat boost; percent part in BuffBoostPct. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable") TMap<FName, int32> BuffBoostFlat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable") TMap<FName, float> BuffBoostPct;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable") int32 BuffTicks = 0;

	UFUNCTION(BlueprintPure, Category="Item") bool IsFood() const { return HealAmount > 0; }
	UFUNCTION(BlueprintPure, Category="Item") bool IsTool() const { return ToolType != EToolType::None && EquipSlot == EEquipSlot::None; }
	UFUNCTION(BlueprintPure, Category="Item") float GetEffect(FName Key) const { const float* V = Effects.Find(Key); return V ? *V : 0.f; }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Item"), GetFName()); }
};
