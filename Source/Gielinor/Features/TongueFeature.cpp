#include "Features/TongueFeature.h"
#include "Features/CompanionsFeature.h"
#include "Core/GielinorPlayerState.h"
#include "Quests/QuestComponent.h"
#include "Data/GameDatabase.h"
#include "Data/QuestDef.h"
#include "Data/NpcDef.h"

namespace TongueData
{
	struct FQ { const TCHAR* Quest; ETongueCheck Kind; int32 Need; const TCHAR* Text; };
	static const FQ Quests[] = {
		{ TEXT("hilda_pie"), ETongueCheck::Persuade, 20, TEXT("Talk Granny Hilda into paying for the berries up front") },
		{ TEXT("pip_pockets"), ETongueCheck::Lie, 20, TEXT("Tell Pip you already know who took the purse") },
		{ TEXT("osric_graves"), ETongueCheck::Persuade, 30, TEXT("Convince Osric the dead deserve a fee for their peace") },
		{ TEXT("lena_dark"), ETongueCheck::Intimidate, 30, TEXT("Insist on danger money for the Dark Wood") },
		{ TEXT("bram_brew"), ETongueCheck::Lie, 25, TEXT("Claim the last brewer paid you double") },
		{ TEXT("tamsin_tower"), ETongueCheck::Persuade, 35, TEXT("Ask Tamsin to share a spare rune pouch") },
		{ TEXT("aldo_giant"), ETongueCheck::Intimidate, 40, TEXT("Tell Aldo the giant is worth more than he offers") },
		{ TEXT("mara_silk"), ETongueCheck::Lie, 35, TEXT("Say the spiders took your last pair of gloves") },
		{ TEXT("grell_hound"), ETongueCheck::Persuade, 40, TEXT("Promise Grell you will look after the hound") },
		{ TEXT("red_grog"), ETongueCheck::Intimidate, 35, TEXT("Stare down Saltbeard over the price of grog") },
		{ TEXT("red_crew"), ETongueCheck::Lie, 50, TEXT("Pretend to be a navy recruiter to rattle the press gang") },
		{ TEXT("wydin_spice"), ETongueCheck::Persuade, 40, TEXT("Win a merchant's discount from Wendric") },
		{ TEXT("brice_customs"), ETongueCheck::Lie, 55, TEXT("Bluff the customs ledger in your favour") },
		{ TEXT("thurgo_steel"), ETongueCheck::Persuade, 50, TEXT("Flatter Durgan's smithing until he adds a bar") },
		{ TEXT("velma_brew"), ETongueCheck::Intimidate, 55, TEXT("Refuse to drink anything until she pays first") } };
	static const TCHAR* Skill(ETongueCheck K) { return K == ETongueCheck::Persuade ? TEXT("Prayer") : K == ETongueCheck::Lie ? TEXT("Thieving") : TEXT("Strength"); }
	static EWheelTone Tone(ETongueCheck K) { return K == ETongueCheck::Persuade ? EWheelTone::Kind : K == ETongueCheck::Lie ? EWheelTone::Cunning : EWheelTone::Firm; }
}

const TCHAR* UTongueFeature::CheckName(ETongueCheck K) { return K == ETongueCheck::Persuade ? TEXT("Persuade") : K == ETongueCheck::Lie ? TEXT("Lie") : TEXT("Intimidate"); }

int32 UTongueFeature::Score(ETongueCheck Kind) const
{
	int32 S = SkillLevel(TongueData::Skill(Kind)) / 2;
	if (const UCompanionsFeature* C = Feature<UCompanionsFeature>())
	{
		S += FMath::Min(30, 2 * C->GetAlignment(TongueData::Tone(Kind)));
		if (C->ActiveBond() >= 6) S += 10;
	}
	return S;
}

bool UTongueFeature::OpenCheck(FName NpcId, FTongueOffer& Out) const
{
	const UGameDatabase* Db = UGameDatabase::Get(); const AGielinorPlayerState* PS = GetPS();
	if (!Db || !PS || !PS->Quests) return false;
	for (const TongueData::FQ& Q : TongueData::Quests)
	{
		if (Done.Contains(FString(TEXT("q:")) + Q.Quest)) continue;
		const UQuestDef* Def = Db->FindQuest(FName(Q.Quest));
		if (!Def || !Def->Giver || Def->Giver->Id != NpcId) continue;
		if (PS->Quests->GetState(Def) == EQuestState::NotStarted) continue;
		Out.QuestId = FName(Q.Quest); Out.Kind = Q.Kind; Out.Need = Q.Need; Out.Score = Score(Q.Kind);
		Out.Label = FString::Printf(TEXT("[%s %d/%d] %s"), CheckName(Q.Kind), Out.Score, Q.Need, Q.Text);
		return true;
	}
	return false;
}

bool UTongueFeature::Check(const FString& Key, ETongueCheck Kind, int32 Need)
{
	const bool bOk = Score(Kind) >= Need;
	if (!Key.IsEmpty()) Done.Add(Key, bOk);
	if (bOk) ++Passes; else ++Fails;
	return bOk;
}

bool UTongueFeature::Resolve(const FTongueOffer& Offer)
{
	const FString Key = TEXT("q:") + Offer.QuestId.ToString();
	if (Done.Contains(Key)) return false;
	const bool bOk = Check(Key, Offer.Kind, Offer.Need);
	if (bOk) { GiveCoins(Offer.Need * 12); GiveXp(TongueData::Skill(Offer.Kind), 150.f); Say(FString::Printf(TEXT("%s succeeded. (+%d coins)"), CheckName(Offer.Kind), Offer.Need * 12)); }
	else Say(FString::Printf(TEXT("%s failed. That chance is gone."), CheckName(Offer.Kind)));
	return bOk;
}

void UTongueFeature::SaveState(FJsonObject& Out) const
{
	TSharedPtr<FJsonObject> D = MakeShared<FJsonObject>();
	for (const TPair<FString, bool>& P : Done) D->SetStringField(P.Key, P.Value ? TEXT("pass") : TEXT("fail"));
	Out.SetObjectField(TEXT("done"), D); Out.SetNumberField(TEXT("passes"), Passes); Out.SetNumberField(TEXT("fails"), Fails);
}

void UTongueFeature::LoadState(const FJsonObject& In)
{
	if (TSharedPtr<FJsonObject> D = JObj(In, TEXT("done")))
		for (const auto& P : D->Values)
		{
			FString V; if (!P.Value.IsValid() || !P.Value->TryGetString(V)) continue;
			if (V == TEXT("pass") || V == TEXT("fail")) Done.Add(FString(*P.Key), V == TEXT("pass"));
		}
	Passes = FMath::Max(0, (int32)JInt(In, TEXT("passes"))); Fails = FMath::Max(0, (int32)JInt(In, TEXT("fails")));
}
