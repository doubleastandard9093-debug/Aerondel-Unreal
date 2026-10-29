#include "Features/CharterFeature.h"
#include "Features/TongueFeature.h"
#include "Features/StandingFeature.h"
#include "Features/CompanionsFeature.h"
#include "Features/SagaFeature.h"

namespace CharterData
{
	static const FName Charter(TEXT("charter")), Commons(TEXT("commons")), Deal(TEXT("deal")), Betray(TEXT("betray"));
	static bool Valid(FName S) { return S == Charter || S == Commons || S == Deal || S == Betray; }
	// Companion bond change by the tone they like: kind / firm / cunning x charter, commons, deal, betray.
	static const int32 Approval[3][4] = { { -1, 3, 3, -5 }, { 3, -1, 1, -3 }, { 1, -1, 2, 3 } };
	static int32 SideIdx(FName S) { return S == Charter ? 0 : S == Commons ? 1 : S == Deal ? 2 : 3; }
	struct FL { const TCHAR* Item; int32 N; int32 Cost; };
	static const FL CharterStall[] = { { TEXT("Steel bar"), 5, 450 }, { TEXT("Rune bar"), 1, 2400 }, { TEXT("Coal"), 20, 300 } };
	static const FL CommonsStall[] = { { TEXT("Grimy ranarr"), 3, 600 }, { TEXT("Bread"), 10, 120 }, { TEXT("Feather"), 50, 150 } };
	static const FL DealStall[] = { { TEXT("Steel bar"), 5, 500 }, { TEXT("Bread"), 10, 130 }, { TEXT("Coal"), 20, 320 } };
	static constexpr int32 SideRep = 80, DealRep = 40, BetrayCoins = 5000;
}

bool UCharterFeature::Resolve(FName NewSide)
{
	using namespace CharterData;
	if (!Side.IsNone() || !Valid(NewSide)) return false;
	UTongueFeature* T = Feature<UTongueFeature>();
	if (NewSide == Deal && (!T || !T->Check(TEXT("wellmere_deal"), ETongueCheck::Persuade, DealNeed)))
	{ Say(FString::Printf(TEXT("They won't listen. (Persuade %d/%d)"), T ? T->Score(ETongueCheck::Persuade) : 0, DealNeed)); return false; }
	if (NewSide == Betray && (!T || !T->Check(TEXT("wellmere_betray"), ETongueCheck::Lie, BetrayNeed)))
	{ Say(FString::Printf(TEXT("Neither side believes you. (Lie %d/%d)"), T ? T->Score(ETongueCheck::Lie) : 0, BetrayNeed)); return false; }
	Side = NewSide;
	UStandingFeature* St = Feature<UStandingFeature>();
	if (NewSide == Charter) { RepCharter += 200; RepCommons -= 200; if (St) St->AddRep(TEXT("watch"), SideRep); }
	else if (NewSide == Commons) { RepCommons += 200; RepCharter -= 200; if (St) St->AddRep(TEXT("lantern"), SideRep); }
	else if (NewSide == Deal) { RepCharter += 120; RepCommons += 120; if (St) { St->AddRep(TEXT("watch"), DealRep); St->AddRep(TEXT("lantern"), DealRep); } }
	else { RepCharter -= 300; RepCommons -= 300; GiveCoins(BetrayCoins); }
	if (USagaFeature* Sg = Feature<USagaFeature>()) Sg->SetFlag(TEXT("wellmere_") + NewSide.ToString());
	if (UCompanionsFeature* C = Feature<UCompanionsFeature>())
		if (const FCompanionDef* D = UCompanionsFeature::Def(C->GetActive()))
		{
			const int32 N = Approval[(int32)D->Likes][SideIdx(NewSide)];
			C->AddBond(D->Id, N);
			Say(FString::Printf(TEXT("%s %s (%+d Bond)."), *D->Name, N > 0 ? TEXT("approves") : TEXT("disapproves"), N));
		}
	Say(NewSide == Betray ? FString::Printf(TEXT("You sell the spring deeds to both sides and pocket %d coins. Wellmere empties."), BetrayCoins)
		: FString::Printf(TEXT("Wellmere's spring goes to %s. The hamlet will remember."), NewSide == Charter ? TEXT("the Charter") : NewSide == Commons ? TEXT("the Commons") : TEXT("both sides")));
	OnResolved.Broadcast(Side);
	return true;
}

TArray<FCharterStall> UCharterFeature::GetStall() const
{
	using namespace CharterData;
	TArray<FCharterStall> Out;
	auto Fill = [&Out](const FL* L, int32 N) { for (int32 i = 0; i < N; ++i) { FCharterStall S; S.Item = L[i].Item; S.Count = L[i].N; S.Cost = L[i].Cost; Out.Add(S); } };
	if (Side == Charter) Fill(CharterStall, UE_ARRAY_COUNT(CharterStall));
	else if (Side == Commons) Fill(CommonsStall, UE_ARRAY_COUNT(CommonsStall));
	else if (Side == Deal) Fill(DealStall, UE_ARRAY_COUNT(DealStall));
	return Out;
}

bool UCharterFeature::Buy(int32 Index)
{
	const TArray<FCharterStall> L = GetStall();
	if (!L.IsValidIndex(Index)) return false;
	const FCharterStall& S = L[Index];
	if (CountItem(TEXT("Coins")) < S.Cost) { Say(FString::Printf(TEXT("That costs %d coins."), S.Cost)); return false; }
	TakeCoins(S.Cost); GiveItem(S.Item, S.Count); ++Bought;
	return true;
}

bool UCharterFeature::IsNpcPresent(FName NpcId) const
{
	using namespace CharterData;
	if (Side == Betray) return false;
	if (NpcId == FName(TEXT("ysmay"))) return Side != Charter;
	if (NpcId == FName(TEXT("brannoch"))) return Side != Commons;
	return true;
}

FString UCharterFeature::Greeting(FName NpcId) const
{
	using namespace CharterData;
	const bool bCharterNpc = NpcId != FName(TEXT("ysmay"));
	if (Side == Betray) return TEXT("...");
	if (!Side.IsNone())
		return bCharterNpc ? FString(TEXT("Brannoch, factor. ")) + (Side == Deal ? TEXT("The Accord holds. Trade is good for everyone.") : TEXT("The Charter runs Wellmere now. Our guards keep the spring."))
			: FString(TEXT("Ysmay, elder. ")) + (Side == Deal ? TEXT("The Accord holds. The spring is shared.") : TEXT("The Commons keep the spring for all who work."));
	return bCharterNpc ? TEXT("Brannoch, factor of the Wellmere Charter. That spring was bought fair. The Commons squat on it and call it theirs. Help me keep order and the Watch will hear of it.")
		: TEXT("Ysmay, elder of the Commons. We dug that spring with our own hands. The Charter wants to sell our water back to us. Stand with us.");
}

void UCharterFeature::SaveState(FJsonObject& Out) const
{
	Out.SetBoolField(TEXT("met"), bMet);
	if (!Side.IsNone()) Out.SetStringField(TEXT("side"), Side.ToString());
	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetNumberField(TEXT("charter"), RepCharter); R->SetNumberField(TEXT("commons"), RepCommons);
	Out.SetObjectField(TEXT("rep"), R);
	Out.SetNumberField(TEXT("bought"), Bought);
}

void UCharterFeature::LoadState(const FJsonObject& In)
{
	bMet = JBool(In, TEXT("met"));
	const FName S(*JStr(In, TEXT("side")));
	Side = CharterData::Valid(S) ? S : NAME_None;
	if (TSharedPtr<FJsonObject> R = JObj(In, TEXT("rep"))) { RepCharter = (int32)JInt(*R, TEXT("charter")); RepCommons = (int32)JInt(*R, TEXT("commons")); }
	Bought = FMath::Max<int32>(0, (int32)JInt(In, TEXT("bought")));
}
