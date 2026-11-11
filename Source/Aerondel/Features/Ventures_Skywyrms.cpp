#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Combat/AerondelMob.h"
#include "Data/GameDatabase.h"
#include "Data/MobDef.h"
#include "World/AerondelScalability.h"
#include "Dom/JsonObject.h"

// Ventures group 31 (web adapt/skywyrms.js): occasional overworld flyers you can fight or leave.
// A kill yields a soul for Veilcraft, a little Hunter and Prayer, and nothing like a Colossus chest.
// Ashwyrm stays a Colossus on the Contested Wilds route. At most one sighting at a time.
// On Low the fight is a landed stand-in, not a huge flying mesh.

namespace VenturesWyrms
{
	static constexpr int32 SoulCap = 8, HunterXp = 30, PrayerXp = 8;
	static constexpr int64 CadenceMin = 18ll * 60 * 1000, CadenceSpan = 10ll * 60 * 1000, WarnMs = 3ll * 60 * 1000, FightMs = 8ll * 60 * 1000;
	struct FReg { const TCHAR* Id; const TCHAR* Name; float X, Z; };
	static const FReg Regions[] = {
		{ TEXT("rimeholt"),    TEXT("Rimeholt Tundra"),      -240.f, -347.f },
		{ TEXT("skyreach"),    TEXT("Skyreach Highlands"),    245.f, -342.f },
		{ TEXT("ashveil"),     TEXT("Ashveil Wastes"),        365.f, -132.f },
		{ TEXT("sunscour"),    TEXT("Sunscour Dunes"),        370.f,  128.f },
		{ TEXT("vinehollow"),  TEXT("Verdant Tangle"),        220.f,  368.f },
		{ TEXT("mirelow"),     TEXT("Mirelow Fen"),          -350.f,  193.f },
		{ TEXT("grimhollow"),  TEXT("Grimhollow Barrens"),    550.f,    8.f },
		{ TEXT("thornmere"),   TEXT("Thornmere Wilds"),       390.f,  408.f },
		{ TEXT("hollowfrost"), TEXT("Hollowfrost Expanse"),    10.f,  548.f },
		{ TEXT("stormcrown"),  TEXT("Stormcrown Peaks"),     -380.f,  408.f },
		{ TEXT("duskmarsh"),   TEXT("Duskmarsh Deeps"),      -535.f,   -2.f },
		{ TEXT("glasswind"),   TEXT("Glasswind Erg"),        -400.f, -432.f },
		{ TEXT("emberdeep"),   TEXT("Emberdeep Caldera"),      10.f, -537.f },
		{ TEXT("sundered"),    TEXT("The Sundered Reach"),    430.f, -422.f },
	};
	static bool IsLow() { return UAerondelScalability::GetSavedTier() <= EAerondelTier::Low; }
	static void VAct31(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesWyrms;

int32 UVenturesFeature::WyrmPickRegion() const
{
	return FMath::RandRange(0, (int32)UE_ARRAY_COUNT(Regions) - 1);
}

int32 UVenturesFeature::WyrmSouls() const { return VeilSoulCount; }

int32 UVenturesFeature::WyrmGrantSoul(int32 N)
{
	N = FMath::Max(0, N);
	const int32 Before = VeilSoulCount;
	VeilSoulCount = FMath::Min(SoulCap, VeilSoulCount + N);
	if (VeilSoulCount > WyrmBest) WyrmBest = VeilSoulCount;
	SyncSoulPouch();
	return VeilSoulCount - Before;
}

bool UVenturesFeature::WyrmSpendSoul(int32 N)
{
	N = FMath::Max(1, N);
	if (VeilSoulCount < N) return false;
	VeilSoulCount -= N;
	SyncSoulPouch();
	return true;
}

bool UVenturesFeature::WyrmActive() const
{
	const AAerondelMob* M = WyrmLive.Get();
	return M && !M->IsDead();
}

void UVenturesFeature::WyrmClearLive()
{
	if (AAerondelMob* M = WyrmLive.Get()) M->Destroy();
	if (AAerondelMob* A = WyrmAtt.Get()) A->Destroy();
	WyrmLive.Reset();
	WyrmAtt.Reset();
	WyrmFightStart = 0;
}

void UVenturesFeature::ResetWyrms()
{
	WyrmSeen = WyrmSoulCount = WyrmBest = 0;
	WyrmLast = WyrmNext = WyrmOfferUntil = WyrmFightStart = 0;
	bWyrmOffered = false;
	WyrmRegion = -1;
	WyrmClearLive();
}

void UVenturesFeature::SaveWyrms(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("seen"), WyrmSeen);
	S->SetNumberField(TEXT("souls"), WyrmSoulCount);
	S->SetNumberField(TEXT("last"), WyrmLast);
	S->SetNumberField(TEXT("best"), WyrmBest);
	S->SetNumberField(TEXT("next"), WyrmNext);
	A.SetObjectField(TEXT("wyrms"), S);
}

void UVenturesFeature::LoadWyrms(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("wyrms"));
	if (!S) { WyrmNext = NowMs() + CadenceMin + (int64)(FMath::FRand() * (double)CadenceSpan); return; }
	WyrmSeen = (int32)JInt(*S, TEXT("seen"));
	WyrmSoulCount = FMath::Clamp((int32)JInt(*S, TEXT("souls")), 0, SoulCap);
	WyrmLast = JInt(*S, TEXT("last"));
	WyrmBest = (int32)JInt(*S, TEXT("best"));
	WyrmNext = JInt(*S, TEXT("next"));
	if (WyrmSoulCount > WyrmBest) WyrmBest = WyrmSoulCount;
	if (!WyrmNext) WyrmNext = (WyrmLast ? WyrmLast + CadenceMin : NowMs() + CadenceMin) + (int64)(FMath::FRand() * (double)CadenceSpan);
}

bool UVenturesFeature::WyrmOffer()
{
	if (WyrmActive() || bWyrmOffered) return false;
	WyrmRegion = WyrmPickRegion();
	bWyrmOffered = true;
	WyrmOfferUntil = NowMs() + WarnMs;
	++WyrmSeen;
	WyrmLast = NowMs();
	WyrmNext = NowMs() + CadenceMin + (int64)(FMath::FRand() * (double)CadenceSpan);
	const FReg& R = Regions[FMath::Clamp(WyrmRegion, 0, (int32)UE_ARRAY_COUNT(Regions) - 1)];
	Say(FString::Printf(TEXT("A Skywyrm is sighted over %s. You can fly or travel to it, or leave it. Ventures → Skywyrms."), R.Name));
	return true;
}

bool UVenturesFeature::WyrmPass()
{
	if (WyrmActive()) { Say(TEXT("The Skywyrm is already on the ground. Fight or walk away; it will leave in time.")); return false; }
	if (!bWyrmOffered) return false;
	const FReg& R = Regions[FMath::Clamp(WyrmRegion, 0, (int32)UE_ARRAY_COUNT(Regions) - 1)];
	bWyrmOffered = false;
	WyrmOfferUntil = 0;
	Say(FString::Printf(TEXT("You let the Skywyrm over %s pass. It was not a Colossus, and nothing is owed."), R.Name));
	return true;
}

bool UVenturesFeature::WyrmFight()
{
	if (WyrmActive()) { Say(TEXT("A Skywyrm is already circling. There is never more than one at a time.")); return false; }
	if (WyrmRegion < 0) WyrmRegion = WyrmPickRegion();
	const FReg& R = Regions[FMath::Clamp(WyrmRegion, 0, (int32)UE_ARRAY_COUNT(Regions) - 1)];
	const bool bLow = IsLow();
	int32 L = FMath::Max(25, CombatLevel() + (LumenOn() ? 8 : 0));
	int32 Hp = 750, MaxHit = 80, Att = 60, Def = 55, Lvl = 79;
	if (const UGameDatabase* Db = UGameDatabase::Get())
		if (const UMobDef* B = Db->FindMob(Id(TEXT("Green Dragon"))))
		{
			Hp = B->Hitpoints; MaxHit = B->MaxHit; Att = B->AttackLevel; Def = B->DefenceLevel; Lvl = B->CombatLevel;
		}
	Lvl = FMath::Max(Lvl, L);
	Hp = FMath::Max(1, FMath::RoundToInt((float)Hp * (bLow ? 0.7f : 0.9f)));
	const float Scale = bLow ? 1.05f : 1.35f;
	AAerondelMob* W = SpawnTunedMob(TEXT("Green Dragon"), FVector2D(R.X, R.Z), TEXT("Skywyrm"), Hp, MaxHit, Att, Def, Lvl, Scale, false);
	if (!W) { Say(TEXT("The Skywyrm could not land here.")); return false; }
	if (!bLow)
		if (AAerondelMob* A = SpawnTunedMob(TEXT("Wolf"), FVector2D(R.X + 3.f, R.Z + 2.f), TEXT("Skywyrm attendant"), 140, 18, 12, 10, 14, 1.f, false))
			WyrmAtt = A;
	WyrmLive = W;
	bWyrmOffered = false;
	WyrmOfferUntil = 0;
	WyrmFightStart = NowMs();
	Say(FString::Printf(TEXT("The Skywyrm over %s comes down to fight. This is not Ashwyrm, and there is no weekly chest — only a soul if you prevail."), R.Name));
	return true;
}

bool UVenturesFeature::WyrmGo()
{
	if (!bWyrmOffered) { Say(TEXT("No Skywyrm is sighted right now.")); return false; }
	const FReg& R = Regions[FMath::Clamp(WyrmRegion, 0, (int32)UE_ARRAY_COUNT(Regions) - 1)];
	TeleportPlayer(FVector2D(R.X, R.Z));
	return WyrmFight();
}

bool UVenturesFeature::WyrmLeave()
{
	if (!WyrmActive() && !WyrmLive.IsValid()) return false;
	WyrmClearLive();
	Say(TEXT("The Skywyrm wheels off. You can meet another later."));
	return true;
}

void UVenturesFeature::TickWyrm()
{
	const int64 Now = NowMs();
	if (!bWyrmOffered && !WyrmActive() && Now > WyrmNext) WyrmOffer();
	if (bWyrmOffered && Now > WyrmOfferUntil) WyrmPass();
	if (WyrmFightStart && Now - WyrmFightStart > FightMs) WyrmLeave();
}

void UVenturesFeature::DiedWyrm(AAerondelMob* Mob, bool bByPlayer)
{
	if (!Mob) return;
	if (WyrmAtt.Get() == Mob) { WyrmAtt.Reset(); return; }
	if (WyrmLive.Get() != Mob) return;
	const int32 Gained = bByPlayer ? WyrmGrantSoul(1) : 0;
	if (bByPlayer)
	{
		GiveXp(TEXT("Hunter"), (float)HunterXp);
		GiveXp(TEXT("Prayer"), (float)PrayerXp);
	}
	if (Gained) Say(FString::Printf(TEXT("The Skywyrm falls. A soul settles in your pouch (%d/%d). +%d Hunter, +%d Prayer. This was not a Colossus chest."), WyrmSouls(), SoulCap, HunterXp, PrayerXp));
	else if (bByPlayer) Say(FString::Printf(TEXT("The Skywyrm falls, but your soul pouch is full (%d)."), SoulCap));
	if (AAerondelMob* A = WyrmAtt.Get()) A->Destroy();
	WyrmAtt.Reset();
	WyrmLive.Reset();
	WyrmFightStart = 0;
}

void UVenturesFeature::TabWyrms(FGPanelModel& M)
{
	M.Title = TEXT("Skywyrms");
	M.Lines.Add(TEXT("Occasional flyers over the wilds. Fighting is optional. A kill yields a soul for Veilcraft, a little Hunter or Prayer, and nothing like a Colossus chest. Ashwyrm stays a Colossus on the Contested Wilds. At most one sighting at a time."));
	M.Lines.Add(FString::Printf(TEXT("Soul pouch: %d/%d · Sightings: %d"), WyrmSouls(), SoulCap, WyrmSeen));
	if (WyrmActive())
	{
		const FReg& R = Regions[FMath::Clamp(WyrmRegion, 0, (int32)UE_ARRAY_COUNT(Regions) - 1)];
		M.Lines.Add(FString::Printf(TEXT("A Skywyrm is down over %s. Walk away if you want — it will leave in time."), R.Name));
		VAct31(M, TEXT("Let it leave"), [this] { WyrmLeave(); });
	}
	else if (bWyrmOffered)
	{
		const FReg& R = Regions[FMath::Clamp(WyrmRegion, 0, (int32)UE_ARRAY_COUNT(Regions) - 1)];
		M.Lines.Add(FString::Printf(TEXT("A Skywyrm is sighted over %s. You can go to it, or let it pass. Nothing is owed if you leave it."), R.Name));
		VAct31(M, TEXT("Go"), [this] { WyrmGo(); }, true, true);
		VAct31(M, TEXT("Not now"), [this] { WyrmPass(); });
	}
	else
	{
		const int32 Mins = FMath::Max(0, (int32)FMath::CeilToInt((double)(WyrmNext - NowMs()) / 60000.0));
		M.Lines.Add(FString::Printf(TEXT("No sighting right now. The next one comes in about %d minute%s of play."), Mins, Mins == 1 ? TEXT("") : TEXT("s")));
	}
}
