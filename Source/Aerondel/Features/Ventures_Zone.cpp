#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Interaction/ResourceNode.h"
#include "World/AerondelScalability.h"
#include "Data/GameDatabase.h"
#include "Data/ResourceNodeDef.h"
#include "Data/MobDef.h"
#include "GameFramework/Character.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ============================================================================================
// Ventures group 3 (web adapt/zone.js, tides.js, breach.js, tether.js):
// shared instanced pads, Burst Tides, Breach Runs, Tetherline.
// ============================================================================================

namespace VenturesZone
{
	static const TCHAR* TideBosses[] = { TEXT("Cinder Warden"), TEXT("Thornback Prowler"), TEXT("Bramble Colossus"), TEXT("Hoarfrost Stalker"), TEXT("Glacier Behemoth"), TEXT("Storm Gryphon"), TEXT("Peak Warlord"), TEXT("Bog Revenant"), TEXT("Dune Tyrant") };
	static const TCHAR* BreachFoes[4][3] = {
		{ TEXT("Ashen raider"), TEXT("Sand Reaver"), TEXT("Rime Wraith") },
		{ TEXT("Canopy Stalker"), TEXT("Magma Brute"), TEXT("Barrow Ghoul") },
		{ TEXT("Thornback Prowler"), TEXT("Hoarfrost Stalker"), TEXT("Storm Gryphon") },
		{ TEXT("Murk Leech"), TEXT("Glass Scorpion"), TEXT("Voidborn Knight") },
	};
	struct FBreachMode { const TCHAR* Key; const TCHAR* Name; const TCHAR* Desc; int32 Par; };
	static const FBreachMode Modes[] = {
		{ TEXT("ward"), TEXT("Hold the Ward"), TEXT("Keep the ward standing through 3 waves. Every foe alive drains it."), 150 },
		{ TEXT("nodes"), TEXT("Sever the Nodes"), TEXT("Stand on each of 3 breach nodes until it closes. Foes guard them."), 120 },
		{ TEXT("purge"), TEXT("Purge"), TEXT("Defeat 24 foes as fast as you can."), 140 },
	};
	struct FBreachMod { const TCHAR* Key; const TCHAR* Name; const TCHAR* Desc; float Mul; };
	static const FBreachMod BreachMods[] = {
		{ TEXT("burning"), TEXT("Burning ground"), TEXT("You lose 1% health every 3 ticks"), 1.25f },
		{ TEXT("shielded"), TEXT("Elites shielded"), TEXT("Foes have +50% health"), 1.2f },
		{ TEXT("frenzy"), TEXT("Frenzy"), TEXT("Foes hit 30% harder"), 1.2f },
		{ TEXT("gilded"), TEXT("Gilded"), TEXT("+50% coins"), 1.f },
		{ TEXT("calm"), TEXT("Calm"), TEXT("No twist"), 1.f },
	};
	static const FBreachMode* FindMode(const FString& K) { for (const FBreachMode& M : Modes) if (K == M.Key) return &M; return nullptr; }
	static const FBreachMod* FindMod(const FString& K) { for (const FBreachMod& M : BreachMods) if (K == M.Key) return &M; return nullptr; }
	static int32 GradeIdx(const FString& G) { return G == TEXT("S") ? 0 : G == TEXT("A") ? 1 : G == TEXT("B") ? 2 : G == TEXT("C") ? 3 : 9; }
	static int64 TideGapMs() { return (int64)((20.0 + FMath::FRand() * 10.0) * 60000.0); }
}
using namespace VenturesZone;

static void VAct3(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }

FString UVenturesFeature::StyleSkill(const FString& St)
{
	if (St == TEXT("Ranged")) return TEXT("Ranged");
	if (St == TEXT("Magic")) return TEXT("Magic");
	if (St == TEXT("Swordsmanship")) return TEXT("Swordsmanship");
	return TEXT("Attack");
}

// ---------------------------------------------------------------- state

void UVenturesFeature::ResetZone()
{
	ZonePark();
	Run = FVentureRun();
	for (const TWeakObjectPtr<AAerondelMob>& W : TideWaveMobs) if (W.IsValid()) W->Destroy();
	TideWaveMobs.Reset();
	TideRegion = -1;
	TideUntil = 0;
	TideNext = NowMs() + TideGapMs();
	TideStacks = 0;
	bTideWaved = false;
	TidesSeen = TidesBest = 0;
	BreachRuns = BreachAnima = 0;
	BreachBest.Reset();
	TetherReady = 0;
}

void UVenturesFeature::SaveZone(FJsonObject& A) const
{
	TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
	T->SetNumberField(TEXT("seen"), TidesSeen);
	T->SetNumberField(TEXT("best"), TidesBest);
	A.SetObjectField(TEXT("tides"), T);
	TSharedRef<FJsonObject> B = MakeShared<FJsonObject>(), Best = MakeShared<FJsonObject>();
	B->SetNumberField(TEXT("runs"), BreachRuns);
	B->SetNumberField(TEXT("anima"), BreachAnima);
	for (const auto& E : BreachBest) Best->SetStringField(E.Key, E.Value);
	B->SetObjectField(TEXT("best"), Best);
	A.SetObjectField(TEXT("breach"), B);
}

void UVenturesFeature::LoadZone(const FJsonObject& A)
{
	if (const TSharedPtr<FJsonObject> T = JObj(A, TEXT("tides")))
	{
		TidesSeen = FMath::Max(0, (int32)JInt(*T, TEXT("seen")));
		TidesBest = FMath::Max(0, (int32)JInt(*T, TEXT("best")));
	}
	if (const TSharedPtr<FJsonObject> B = JObj(A, TEXT("breach")))
	{
		BreachRuns = FMath::Max(0, (int32)JInt(*B, TEXT("runs")));
		BreachAnima = FMath::Max(0, (int32)JInt(*B, TEXT("anima")));
		if (const TSharedPtr<FJsonObject> Best = JObj(*B, TEXT("best")))
			for (const FBreachMode& M : Modes)
			{
				const FString G = JStr(*Best, M.Key);
				if (GradeIdx(G) < 4) BreachBest.Add(M.Key, G);
			}
	}
}

// ---------------------------------------------------------------- zones

FVector2D UVenturesFeature::ZoneC(const FString& ZoneId)
{
	double Z = 205.0;
	if (ZoneId == TEXT("vigil")) Z = 135.0;
	else if (ZoneId == TEXT("hold")) Z = 270.0;
	else if (ZoneId == TEXT("horde")) Z = 340.0;
	else if (ZoneId == TEXT("marks")) Z = 410.0;
	else if (ZoneId == TEXT("spire")) Z = 480.0;
	return FVector2D(-1000.0, Z);
}

bool UVenturesFeature::OnPad(const FString& ZoneId) const
{
	FVector2D P;
	if (!PlayerWebPos(P)) return false;
	const FVector2D C = ZoneC(ZoneId);
	return FMath::Abs(P.X - C.X) < ZoneHalf + 2.0 && FMath::Abs(P.Y - C.Y) < ZoneHalf + 2.0;
}

AAerondelMob* UVenturesFeature::ZoneSpawn(const FString& ZoneId, const FString& Type, float Dx, float Dz, float L, float HpMul, bool bBoss)
{
	const UGameDatabase* Db = UGameDatabase::Get();
	const UMobDef* T = Db ? Db->FindMob(Id(Type)) : nullptr;
	if (!T) return nullptr;
	const float K = FMath::Max(0.2f, L / (float)FMath::Max(1, T->CombatLevel));
	const FVector2D C = ZoneC(ZoneId);
	const float H = ZoneHalf - 1.f;
	const FVector2D P(FMath::Clamp(C.X + Dx, C.X - H, C.X + H), FMath::Clamp(C.Y + Dz, C.Y - H, C.Y + H));
	const int32 Hp = FMath::Max(1, FMath::RoundToInt(T->Hitpoints * K * HpMul));
	AAerondelMob* M = SpawnTunedMob(Type, P, T->DisplayName.ToString(), Hp,
		FMath::Max(3, FMath::RoundToInt(T->MaxHit * FMath::Sqrt(K))), FMath::Max(1, FMath::RoundToInt(T->AttackLevel * K)),
		FMath::Max(1, FMath::RoundToInt(T->DefenceLevel * K)), FMath::RoundToInt(L), 1.f, bBoss);
	if (M) Run.Foes.Add(M);
	return M;
}

TArray<AAerondelMob*> UVenturesFeature::ZoneAlive() const
{
	TArray<AAerondelMob*> Out;
	for (const TWeakObjectPtr<AAerondelMob>& W : Run.Foes) if (W.IsValid() && !W->IsDead()) Out.Add(W.Get());
	return Out;
}

void UVenturesFeature::ZonePark()
{
	for (const TWeakObjectPtr<AAerondelMob>& W : Run.Foes) if (W.IsValid()) W->Destroy();
	Run.Foes.Reset();
}

bool UVenturesFeature::RunStart(const FString& ZoneId, const FString& Kind)
{
	if (Run.bOn || Puz.bOn) { Say(TEXT("Finish what you are doing first.")); return false; }
	Run = FVentureRun();
	Run.bOn = true;
	Run.Zone = ZoneId;
	Run.Kind = Kind;
	Run.StartMs = NowMs();
	FVector2D P;
	if (PlayerWebPos(P) && P.X > -900.0) Run.Return = P;
	const FVector2D C = ZoneC(ZoneId);
	TeleportPlayer(FVector2D(C.X, C.Y + ZoneHalf - 3.0));
	return true;
}

void UVenturesFeature::RunEnd(bool bWin)
{
	if (!Run.bOn) return;
	Run.bOn = false;
	if (Run.Kind == TEXT("breach")) BreachFinish(bWin);
	if (Run.Kind == TEXT("vigil")) VigilFinish(bWin);
	if (Run.Kind == TEXT("horde")) HordeFinish(bWin);
	if (Run.Kind == TEXT("marks")) MarkFinish(bWin);
	if (Run.Kind == TEXT("spire")) SpireFinish(bWin);
	ZonePark();
	const FString Z = Run.Zone;
	const FVector2D Back = Run.Return;
	if (!OnPad(Z)) return;
	if (!bWin) { TeleportPlayer(Back); return; }
	if (UGameInstance* GI = GetGameInstance())
	{
		TWeakObjectPtr<UVenturesFeature> Self(this);
		FTimerHandle H;
		GI->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self, Z, Back] { if (Self.IsValid() && !Self->Run.bOn && Self->OnPad(Z)) Self->TeleportPlayer(Back); }), 1.5f, false);
	}
}

float UVenturesFeature::TakenZone() const
{
	if (Run.bOn && Run.Kind == TEXT("breach") && Run.Mod == TEXT("frenzy")) return 1.3f;
	if (InRunKind(TEXT("marks")) && Run.Mods.Contains(TEXT("frenzy"))) return 1.12f;
	return 1.f;
}

// ---------------------------------------------------------------- tick / kills

void UVenturesFeature::TickZone()
{
	if (Run.bOn)
	{
		if (!OnPad(Run.Zone)) { if (++Run.Away > 3) RunEnd(false); }
		else
		{
			Run.Away = 0;
			if (Run.Kind == TEXT("breach")) BreachStep();
			else if (Run.Kind == TEXT("vigil")) VigilStep();
			else if (Run.Kind == TEXT("horde")) TickHorde();
			else if (Run.Kind == TEXT("marks")) TickMark();
			else if (Run.Kind == TEXT("spire")) TickSpire();
		}
	}
	if (TickN % 5 != 0) return;
	const int64 Now = NowMs();
	if (TideRegion < 0 && Now > TideNext) TideStart();
	if (TideRegion >= 0 && !bTideWaved && Now > TideUntil - 60000) { bTideWaved = true; TideWave(); }
	if (TideRegion >= 0 && Now > TideUntil) { bTideWaved = false; TideEnd(); }
	// Safety: standing on a Ventures pad with no run (e.g. after a reload) sends you home.
	FVector2D P;
	if (!Run.bOn && PlayerWebPos(P) && P.X < -980.0 && (OnPad(TEXT("breach")) || OnPad(TEXT("vigil")) || OnPad(TEXT("horde")) || OnPad(TEXT("marks")) || OnPad(TEXT("spire"))))
		TeleportPlayer(FVector2D(0.0, 10.0));
}

void UVenturesFeature::DiedZone(AAerondelMob* Mob, bool bByPlayer)
{
	bool bVenture = false;
	for (const TWeakObjectPtr<AAerondelMob>& W : Run.Foes) if (W.Get() == Mob) bVenture = true;
	if (bVenture && Run.bOn) Run.Kills++;
	bool bTideBoss = false;
	for (const TWeakObjectPtr<AAerondelMob>& W : TideWaveMobs) if (W.Get() == Mob) bTideBoss = true;
	DiedGear(Mob, bByPlayer, bTideBoss);
	DiedRaid(Mob, bByPlayer);
	DiedSeason(Mob, bByPlayer, bTideBoss);
	if (bByPlayer && !bVenture && InTide())
	{
		TideStacks = FMath::Min(50, TideStacks + 1);
		if (bTideBoss) BonusDrop(Mob, TEXT("the tide"));
	}
}

float UVenturesFeature::RespawnZone(const AAerondelMob* Mob) const
{
	if (TideRegion < 0 || !Mob || !TideRegions.IsValidIndex(TideRegion)) return 1.f;
	const FTideRegion& R = TideRegions[TideRegion];
	const FVector L = Mob->GetActorLocation();
	return FVector2D::Distance(FVector2D(L.X / 100.0, L.Y / 100.0), R.C) < R.R + 10.f ? 1.f / 3.f : 1.f;
}

float UVenturesFeature::XpZone(FName SkillId) const { return InTide() ? TideStacks * 0.01f : 0.f; }

// ---------------------------------------------------------------- Burst Tides

void UVenturesFeature::LoadTideRegions()
{
	bTideLoaded = true;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("world.json")))) return;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return;
	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (!Root->TryGetArrayField(TEXT("regions"), Regions) || !Regions) return;
	for (const TSharedPtr<FJsonValue>& RV : *Regions)
	{
		const TSharedPtr<FJsonObject> R = RV.IsValid() ? RV->AsObject() : nullptr;
		if (!R.IsValid()) continue;
		FTideRegion T;
		T.Name = JStr(*R, TEXT("name"));
		T.C = FVector2D(JNum(*R, TEXT("x")), JNum(*R, TEXT("z")));
		T.R = (float)JNum(*R, TEXT("r"), 50.0);
		if (const TSharedPtr<FJsonObject> C = JObj(*R, TEXT("camp")))
		{
			T.bCamp = true;
			T.Camp = JStr(*C, TEXT("name"));
			T.CampOff = FVector2D(JNum(*C, TEXT("dx")), JNum(*C, TEXT("dz")));
		}
		TideRegions.Add(MoveTemp(T));
	}
}

bool UVenturesFeature::InTide() const
{
	if (TideRegion < 0 || !TideRegions.IsValidIndex(TideRegion)) return false;
	FVector2D P;
	if (!PlayerWebPos(P)) return false;
	const FTideRegion& R = TideRegions[TideRegion];
	return FVector2D::Distance(P, R.C) < R.R + 10.f;
}

void UVenturesFeature::TideStart(int32 RegionIndex)
{
	if (!bTideLoaded) LoadTideRegions();
	if (!TideRegions.Num()) { TideNext = NowMs() + TideGapMs(); return; }
	TideRegion = TideRegions.IsValidIndex(RegionIndex) ? RegionIndex : FMath::RandRange(0, TideRegions.Num() - 1);
	TideUntil = NowMs() + 10 * 60000;
	TideStacks = 0;
	bTideWaved = false;
	TideWaveMobs.Reset();
	TidesSeen++;
	Say(FString::Printf(TEXT("Burst Tide! %s is surging for 10 minutes: foes return fast and every kill there stacks bonus XP."), *TideRegions[TideRegion].Name));
}

void UVenturesFeature::TideEnd()
{
	if (TideRegion < 0) return;
	const FString N = TideRegions.IsValidIndex(TideRegion) ? TideRegions[TideRegion].Name : FString();
	TideRegion = -1;
	TideNext = NowMs() + TideGapMs();
	TidesBest = FMath::Max(TidesBest, TideStacks);
	for (const TWeakObjectPtr<AAerondelMob>& W : TideWaveMobs) if (W.IsValid()) W->Destroy();
	TideWaveMobs.Reset();
	Say(FString::Printf(TEXT("The tide over %s recedes."), *N));
	ScoreOn(TEXT("tide"));
}

void UVenturesFeature::TideWave()
{
	if (!TideRegions.IsValidIndex(TideRegion)) return;
	const FTideRegion& R = TideRegions[TideRegion];
	const int32 Lvl = FMath::Max(30, VLevel());
	const int32 N = UAerondelScalability::GetAppliedTier() <= EAerondelTier::Low ? 2 : 3;
	const FString Type = TideBosses[FMath::Min((int32)UE_ARRAY_COUNT(TideBosses) - 1, Lvl / 110)];
	const FVector2D C = R.bCamp ? R.C + R.CampOff + FVector2D(14.0, 14.0) : R.C;
	const UGameDatabase* Db = UGameDatabase::Get();
	const UMobDef* T = Db ? Db->FindMob(Id(Type)) : nullptr;
	if (!T) return;
	for (int32 i = 0; i < N; ++i)
		if (AAerondelMob* M = SpawnTunedMob(Type, C + FVector2D(i * 3.0, 0.0), T->DisplayName.ToString(), T->Hitpoints, T->MaxHit, T->AttackLevel, T->DefenceLevel, FMath::Max(T->CombatLevel, Lvl), 1.f, T->bBoss))
			TideWaveMobs.Add(M);
	Say(FString::Printf(TEXT("The tide crests: a %s wave rises near %s!"), *Type, R.bCamp ? *R.Camp : *R.Name));
}

void UVenturesFeature::TabTides(FGPanelModel& M)
{
	if (TideRegions.IsValidIndex(TideRegion))
	{
		const FTideRegion& R = TideRegions[TideRegion];
		M.Title = FString::Printf(TEXT("%s is surging"), *R.Name);
		M.Lines.Add(FString::Printf(TEXT("%d min left - stacks %d (+%d%% XP while there)%s"), (int32)FMath::Max<int64>(0, (TideUntil - NowMs() + 59999) / 60000), TideStacks, TideStacks, InTide() ? TEXT("") : TEXT(" - you are not in the region")));
		M.Lines.Add(FString::Printf(TEXT("Boss wave in the last minute near %s."), R.bCamp ? *R.Camp : TEXT("the region centre")));
		M.Lines.Add(FString::Printf(TEXT("Region centre: (%d, %d)."), FMath::RoundToInt(R.C.X), FMath::RoundToInt(R.C.Y)));
	}
	else
	{
		M.Title = TEXT("No tide right now");
		M.Lines.Add(FString::Printf(TEXT("The next surge comes in about %d minutes of play."), (int32)FMath::Max<int64>(1, (TideNext - NowMs()) / 60000)));
		M.Lines.Add(FString::Printf(TEXT("Tides seen: %d - best stack: %d"), TidesSeen, TidesBest));
	}
	M.Lines.Add(TEXT("Every 20-30 minutes a region surges for 10 minutes. Foes there come back 3x faster and each kill adds +1% XP (up to +50%)."));
}

// ---------------------------------------------------------------- Breach Runs

void UVenturesFeature::BreachStart(const FString& Mode, bool bHard)
{
	if (!FindMode(Mode)) return;
	if (bHard && VLevel() < 500) { Say(TEXT("Hard breaches open at an average combat level of 500.")); return; }
	const FString Mod = BreachMods[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(BreachMods) - 1)].Key;
	if (!RunStart(TEXT("breach"), TEXT("breach"))) return;
	Run.Mode = Mode;
	Run.Mod = Mod;
	Run.bHard = bHard;
	Run.L = FMath::RoundToInt(VLevel() * (bHard ? 1.5f : 1.f));
	Run.Set = FMath::Min(3, VLevel() / 150);
	const FBreachMod* Md = FindMod(Mod);
	Say(FString::Printf(TEXT("%s%s. Twist: %s - %s."), FindMode(Mode)->Name, bHard ? TEXT(" (Hard)") : TEXT(""), Md->Name, Md->Desc));
	BreachSetup();
}

static void BreachSpawn(UVenturesFeature* V, int32 N, const FVector2D* Near, int32 Cap, int32 Alive, int32 Set, int32 L, bool bShield, TFunctionRef<AAerondelMob*(const FString&, float, float)> Spawn)
{
	const int32 Room = Cap - Alive;
	for (int32 i = 0; i < FMath::Min(N, Room); ++i)
	{
		const FVector2D D = Near ? *Near : FVector2D((FMath::FRand() - 0.5) * 20.0, -4.0 - FMath::FRand() * 8.0);
		Spawn(BreachFoes[Set][FMath::RandRange(0, 2)], D.X + (FMath::FRand() - 0.5f) * 3.f, D.Y + (FMath::FRand() - 0.5f) * 3.f);
	}
}

void UVenturesFeature::BreachSetup()
{
	const float HpMul = Run.Mod == TEXT("shielded") ? 1.5f : 1.f;
	auto Spawn = [this, HpMul](const FString& T, float Dx, float Dz) { return ZoneSpawn(TEXT("breach"), T, Dx, Dz, (float)Run.L, HpMul); };
	if (Run.Mode == TEXT("ward"))
	{
		Run.Wave = 1;
		BreachSpawn(this, 5, nullptr, CapFoes(15), ZoneAlive().Num(), Run.Set, Run.L, HpMul > 1.f, Spawn);
	}
	if (Run.Mode == TEXT("nodes"))
	{
		static const FVector2D Off[] = { FVector2D(-8.0, -6.0), FVector2D(8.0, -6.0), FVector2D(0.0, -11.0) };
		for (const FVector2D& O : Off)
		{
			Run.NodePos.Add(O);
			Run.NodeP.Add(0.f);
			BreachSpawn(this, 2, &O, CapFoes(15), ZoneAlive().Num(), Run.Set, Run.L, HpMul > 1.f, Spawn);
		}
	}
	if (Run.Mode == TEXT("purge"))
	{
		Run.Goal = 24;
		BreachSpawn(this, 8, nullptr, CapFoes(15), ZoneAlive().Num(), Run.Set, Run.L, HpMul > 1.f, Spawn);
	}
}

void UVenturesFeature::BreachStep()
{
	const float HpMul = Run.Mod == TEXT("shielded") ? 1.5f : 1.f;
	auto Spawn = [this, HpMul](const FString& T, float Dx, float Dz) { return ZoneSpawn(TEXT("breach"), T, Dx, Dz, (float)Run.L, HpMul); };
	const int32 Alive = ZoneAlive().Num();
	const int32 Cap = CapFoes(15);
	AAerondelPlayerState* PS = GetPS();
	if (Run.Mod == TEXT("burning") && TickN % 3 == 0 && PS && PS->Health && PS->Health->GetCurrent() > 1)
		PS->Health->SetCurrentRaw(FMath::Max(1, PS->Health->GetCurrent() - FMath::CeilToInt(PS->Health->GetMax() * 0.01f)));
	if (NowMs() - Run.StartMs > 8 * 60000) { Say(TEXT("The breach collapses - out of time.")); RunEnd(false); return; }
	if (Run.Mode == TEXT("ward"))
	{
		Run.Ward -= Alive * 0.25f;
		if (Run.Ward <= 0.f) { Say(TEXT("The ward shatters!")); RunEnd(false); return; }
		if (!Alive)
		{
			if (Run.Wave >= 3) { RunEnd(true); return; }
			Run.Wave++;
			Say(FString::Printf(TEXT("Wave %d/3."), Run.Wave));
			BreachSpawn(this, 4 + Run.Wave * 2, nullptr, Cap, 0, Run.Set, Run.L, HpMul > 1.f, Spawn);
		}
	}
	else if (Run.Mode == TEXT("nodes"))
	{
		FVector2D P;
		PlayerWebPos(P);
		const FVector2D C = ZoneC(TEXT("breach"));
		bool bAll = true;
		for (int32 i = 0; i < Run.NodePos.Num(); ++i)
		{
			if (Run.NodeP[i] < 100.f && FVector2D::Distance(P, C + Run.NodePos[i]) < 3.0)
			{
				Run.NodeP[i] += 8.f;
				if (Run.NodeP[i] >= 100.f)
				{
					Say(TEXT("A breach node closes."));
					bool bMore = false;
					for (float Q : Run.NodeP) if (Q < 100.f) bMore = true;
					if (bMore) BreachSpawn(this, 3, nullptr, Cap, ZoneAlive().Num(), Run.Set, Run.L, HpMul > 1.f, Spawn);
				}
			}
			if (Run.NodeP[i] < 100.f) bAll = false;
		}
		if (bAll) { RunEnd(true); return; }
		if (TickN % 12 == 0 && Alive < 4) BreachSpawn(this, 2, nullptr, Cap, Alive, Run.Set, Run.L, HpMul > 1.f, Spawn);
	}
	else if (Run.Mode == TEXT("purge"))
	{
		if (Run.Kills >= Run.Goal) { RunEnd(true); return; }
		if (Alive < 4 && Run.Kills + Alive < Run.Goal) BreachSpawn(this, FMath::Min(5, Run.Goal - Run.Kills - Alive), nullptr, Cap, Alive, Run.Set, Run.L, HpMul > 1.f, Spawn);
	}
}

void UVenturesFeature::BreachFinish(bool bWin)
{
	if (!bWin) { Say(TEXT("The breach run failed. No reward.")); return; }
	const FBreachMode* Md = FindMode(Run.Mode);
	const FBreachMod* Mo = FindMod(Run.Mod);
	if (!Md || !Mo) return;
	const float Secs = (NowMs() - Run.StartMs) / 1000.f;
	const float Par = (float)Md->Par;
	const FString Grade = Secs < Par ? TEXT("S") : Secs < Par * 1.5f ? TEXT("A") : Secs < Par * 2.2f ? TEXT("B") : TEXT("C");
	const float GBase = Grade == TEXT("S") ? 2.f : Grade == TEXT("A") ? 1.5f : Grade == TEXT("B") ? 1.2f : 1.f;
	const float Gm = GBase * Mo->Mul * (Run.bHard ? 2.f : 1.f);
	const int32 CoinsWon = FMath::RoundToInt((200 + Run.L * 6) * Gm * (Run.Mod == TEXT("gilded") ? 1.5f : 1.f));
	const float Xp = FMath::RoundToFloat((60 + Run.L * 4) * Gm);
	const int32 Anima = FMath::RoundToInt(2 * Gm);
	GiveCoins(CoinsWon);
	GiveXp(StyleSkill(Style()), Xp);
	GiveXp(TEXT("Hitpoints"), Xp / 3.f);
	BreachRuns++;
	BreachAnima += Anima;
	const FString* Prev = BreachBest.Find(Run.Mode);
	if (!Prev || GradeIdx(Grade) < GradeIdx(*Prev)) BreachBest.Add(Run.Mode, Grade);
	Say(FString::Printf(TEXT("Breach cleared in %ds - grade %s! +%d coins, +%d Anima."), FMath::RoundToInt(Secs), *Grade, CoinsWon, Anima));
	if (Grade == TEXT("S") || Run.bHard || FMath::FRand() < 0.35f) BonusDrop(nullptr, TEXT("the breach"));
	BreachClearGear(Run.Mode);
	ChronBreach();
	MarkFromBreach(Run.bHard, Grade);
}

void UVenturesFeature::TabBreach(FGPanelModel& M)
{
	if (InRunKind(TEXT("breach")))
	{
		const FBreachMode* Md = FindMode(Run.Mode);
		const FBreachMod* Mo = FindMod(Run.Mod);
		int32 Closed = 0;
		for (float Q : Run.NodeP) if (Q >= 100.f) ++Closed;
		M.Title = FString::Printf(TEXT("%s - %s"), Md ? Md->Name : TEXT("Breach"), Mo ? Mo->Name : TEXT(""));
		const FString St = Run.Mode == TEXT("ward") ? FString::Printf(TEXT("Ward %d%% - wave %d/3"), FMath::Max(0, FMath::RoundToInt(Run.Ward)), Run.Wave)
			: Run.Mode == TEXT("nodes") ? FString::Printf(TEXT("Nodes closed %d/3"), Closed) : FString::Printf(TEXT("Kills %d/%d"), Run.Kills, Run.Goal);
		M.Lines.Add(FString::Printf(TEXT("%s - %ds"), *St, (int32)((NowMs() - Run.StartMs) / 1000)));
		if (Md) M.Lines.Add(Md->Desc);
		if (Run.Mode == TEXT("nodes"))
			for (int32 i = 0; i < Run.NodePos.Num(); ++i)
				M.Lines.Add(FString::Printf(TEXT("Node %d at pad (%+d, %+d): %d%%"), i + 1, FMath::RoundToInt(Run.NodePos[i].X), FMath::RoundToInt(Run.NodePos[i].Y), FMath::Min(100, FMath::RoundToInt(Run.NodeP[i]))));
		VAct3(M, TEXT("Abandon run"), [this] { RunEnd(false); });
		return;
	}
	M.Title = FString::Printf(TEXT("Breach Runs - runs %d, Anima %d"), BreachRuns, BreachAnima);
	const bool bHardOk = VLevel() >= 500;
	for (const FBreachMode& Md : Modes)
	{
		const FString K = Md.Key;
		const FString* B = BreachBest.Find(K);
		M.Lines.Add(FString::Printf(TEXT("%s (best %s): %s"), Md.Name, B ? **B : TEXT("-"), Md.Desc));
		VAct3(M, FString::Printf(TEXT("Start %s"), Md.Name), [this, K] { BreachStart(K, false); }, !Run.bOn, true);
		if (bHardOk) VAct3(M, FString::Printf(TEXT("Start %s (Hard)"), Md.Name), [this, K] { BreachStart(K, true); }, !Run.bOn, true);
	}
	M.Lines.Add(FString::Printf(TEXT("Foes match your level (%d). Time limit 8 minutes. At most %d foes at once."), VLevel(), CapFoes(15)));
}

// ---------------------------------------------------------------- Tetherline

bool UVenturesFeature::Tether()
{
	const int64 Now = NowMs();
	if (Now < TetherReady) { Say(FString::Printf(TEXT("Tetherline recharging (%ds)."), (int32)((TetherReady - Now + 999) / 1000))); return false; }
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	if (!C) return false;
	const FVector P = C->GetActorLocation();
	AAerondelMob* Mob = nullptr;
	FVector Goal = FVector::ZeroVector;
	float Best = 0.f;
	if (AAerondelMob* T = CurrentTarget())
	{
		const float D = FVector::Dist2D(T->GetActorLocation(), P) / 100.f;
		if (D > 3.f && D < 18.f) { Mob = T; Goal = T->GetActorLocation(); Best = D; }
	}
	if (!Mob)
	{
		const FVector F = C->GetActorForwardVector().GetSafeNormal2D();
		for (TActorIterator<AResourceNode> It(C->GetWorld()); It; ++It)
		{
			const AResourceNode* N = *It;
			if (!N || N->IsDepleted() || !N->Definition) continue;
			if (N->Definition->Kind != EResourceKind::Tree && N->Definition->Kind != EResourceKind::Rock) continue;
			const FVector Dv = N->GetActorLocation() - P;
			const float D = FVector(Dv.X, Dv.Y, 0.f).Size() / 100.f;
			if (D < 4.f || D > 18.f) continue;
			if (FVector::DotProduct(FVector(Dv.X, Dv.Y, 0.f).GetSafeNormal(), F) < 0.5f) continue;
			if (Best <= 0.f || D < Best) { Best = D; Goal = N->GetActorLocation(); }
		}
	}
	if (Best <= 0.f) { Say(TEXT("Nothing to tether to - face a tree or rock, or target a foe 4-18 m away.")); return false; }
	const FVector Dir = FVector(Goal.X - P.X, Goal.Y - P.Y, 0.f).GetSafeNormal();
	const float GoCm = (Best - 2.f) * 100.f;
	C->LaunchCharacter(Dir * (GoCm / 0.26f) + FVector(0.f, 0.f, 150.f), true, true);
	TetherReady = Now + (int64)FMath::Max(3, 10 - SkillLevel(TEXT("Agility")) / 15) * 1000;
	if (Mob)
	{
		TWeakObjectPtr<UVenturesFeature> Self(this);
		TWeakObjectPtr<AAerondelMob> WM(Mob);
		FTimerHandle H;
		if (UGameInstance* GI = GetGameInstance())
			GI->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self, WM]
			{
				if (!Self.IsValid() || !WM.IsValid() || WM->IsDead()) return;
				Self->Strike(WM.Get(), 0.3f, 0.2f, TEXT("tether"));
				Self->Stagger(WM.Get(), 14.f);
			}), 0.26f, false);
	}
	return true;
}

void UVenturesFeature::TabTether(FGPanelModel& M)
{
	M.Title = TEXT("Tetherline (L)");
	const int64 Left = TetherReady - NowMs();
	M.Lines.Add(Left > 0 ? FString::Printf(TEXT("Recharging: %ds."), (int32)((Left + 999) / 1000)) : FString(TEXT("Ready.")));
	M.Lines.Add(FString::Printf(TEXT("Zip to your target foe (a strike that builds Crestbreak stagger) or to the tree or rock you face, 4-18 m away. Cooldown %ds (10 s, down to 3 s with Agility)."), FMath::Max(3, 10 - SkillLevel(TEXT("Agility")) / 15)));
	VAct3(M, TEXT("Tether now"), [this] { Tether(); }, Left <= 0, true);
}
