#include "Features/WritsFeature.h"
#include "Features/StandingFeature.h"
#include "Features/ArenaFeature.h"
#include "Core/AerondelEvents.h"
#include "Data/MobDef.h"
#include "Data/SkillDef.h"
#include "Dom/JsonValue.h"

namespace WritData
{
	struct FS { bool bKill; const TCHAR* Target; int32 N; };
	static const TCHAR* Factions[] = { TEXT("watch"), TEXT("mariners"), TEXT("circle"), TEXT("lantern") };
	static const FS Watch[] = { { true, TEXT("Goblin"), 12 }, { true, TEXT("Bandit"), 6 }, { true, TEXT("Crag Giant"), 4 }, { true, TEXT("Wolf"), 8 }, { false, TEXT("Defence"), 40 }, { true, TEXT("Ironveil Knight"), 3 } };
	static const FS Mariners[] = { { false, TEXT("Fishing"), 25 }, { false, TEXT("Cooking"), 25 }, { true, TEXT("Drowned Deckhand"), 5 }, { false, TEXT("Ranged"), 40 }, { true, TEXT("Drowned Gunner"), 3 } };
	static const FS Circle[] = { { false, TEXT("Magic"), 40 }, { false, TEXT("Glyphcraft"), 20 }, { false, TEXT("Herbalism"), 15 }, { true, TEXT("Skeleton"), 6 }, { true, TEXT("Zombie"), 8 } };
	static const FS Lantern[] = { { false, TEXT("Prayer"), 15 }, { false, TEXT("Smithing"), 25 }, { false, TEXT("Mining"), 30 }, { true, TEXT("Bog Giant"), 4 }, { true, TEXT("Giant Spider"), 6 } };
	static const FS WeeklyPool[] = { { true, TEXT("Grumsh"), 1 }, { true, TEXT("Captain Saltgrave"), 1 }, { true, TEXT("Cinder Fiend"), 5 }, { true, TEXT("Ogre"), 10 } };
	static constexpr int64 DayMs = 86400000;
	static uint32 Hash(int64 A, int64 B)
	{
		uint32 H = (uint32)((uint64)A * 2654435761ULL + (uint64)B * 40503ULL);
		H ^= H >> 13; H = H * 2246822519U; return H;
	}
	static FWrit Make(const TCHAR* Fac, const FS& S) { FWrit W; W.Faction = FName(Fac); W.bKill = S.bKill; W.Target = S.Target; W.N = S.N; return W; }
}

void UWritsFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) { Ev->OnMobKilled.AddDynamic(this, &UWritsFeature::HandleMobKilled); Ev->OnXpGained.AddDynamic(this, &UWritsFeature::HandleXp); }
}

void UWritsFeature::OnGameTick()
{
	++PlayTicks;
	if (++Ticks % 100 == 5 && MaybeRoll(false) == TEXT("daily")) Say(TEXT("New Daily Writs are up on Writkeeper Maelis's board in Larkhaven."));
}

FString UWritsFeature::MaybeRoll(bool bForce)
{
	const int64 Now = NowMs();
	if (LastNow && Now < LastNow - 3600000) return FString();
	LastNow = FMath::Max(LastNow, Now);
	const int64 D = Now / WritData::DayMs, Wk = (Now / WritData::DayMs + 3) / 7;
	FString Out;
	const bool bFirst = Daily.Num() == 0;
	if ((D > Day && (bForce || bFirst || PlayTicks >= 600)) || bFirst)
	{
		Day = D; Daily.Reset(); PlayTicks = 0;
		for (int32 Fi = 0; Fi < 4; ++Fi)
		{
			const WritData::FS* Pool = Fi == 0 ? WritData::Watch : Fi == 1 ? WritData::Mariners : Fi == 2 ? WritData::Circle : WritData::Lantern;
			const int32 Len = Fi == 0 ? UE_ARRAY_COUNT(WritData::Watch) : 5;
			const int32 Start = WritData::Hash(D, Fi) % Len;
			for (int32 K = 0; K < 3; ++K) Daily.Add(WritData::Make(WritData::Factions[Fi], Pool[(Start + K * 2) % Len]));
		}
		Out = TEXT("daily");
	}
	if (Wk > WeekN || !bHasWeekly)
	{
		WeekN = Wk; bHasWeekly = true;
		Weekly = WritData::Make(WritData::Factions[WritData::Hash(Wk, 7) % 4], WritData::WeeklyPool[WritData::Hash(Wk, 99) % UE_ARRAY_COUNT(WritData::WeeklyPool)]);
		if (Out.IsEmpty()) Out = TEXT("weekly");
	}
	return Out;
}

FString UWritsFeature::Label(const FWrit& W)
{
	return FString::Printf(TEXT("%s (%d/%d)%s"), W.bKill ? *FString::Printf(TEXT("Defeat %d %s"), W.N, *W.Target) : *FString::Printf(TEXT("%d %s actions"), W.N, *W.Target), W.Got, W.N, W.bDone ? TEXT(" (done)") : TEXT(""));
}

void UWritsFeature::Reward(const FWrit& W, bool bWeekly)
{
	if (UStandingFeature* St = Feature<UStandingFeature>()) St->AddRep(W.Faction, bWeekly ? 80 : 20);
	GiveCoins(bWeekly ? 2000 : 150);
	if (bWeekly) { if (UArenaFeature* A = Feature<UArenaFeature>()) A->AddTokens(10); ++WeeklyDone; } else ++Done;
	Say(FString::Printf(TEXT("%s done: %s."), bWeekly ? TEXT("Great deed") : TEXT("Writ"), *Label(W)));
}

void UWritsFeature::Progress(bool bKill, const FString& Target, int32 N)
{
	auto Step = [&](FWrit& W, bool bWeekly)
	{
		if (W.bDone || W.bKill != bKill || Id(W.Target) != Id(Target)) return;
		W.Got = FMath::Min(W.N, W.Got + N);
		if (W.Got >= W.N) { W.bDone = true; Reward(W, bWeekly); }
	};
	for (FWrit& W : Daily) Step(W, false);
	if (bHasWeekly) Step(Weekly, true);
}

void UWritsFeature::HandleMobKilled(UMobDef* Mob) { if (Mob) Progress(true, Mob->Id.ToString()); }
void UWritsFeature::HandleXp(USkillDef* Skill, float Amount, float TotalXp, int32 Level) { if (Skill && Amount > 0.f) Progress(false, Skill->Id.ToString()); }

static TSharedPtr<FJsonObject> WritToJson(const FWrit& W)
{
	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("fac"), W.Faction.ToString()); J->SetStringField(TEXT("kind"), W.bKill ? TEXT("kill") : TEXT("act"));
	J->SetStringField(TEXT("target"), W.Target); J->SetNumberField(TEXT("n"), W.N); J->SetNumberField(TEXT("got"), W.Got); J->SetBoolField(TEXT("done"), W.bDone);
	return J;
}

static bool WritFromJson(const TSharedPtr<FJsonObject>& J, FWrit& W)
{
	if (!J.IsValid()) return false;
	FString Fac, Kind, Target;
	if (!J->TryGetStringField(TEXT("fac"), Fac) || !J->TryGetStringField(TEXT("kind"), Kind) || !J->TryGetStringField(TEXT("target"), Target)) return false;
	bool bOkFac = false; for (const TCHAR* F : WritData::Factions) if (Fac == F) bOkFac = true;
	if (!bOkFac || (Kind != TEXT("kill") && Kind != TEXT("act"))) return false;
	double N = 1, Got = 0; bool bDone = false;
	J->TryGetNumberField(TEXT("n"), N); J->TryGetNumberField(TEXT("got"), Got); J->TryGetBoolField(TEXT("done"), bDone);
	W.Faction = FName(*Fac); W.bKill = Kind == TEXT("kill"); W.Target = Target; W.N = FMath::Max(1, (int32)N); W.Got = FMath::Max(0, (int32)Got); W.bDone = bDone;
	return true;
}

void UWritsFeature::SaveState(FJsonObject& Out) const
{
	Out.SetNumberField(TEXT("day"), (double)Day); Out.SetNumberField(TEXT("week"), (double)WeekN); Out.SetNumberField(TEXT("lastNow"), (double)LastNow);
	Out.SetNumberField(TEXT("playTicks"), PlayTicks); Out.SetNumberField(TEXT("done"), Done); Out.SetNumberField(TEXT("weeklyDone"), WeeklyDone);
	TArray<TSharedPtr<FJsonValue>> A; for (const FWrit& W : Daily) A.Add(MakeShared<FJsonValueObject>(WritToJson(W)));
	Out.SetArrayField(TEXT("daily"), A);
	if (bHasWeekly) Out.SetObjectField(TEXT("weekly"), WritToJson(Weekly));
}

void UWritsFeature::LoadState(const FJsonObject& In)
{
	Day = FMath::Max<int64>(0, JInt(In, TEXT("day"))); WeekN = FMath::Max<int64>(0, JInt(In, TEXT("week"))); LastNow = FMath::Max<int64>(0, JInt(In, TEXT("lastNow")));
	PlayTicks = FMath::Max(0, (int32)JInt(In, TEXT("playTicks"))); Done = FMath::Max(0, (int32)JInt(In, TEXT("done"))); WeeklyDone = FMath::Max(0, (int32)JInt(In, TEXT("weeklyDone")));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (In.TryGetArrayField(TEXT("daily"), A) && A) for (const TSharedPtr<FJsonValue>& V : *A) { FWrit W; if (V.IsValid() && Daily.Num() < 12 && WritFromJson(V->AsObject(), W)) Daily.Add(W); }
	bHasWeekly = WritFromJson(JObj(In, TEXT("weekly")), Weekly);
}
