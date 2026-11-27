#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Combat/AerondelMob.h"
#include "World/AerondelScalability.h"
#include "Dom/JsonObject.h"

// Ventures group 24 (web adapt/holdraids.js): occasional PvE attacks on your claimed wild plot,
// never on a town house or the farm. Repairing afterward trains Construction.
// Chip throttle: foes chip a nearby piece at most once every 8s (web polish).

namespace VenturesRaids
{
	static constexpr float PlotX = 22.f, PlotZ = 78.f, PlotHalf = 12.f, Grid = 1.5f;
	static constexpr int64 DelayMs = 8 * 60 * 1000;
	static constexpr int64 WarnMs = 45 * 1000;
	static constexpr int64 ChipMs = 8000;
	static const TCHAR* Foes[] = { TEXT("Goblin"), TEXT("Wolf"), TEXT("Ashen raider"), TEXT("Sand Reaver") };
	static bool GfxLow()
	{
		const EAerondelTier T = UAerondelScalability::GetSavedTier();
		return T == EAerondelTier::Low || T == EAerondelTier::Quest;
	}
	static void VAct24(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesRaids;

void UVenturesFeature::RaidClearMobs()
{
	for (const TWeakObjectPtr<AAerondelMob>& W : RaidMobs) if (AAerondelMob* M = W.Get()) M->Destroy();
	RaidMobs.Reset();
}

void UVenturesFeature::ResetRaids()
{
	RaidClearMobs();
	bRaidOn = false;
	bRaidWarned = false;
	RaidHits = 0;
	RaidLastChip = 0;
	RaidStartMs = 0;
	RaidNext = 0;
	RaidRuns = 0;
	RaidSkipped = 0;
	RaidDelayed = 0;
}

void UVenturesFeature::SaveRaids(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("next"), (double)RaidNext);
	S->SetNumberField(TEXT("runs"), RaidRuns);
	S->SetNumberField(TEXT("skipped"), RaidSkipped);
	S->SetNumberField(TEXT("delayed"), RaidDelayed);
	A.SetObjectField(TEXT("raids"), S);
}

void UVenturesFeature::LoadRaids(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("raids"));
	if (!S) return;
	RaidNext = FMath::Max((int64)0, JInt(*S, TEXT("next")));
	RaidRuns = FMath::Max(0, (int32)JInt(*S, TEXT("runs")));
	RaidSkipped = FMath::Max(0, (int32)JInt(*S, TEXT("skipped")));
	RaidDelayed = FMath::Max(0, (int32)JInt(*S, TEXT("delayed")));
}

bool UVenturesFeature::RaidActive() const { return bRaidOn; }

bool UVenturesFeature::RaidStart()
{
	if (!FrameClaimed()) { Say(TEXT("Raids only come for a claimed wild plot. Claim Claimridge first. Town houses and the farm are never raided.")); return false; }
	if (bRaidOn) { Say(TEXT("A raid is already on the plot.")); return false; }
	const int32 L = VLevel();
	const int32 N = FMath::Min(CapFoes(15), GfxLow() ? 6 : 10);
	for (int32 i = 0; i < N; ++i)
	{
		const TCHAR* Type = Foes[FMath::Min((int32)UE_ARRAY_COUNT(Foes) - 1, L / 80 + (int32)(i * 0.2f))];
		const float A = (float)i / (float)N * 2.f * PI, Rad = PlotHalf + 2.f;
		const FVector2D P(PlotX + FMath::Cos(A) * Rad, PlotZ + FMath::Sin(A) * Rad);
		const int32 Hp = 20 + L * 3;
		if (AAerondelMob* M = SpawnTunedMob(Type, P, Type, Hp, FMath::Max(2, L / 8), 4 + L / 10, 4 + L / 10, FMath::Max(1, L), 1.f, false))
			RaidMobs.Add(M);
	}
	bRaidOn = true;
	bRaidWarned = false;
	RaidHits = 0;
	RaidLastChip = 0;
	RaidStartMs = NowMs();
	RaidNext = NowMs() + DelayMs * 2;
	RaidRuns++;
	Say(TEXT("A Holdraid falls on Claimridge! Defend your builds. Town and farm are left alone."));
	return true;
}

bool UVenturesFeature::RaidEnd(bool bWin)
{
	if (!bRaidOn) return false;
	RaidClearMobs();
	bRaidOn = false;
	bRaidWarned = false;
	Say(bWin ? TEXT("The raid breaks. Repair what they broke — that trains Construction.") : TEXT("The raid fades. Repair any damage on the plot."));
	return true;
}

bool UVenturesFeature::RaidDelay()
{
	if (bRaidOn) { Say(TEXT("Too late — the raid is already here. Fight or wait it out.")); return false; }
	RaidNext = NowMs() + 5 * 60 * 1000;
	RaidDelayed++;
	bRaidWarned = false;
	Say(TEXT("You put the raid off for a few minutes."));
	return true;
}

bool UVenturesFeature::RaidSkip()
{
	if (bRaidOn) return false;
	RaidNext = NowMs() + DelayMs * 2;
	RaidSkipped++;
	bRaidWarned = false;
	Say(TEXT("You let this raid pass. The next one will come later."));
	return true;
}

int32 UVenturesFeature::RaidLive() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<AAerondelMob>& W : RaidMobs) if (AAerondelMob* M = W.Get()) if (!M->IsDead()) ++N;
	return N;
}

void UVenturesFeature::TickRaids()
{
	if (!FrameClaimed()) return;
	const int64 Now = NowMs();
	if (RaidNext <= 0) RaidNext = Now + DelayMs + (int64)(FMath::FRand() * DelayMs);
	if (!bRaidOn && !bRaidWarned && Now > RaidNext - WarnMs && Now < RaidNext)
	{
		bRaidWarned = true;
		Say(TEXT("A Holdraid is gathering on Claimridge. You can delay or skip it from Ventures → Holdraids."));
	}
	if (!bRaidOn && Now >= RaidNext) { RaidStart(); return; }
	if (!bRaidOn) return;
	const int32 Live = RaidLive();
	// Foes chip a nearby piece at most once every 8s, so the first piece is not shredded every tick. They never march into town or the farm.
	if (FrameList.Num() && Live && Now - RaidLastChip >= ChipMs)
	{
		const FFramePiece& P = FrameList[0];
		FrameDamage(PlotX + P.Gx * Grid, PlotZ + P.Gz * Grid, 1);
		RaidHits++;
		RaidLastChip = Now;
	}
	if (Now - RaidStartMs > 3 * 60 * 1000 || (RaidMobs.Num() && Live == 0)) RaidEnd(true);
}

void UVenturesFeature::TabRaids(FGPanelModel& M)
{
	if (!FrameClaimed())
	{
		M.Title = TEXT("Holdraids");
		M.Lines.Add(TEXT("Raids only come for a claimed wild plot. Claim Claimridge (Freeframe) first. Town houses and the farm are never raided, and nothing will punish you for not having a plot."));
		return;
	}
	if (bRaidOn)
	{
		M.Title = TEXT("Raid on Claimridge");
		M.Lines.Add(FString::Printf(TEXT("%d foes remain. They will chip your builds if left alone (at most once every 8 seconds). Repair afterward for Construction XP."), RaidLive()));
		VAct24(M, TEXT("The raid has passed"), [this] { RaidEnd(true); }, true, true);
		return;
	}
	const int32 Mins = FMath::Max(0, (int32)FMath::CeilToInt((float)(RaidNext - NowMs()) / 60000.f));
	M.Title = TEXT("Claimridge is quiet");
	M.Lines.Add(FString::Printf(TEXT("Next raid in about %s. A warning comes first. You can delay or skip so a raid never jumps you unprepared."), Mins <= 0 ? TEXT("less than a minute") : *FString::Printf(TEXT("%d minute%s"), Mins, Mins == 1 ? TEXT("") : TEXT("s"))));
	M.Lines.Add(FString::Printf(TEXT("Raids stood: %d · delayed %d · skipped %d"), RaidRuns, RaidDelayed, RaidSkipped));
	VAct24(M, TEXT("Delay a few minutes"), [this] { RaidDelay(); });
	VAct24(M, TEXT("Skip this one"), [this] { RaidSkip(); });
}
