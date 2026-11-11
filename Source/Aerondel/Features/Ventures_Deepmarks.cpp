#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 18 (web adapt/deepmarks.js): a climbing instanced dungeon ladder with stacking modifiers.
// Tiers past 1000 scale with Lumen. Breach stays the short objective and can award a low-tier mark.

namespace VenturesMarks
{
	static const TCHAR* Foes[] = { TEXT("Ashen raider"), TEXT("Sand Reaver"), TEXT("Canopy Stalker"), TEXT("Magma Brute"), TEXT("Ember Imp"), TEXT("Rime Wraith"), TEXT("Thornback Prowler"), TEXT("Hoarfrost Stalker"), TEXT("Murk Leech"), TEXT("Glass Scorpion"), TEXT("Voidborn Knight") };
	struct FMod { const TCHAR* Id; const TCHAR* Name; const TCHAR* Desc; };
	static const FMod Mods[] = {
		{ TEXT("burning"),  TEXT("Burning ground"),  TEXT("You lose a little health every few ticks.") },
		{ TEXT("shielded"), TEXT("Elites shielded"), TEXT("Foes take a little more killing (more health, not more damage).") },
		{ TEXT("frenzy"),   TEXT("Frenzy"),          TEXT("Foes wind up a little faster — they do not hit much harder.") },
		{ TEXT("echo"),     TEXT("Echo pack"),       TEXT("A second foe can appear near a kill.") },
		{ TEXT("hush"),     TEXT("Hush"),            TEXT("Prayer feels slower to recover in this mark (rules, not raw damage).") },
		{ TEXT("drift"),    TEXT("Drift"),           TEXT("Foes wander in from a wider angle.") },
	};
	static const FMod* FindModId(const FString& Id) { for (const FMod& M : Mods) if (Id == M.Id) return &M; return nullptr; }
	static constexpr int32 MaxTier = 14;
	static int32 Gate(int32 T) { return T <= 4 ? 500 : T <= 8 ? 750 : 1000; }
	static int32 ModsFor(int32 T) { return T <= 4 ? 2 : T <= 8 ? 3 : 4; }
	static bool NeedLumen(int32 T) { return T >= 11; }
	static FString WeekKey()
	{
		FDateTime D = FDateTime::UtcNow();
		const int32 Dow = (int32)D.GetDayOfWeek(); // Monday=0 in UE
		D -= FTimespan::FromDays(Dow);
		return FString::Printf(TEXT("%04d-%02d-%02d"), D.GetYear(), D.GetMonth(), D.GetDay());
	}
	static const TCHAR* PickFoe() { return Foes[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(Foes) - 1)]; }
	static void VAct18(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesMarks;

void UVenturesFeature::ResetMark()
{
	MarkTierUnlocked = 1;
	MarkClears.Reset();
	MarkStash.Reset();
	MarkWeekPinn.Reset();
}

void UVenturesFeature::SaveMark(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("tierUnlocked"), MarkTierUnlocked);
	TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
	for (const auto& E : MarkClears) C->SetNumberField(E.Key, E.Value);
	S->SetObjectField(TEXT("clears"), C);
	TArray<TSharedPtr<FJsonValue>> St;
	for (const FString& M : MarkStash) St.Add(MakeShared<FJsonValueString>(M));
	S->SetArrayField(TEXT("stash"), St);
	S->SetStringField(TEXT("weekPinn"), MarkWeekPinn);
	A.SetObjectField(TEXT("marks"), S);
}

void UVenturesFeature::LoadMark(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("marks"));
	if (!S) return;
	MarkTierUnlocked = FMath::Clamp((int32)JInt(*S, TEXT("tierUnlocked"), 1), 1, MaxTier);
	if (const TSharedPtr<FJsonObject> C = JObj(*S, TEXT("clears")))
		for (const auto& E : C->Values)
		{
			const int32 T = FCString::Atoi(*E.Key);
			if (T >= 1 && T <= MaxTier && E.Value.IsValid()) MarkClears.Add(E.Key, FMath::Max(0, (int32)E.Value->AsNumber()));
		}
	if (const TArray<TSharedPtr<FJsonValue>>* St = nullptr; S->TryGetArrayField(TEXT("stash"), St) && St)
		for (const TSharedPtr<FJsonValue>& V : *St)
			if (V.IsValid() && V->Type == EJson::String)
			{
				const FString Id = V->AsString();
				int32 T = 0;
				if (Id.StartsWith(TEXT("t")) && Id.Len() <= 3) T = FCString::Atoi(*Id.Mid(1));
				if (T >= 1 && T <= MaxTier && MarkStash.Num() < 24) MarkStash.Add(Id);
			}
	MarkWeekPinn = JStr(*S, TEXT("weekPinn"));
}

int32 UVenturesFeature::MarkLevelFor(int32 T) const
{
	const float Base = VLevel() * (T <= 4 ? 1.f : T <= 8 ? 1.15f : 1.3f);
	float Over = 0.f;
	if (T > 10)
	{
		Over = (T - 10) * 40.f;
		if (LumenOn()) Over += FMath::Max(0.f, (LumenScore - 1000.f) * 2.f);
	}
	return FMath::Min(2000, FMath::RoundToInt(Base + Over));
}

void UVenturesFeature::MarkSpawnRoom()
{
	for (AAerondelMob* M : ZoneAlive()) if (M) M->Destroy();
	Run.Foes.Reset();
	const int32 N = Run.Room == 3 ? 1 : (Run.Room == 1 ? 4 : 5);
	const FString Type = Run.Room == 3 ? TEXT("Voidborn Knight") : PickFoe();
	const float HpMul = (Run.Mods.Contains(TEXT("shielded")) ? 1.4f : 1.f) * (Run.Room == 3 ? 1.8f : 1.f);
	const int32 Room = CapFoes(15) - ZoneAlive().Num();
	const float Spread = Run.Mods.Contains(TEXT("drift")) ? 6.f : 4.f;
	for (int32 i = 0; i < FMath::Min(N, Room); ++i)
	{
		const float A = FMath::FRand() * 2.f * PI;
		const float Rad = 4.f + FMath::FRand() * Spread;
		ZoneSpawn(TEXT("marks"), Type, FMath::Cos(A) * Rad, FMath::Sin(A) * Rad - 3.f, (float)Run.L, HpMul, Run.Room == 3 && i == 0);
	}
}

bool UVenturesFeature::MarkStart(int32 Tier)
{
	const int32 T = FMath::Clamp(Tier, 1, MaxTier);
	if (VLevel() < Gate(T)) { Say(FString::Printf(TEXT("Deepmarks of this height open at an average combat level of %d."), Gate(T))); return false; }
	if (NeedLumen(T) && !LumenOn()) { Say(TEXT("Tiers past 1000 need Lumen Rise (a combat skill at 1000).")); return false; }
	if (T > MarkTierUnlocked) { Say(TEXT("You have not opened this Deepmark height yet.")); return false; }
	const int32 I = MarkStash.Find(FString::Printf(TEXT("t%d"), T));
	if (I == INDEX_NONE) { Say(TEXT("You need a mark of this height in your stash. Breach clears can award a low-tier mark.")); return false; }
	if (!RunStart(TEXT("marks"), TEXT("marks"))) { return false; }
	MarkStash.RemoveAt(I);
	TArray<FString> Keys; for (const FMod& M : Mods) Keys.Add(M.Id);
	Run.Mods.Reset();
	while (Run.Mods.Num() < ModsFor(T) && Keys.Num())
	{
		const int32 K = FMath::RandRange(0, Keys.Num() - 1);
		Run.Mods.Add(Keys[K]); Keys.RemoveAt(K);
	}
	Run.L = MarkLevelFor(T);
	Run.Level = T;
	Run.Room = 1;
	Run.Kills = 0;
	MarkSpawnRoom();
	TArray<FString> Names; for (const FString& Id : Run.Mods) if (const FMod* M = FindModId(Id)) Names.Add(M->Name);
	Say(FString::Printf(TEXT("Deepmark height %d. %s. Three rooms, one at a time."), T, *FString::Join(Names, TEXT(" · "))));
	return true;
}

void UVenturesFeature::TickMark()
{
	if (!InRunKind(TEXT("marks"))) return;
	if (Run.Mods.Contains(TEXT("burning")) && TickN % 3 == 0)
	{
		if (AAerondelPlayerState* PS = GetPS()) if (PS->Health && PS->Health->GetCurrent() > 1)
		{
			const int32 D = FMath::Max(1, FMath::CeilToInt(PS->Health->GetMax() * 0.01f));
			if (PS->Stats) PS->Stats->ApplyPlayerDamage(D, nullptr);
		}
	}
	if (NowMs() - Run.StartMs > 10 * 60 * 1000) { Say(TEXT("The mark collapses — out of time.")); RunEnd(false); return; }
	if (ZoneAlive().Num() == 0)
	{
		if (Run.Room >= 3) { RunEnd(true); return; }
		Run.Room++; Run.Kills = 0;
		Say(Run.Room == 3 ? TEXT("The last chamber — a guardian.") : FString::Printf(TEXT("Room %d of 3."), Run.Room));
		MarkSpawnRoom();
	}
}

void UVenturesFeature::DiedMark(AAerondelMob* Mob, bool bByPlayer)
{
	if (!InRunKind(TEXT("marks")) || !Mob || !bByPlayer) return;
	bool bOurs = false;
	for (const TWeakObjectPtr<AAerondelMob>& W : Run.Foes) if (W.Get() == Mob) { bOurs = true; break; }
	if (!bOurs) return;
	Run.Kills++;
	if (Run.Mods.Contains(TEXT("echo")) && FMath::FRand() < 0.25f && ZoneAlive().Num() < CapFoes(15))
	{
		const float HpMul = Run.Mods.Contains(TEXT("shielded")) ? 1.4f : 0.8f;
		ZoneSpawn(TEXT("marks"), PickFoe(), (FMath::FRand() - 0.5f) * 8.f, -4.f, (float)Run.L, HpMul);
	}
}

void UVenturesFeature::MarkFinish(bool bWin)
{
	if (!bWin) { Say(TEXT("The Deepmark fails. The height stays as it was, and the spent mark is gone.")); return; }
	const int32 T = Run.Level;
	MarkClears.FindOrAdd(FString::FromInt(T))++;
	if (T >= MarkTierUnlocked && T < MaxTier) MarkTierUnlocked = T + 1;
	MarkStash.Add(FString::Printf(TEXT("t%d"), T));
	if (T < MaxTier && FMath::FRand() < 0.7f) MarkStash.Add(FString::Printf(TEXT("t%d"), T + 1));
	if (MarkStash.Num() > 24) MarkStash.SetNum(24);
	const int32 Coins = FMath::RoundToInt(120 + Run.L * 3 + T * 20);
	GiveCoins(Coins);
	Say(FString::Printf(TEXT("Deepmark height %d is clear. +%d coins. A mark returns to your stash."), T, Coins));
	const FString W = WeekKey();
	if (T >= 5 && MarkWeekPinn != W)
	{
		MarkWeekPinn = W;
		Pinnacle(TEXT("Deepmark"));
	}
}

void UVenturesFeature::MarkFromBreach(bool bHard, const FString& Grade)
{
	if (!(Grade == TEXT("S") || bHard || FMath::FRand() < 0.4f)) return;
	int32 T1 = 0; for (const FString& S : MarkStash) if (S == TEXT("t1")) ++T1;
	if (T1 >= 6) return;
	MarkStash.Add(TEXT("t1"));
	Say(TEXT("A low-tier Deepmark settles into your stash."));
}

float UVenturesFeature::PrayerRegenMul() const
{
	const float Mark = (InRunKind(TEXT("marks")) && Run.Mods.Contains(TEXT("hush"))) ? 0.35f : 1.f;
	return Mark * RoadRegenMul();
}

float UVenturesFeature::PrayerRegenMul(const UObject* WorldContext)
{
	const UVenturesFeature* V = Get(WorldContext);
	return V ? V->PrayerRegenMul() : 1.f;
}

void UVenturesFeature::TabMark(FGPanelModel& M)
{
	if (InRunKind(TEXT("marks")))
	{
		M.Title = FString::Printf(TEXT("Height %d · room %d/3"), Run.Level, Run.Room);
		for (const FString& Id : Run.Mods) if (const FMod* Md = FindModId(Id)) M.Lines.Add(FString::Printf(TEXT("%s — %s"), Md->Name, Md->Desc));
		M.Lines.Add(FString::Printf(TEXT("Foes %d/%d"), ZoneAlive().Num(), CapFoes(15)));
		VAct18(M, TEXT("Abandon run"), [this] { RunEnd(false); }, true, true);
		return;
	}
	M.Title = FString::Printf(TEXT("Deepmarks · open to height %d · your level %d"), MarkTierUnlocked, VLevel());
	M.Lines.Add(TEXT("Spend a mark to walk three rooms, one at a time, with stacked twists. Clearing a height opens the next and often leaves a higher mark. Breach clears can award a low-tier mark. A weekly pinnacle is shared with Vigil and Burst Tides. Tiers past 10 need Lumen."));
	TMap<FString, int32> Have;
	for (const FString& S : MarkStash) Have.FindOrAdd(S)++;
	const int32 Cap = FMath::Min(MaxTier, MarkTierUnlocked);
	for (int32 T = 1; T <= Cap; ++T)
	{
		const int32 N = Have.FindRef(FString::Printf(TEXT("t%d"), T));
		const int32 Cl = MarkClears.FindRef(FString::FromInt(T));
		const bool bLock = VLevel() < Gate(T) || (NeedLumen(T) && !LumenOn());
		M.Lines.Add(FString::Printf(TEXT("Height %d — cleared %d · marks %d · opens at %d%s"), T, Cl, N, Gate(T), NeedLumen(T) ? TEXT(" + Lumen") : TEXT("")));
		VAct18(M, FString::Printf(TEXT("Enter height %d"), T), [this, T] { MarkStart(T); }, N > 0 && !bLock && !Run.bOn, true);
	}
}
