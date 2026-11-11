#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Dom/JsonObject.h"

// Ventures group 37 (web adapt/seasonboard.js): daily and weekly challenges award Season score, not Standing.
// Tiers unlock a Lookplate dye, the Ash-thread Kitwright schematic, and a wandering vendor.
// No paid pass, no other-player ladder, no punishment for skipping a day.
// Writs and the Hearthboard stay the Standing track.

namespace VenturesScore
{
	static const TCHAR* SeasonId = TEXT("ashen-paths");
	struct FTask { const TCHAR* Id; const TCHAR* Kind; int32 Pts; const TCHAR* Name; };
	static const FTask Tasks[] = {
		{ TEXT("mine"), TEXT("daily"), 10, TEXT("Mine a rock") },
		{ TEXT("hunt"), TEXT("daily"), 10, TEXT("Carve a hunt") },
		{ TEXT("climb"), TEXT("daily"), 10, TEXT("Climb a face") },
		{ TEXT("tide"), TEXT("weekly"), 25, TEXT("Finish a Tide") },
		{ TEXT("camp"), TEXT("weekly"), 25, TEXT("Place a Waycamp") },
		{ TEXT("trail"), TEXT("weekly"), 25, TEXT("Win a Trailhand hand") },
	};
	struct FTier { const TCHAR* Id; int32 At; const TCHAR* Name; };
	static const FTier Tiers[] = {
		{ TEXT("dye-ashveil"), 20, TEXT("Ashveil dye") },
		{ TEXT("kit-ashthread"), 50, TEXT("Ash-thread kit schematic") },
		{ TEXT("vendor"), 80, TEXT("Waresmith Ryn's visit") },
	};
	static const TCHAR* Ashveil = TEXT("Ashveil");
	static constexpr uint32 AshveilHex = 0xb09070;
	static const FTask* FindTask(const FString& Id) { for (const FTask& T : Tasks) if (Id == T.Id) return &T; return nullptr; }
	static const FTier* FindTier(const FString& Id) { for (const FTier& T : Tiers) if (Id == T.Id) return &T; return nullptr; }
	static void VAct37(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesScore;

void UVenturesFeature::ResetScore()
{
	ScoreSeasonId = SeasonId;
	ScoreValue = 0;
	ScoreClaimed.Reset(); ScoreOwned.Reset(); ScoreDayKeys.Reset(); ScoreWeekKeys.Reset();
	ScoreUnlocks.Reset();
	ScoreDay = ScoreDayKey(NowMs());
	ScoreWeek = ScoreWeekKey(NowMs());
}

FString UVenturesFeature::ScoreDayKey(int64 Ms) const
{
	const FDateTime T = FDateTime::FromUnixTimestamp(FMath::Max<int64>(0, Ms) / 1000);
	return FString::Printf(TEXT("%04d-%02d-%02d"), T.GetYear(), T.GetMonth(), T.GetDay());
}

FString UVenturesFeature::ScoreWeekKey(int64 Ms) const
{
	const FDateTime T = FDateTime::FromUnixTimestamp(FMath::Max<int64>(0, Ms) / 1000);
	const FDateTime OneJan(T.GetYear(), 1, 1);
	const int32 W = FMath::Max(1, FMath::CeilToInt(((T - OneJan).GetTotalDays() + (int32)OneJan.GetDayOfWeek() + 1) / 7.0));
	return FString::Printf(TEXT("%d-W%02d"), T.GetYear(), W);
}

void UVenturesFeature::ScoreRoll()
{
	const int64 Now = NowMs();
	const FString D = ScoreDayKey(Now), W = ScoreWeekKey(Now);
	if (ScoreSeasonId != SeasonId)
	{
		ScoreSeasonId = SeasonId;
		ScoreValue = 0;
		ScoreClaimed.Reset();
		ScoreDayKeys.Reset();
		ScoreWeekKeys.Reset();
		// owned cosmetics stay
	}
	if (ScoreDay != D) { ScoreDay = D; ScoreDayKeys.Reset(); }
	if (ScoreWeek != W) { ScoreWeek = W; ScoreWeekKeys.Reset(); }
}

bool UVenturesFeature::ScoreHas(const FString& Id) const
{
	return ScoreOwned.Contains(Id) || ScoreClaimed.Contains(Id) || ScoreUnlocks.Contains(Id);
}

void UVenturesFeature::SaveScore(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetStringField(TEXT("seasonId"), ScoreSeasonId.IsEmpty() ? FString(SeasonId) : ScoreSeasonId);
	S->SetNumberField(TEXT("score"), ScoreValue);
	PutStrArr(*S, TEXT("claimed"), ScoreClaimed);
	PutStrArr(*S, TEXT("owned"), ScoreOwned);
	S->SetStringField(TEXT("day"), ScoreDay);
	S->SetStringField(TEXT("week"), ScoreWeek);
	PutStrArr(*S, TEXT("dayKeys"), ScoreDayKeys);
	PutStrArr(*S, TEXT("weekKeys"), ScoreWeekKeys);
	A.SetObjectField(TEXT("score"), S);
}

void UVenturesFeature::LoadScore(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("score"));
	if (!S) { ScoreSeasonId = SeasonId; ScoreDay = ScoreDayKey(NowMs()); ScoreWeek = ScoreWeekKey(NowMs()); return; }
	ScoreSeasonId = JStr(*S, TEXT("seasonId"));
	ScoreValue = FMath::Max(0, (int32)JInt(*S, TEXT("score")));
	ScoreClaimed = JStrArr(*S, TEXT("claimed"));
	ScoreOwned = JStrArr(*S, TEXT("owned"));
	for (const FString& Id : ScoreClaimed) ScoreOwned.AddUnique(Id);
	for (const FString& Id : ScoreOwned) ScoreUnlocks.Add(Id);
	ScoreDay = JStr(*S, TEXT("day"));
	ScoreWeek = JStr(*S, TEXT("week"));
	ScoreDayKeys = JStrArr(*S, TEXT("dayKeys"));
	ScoreWeekKeys = JStrArr(*S, TEXT("weekKeys"));
	ScoreRoll();
}

int32 UVenturesFeature::ScoreOn(const FString& TaskId)
{
	const FTask* T = FindTask(TaskId); if (!T) return 0;
	ScoreRoll();
	TArray<FString>& Bag = (FCString::Strcmp(T->Kind, TEXT("weekly")) == 0) ? ScoreWeekKeys : ScoreDayKeys;
	if (Bag.Contains(TaskId)) return 0;
	Bag.Add(TaskId);
	ScoreValue += T->Pts;
	Say(FString::Printf(TEXT("Seasonboard: %s (+%d season score, now %d). This is not Standing."), T->Name, T->Pts, ScoreValue));
	return T->Pts;
}

FString UVenturesFeature::ScoreClaim(const FString& TierId)
{
	const FTier* T = FindTier(TierId); if (!T) return TEXT("Unknown tier.");
	if (ScoreClaimed.Contains(TierId)) return TEXT("Already claimed.");
	if (ScoreValue < T->At) return FString::Printf(TEXT("You need %d season score to claim %s."), T->At, T->Name);
	ScoreClaimed.Add(TierId);
	ScoreOwned.AddUnique(TierId);
	ScoreUnlocks.Add(TierId);
	Say(FString::Printf(TEXT("Seasonboard: you claim %s."), T->Name));
	if (TierId == TEXT("dye-ashveil")) Say(TEXT("Ashveil is now a Lookplate dye in the Wardrobe."));
	if (TierId == TEXT("kit-ashthread")) Say(TEXT("The Ash-thread kit schematic is yours. Craft it at the kit bench."));
	if (TierId == TEXT("vendor")) Say(TEXT("Waresmith Ryn is in Larkhaven with this season's stall."));
	return FString();
}

bool UVenturesFeature::ScoreVendorIn() const { return ScoreHas(TEXT("vendor")); }

bool UVenturesFeature::ScoreBuy(const FString& Item, int32 Price)
{
	if (!ScoreVendorIn()) { Say(TEXT("Ryn has not opened a stall this season. Claim the vendor tier first.")); return false; }
	if (!TakeCoins(Price)) { Say(TEXT("You cannot afford that.")); return false; }
	GiveItem(Item, 1);
	Say(FString::Printf(TEXT("You buy a %s from Waresmith Ryn for %d coins."), *Item, Price));
	return true;
}

float UVenturesFeature::ClimbDrainMul() const
{
	return CountItem(TEXT("Ridgewalk soles")) > 0 ? 0.9f : 1.f;
}

int32 UVenturesFeature::WardrobeDyeCount() const
{
	return 12 + (ScoreHas(TEXT("dye-ashveil")) ? 1 : 0);
}

FString UVenturesFeature::WardrobeDyeName(int32 Index) const
{
	static const TCHAR* Base[] = { TEXT("Emberglow"), TEXT("Rimefrost"), TEXT("Mossbank"), TEXT("Duskviolet"), TEXT("Saltwhite"), TEXT("Ironsoot"), TEXT("Goldleaf"), TEXT("Tidewater"), TEXT("Bloodroot"), TEXT("Ashgrey"), TEXT("Sunflax"), TEXT("Nightjet") };
	if (Index >= 0 && Index < 12) return Base[Index];
	if (Index == 12 && ScoreHas(TEXT("dye-ashveil"))) return Ashveil;
	return FString();
}

uint32 UVenturesFeature::WardrobeDyeHex(int32 Index) const
{
	static const uint32 Base[] = { 0xc0401a, 0xa8d8f0, 0x4a7a3a, 0x5a3a8a, 0xe8e4d8, 0x2a2a2e, 0xd4a830, 0x2a6a8a, 0x7a1a24, 0x7a7a78, 0xe0c870, 0x14141c };
	if (Index >= 0 && Index < 12) return Base[Index];
	if (Index == 12 && ScoreHas(TEXT("dye-ashveil"))) return AshveilHex;
	return 0;
}

void UVenturesFeature::TabScore(FGPanelModel& M)
{
	ScoreRoll();
	M.Title = TEXT("Seasonboard — Ashen Paths");
	M.Lines.Add(TEXT("Season score is not Standing. Daily Writs and the Weekly Hearthboard still pay Standing. Miss a day and nothing is taken from you. There is no paid pass and no other-player ladder."));
	M.Lines.Add(FString::Printf(TEXT("Score %d"), ScoreValue));
	M.Lines.Add(TEXT("Today / this week"));
	for (const FTask& T : Tasks)
	{
		const bool bDone = ((FCString::Strcmp(T.Kind, TEXT("weekly")) == 0) ? ScoreWeekKeys : ScoreDayKeys).Contains(T.Id);
		M.Lines.Add(FString::Printf(TEXT("%s %s  (%s · %d)"), bDone ? TEXT("☑") : TEXT("☐"), T.Name, T.Kind, T.Pts));
	}
	M.Lines.Add(TEXT("Tiers"));
	for (const FTier& T : Tiers)
	{
		const bool bClaimed = ScoreClaimed.Contains(T.Id);
		const bool bReady = !bClaimed && ScoreValue >= T.At;
		M.Lines.Add(FString::Printf(TEXT("%s %d: %s"), bClaimed ? TEXT("★") : (ScoreValue >= T.At ? TEXT("○") : TEXT("·")), T.At, T.Name));
		if (bReady) VAct37(M, FString::Printf(TEXT("Claim %s"), T.Name), [this, Id = FString(T.Id)] { const FString Why = ScoreClaim(Id); if (!Why.IsEmpty()) Say(Why); });
	}
	M.Lines.Add(TEXT("Vendor"));
	if (ScoreVendorIn())
	{
		M.Lines.Add(TEXT("Waresmith Ryn is in Larkhaven this season. Ridgewalk soles ease climb drain a little while you carry them. A Season flask helps thirst when Hardroad is on. Behaviour and small numbers, not a damage race."));
		VAct37(M, TEXT("Ridgewalk soles (200)"), [this] { ScoreBuy(TEXT("Ridgewalk soles"), 200); }, Coins() >= 200);
		VAct37(M, TEXT("Season flask (40)"), [this] { ScoreBuy(TEXT("Season flask"), 40); }, Coins() >= 40);
	}
	else M.Lines.Add(TEXT("The wandering vendor is not in town yet. Claim that tier to bring Waresmith Ryn to Larkhaven."));
}
