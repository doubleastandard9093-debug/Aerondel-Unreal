#include "Quests/QuestComponent.h"
#include "Data/QuestDef.h"
#include "Data/NpcDef.h"
#include "Data/ItemDef.h"
#include "Data/GameDatabase.h"
#include "Core/GielinorEvents.h"
#include "Core/GielinorPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/SkillsComponent.h"
#include "Data/SkillDef.h"
#include "Data/ContentDefs.h"
#include "Core/PlayerStatsComponent.h"

#define LOCTEXT_NAMESPACE "GielinorQuests"

AGielinorPlayerState* UQuestComponent::PS() const { return Cast<AGielinorPlayerState>(GetOwner()); }

void UQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		Ev->OnItemGained.AddDynamic(this, &UQuestComponent::OnItemGained);
		Ev->OnMobKilled.AddDynamic(this, &UQuestComponent::OnMobKilled);
		Ev->OnPlaceVisited.AddDynamic(this, &UQuestComponent::OnPlaceVisited);
		Ev->OnBonesBuried.AddDynamic(this, &UQuestComponent::OnBonesBuried);
		Ev->OnXpGained.AddDynamic(this, &UQuestComponent::OnXpGained);
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

bool UQuestComponent::CanStart(const UQuestDef* Quest, FText& OutReason) const
{
	if (!Quest) return false;
	const UGameDatabase* Db = UGameDatabase::Get();
	for (const FName QId : Quest->RequiredQuests)
	{
		const UQuestDef* Pre = Db ? Db->FindQuest(QId) : nullptr;
		if (Pre && GetState(Pre) != EQuestState::Completed) { OutReason = FText::Format(LOCTEXT("NeedQuest", "You need to complete {0} first."), Pre->DisplayName); return false; }
	}
	if (const AGielinorPlayerState* S = PS())
	{
		for (const auto& L : Quest->RequiredLevels)
		{
			if (S->Skills->GetLevelById(L.Key) < L.Value)
			{
				const USkillDef* Sk = Db ? Db->FindSkill(L.Key) : nullptr;
				OutReason = FText::Format(LOCTEXT("NeedLvl", "You need level {0} {1} to start this quest."), L.Value, Sk ? Sk->DisplayName : FText::FromName(L.Key));
				return false;
			}
		}
	}
	return true;
}

bool UQuestComponent::StartQuest(UQuestDef* Quest)
{
	if (!Quest) return false;
	FText Why;
	if (!CanStart(Quest, Why)) { if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(Why); return false; }
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
	if (S.Type == EQuestStepType::Gather || S.Type == EQuestStepType::Kill || S.Type == EQuestStepType::Bury || S.Type == EQuestStepType::Xp)
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
		if (S.Type == EQuestStepType::Talk) { Say(Npc, S.NpcLines.Num() ? S.NpcLines : TArray<FText>{ S.Description }); Advance(P); return true; }
		if (S.Type == EQuestStepType::Deliver)
		{
			UInventoryComponent* Inv = PS() ? PS()->Inventory.Get() : nullptr;
			TArray<FItemAmount> Need = S.Items;
			if (S.Item) { FItemAmount One; One.Item = S.Item; One.Amount = S.Amount; Need.Add(One); }
			bool bHasAll = Inv != nullptr;
			for (const FItemAmount& N : Need) if (bHasAll && !Inv->Has(N.Item, N.Amount)) bHasAll = false;
			if (bHasAll)
			{
				for (const FItemAmount& N : Need) Inv->Remove(N.Item, N.Amount);
				Advance(P);
				if (P.State != EQuestState::Completed) Say(Npc, { P.Quest->ThanksLine.IsEmpty() ? LOCTEXT("Thanks", "Thank you!") : P.Quest->ThanksLine });
				return true;
			}
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
				FText Why;
				if (!CanStart(Q, Why)) { Say(Npc, { Why }); return true; }
				Say(Npc, Q->IntroLines);
				StartQuest(Q); // Desktop/VR UI can later add an accept/decline choice.
				return true;
			}
		}
	}
	return false;
}

void UQuestComponent::Bump(TFunctionRef<bool(const FQuestStep&)> Pred, int32 By)
{
	for (FQuestProgress& P : Progress)
	{
		if (P.State != EQuestState::InProgress || !P.Quest || !P.Quest->Steps.IsValidIndex(P.Step)) continue;
		const FQuestStep& S = P.Quest->Steps[P.Step];
		if (!Pred(S)) continue;
		P.Counter += By;
		if (P.Counter >= S.Amount) Advance(P);
		else if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnQuestChanged.Broadcast(P.Quest);
	}
}

void UQuestComponent::OnItemGained(UItemDef* Item, int32 Count) { Bump([Item](const FQuestStep& S) { return S.Type == EQuestStepType::Gather && S.Item == Item; }, Count); }
void UQuestComponent::OnMobKilled(UMobDef* Mob) { Bump([Mob](const FQuestStep& S) { return S.Type == EQuestStepType::Kill && S.Mob == Mob; }, 1); }
void UQuestComponent::OnBonesBuried(UItemDef*) { Bump([](const FQuestStep& S) { return S.Type == EQuestStepType::Bury; }, 1); }
void UQuestComponent::OnPlaceVisited(FName PlaceId)
{
	Bump([PlaceId](const FQuestStep& S) { return S.Type == EQuestStepType::Visit && S.PlaceId == PlaceId; }, MAX_int32 / 2);
}
void UQuestComponent::OnXpGained(USkillDef* Skill, float Amount, float, int32)
{
	if (!Skill) return;
	const FName SkillId = Skill->Id;
	Bump([SkillId](const FQuestStep& S) { return S.Type == EQuestStepType::Xp && S.SkillId == SkillId; }, FMath::CeilToInt(Amount));
}

void UQuestComponent::CheckMilestones()
{
	const UGameDatabase* Db = UGameDatabase::Get();
	AGielinorPlayerState* S = PS();
	if (!Db || !S) return;
	const int32 Qp = QuestPoints();
	for (int32 i = 0; i < Db->QuestPointMilestones.Num(); ++i)
	{
		const FQuestPointMilestone& M = Db->QuestPointMilestones[i];
		if (Qp < M.Points || ClaimedMilestones.Contains(i)) continue;
		ClaimedMilestones.Add(i);
		if (M.Item) S->Inventory->Add(M.Item, 1, false);
		if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(FText::Format(LOCTEXT("Milestone", "Quest point milestone ({0} QP): {1}"), M.Points, M.Description));
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
		if (S->Stats) S->Stats->RefreshMaxHp(false); // maxHp quest bonuses
		for (const FItemAmount& R : Q->ItemRewards) S->Inventory->Add(R.Item, R.Amount, false);
		if (Q->Coins > 0) if (const UGameDatabase* Db = UGameDatabase::Get()) S->Inventory->Add(Db->FindItem(TEXT("coins")), Q->Coins, false);
	}
	CheckMilestones();
	Say(Q->Giver, { Q->EndLine });
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		Ev->Message(FText::Format(LOCTEXT("Complete", "Congratulations! Quest complete: {0}"), Q->DisplayName));
		Ev->OnQuestChanged.Broadcast(Q);
	}
}

#undef LOCTEXT_NAMESPACE
