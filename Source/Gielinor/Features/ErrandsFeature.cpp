#include "Features/ErrandsFeature.h"
#include "Features/CompanionsFeature.h"
#include "Features/StandingFeature.h"
#include "Features/HearthFeature.h"
#include "Dom/JsonValue.h"

const TArray<FErrandDef>& UErrandsFeature::Defs()
{
	static TArray<FErrandDef> D;
	if (D.Num()) return D;
	auto Add = [](const TCHAR* Id, const TCHAR* Name, int32 Mins, const TCHAR* Skill, int32 Xp, TMap<FString, int32> Items, const TCHAR* Fac, int32 Rep)
	{ FErrandDef& E = D.AddDefaulted_GetRef(); E.Id = Id; E.Name = Name; E.Minutes = Mins; E.Skill = Skill; E.Xp = Xp; E.Items = MoveTemp(Items); E.Faction = Fac; E.Rep = Rep; };
	Add(TEXT("e_copper"), TEXT("Dig copper at the east mine"), 15, TEXT("Mining"), 120, { { TEXT("Copper ore"), 12 } }, TEXT("watch"), 10);
	Add(TEXT("e_logs"), TEXT("Cut firewood for the Watch"), 20, TEXT("Woodcutting"), 140, { { TEXT("Logs"), 15 } }, TEXT("watch"), 12);
	Add(TEXT("e_shrimp"), TEXT("Net shrimp off the docks"), 20, TEXT("Fishing"), 140, { { TEXT("Raw shrimp"), 15 } }, TEXT("mariners"), 12);
	Add(TEXT("e_hides"), TEXT("Trade for cowhides"), 30, TEXT("Crafting"), 180, { { TEXT("Cowhide"), 8 } }, TEXT("watch"), 15);
	Add(TEXT("e_feathers"), TEXT("Gather feathers on the farms"), 30, TEXT("Fletching"), 160, { { TEXT("Feather"), 60 } }, TEXT("watch"), 15);
	Add(TEXT("e_trout"), TEXT("Fish the river for trout"), 45, TEXT("Fishing"), 300, { { TEXT("Raw trout"), 15 } }, TEXT("mariners"), 20);
	Add(TEXT("e_iron"), TEXT("Haul iron from the deep seams"), 60, TEXT("Mining"), 420, { { TEXT("Iron ore"), 15 }, { TEXT("Coal"), 10 } }, TEXT("lantern"), 25);
	Add(TEXT("e_oak"), TEXT("Fell oaks for the shipwrights"), 60, TEXT("Woodcutting"), 400, { { TEXT("Oak logs"), 20 } }, TEXT("mariners"), 25);
	Add(TEXT("e_herbs"), TEXT("Forage herbs in the wilds"), 90, TEXT("Herblore"), 500, { { TEXT("Grimy guam leaf"), 10 } }, TEXT("circle"), 30);
	Add(TEXT("e_runes"), TEXT("Ferry essence to the Circle"), 120, TEXT("Runecrafting"), 650, { { TEXT("Rune essence"), 60 } }, TEXT("circle"), 40);
	Add(TEXT("e_bars"), TEXT("Work the Falador forges"), 240, TEXT("Smithing"), 1400, { { TEXT("Iron bar"), 12 }, { TEXT("Steel bar"), 4 } }, TEXT("lantern"), 70);
	Add(TEXT("e_patrol"), TEXT("Ride the long border patrol"), 480, TEXT("Defence"), 2600, { { TEXT("Bones"), 30 } }, TEXT("watch"), 120);
	return D;
}

float UErrandsFeature::SuccessChance(FName Companion) const
{
	const UCompanionsFeature* C = Feature<UCompanionsFeature>();
	const UHearthFeature* H = Feature<UHearthFeature>();
	const int32 Bond = C ? C->GetState(Companion).Bond : 0;
	return FMath::Min(1.f, 0.7f + 0.03f * Bond + (H ? H->ErrandBonus() : 0.f));
}

bool UErrandsFeature::Start(FName Errand, FName Companion)
{
	UCompanionsFeature* C = Feature<UCompanionsFeature>();
	const FErrandDef* E = Defs().FindByPredicate([Errand](const FErrandDef& X) { return X.Id == Errand; });
	if (!C || !E || !C->GetState(Companion).bRecruited || C->GetActive() == Companion || IsAway(Companion)) return false;
	FErrandRun& R = Running.AddDefaulted_GetRef();
	R.Errand = Errand; R.Companion = Companion; R.Start = NowMs(); R.End = R.Start + (int64)E->Minutes * 60000; R.bOk = FMath::FRand() < SuccessChance(Companion);
	return true;
}

int32 UErrandsFeature::CollectFinished()
{
	UCompanionsFeature* C = Feature<UCompanionsFeature>();
	UStandingFeature* St = Feature<UStandingFeature>();
	const int64 Now = NowMs();
	int32 N = 0;
	for (int32 i = Running.Num() - 1; i >= 0; --i)
	{
		const FErrandRun R = Running[i];
		if (R.End > Now) continue;
		Running.RemoveAt(i); ++N;
		const FErrandDef* E = Defs().FindByPredicate([&R](const FErrandDef& X) { return X.Id == R.Errand; });
		FCompanionState* S = C ? C->Find(R.Companion) : nullptr;
		if (!E || !S) continue;
		const int32 Lvl = CrewLevel(S->Crew.FindRef(E->Skill));
		if (R.bOk)
		{
			for (const TPair<FString, int32>& It : E->Items) GiveToBank(It.Key, FMath::Max(1, FMath::RoundToInt(It.Value * CrewYield(Lvl))));
			GiveXp(E->Skill, (float)E->Xp);
			if (St) St->AddRep(E->Faction, E->Rep);
		}
		else GiveXp(E->Skill, E->Xp / 4.f);
		// Crew XP: a failed errand still teaches a quarter.
		int32& CX = S->Crew.FindOrAdd(E->Skill);
		CX += FMath::RoundToInt(E->Xp * (R.bOk ? 1.f : 0.25f));
		const int32 After = CrewLevel(CX);
		const FCompanionDef* D = UCompanionsFeature::Def(R.Companion);
		Say(FString::Printf(TEXT("%s is back from \"%s\": %s%s"), D ? *D->Name : *R.Companion.ToString(), *E->Name,
			R.bOk ? TEXT("success, the haul is in your bank.") : TEXT("they came back empty-handed, but learned something."),
			After > Lvl ? *FString::Printf(TEXT(" Crew %s is now %d."), *E->Skill, After) : TEXT("")));
	}
	return N;
}

void UErrandsFeature::SaveState(FJsonObject& Out) const
{
	TArray<TSharedPtr<FJsonValue>> A;
	for (const FErrandRun& R : Running)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("id"), R.Errand.ToString()); J->SetStringField(TEXT("comp"), R.Companion.ToString());
		J->SetNumberField(TEXT("start"), (double)R.Start); J->SetNumberField(TEXT("end"), (double)R.End); J->SetBoolField(TEXT("ok"), R.bOk);
		A.Add(MakeShared<FJsonValueObject>(J));
	}
	Out.SetArrayField(TEXT("running"), A);
}

void UErrandsFeature::LoadState(const FJsonObject& In)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!In.TryGetArrayField(TEXT("running"), A) || !A) return;
	for (const TSharedPtr<FJsonValue>& V : *A)
	{
		const TSharedPtr<FJsonObject> J = V.IsValid() ? V->AsObject() : nullptr;
		if (!J.IsValid()) continue;
		FErrandRun R; R.Errand = FName(*JStr(*J, TEXT("id"))); R.Companion = FName(*JStr(*J, TEXT("comp")));
		R.Start = JInt(*J, TEXT("start")); R.End = JInt(*J, TEXT("end")); R.bOk = JBool(*J, TEXT("ok"));
		if (UCompanionsFeature::Def(R.Companion) && !IsAway(R.Companion)) Running.Add(R);
	}
}
