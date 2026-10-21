#include "Quests/QuestComponent.h"
#include "Data/QuestDef.h"
#include "Data/NpcDef.h"
#include "Data/ItemDef.h"
#include "Data/GameDatabase.h"
#include "Core/GielinorEvents.h"
#include "Core/GielinorPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "GielinorQuests"

AGielinorPlayerState* UQuestComponent::PS() const { return Cast<AGielinorPlayerState>(GetOwner()); }

void UQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		Ev->OnItemGained.AddDynamic(this, &UQuestComponent::OnItemGained);
		Ev->OnMobKilled.AddDynamic(this, &UQuestComponent::OnMobKilled);
	}
}

FQuestProgress& UQuestComponent::FindOrAdd(UQuestDef* Quest)
{
	if (FQuestProgress* F = Progress.FindByPredicate([Quest](const FQuestProgress& P) { return P.Quest == Quest; })) return *F;
	FQuestProgress N; N.Quest = Quest; return Progress.Add_GetRef(N);
}

EQuestState UQuestComponent::GetState(const UQuestDef* Quest) const
{
	const FQuestProgress* F = Progress.FindByPredicate([Quest](const FQuestProgress& P) { return P.Quest == Quest; });
	return F ? F->State : EQuestState::NotStarted;
}

bool UQuestComponent::StartQuest(UQuestDef* Quest)
{
	if (!Quest) return false;
	FQuestProgress& P = FindOrAdd(Quest);
	if (P.State != EQuestState::NotStarted) return false;
	P.State = EQuestState::InProgress; P.Step = 0; P.Counter = 0;
	// Step 0 is usually "talk to the giver", which this conversation satisfies.
	if (Quest->Steps.IsValidIndex(0) && Quest->Steps[0].Type == EQuestStepType::Talk && Quest->Steps[0].Npc == Quest->Giver) Advance(P);
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		Ev->Message(FText::Format(LOCTEXT("Started", "Quest started: {0}"), Quest->DisplayName));
		Ev->OnQuestChanged.Broadcast(Quest);
	}
	return true;
}

FText UQuestComponent::CurrentStepText(const UQuestDef* Quest) const
{
	const FQuestProgress* F = Progress.FindByPredicate([Quest](const FQuestProgress& P) { return P.Quest == Quest; });
	if (!F || !Quest) return LOCTEXT("NotStarted", "Not started.");
	if (F->State == EQuestState::Completed) return LOCTEXT("Done", "Quest complete!");
	if (!Quest->Steps.IsValidIndex(F->Step)) return FText::GetEmpty();
	const FQuestStep& S = Quest->Steps[F->Step];
	if (S.Type == EQuestStepType::Gather || S.Type == EQuestStepType::Kill)
		return FText::Format(LOCTEXT("Counter", "{0} ({1}/{2})"), S.Description, F->Counter, S.Amount);
	return S.Description;
}

int32 UQuestComponent::QuestPoints() const
{
	int32 T = 0; for (const FQuestProgress& P : Progress) if (P.State == EQuestState::Completed && P.Quest) T += P.Quest->QuestPoints; return T;
}

void UQuestComponent::Say(UNpcDef* Npc, const TArray<FText>& Lines)
{
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnDialogue.Broadcast(Npc ? Npc->DisplayName : FText::GetEmpty(), Lines);
}

bool UQuestComponent::HandleTalk(UNpcDef* Npc)
{
	if (!Npc) return false;
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnNpcTalked.Broadcast(Npc);

	// 1) Progress active quests.
	for (FQuestProgress& P : Progress)
	{
		if (P.State != EQuestState::InProgress || !P.Quest || !P.Quest->Steps.IsValidIndex(P.Step)) continue;
		const FQuestStep& S = P.Quest->Steps[P.Step];
		if (S.Npc != Npc) continue;
		if (S.Type == EQuestStepType::Talk) { Say(Npc, { S.Description }); Advance(P); return true; }
		if (S.Type == EQuestStepType::Deliver)
		{
			UInventoryComponent* Inv = PS() ? PS()->Inventory.Get() : nullptr;
			if (Inv && Inv->Has(S.Item, S.Amount)) { Inv->Remove(S.Item, S.Amount); Advance(P); if (P.State != EQuestState::Completed) Say(Npc, { LOCTEXT("Thanks", "Thank you, dearie!") }); return true; }
			Say(Npc, { S.Description }); return true;
		}
	}
	// 2) Offer quests this NPC gives.
	if (const UGameDatabase* Db = UGameDatabase::Get())
	{
		for (UQuestDef* Q : Db->Quests)
		{
			if (Q && Q->Giver == Npc && GetState(Q) == EQuestState::NotStarted)
			{
				Say(Npc, Q->IntroLines);
				StartQuest(Q); // Desktop/VR UI can later add an accept/decline choice.
				return true;
			}
		}
	}
	return false;
}

void UQuestComponent::OnItemGained(UItemDef* Item, int32 Count)
{
	for (FQuestProgress& P : Progress)
	{
		if (P.State != EQuestState::InProgress || !P.Quest->Steps.IsValidIndex(P.Step)) continue;
		const FQuestStep& S = P.Quest->Steps[P.Step];
		if (S.Type == EQuestStepType::Gather && S.Item == Item)
		{
			P.Counter += Count;
			if (P.Counter >= S.Amount) Advance(P);
			else if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnQuestChanged.Broadcast(P.Quest);
		}
	}
}

void UQuestComponent::OnMobKilled(UMobDef* Mob)
{
	for (FQuestProgress& P : Progress)
	{
		if (P.State != EQuestState::InProgress || !P.Quest->Steps.IsValidIndex(P.Step)) continue;
		const FQuestStep& S = P.Quest->Steps[P.Step];
		if (S.Type == EQuestStepType::Kill && S.Mob == Mob)
		{
			P.Counter += 1;
			if (P.Counter >= S.Amount) Advance(P);
			else if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnQuestChanged.Broadcast(P.Quest);
		}
	}
}

void UQuestComponent::Advance(FQuestProgress& P)
{
	P.Step++; P.Counter = 0;
	if (!P.Quest->Steps.IsValidIndex(P.Step)) { Complete(P); return; }
	// A Gather step may already be satisfied by what's in the backpack.
	const FQuestStep& S = P.Quest->Steps[P.Step];
	if (S.Type == EQuestStepType::Gather && PS() && PS()->Inventory->Has(S.Item, S.Amount)) { Advance(P); return; }
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnQuestChanged.Broadcast(P.Quest);
}

void UQuestComponent::Complete(FQuestProgress& P)
{
	P.State = EQuestState::Completed;
	UQuestDef* Q = P.Quest;
	if (AGielinorPlayerState* S = PS())
	{
		for (const FXpReward& R : Q->XpRewards) S->Skills->AddXp(R.Skill, R.Xp);
		for (const FItemAmount& R : Q->ItemRewards) S->Inventory->Add(R.Item, R.Amount, false);
		if (Q->Coins > 0) if (const UGameDatabase* Db = UGameDatabase::Get()) S->Inventory->Add(Db->FindItem(TEXT("coins")), Q->Coins, false);
	}
	Say(Q->Giver, { Q->EndLine });
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		Ev->Message(FText::Format(LOCTEXT("Complete", "Congratulations! Quest complete: {0}"), Q->DisplayName));
		Ev->OnQuestChanged.Broadcast(Q);
	}
}

#undef LOCTEXT_NAMESPACE
