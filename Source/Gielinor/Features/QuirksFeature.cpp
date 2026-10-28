#include "Features/QuirksFeature.h"
#include "Core/GielinorEvents.h"
#include "Core/GielinorPlayerState.h"
#include "Rules/GielinorRules.h"
#include "Data/MobDef.h"

namespace QuirkData
{
	struct FQ { const TCHAR* Id; const TCHAR* Name; const TCHAR* Counter; int32 At; const TCHAR* Why; const TCHAR* Down; const TCHAR* Up; };
	static const FQ All[] = {
		{ TEXT("ember"), TEXT("Ember-shy"), TEXT("fire"), 300, TEXT("You have been burned a lot."), TEXT("Fire-touched foes hit you 10% harder"), TEXT("+5% Cooking and Smithing XP") },
		{ TEXT("skittish"), TEXT("Skittish"), TEXT("flee"), 8, TEXT("You run from a lot of fights."), TEXT("You take 10% more damage under 30% hitpoints"), TEXT("Run energy drains about a third slower") },
		{ TEXT("glassjaw"), TEXT("Glass Jaw"), TEXT("deaths"), 5, TEXT("You have been knocked out a few times."), TEXT("You take 5% more damage"), TEXT("+3% XP in every combat skill") },
		{ TEXT("brawler"), TEXT("Knuckle-head"), TEXT("meleeKills"), 300, TEXT("You settle almost everything up close."), TEXT("-10% ranged accuracy"), TEXT("+5% melee damage") } };
	static const FQ* Find(const FString& Id) { for (const FQ& Q : All) if (Id == Q.Id) return &Q; return nullptr; }
	static bool IsFire(const UMobDef* M)
	{
		if (!M) return false;
		const FString N = M->Id.ToString().ToLower();
		for (const TCHAR* W : { TEXT("dragon"), TEXT("fire"), TEXT("flame"), TEXT("ember"), TEXT("lava"), TEXT("infernal"), TEXT("blaze"), TEXT("cinder"), TEXT("ash") }) if (N.Contains(W)) return true;
		return false;
	}
}

void UQuirksFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UGielinorEvents* Ev = Collection.InitializeDependency<UGielinorEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UQuirksFeature::HandleMobKilled);
}

void UQuirksFeature::HandleMobKilled(UMobDef* Mob) { if (!UGielinorRules::IsRanged(GetPS())) Bump(TEXT("meleeKills")); }

void UQuirksFeature::Bump(const FString& Counter, int32 N) { Counters.FindOrAdd(Counter) += N; CheckDue(); }

void UQuirksFeature::CheckDue()
{
	if (!Offered.IsEmpty() || Held.Num() >= MaxHeld) return;
	for (const QuirkData::FQ& Q : QuirkData::All)
		if (!Held.Contains(Q.Id) && !Declined.Contains(Q.Id) && Counters.FindRef(Q.Counter) >= Q.At)
		{ Offered = Q.Id; Say(FString::Printf(TEXT("%s A quirk is offered: %s (%s; %s)."), Q.Why, Q.Name, Q.Down, Q.Up)); return; }
}

bool UQuirksFeature::Accept()
{
	if (Offered.IsEmpty() || Held.Num() >= MaxHeld) return false;
	Held.AddUnique(Offered); Offered.Reset(); return true;
}

void UQuirksFeature::Decline() { if (!Offered.IsEmpty()) { Declined.AddUnique(Offered); Offered.Reset(); } }

FString UQuirksFeature::Remedy(const FString& QuirkId)
{
	if (!Held.Contains(QuirkId)) return TEXT("You don't have that quirk.");
	if (SkillLevel(TEXT("Herblore")) < 30) return TEXT("Ysolde's remedy needs Herblore 30.");
	if (CountItem(TEXT("Grimy guam leaf")) < 5 || CountItem(TEXT("Coins")) < 2000) return TEXT("The remedy needs 5 Grimy guam leaf and 2000 coins.");
	TakeItem(TEXT("Grimy guam leaf"), 5); TakeCoins(2000); GiveXp(TEXT("Herblore"), 400.f);
	Held.Remove(QuirkId); Declined.AddUnique(QuirkId); ++Removed;
	return TEXT("The remedy works. The quirk fades.");
}

FString UQuirksFeature::Describe(const FString& QuirkId) const
{
	const QuirkData::FQ* Q = QuirkData::Find(QuirkId);
	return Q ? FString::Printf(TEXT("%s: %s. %s."), Q->Name, Q->Down, Q->Up) : FString();
}

float UQuirksFeature::XpBonus(FName SkillId) const
{
	float B = 0.f;
	if (Has(TEXT("ember")) && (SkillId == Id(TEXT("Cooking")) || SkillId == Id(TEXT("Smithing")))) B += 0.05f;
	if (Has(TEXT("glassjaw")))
		for (const TCHAR* S : { TEXT("Attack"), TEXT("Strength"), TEXT("Defence"), TEXT("Ranged"), TEXT("Magic"), TEXT("Hitpoints") }) if (SkillId == Id(S)) B += 0.03f;
	return B;
}

float UQuirksFeature::DamageTakenMult(const UMobDef* Source, float HpFrac) const
{
	const bool bFire = QuirkData::IsFire(Source);
	if (bFire) Counters.FindOrAdd(TEXT("fire")) += 1;
	float M = 1.f;
	if (bFire && Has(TEXT("ember"))) M *= 1.1f;
	if (HpFrac < 0.3f && Has(TEXT("skittish"))) M *= 1.1f;
	if (Has(TEXT("glassjaw"))) M *= 1.05f;
	return M;
}

float UQuirksFeature::DamageDealtBonus(const UMobDef* Target) const { return Has(TEXT("brawler")) && !UGielinorRules::IsRanged(GetPS()) ? 0.05f : 0.f; }

void UQuirksFeature::SaveState(FJsonObject& Out) const
{
	PutStrArr(Out, TEXT("held"), Held); PutStrArr(Out, TEXT("declined"), Declined); PutIntMap(Out, TEXT("counters"), Counters);
	if (!Offered.IsEmpty()) Out.SetStringField(TEXT("offered"), Offered);
	Out.SetNumberField(TEXT("removed"), Removed);
}

void UQuirksFeature::LoadState(const FJsonObject& In)
{
	for (const FString& S : JStrArr(In, TEXT("held"))) if (QuirkData::Find(S) && Held.Num() < MaxHeld) Held.AddUnique(S);
	for (const FString& S : JStrArr(In, TEXT("declined"))) if (QuirkData::Find(S)) Declined.AddUnique(S);
	for (const TPair<FString, int32>& P : JIntMap(In, TEXT("counters"))) Counters.Add(P.Key, FMath::Max(0, P.Value));
	const FString O = JStr(In, TEXT("offered")); if (QuirkData::Find(O) && !Held.Contains(O)) Offered = O;
	Removed = FMath::Max(0, (int32)JInt(In, TEXT("removed")));
}
