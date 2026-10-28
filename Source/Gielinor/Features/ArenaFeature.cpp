#include "Features/ArenaFeature.h"
#include "Features/StandingFeature.h"
#include "Math/RandomStream.h"

namespace ArenaData
{
	static const TCHAR* Names[] = { TEXT("Sela Thorn"), TEXT("Garrick Vane"), TEXT("Old Dunmore"), TEXT("Pip Ashcombe"), TEXT("Rhosyn Vale"), TEXT("Tobiah Marsh"), TEXT("Ilka Brand"), TEXT("Corwen Hale"), TEXT("Mabry Fenn") };
}

TArray<FLadderEntry> UArenaFeature::Ladder(int64 WeekNumber)
{
	FRandomStream R((int32)(9000 + WeekNumber));
	TArray<FLadderEntry> L;
	for (const TCHAR* N : ArenaData::Names) { FLadderEntry E; E.Name = N; E.Rating = FMath::RoundToInt(850.f + R.FRand() * 750.f); L.Add(E); }
	L.Sort([](const FLadderEntry& A, const FLadderEntry& B) { return A.Rating > B.Rating; });
	return L;
}

int32 UArenaFeature::Rank() const
{
	int32 R = 1; for (const FLadderEntry& E : Ladder(Week ? Week : WeekNo(NowMs()))) if (E.Rating > Rating) ++R;
	return R;
}

FString UArenaFeature::WeekRoll()
{
	const int64 W = WeekNo(NowMs()); if (Week == W) return FString();
	FString Out;
	if (Week && Wins + Losses > 0)
	{
		const int32 R = Rank(), Pay = R == 1 ? 40 : R <= 3 ? 25 : R <= 6 ? 15 : 5;
		Tokens += Pay; Rating = FMath::RoundToInt(1000.f + (Rating - 1000) / 2.f);
		Out = FString::Printf(TEXT("Last week you placed #%d on the ladder: +%d arena tokens."), R, Pay);
	}
	Week = W; return Out;
}

FLadderEntry UArenaFeature::PickOpponent() const
{
	const TArray<FLadderEntry> L = Ladder(WeekNo(NowMs()));
	TArray<FLadderEntry> Above; for (int32 i = L.Num() - 1; i >= 0; --i) if (L[i].Rating >= Rating - 60) Above.Add(L[i]);
	const TArray<FLadderEntry>& Pool = Above.Num() ? Above : L;
	const int32 N = FMath::Min(3, Pool.Num());
	return N ? Pool[FMath::RandRange(0, N - 1)] : FLadderEntry();
}

FDuellistStats UArenaFeature::DuellistStats(const FDuellistStats& Me, int32 OppRating)
{
	const float K = FMath::Clamp(0.85f + (OppRating - 1000) / 2000.f, 0.7f, 1.25f);
	FDuellistStats S;
	S.Hp = FMath::RoundToInt(Me.Hp * K); S.MaxHit = FMath::Max(6, FMath::RoundToInt(Me.MaxHit * K));
	S.Att = FMath::Max(5, FMath::RoundToInt(Me.Att * K)); S.Def = FMath::Max(5, FMath::RoundToInt(Me.Def * K));
	S.Level = FMath::Max(3, FMath::RoundToInt(Me.Level * K)); S.bRanged = Me.bRanged;
	return S;
}

int32 UArenaFeature::RateDuel(int32 OppRating, bool bWin)
{
	const float Exp = 1.f / (1.f + FMath::Pow(10.f, (OppRating - Rating) / 400.f));
	const int32 D = FMath::RoundToInt(32.f * ((bWin ? 1.f : 0.f) - Exp));
	Rating = FMath::Clamp(Rating + D, 100, 3000); Best = FMath::Max(Best, Rating);
	if (bWin) { ++Wins; Tokens += 10; } else { ++Losses; Tokens += 2; }
	return D;
}

FString UArenaFeature::Buy(FName ShopId)
{
	struct FS { const TCHAR* Id; int32 Cost; int32 Rank; };
	static const FS Shop[] = { { TEXT("cape"), 60, 0 }, { TEXT("band"), 120, 3 }, { TEXT("watch"), 25, 0 }, { TEXT("tutor"), 15, 0 } };
	const FS* S = nullptr; for (const FS& X : Shop) if (ShopId == FName(X.Id)) S = &X;
	if (!S) return TEXT("Unknown item.");
	if (S->Rank && Rank() > S->Rank) return FString::Printf(TEXT("Only the top %d of the ladder may buy that."), S->Rank);
	if (Tokens < S->Cost) return FString::Printf(TEXT("That costs %d tokens; you have %d."), S->Cost, Tokens);
	Tokens -= S->Cost;
	if (ShopId == FName(TEXT("cape"))) GiveItem(TEXT("Duellist's cape"), 1);
	else if (ShopId == FName(TEXT("band"))) GiveItem(TEXT("Victor's band"), 1);
	else if (ShopId == FName(TEXT("watch"))) { if (UStandingFeature* St = Feature<UStandingFeature>()) St->AddRep(FName(TEXT("watch")), 100); }
	else
	{
		const TCHAR* Low = TEXT("Attack"); int32 L = SkillLevel(Low);
		for (const TCHAR* Sk : { TEXT("Strength"), TEXT("Defence"), TEXT("Ranged"), TEXT("Magic"), TEXT("Hitpoints") }) if (SkillLevel(Sk) < L) { L = SkillLevel(Sk); Low = Sk; }
		GiveXp(Low, 2500.f);
	}
	return TEXT("Bought.");
}

void UArenaFeature::SaveState(FJsonObject& Out) const
{
	Out.SetNumberField(TEXT("rating"), Rating); Out.SetNumberField(TEXT("tokens"), Tokens); Out.SetNumberField(TEXT("wins"), Wins);
	Out.SetNumberField(TEXT("losses"), Losses); Out.SetNumberField(TEXT("best"), Best); Out.SetNumberField(TEXT("week"), (double)Week);
}

void UArenaFeature::LoadState(const FJsonObject& In)
{
	Rating = FMath::Clamp((int32)JInt(In, TEXT("rating"), 1000), 100, 3000); Tokens = FMath::Clamp((int32)JInt(In, TEXT("tokens")), 0, 1000000);
	Wins = FMath::Max(0, (int32)JInt(In, TEXT("wins"))); Losses = FMath::Max(0, (int32)JInt(In, TEXT("losses")));
	Best = FMath::Max(Rating, (int32)JInt(In, TEXT("best"), 1000)); Week = FMath::Max<int64>(0, JInt(In, TEXT("week")));
}
