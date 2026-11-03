#include "Data/SkillDef.h"

const TArray<int64>& UXpCurve::Table()
{
	// v13 web curve to 100: floor(sum floor(l + 300*2^(l/7)) / 4 * 0.3). L99 = 3,910,329.
	// v21 (skills.js XP_TABLE): from 101 each level costs the 98->99 step x (1 + (l-1-99)/40). Needs int64.
	static TArray<int64> T = []
	{
		TArray<int64> Out;
		Out.SetNumZeroed(MaxTableLevel + 1);
		int64 Pts = 0;
		for (int32 Lvl = 1; Lvl < 100; ++Lvl)
		{
			Pts += (int64)FMath::FloorToDouble(Lvl + 300.0 * FMath::Pow(2.0, Lvl / 7.0));
			Out[Lvl + 1] = (int64)FMath::FloorToDouble(Pts / 4.0 * XpScale);
		}
		const int64 D99 = Out[99] - Out[98];
		for (int32 Lvl = 101; Lvl <= MaxTableLevel; ++Lvl)
			Out[Lvl] = Out[Lvl - 1] + (int64)FMath::FloorToDouble((double)D99 * (1.0 + (Lvl - 1 - 99) / 40.0));
		return Out;
	}();
	return T;
}

const TArray<int32>& UXpCurve::TableV1()
{
	static TArray<int32> T = []
	{
		TArray<int32> Out;
		Out.SetNumZeroed(MaxTableLevel + 1);
		int64 Xp = 0;
		for (int32 Lvl = 1; Lvl < MaxTableLevel; ++Lvl)
		{
			Xp += (int64)FMath::FloorToDouble(Lvl * 50.0 + FMath::Pow((double)Lvl, 1.6) * 2.0);
			Out[Lvl + 1] = (int32)FMath::Min<int64>(Xp, MAX_int32);
		}
		return Out;
	}();
	return T;
}

float UXpCurve::ConvertXpV1(float Xp)
{
	const TArray<int32>& Old = TableV1();
	Xp = FMath::Max(0.f, Xp);
	int32 L = 1;
	while (L < 99 && Xp >= Old[L + 1]) ++L;
	if (L >= 99) return (float)XpForLevel(99) + FMath::Max(0.f, Xp - Old[99]) * 10.f;
	const float F = (Xp - Old[L]) / FMath::Max(1, Old[L + 1] - Old[L]);
	return FMath::FloorToFloat((float)XpForLevel(L) + F * (float)(XpForLevel(L + 1) - XpForLevel(L)));
}

int64 UXpCurve::XpForLevel(int32 Level)
{
	return Table()[FMath::Clamp(Level, 1, MaxTableLevel)];
}

int32 UXpCurve::LevelForXp(float Xp, int32 MaxLevel)
{
	const TArray<int64>& T = Table();
	int32 Lvl = 1;
	const int32 Cap = FMath::Clamp(MaxLevel, 1, MaxTableLevel);
	while (Lvl < Cap && (double)Xp >= (double)T[Lvl + 1]) { ++Lvl; }
	return Lvl;
}

int32 UXpCurve::MaxLevelFor(const USkillDef* Skill)
{
	if (!Skill) return SkillMaxLevel;
	static const FName Combat[] = { TEXT("attack"), TEXT("strength"), TEXT("defence"), TEXT("hitpoints"), TEXT("ranged"), TEXT("magic"), TEXT("prayer"), TEXT("swordsmanship") };
	for (const FName& C : Combat) if (Skill->Id == C) return CombatMaxLevel; // FName compare is case-insensitive
	return FMath::Clamp(Skill->MaxLevel, 1, MaxTableLevel - 1);
}
