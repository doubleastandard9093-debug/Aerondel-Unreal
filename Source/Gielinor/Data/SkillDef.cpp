#include "Data/SkillDef.h"

const TArray<int32>& UXpCurve::Table()
{
	// v13 web curve: floor(sum floor(l + 300*2^(l/7)) / 4 * 0.3). L99 = 3,910,329; L120 fits in int32.
	static TArray<int32> T = []
	{
		TArray<int32> Out;
		Out.SetNumZeroed(MaxTableLevel + 1);
		int64 Pts = 0;
		for (int32 Lvl = 1; Lvl < MaxTableLevel; ++Lvl)
		{
			Pts += (int64)FMath::FloorToDouble(Lvl + 300.0 * FMath::Pow(2.0, Lvl / 7.0));
			Out[Lvl + 1] = (int32)FMath::Min<int64>((int64)FMath::FloorToDouble(Pts / 4.0 * XpScale), MAX_int32);
		}
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
	if (L >= 99) return XpForLevel(99) + FMath::Max(0.f, Xp - Old[99]) * 10.f;
	const float F = (Xp - Old[L]) / FMath::Max(1, Old[L + 1] - Old[L]);
	return FMath::FloorToFloat(XpForLevel(L) + F * (XpForLevel(L + 1) - XpForLevel(L)));
}

int32 UXpCurve::XpForLevel(int32 Level)
{
	return Table()[FMath::Clamp(Level, 1, MaxTableLevel)];
}

int32 UXpCurve::LevelForXp(float Xp, int32 MaxLevel)
{
	const TArray<int32>& T = Table();
	int32 Lvl = 1;
	const int32 Cap = FMath::Clamp(MaxLevel, 1, MaxTableLevel);
	while (Lvl < Cap && Xp >= T[Lvl + 1]) { ++Lvl; }
	return Lvl;
}
