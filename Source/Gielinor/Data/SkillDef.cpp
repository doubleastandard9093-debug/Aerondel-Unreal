#include "Data/SkillDef.h"

const TArray<int32>& UXpCurve::Table()
{
	static TArray<int32> T = []
	{
		TArray<int32> Out;
		Out.SetNumZeroed(MaxTableLevel + 1);
		double Points = 0.0;
		for (int32 Lvl = 1; Lvl < MaxTableLevel; ++Lvl)
		{
			Points += FMath::FloorToDouble(Lvl + 300.0 * FMath::Pow(2.0, Lvl / 7.0));
			Out[Lvl + 1] = (int32)FMath::FloorToDouble(Points / 4.0);
		}
		return Out;
	}();
	return T;
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
