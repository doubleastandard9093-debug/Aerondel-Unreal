#include "Features/StandingFeature.h"
#include "Core/GielinorPlayerState.h"
#include "Quests/QuestComponent.h"
#include "Data/QuestDef.h"

const TArray<FFactionDef>& UStandingFeature::Factions()
{
	static TArray<FFactionDef> D;
	if (D.Num()) return D;
	auto Add = [](FName Id, const TCHAR* Name, TArray<FString> Towns, TArray<FString> Skills) { FFactionDef& F = D.AddDefaulted_GetRef(); F.Id = Id; F.Name = Name; F.Towns = MoveTemp(Towns); F.Skills = MoveTemp(Skills); };
	Add(TEXT("watch"), TEXT("Brightwater Watch"), { TEXT("Brightwater"), TEXT("Ashford"), TEXT("Wilds") }, { TEXT("Attack"), TEXT("Defence"), TEXT("Strength"), TEXT("Hitpoints") });
	Add(TEXT("mariners"), TEXT("Sarim Mariners"), { TEXT("Port Sarim") }, { TEXT("Fishing"), TEXT("Cooking"), TEXT("Ranged") });
	Add(TEXT("circle"), TEXT("Varrock Arcane Circle"), { TEXT("Varrock") }, { TEXT("Magic"), TEXT("Runecrafting"), TEXT("Herblore") });
	Add(TEXT("drowned"), TEXT("Drowned Crew"), {}, { TEXT("Agility"), TEXT("Thieving") });
	Add(TEXT("lantern"), TEXT("Order of the White Lantern"), { TEXT("Falador") }, { TEXT("Prayer"), TEXT("Smithing"), TEXT("Mining") });
	return D;
}

static const int32 GTierAt[] = { 0, 100, 300, 700, 1500 };
FString UStandingFeature::TierName(int32 T) { static const TCHAR* N[] = { TEXT("Unknown"), TEXT("Friendly"), TEXT("Trusted"), TEXT("Honoured"), TEXT("Exalted") }; return N[FMath::Clamp(T, 0, 4)]; }
int32 UStandingFeature::TierOf(int32 Rep) { int32 T = 0; for (int32 i = 0; i < 5; ++i) if (Rep >= GTierAt[i]) T = i; return T; }
int32 UStandingFeature::NextTier(int32 Rep) { const int32 T = TierOf(Rep); return T + 1 < 5 ? GTierAt[T + 1] : -1; }

int32 UStandingFeature::QuestRepFor(const FFactionDef& F) const
{
	const AGielinorPlayerState* PS = GetPS();
	if (!PS || !PS->Quests || F.Towns.Num() == 0) return 0;
	int32 N = 0;
	for (const FQuestProgress& Q : PS->Quests->GetAll())
		if (Q.Quest && Q.State == EQuestState::Completed && !Q.Quest->Town.IsNone())
			for (const FString& T : F.Towns) if (Q.Quest->Town == Id(T) || Q.Quest->Town.ToString().Equals(T, ESearchCase::IgnoreCase)) { ++N; break; }
	return N * QuestRep;
}

int32 UStandingFeature::RepOf(FName Faction) const
{
	const FFactionDef* F = Factions().FindByPredicate([Faction](const FFactionDef& X) { return X.Id == Faction; });
	if (!F) return 0;
	const int32* E = Earned.Find(Faction);
	return FMath::Min(99999, (E ? *E : 0) + QuestRepFor(*F));
}

int32 UStandingFeature::BestTier() const { int32 B = 0; for (const FFactionDef& F : Factions()) B = FMath::Max(B, TierFor(F.Id)); return B; }

bool UStandingFeature::AddRep(FName Faction, int32 N)
{
	if (!Factions().ContainsByPredicate([Faction](const FFactionDef& X) { return X.Id == Faction; })) return false;
	const int32 Before = TierFor(Faction);
	int32& E = Earned.FindOrAdd(Faction);
	E = FMath::Clamp(E + N, 0, 99999);
	const int32 After = TierFor(Faction);
	if (After > Before) Say(FString::Printf(TEXT("Your standing has risen: %s with %s."), *TierName(After), *Faction.ToString()));
	return After > Before;
}

int32 UStandingFeature::ClaimTierRewards()
{
	struct FReward { int32 Gold; const TCHAR* Item; int32 Qty; };
	static const FReward R[] = { { 0, nullptr, 0 }, { 250, nullptr, 0 }, { 600, TEXT("Steel bar"), 5 }, { 1200, TEXT("Pure essence"), 50 }, { 3000, TEXT("Iron bar"), 20 } };
	int32 Paid = 0;
	for (int32 T = ClaimedTier + 1; T <= BestTier(); ++T)
	{
		GiveCoins(R[T].Gold);
		if (R[T].Item) GiveItem(R[T].Item, R[T].Qty);
		ClaimedTier = T; ++Paid;
	}
	if (Paid) Say(TEXT("Quartermaster Odile hands over your standing rewards."));
	return Paid;
}

FName UStandingFeature::FactionForTown(const FString& Town) const
{
	for (const FFactionDef& F : Factions()) for (const FString& T : F.Towns) if (T.Equals(Town, ESearchCase::IgnoreCase)) return F.Id;
	return NAME_None;
}

float UStandingFeature::XpBonus(FName SkillId) const
{
	float B = 0.f;
	for (const FFactionDef& F : Factions()) for (const FString& S : F.Skills) if (Id(S) == SkillId) B += TierFor(F.Id) * 0.01f;
	return B;
}

void UStandingFeature::SaveState(FJsonObject& Out) const
{
	for (const TPair<FName, int32>& P : Earned) Out.SetNumberField(P.Key.ToString(), P.Value);
	Out.SetNumberField(TEXT("_claimed"), ClaimedTier);
}

void UStandingFeature::LoadState(const FJsonObject& In)
{
	for (const FFactionDef& F : Factions()) { const int64 V = JInt(In, *F.Id.ToString()); if (V > 0) Earned.Add(F.Id, (int32)FMath::Min<int64>(V, 99999)); }
	ClaimedTier = FMath::Clamp((int32)JInt(In, TEXT("_claimed")), 0, 4);
}
