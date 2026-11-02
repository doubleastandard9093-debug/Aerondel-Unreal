#include "Combat/CombatMath.h"

int32 UCombatMath::AttackRoll(int32 AttackLevel, int32 AttackBonus) { return (AttackLevel + 8) * (AttackBonus + 64); }
int32 UCombatMath::DefenceRoll(int32 DefenceLevel, int32 DefenceBonus) { return (DefenceLevel + 8) * (DefenceBonus + 64); }

float UCombatMath::HitChance(int32 A, int32 D)
{
	if (A > D) return 1.f - (D + 2.f) / (2.f * (A + 1.f));
	return A / (2.f * (D + 1.f));
}

int32 UCombatMath::MaxHit(int32 StrengthLevel, int32 StrengthBonus)
{
	return FMath::Max(1, FMath::FloorToInt(0.5f + (StrengthLevel + 8) * (StrengthBonus + 64) / 640.f));
}

int32 UCombatMath::RollDamage(int32 AttRoll, int32 DefRoll, int32 MaxHitValue, float Power)
{
	if (FMath::FRand() > HitChance(AttRoll, DefRoll)) return 0;
	const int32 Cap = FMath::Max(0, FMath::RoundToInt(MaxHitValue * FMath::Clamp(Power, 0.f, 1.f)));
	return FMath::RandRange(0, Cap);
}
