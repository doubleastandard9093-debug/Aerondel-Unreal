#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/AerondelMob.h"
#include "Data/MobDef.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================================
// Ventures group 6 (web adapt/chronicle.js, glidewalk.js): Season Chronicle and Glidewalk (C dash / glide).
// ============================================================================================

namespace VenturesSeason
{
	struct FChapter { const TCHAR* Season; const TCHAR* Title; const TCHAR* Steps[4]; const TCHAR* Artifact; };
	static const FChapter Chapters[] = {
		{ TEXT("spring"), TEXT("The Thawbound Pact"), { TEXT("Wake the river spirits: defeat 25 foes"), TEXT("Break a boss crest or slay a boss"), TEXT("Clear a Breach Run"), TEXT("Finish a Burst Tide") }, TEXT("Bloomward Seal") },
		{ TEXT("summer"), TEXT("Embers of Solmere"), { TEXT("Drive back the sun-touched: defeat 25 foes"), TEXT("Slay a boss"), TEXT("Clear a Breach Run"), TEXT("Finish a Burst Tide") }, TEXT("Sunforged Seal") },
		{ TEXT("autumn"), TEXT("The Harrowing Lanterns"), { TEXT("Gather the lantern-wraiths: defeat 25 foes"), TEXT("Slay a boss"), TEXT("Clear a Breach Run"), TEXT("Finish a Burst Tide") }, TEXT("Harvestmoon Seal") },
		{ TEXT("winter"), TEXT("Song of the Long Night"), { TEXT("Hold the snowline: defeat 25 foes"), TEXT("Slay a boss"), TEXT("Clear a Breach Run"), TEXT("Finish a Burst Tide") }, TEXT("Rimeheart Seal") },
	};
	struct FChronMod { const TCHAR* Key; const TCHAR* Name; int32 Lv; const TCHAR* Desc; };
	static const FChronMod ChronMods[] = {
		{ TEXT("breaker"), TEXT("Ward-breaker"), 2, TEXT("+12% damage to bosses and elites") },
		{ TEXT("overload"), TEXT("Overload"), 4, TEXT("Hits have a 6% chance to stun a foe for 2 ticks") },
		{ TEXT("steadfast"), TEXT("Steadfast"), 6, TEXT("6% less damage taken") },
		{ TEXT("tidecall"), TEXT("Tidecaller"), 8, TEXT("+8% XP") },
		{ TEXT("radiant"), TEXT("Radiant"), 10, TEXT("+5% accuracy and damage") },
	};
	static const int32 ChronNeed[] = { 25, 1, 1, 1 };
	static int32 ChronFor(int32 Lv) { return 100 + Lv * 60; }
	static const FChronMod* FindChronMod(const FString& K) { for (const FChronMod& M : ChronMods) if (K == M.Key) return &M; return nullptr; }
	static FString RealSeason()
	{
		const int32 M = FDateTime::Now().GetMonth() - 1; // JS getMonth: 0-11, local time
		return M >= 2 && M <= 4 ? TEXT("spring") : M >= 5 && M <= 7 ? TEXT("summer") : M >= 8 && M <= 10 ? TEXT("autumn") : TEXT("winter");
	}
	static const FChapter& Chapter() { const FString S = RealSeason(); for (const FChapter& C : Chapters) if (S == C.Season) return C; return Chapters[1]; }
	static FString ChronKey() { return FString::Printf(TEXT("%s-%d"), *RealSeason(), FDateTime::Now().GetYear()); }
}
using namespace VenturesSeason;

static void VAct6(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }

void UVenturesFeature::ResetSeason()
{
	ChronKeyCur.Reset();
	ChronSteps = { 0, 0, 0, 0 };
	ChronLv = 0;
	ChronXp = 0.f;
	ChronOn.Reset();
	bChronDone = false;
	DashReady = 0;
}

void UVenturesFeature::SaveSeason(FJsonObject& A) const
{
	TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
	C->SetStringField(TEXT("key"), ChronKeyCur);
	TArray<TSharedPtr<FJsonValue>> St;
	for (int32 N : ChronSteps) St.Add(MakeShared<FJsonValueNumber>(N));
	C->SetArrayField(TEXT("step"), St);
	C->SetNumberField(TEXT("lv"), ChronLv);
	C->SetNumberField(TEXT("xp"), ChronXp);
	PutStrArr(*C, TEXT("on"), ChronOn);
	C->SetBoolField(TEXT("done"), bChronDone);
	A.SetObjectField(TEXT("chron"), C);
}

void UVenturesFeature::LoadSeason(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> C = JObj(A, TEXT("chron"));
	if (!C) return;
	ChronKeyCur = JStr(*C, TEXT("key"));
	const TArray<TSharedPtr<FJsonValue>>* St = nullptr;
	if (C->TryGetArrayField(TEXT("step"), St) && St)
		for (int32 i = 0; i < FMath::Min(4, St->Num()); ++i) ChronSteps[i] = FMath::Clamp((int32)(*St)[i]->AsNumber(), 0, 999);
	ChronLv = FMath::Clamp((int32)JInt(*C, TEXT("lv")), 0, 20);
	ChronXp = FMath::Max(0.f, (float)JNum(*C, TEXT("xp")));
	for (const FString& K : JStrArr(*C, TEXT("on"))) if (FindChronMod(K) && ChronOn.Num() < 3) ChronOn.AddUnique(K);
	bChronDone = JBool(*C, TEXT("done"));
}

void UVenturesFeature::ChronCheck()
{
	const FString K = ChronKey();
	if (ChronKeyCur == K) return;
	ChronKeyCur = K;
	ChronSteps = { 0, 0, 0, 0 };
	ChronLv = 0;
	ChronXp = 0.f;
	ChronOn.Reset();
	bChronDone = false;
}

bool UVenturesFeature::ChronModOn(const TCHAR* Key) const
{
	const FChronMod* M = FindChronMod(Key);
	return M && ChronOn.Contains(Key) && ChronLv >= M->Lv && ChronKeyCur == ChronKey();
}

void UVenturesFeature::ChronGain(float N)
{
	ChronCheck();
	if (ChronLv >= 20) return;
	ChronXp += N;
	const FChapter& Ch = Chapter();
	while (ChronLv < 20 && ChronXp >= ChronFor(ChronLv))
	{
		ChronXp -= ChronFor(ChronLv);
		ChronLv++;
		Say(FString::Printf(TEXT("%s rises to level %d."), Ch.Artifact, ChronLv));
		for (const FChronMod& M : ChronMods) if (M.Lv == ChronLv) Say(FString::Printf(TEXT("Artifact mod unlocked: %s (pick it in Ventures -> Chronicle)."), M.Name));
	}
}

void UVenturesFeature::ChronStep(int32 I, int32 N)
{
	ChronCheck();
	if (bChronDone) return;
	int32 Cur = -1;
	for (int32 j = 0; j < 4; ++j) if (ChronSteps[j] < ChronNeed[j]) { Cur = j; break; }
	if (Cur != I) return;
	ChronSteps[I] = FMath::Min(ChronNeed[I], ChronSteps[I] + N);
	if (ChronSteps[I] < ChronNeed[I]) return;
	const FChapter& Ch = Chapter();
	ChronGain(80.f);
	Say(FString::Printf(TEXT("Chronicle - %s: \"%s\" complete."), Ch.Title, Ch.Steps[I]));
	if (I == 3)
	{
		bChronDone = true;
		GiveItem(FString(Ch.Artifact) + TEXT(" fragment"), 1);
		GiveCoins(5000);
		Say(FString::Printf(TEXT("Chapter complete! You earn 5,000 coins and a %s fragment."), Ch.Artifact));
	}
}

void UVenturesFeature::DiedSeason(AAerondelMob* Mob, bool bByPlayer, bool bTideBoss)
{
	if (!bByPlayer || !Mob) return;
	const bool bBossy = IsBossy(Mob);
	ChronGain(bBossy ? 25.f : 3.f);
	ChronStep(0);
	if (bBossy) ChronStep(1);
	if (bTideBoss) ChronStep(3);
}

void UVenturesFeature::ChronBreach() { ChronGain(40.f); ChronStep(2); }

float UVenturesFeature::DmgSeasonActor(const AAerondelMob* Mob) const
{
	float T = ChronModOn(TEXT("radiant")) ? 0.05f : 0.f;
	if (Mob && ChronModOn(TEXT("breaker")) && (IsBossy(Mob) || (Mob->Definition && Mob->Definition->CombatLevel >= 100))) T += 0.12f;
	return T;
}
float UVenturesFeature::AccSeason() const { return ChronModOn(TEXT("radiant")) ? 0.05f : 0.f; }
float UVenturesFeature::DrSeason() const { return ChronModOn(TEXT("steadfast")) ? 0.06f : 0.f; }
float UVenturesFeature::XpSeason() const { return ChronModOn(TEXT("tidecall")) ? 0.08f : 0.f; }

void UVenturesFeature::HitSeason(AAerondelMob* Mob, int32 D)
{
	if (D > 0 && Mob && !Mob->IsDead() && ChronModOn(TEXT("overload")) && FMath::FRand() < 0.06f) { Mob->Stun(2); Say(TEXT("Overload! The foe is stunned.")); }
}

void UVenturesFeature::TabChron(FGPanelModel& M)
{
	ChronCheck();
	const FChapter& Ch = Chapter();
	FString S = Ch.Season;
	S[0] = FChar::ToUpper(S[0]);
	M.Title = FString::Printf(TEXT("%s chapter: %s"), *S, Ch.Title);
	int32 Cur = -1;
	for (int32 j = 0; j < 4; ++j) if (ChronSteps[j] < ChronNeed[j]) { Cur = j; break; }
	for (int32 j = 0; j < 4; ++j)
	{
		const bool bDone = bChronDone || (Cur >= 0 ? j < Cur : true);
		M.Lines.Add(FString::Printf(TEXT("%s %s%s"), bDone ? TEXT("[done]") : j == Cur ? TEXT("[now]") : TEXT("[locked]"), Ch.Steps[j],
			j == Cur && !bDone ? *FString::Printf(TEXT(" (%d/%d)"), ChronSteps[j], ChronNeed[j]) : TEXT("")));
	}
	M.Lines.Add(FString::Printf(TEXT("%s - level %d/20 (%s)"), Ch.Artifact, ChronLv, ChronLv >= 20 ? TEXT("max") : *FString::Printf(TEXT("%d/%d"), FMath::FloorToInt(ChronXp), ChronFor(ChronLv))));
	M.Lines.Add(TEXT("Up to 3 mods at once. The chapter, artifact and mods reset when the season changes."));
	for (const FChronMod& Md : ChronMods)
	{
		const FString K = Md.Key;
		const bool bOn = ChronOn.Contains(K);
		M.Lines.Add(FString::Printf(TEXT("%s (lv %d) - %s"), Md.Name, Md.Lv, Md.Desc));
		VAct6(M, FString::Printf(TEXT("%s: %s"), Md.Name, ChronLv < Md.Lv ? TEXT("locked") : bOn ? TEXT("On") : TEXT("Off")), [this, K]
		{
			if (ChronOn.Contains(K)) ChronOn.Remove(K);
			else if (ChronOn.Num() < 3) ChronOn.Add(K);
			else Say(TEXT("Only 3 artifact mods can be on at once."));
		}, ChronLv >= Md.Lv);
	}
}

// ---------------------------------------------------------------- Glidewalk

float UVenturesFeature::GlideDrop() const
{
	const AAerondelPlayerState* PS = GetPS();
	const ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	UWorld* W = C ? C->GetWorld() : nullptr;
	if (!W) return 0.f;
	const FVector P = C->GetActorLocation();
	const FVector Ahead = P + C->GetActorForwardVector().GetSafeNormal2D() * 800.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VenturesGlide), false, C);
	FHitResult H0, H1;
	const bool b0 = W->LineTraceSingleByChannel(H0, P, P - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Q);
	const bool b1 = W->LineTraceSingleByChannel(H1, Ahead + FVector(0.f, 0.f, 500.f), Ahead - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Q);
	if (!b0) return 0.f;
	const float Z1 = b1 ? H1.ImpactPoint.Z : P.Z - 5000.f;
	return (H0.ImpactPoint.Z - Z1) / 100.f;
}

bool UVenturesFeature::Dash()
{
	if (Climbing()) { ClimbLaunch(); return true; }
	const int64 Now = NowMs();
	if (Now < DashReady) { Say(FString::Printf(TEXT("Dash recharging (%ds)."), (int32)((DashReady - Now + 999) / 1000))); return false; }
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	if (!C) return false;
	if (PS->Stats && PS->Stats->RunEnergy < 8.f) { Say(TEXT("Too tired to dash.")); return false; }
	const int32 Ag = SkillLevel(TEXT("Agility"));
	const bool bGlide = GlideDrop() > 3.f;
	const float Dist = (bGlide ? 14.f : 6.f) * (1.f + Ag / 400.f);
	const float Secs = bGlide ? 0.9f : 0.22f;
	const FVector F = C->GetActorForwardVector().GetSafeNormal2D();
	C->LaunchCharacter(F * (Dist * 100.f / Secs) + FVector(0.f, 0.f, bGlide ? 250.f : 60.f), true, true);
	if (PS->Stats) PS->Stats->RunEnergy = FMath::Max(0.f, PS->Stats->RunEnergy - (bGlide ? 4.f : 8.f));
	DashReady = Now + (int64)FMath::Max(1500, 5000 - Ag * 30);
	if (bGlide) Say(TEXT("You glide off the high ground."));
	return true;
}
