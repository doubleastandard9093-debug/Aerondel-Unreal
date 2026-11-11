#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Interaction/NpcTalk.h"
#include "Core/AerondelPlayerState.h"
#include "Core/AerondelEvents.h"
#include "Quests/QuestComponent.h"
#include "Data/GameDatabase.h"
#include "Data/QuestDef.h"
#include "Data/AerondelTypes.h"
#include "Dom/JsonObject.h"

// Ventures group 26 (web adapt/skillwords.js): dialogue lines gated by Farming, Herbalism,
// Smithing, Agility, Hunter or Construction. A miss is a short refusal and does not spend
// the hook or lock a quest. Silver Tongue still handles Persuade, Lie and Intimidate.

namespace VenturesChecks
{
	struct FHook { const TCHAR* Id; const TCHAR* Skill; int32 Need; const TCHAR* Giver; const TCHAR* Quest; const TCHAR* Text; const TCHAR* Pass; const TCHAR* Fail; int32 Gold; int32 Xp; };
	static const FHook Hooks[] = {
		{ TEXT("hilda_pie"), TEXT("Farming"), 15, TEXT("hilda"), TEXT("hilda_pie"), TEXT("Offer a better berry bed for her garden"), TEXT("Oh! That would keep the birds off. Take this for the advice."), TEXT("I know my own garden, dear. Come back when you have grown a few beds of your own."), 40, 80 },
		{ TEXT("bram_brew"), TEXT("Herbalism"), 20, TEXT("bram"), TEXT("bram_brew"), TEXT("Name the bitter in last season's mash"), TEXT("Nightshade? You have a good nose. Here is a little extra."), TEXT("I would rather not guess at bitters with a stranger."), 50, 80 },
		{ TEXT("thurgo_steel"), TEXT("Smithing"), 30, TEXT("thurgo"), TEXT("thurgo_steel"), TEXT("Point out the hairline crack in the bloom"), TEXT("So there is. I had missed it. Take a coin for the eye."), TEXT("The bloom is fine. Do not tell a smith his fire."), 80, 120 },
		{ TEXT("tamsin_tower"), TEXT("Agility"), 25, TEXT("tamsin"), TEXT("tamsin_tower"), TEXT("Offer to take the high ledge instead of the stairs"), TEXT("If you can make that jump, the errand is shorter. Take this."), TEXT("That ledge is not a shortcut if you cannot hold it."), 60, 100 },
		{ TEXT("vannaka_slayer"), TEXT("Hunter"), 30, TEXT("vannaka"), TEXT("vannaka_slayer"), TEXT("Read the prints beside the last camp"), TEXT("You read the trail correctly. That is worth a little purse."), TEXT("Those prints are not as simple as they look. Come back when you have tracked more than a rabbit."), 80, 120 },
		{ TEXT("bob_tools"), TEXT("Construction"), 15, TEXT("bob"), nullptr, TEXT("Offer to true the hafts on his rack"), TEXT("A straight haft sells twice. Take a little for the work."), TEXT("Those hafts are already true enough, thank you."), 40, 80 },
	};
	static const FHook* FindHook(const FString& Id)
	{
		for (const FHook& H : Hooks) if (Id == H.Id) return &H;
		return nullptr;
	}
	static bool GiverMatch(const FHook& H, FName NpcId)
	{
		const FString Id = NpcId.ToString();
		if (Id.Equals(H.Giver, ESearchCase::IgnoreCase)) return true;
		// Tobin's tool stall is also the Construction hook.
		if (Id == TEXT("bob") || (FCString::Strcmp(H.Giver, TEXT("bob")) == 0 && Id.Contains(TEXT("tools")))) return FCString::Strcmp(H.Giver, TEXT("bob")) == 0;
		return false;
	}
	static void VAct26(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesChecks;

void UVenturesFeature::ResetChecks()
{
	CheckUsed.Reset();
	SkillNpc.Reset();
}

void UVenturesFeature::SaveChecks(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Used;
	for (const FString& Id : CheckUsed) Used.Add(MakeShared<FJsonValueString>(Id));
	S->SetArrayField(TEXT("used"), Used);
	A.SetObjectField(TEXT("checks"), S);
}

void UVenturesFeature::LoadChecks(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("checks"));
	if (!S) return;
	CheckUsed = JStrArr(*S, TEXT("used"));
}

bool UVenturesFeature::SkillQuestReady(const FString& QuestId) const
{
	if (QuestId.IsEmpty()) return true;
	const AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Quests) return false;
	const UGameDatabase* Db = UGameDatabase::Get();
	const UQuestDef* Q = Db ? Db->FindQuest(FName(*QuestId)) : nullptr;
	if (!Q) return false;
	const EQuestState St = PS->Quests->GetState(Q);
	return St == EQuestState::InProgress || St == EQuestState::Completed;
}

bool UVenturesFeature::SkillHasOpen(FName NpcId) const
{
	for (const FHook& H : Hooks)
	{
		if (!GiverMatch(H, NpcId)) continue;
		if (CheckUsed.Contains(H.Id)) continue;
		if (H.Quest && !SkillQuestReady(H.Quest)) continue;
		return true;
	}
	return false;
}

void UVenturesFeature::SkillOpenFrom(AActor* Npc)
{
	SkillNpc = Npc;
	GoTab(TEXT("checks"));
}

void UVenturesFeature::SkillContinue()
{
	ANpcTalk* N = Cast<ANpcTalk>(SkillNpc.Get());
	SkillNpc.Reset();
	if (!N || !N->Definition) return;
	AAerondelPlayerState* PS = GetPS();
	if (PS && PS->Quests && PS->Quests->HandleTalk(N->Definition)) return;
	if (!N->Definition->Service.IsNone())
		if (UAerondelEvents* Ev = UAerondelEvents::Get(this))
		{ Ev->OnServiceRequested.Broadcast(N->Definition->Service, N); return; }
	if (N->Definition->IdleLines.Num() > 0)
		if (UAerondelEvents* Ev = UAerondelEvents::Get(this))
			Ev->OnDialogue.Broadcast(N->Definition->DisplayName, { N->Definition->IdleLines[0] });
}

bool UVenturesFeature::SkillTry(const FString& Id)
{
	const FHook* H = FindHook(Id);
	if (!H) { Say(TEXT("I have nothing to add.")); return false; }
	if (CheckUsed.Contains(Id)) { Say(TEXT("We already spoke of that.")); return false; }
	if (H->Quest && !SkillQuestReady(H->Quest)) { Say(TEXT("That line opens once the related quest is on your journal.")); return false; }
	const int32 Lv = SkillLevel(H->Skill);
	if (Lv < H->Need)
	{
		Say(FString::Printf(TEXT("%s is not high enough (%d/%d)."), H->Skill, Lv, H->Need));
		Say(H->Fail);
		return false; // hook stays open; Silver Tongue is unchanged
	}
	CheckUsed.Add(Id);
	GiveCoins(H->Gold);
	GiveXp(H->Skill, (float)H->Xp);
	Say(FString::Printf(TEXT("%s %s (%d coins)."), H->Pass, TEXT(""), H->Gold));
	Say(FString::Printf(TEXT("%s advice landed. +%d coins."), H->Skill, H->Gold));
	return true;
}

void UVenturesFeature::TabChecks(FGPanelModel& M)
{
	M.Title = TEXT("Skillwords");
	M.Lines.Add(TEXT("Dialogue lines gated by skills you already have. Silver Tongue stays the three social checks (Persuade, Lie and Intimidate). A miss is a short refusal and does not spend the hook or lock a quest."));
	ANpcTalk* Talk = Cast<ANpcTalk>(SkillNpc.Get());
	const FName Talking = (Talk && Talk->Definition) ? Talk->Definition->Id : NAME_None;
	for (const FHook& H : Hooks)
	{
		const bool bUsed = CheckUsed.Contains(H.Id);
		const int32 Lv = SkillLevel(H.Skill);
		const FString Who = H.Quest ? FString::Printf(TEXT("Related quest: %s."), H.Quest) : FString(TEXT("Talk to Tobin (tools) in Larkhaven."));
		M.Lines.Add(FString::Printf(TEXT("%s %d — %s  (yours: %d)%s  %s"), H.Skill, H.Need, H.Text, Lv, bUsed ? TEXT("  Already used.") : TEXT(""), *Who));
		if (!bUsed) VAct26(M, FString::Printf(TEXT("Try: %s"), H.Text), [this, Id = FString(H.Id)] { SkillTry(Id); });
	}
	if (Talking != NAME_None) VAct26(M, TEXT("Continue talking"), [this] { SkillContinue(); }, true, true);
}
