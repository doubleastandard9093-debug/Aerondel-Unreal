#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatMath.generated.h"

/** Classic tick-based melee maths (same formulas as the Unity Combat.cs and the web build). */
UCLASS()
class AERONDEL_API UCombatMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Effective level * (bonus + 64). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Combat") static int32 AttackRoll(int32 AttackLevel, int32 AttackBonus);
	UFUNCTION(BlueprintPure, Category="Aerondel|Combat") static int32 DefenceRoll(int32 DefenceLevel, int32 DefenceBonus);
	UFUNCTION(BlueprintPure, Category="Aerondel|Combat") static float HitChance(int32 AttackRoll, int32 DefenceRoll);
	/** floor(0.5 + effStr * (strBonus + 64) / 640). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Combat") static int32 MaxHit(int32 StrengthLevel, int32 StrengthBonus);
	/** Rolls a hit: 0 on miss, else uniform [0, MaxHit]. VR swings pass a 0..1 power multiplier. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Combat") static int32 RollDamage(int32 AttRoll, int32 DefRoll, int32 MaxHitValue, float Power = 1.f);
	/** 4 xp per damage to the trained style, 1.33 to Hitpoints. */
	static constexpr float XpPerDamage = 4.f;
	static constexpr float HpXpPerDamage = 4.f / 3.f;
};
