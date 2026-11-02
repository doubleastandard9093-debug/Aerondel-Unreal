#include "Features/TasksFeature.h"
#include "Features/StandingFeature.h"
#include "Core/AerondelEvents.h"
#include "Core/AerondelPlayerState.h"
#include "Data/GameDatabase.h"
#include "Data/MobDef.h"
#include "Dom/JsonValue.h"

bool UTasksFeature::IsDrowned(const UMobDef* M)
{
	if (!M) return false;
	const FString S = M->Id.ToString().ToLower();
	return S.Contains(TEXT("drowned")) || S.Contains(TEXT("saltgrave"));
}

void UTasksFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UTasksFeature::HandleMobKilled);
}

FString UTasksFeature::CurrentLabel() const
{
	if (CurMob.IsNone()) return TEXT("No task. Visit Huntmaster Corvane in Saltreach.");
	const UGameDatabase* Db = UGameDatabase::Get(); const UMobDef* M = Db ? Db->FindMob(CurMob) : nullptr;
	return FString::Printf(TEXT("%d %s (%d/%d)"), CurN, M ? *M->DisplayName.ToString() : *CurMob.ToString(), CurGot, CurN);
}

FString UTasksFeature::Assign()
{
	if (!CurMob.IsNone() && CurGot < CurN) return TEXT("Finish your current task first, or pay to skip it.");
	const UGameDatabase* Db = UGameDatabase::Get(); if (!Db) return FString();
	const int32 Cb = CombatLevel();
	TArray<const UMobDef*> Pool;
	for (const UMobDef* M : Db->Mobs)
	{
		if (!M || M->bBoss || M->Id == CurMob || Blocked.Contains(M->Id)) continue;
		const FString S = M->Id.ToString();
		if (S == TEXT("cow") || S == TEXT("guard") || S.Contains(TEXT("ashen")) || S.Contains(TEXT("duellist"))) continue;
		if (M->CombatLevel > Cb + 15 || M->CombatLevel < FMath::Min(Cb - 40, 20)) continue;
		Pool.Add(M);
	}
	const UMobDef* Pick = Pool.Num() ? Pool[FMath::RandRange(0, Pool.Num() - 1)] : Db->FindMob(TEXT("goblin"));
	if (!Pick) return TEXT("Corvane has nothing for you right now.");
	CurMob = Pick->Id; CurGot = 0; CurN = 10 + FMath::RandRange(0, 15) + Cb / 10;
	if (Extended.Contains(CurMob)) CurN = FMath::RoundToInt(CurN * 1.5f);
	return FString::Printf(TEXT("Your new task: %s."), *CurrentLabel());
}

FString UTasksFeature::Skip()
{
	if (CurMob.IsNone()) return TEXT("You have no task to skip.");
	if (Points < 30) return TEXT("Skipping costs 30 Hunt points.");
	Points -= 30; CurMob = NAME_None; return Assign();
}

FString UTasksFeature::Block()
{
	if (CurMob.IsNone()) return TEXT("You have no task to block.");
	if (Blocked.Num() >= 4) return TEXT("You can block at most 4 monsters.");
	if (Points < 100) return TEXT("Blocking costs 100 Hunt points.");
	Points -= 100; Blocked.AddUnique(CurMob); CurMob = NAME_None; return Assign();
}

FString UTasksFeature::Extend()
{
	if (CurMob.IsNone() || Extended.Contains(CurMob)) return TEXT("Nothing to extend.");
	if (Points < 75) return TEXT("Extending costs 75 Hunt points.");
	Points -= 75; Extended.AddUnique(CurMob); return TEXT("Future tasks of this monster will be longer and worth more.");
}

int32 UTasksFeature::PointsFor(const UMobDef* M) const
{
	int32 P = 12 + (M ? M->CombatLevel : 10) / 6;
	if (IsDrowned(M)) P = FMath::RoundToInt(P * 1.5f);
	if (M && Extended.Contains(M->Id)) P = FMath::RoundToInt(P * 1.3f);
	const int32 S = Streak + 1;
	if (S % 50 == 0) P *= 15; else if (S % 10 == 0) P *= 5;
	return P;
}

void UTasksFeature::HandleMobKilled(UMobDef* Mob)
{
	if (!Mob || CurMob.IsNone() || Mob->Id != CurMob || CurGot >= CurN) return;
	if (++CurGot < CurN) { if (CurGot % 5 == 0 || CurN - CurGot <= 3) Say(FString::Printf(TEXT("Hunt task: %s."), *CurrentLabel())); return; }
	const int32 P = PointsFor(Mob); Points += P; ++Streak; ++Done;
	if (IsDrowned(Mob)) if (UStandingFeature* St = Feature<UStandingFeature>()) St->AddRep(FName(TEXT("drowned")), 60);
	Say(FString::Printf(TEXT("Hunt task complete! +%d Hunt points (%d total, streak %d)."), P, Points, Streak));
	CurMob = NAME_None;
}

float UTasksFeature::DamageDealtBonus(const UMobDef* Target) const
{
	if (!Target) return 0.f;
	float B = 0.f;
	if (Target->Id == CurMob && Unlocks.Contains(TEXT("insight"))) B += 0.1f;
	if (IsDrowned(Target) && Unlocks.Contains(TEXT("reefbane"))) B += 0.15f;
	return B;
}

FString UTasksFeature::Buy(FName ShopId)
{
	struct FS { const TCHAR* Id; int32 Cost; bool bUnlock; const TCHAR* Item; int32 Qty; int32 Drowned; };
	static const FS Shop[] = {
		{ TEXT("insight"), 200, true, nullptr, 0, 0 }, { TEXT("ring"), 150, false, TEXT("Hunter's ring"), 1, 0 }, { TEXT("fire"), 25, false, TEXT("Fire rune"), 100, 0 },
		{ TEXT("lamp"), 60, false, nullptr, 0, 0 }, { TEXT("reefward"), 300, false, TEXT("Reefward amulet"), 1, 2 }, { TEXT("reefbane"), 400, true, nullptr, 0, 3 },
		{ TEXT("saltring"), 750, false, TEXT("Saltgrave's signet"), 1, 4 } };
	const FS* S = nullptr; for (const FS& X : Shop) if (ShopId == FName(X.Id)) S = &X;
	if (!S) return TEXT("Unknown item.");
	if (S->bUnlock && Unlocks.Contains(S->Id)) return TEXT("You already have that.");
	if (S->Drowned) { const UStandingFeature* St = Feature<UStandingFeature>(); if (!St || St->TierFor(FName(TEXT("drowned"))) < S->Drowned) return TEXT("The Drowned Crew don't fear you enough yet."); }
	if (Points < S->Cost) return FString::Printf(TEXT("That costs %d points; you have %d."), S->Cost, Points);
	Points -= S->Cost;
	if (S->bUnlock) Unlocks.AddUnique(S->Id);
	else if (S->Item) GiveItem(S->Item, S->Qty);
	else
	{
		const TCHAR* Low = TEXT("Attack"); int32 L = SkillLevel(Low);
		for (const TCHAR* Sk : { TEXT("Strength"), TEXT("Defence"), TEXT("Ranged"), TEXT("Magic"), TEXT("Hitpoints") }) if (SkillLevel(Sk) < L) { L = SkillLevel(Sk); Low = Sk; }
		GiveXp(Low, 2500.f);
	}
	return TEXT("Bought.");
}

static void PutNames(FJsonObject& O, const TCHAR* K, const TArray<FName>& A)
{
	TArray<TSharedPtr<FJsonValue>> V; for (const FName& N : A) V.Add(MakeShared<FJsonValueString>(N.ToString()));
	O.SetArrayField(K, V);
}

void UTasksFeature::SaveState(FJsonObject& Out) const
{
	if (!CurMob.IsNone())
	{ TSharedPtr<FJsonObject> C = MakeShared<FJsonObject>(); C->SetStringField(TEXT("mon"), CurMob.ToString()); C->SetNumberField(TEXT("n"), CurN); C->SetNumberField(TEXT("got"), CurGot); Out.SetObjectField(TEXT("cur"), C); }
	Out.SetNumberField(TEXT("points"), Points); Out.SetNumberField(TEXT("streak"), Streak); Out.SetNumberField(TEXT("done"), Done);
	PutNames(Out, TEXT("blocked"), Blocked); PutNames(Out, TEXT("extended"), Extended); PutStrArr(Out, TEXT("unlocks"), Unlocks);
}

void UTasksFeature::LoadState(const FJsonObject& In)
{
	const UGameDatabase* Db = UGameDatabase::Get();
	auto Valid = [Db](const FString& S) { const FName N = Id(S); return Db && Db->FindMob(N) ? N : NAME_None; };
	if (TSharedPtr<FJsonObject> C = JObj(In, TEXT("cur")))
	{ CurMob = Valid(JStr(*C, TEXT("mon"))); CurN = FMath::Max(1, (int32)JInt(*C, TEXT("n"), 1)); CurGot = FMath::Clamp((int32)JInt(*C, TEXT("got")), 0, CurN); }
	Points = FMath::Max(0, (int32)JInt(In, TEXT("points"))); Streak = FMath::Max(0, (int32)JInt(In, TEXT("streak"))); Done = FMath::Max(0, (int32)JInt(In, TEXT("done")));
	for (const FString& S : JStrArr(In, TEXT("blocked"))) { const FName N = Valid(S); if (!N.IsNone() && Blocked.Num() < 4) Blocked.AddUnique(N); }
	for (const FString& S : JStrArr(In, TEXT("extended"))) { const FName N = Valid(S); if (!N.IsNone() && Extended.Num() < 8) Extended.AddUnique(N); }
	for (const FString& S : JStrArr(In, TEXT("unlocks"))) if (Unlocks.Num() < 16) Unlocks.AddUnique(S);
}
