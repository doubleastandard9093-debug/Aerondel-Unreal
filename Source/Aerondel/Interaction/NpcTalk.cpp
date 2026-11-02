#include "Interaction/NpcTalk.h"
#include "Data/NpcDef.h"
#include "Core/AerondelPlayerState.h"
#include "Core/AerondelEvents.h"
#include "Quests/QuestComponent.h"
#include "Data/GameDatabase.h"

void ANpcTalk::BeginPlay()
{
	Super::BeginPlay();
	if (!Definition && !NpcId.IsNone())
		if (const UGameDatabase* Db = UGameDatabase::Get()) Definition = Db->FindNpc(NpcId);
	if (Definition) DisplayName = Definition->DisplayName;
}

void ANpcTalk::OnConstruction(const FTransform& Xf)
{
	Super::OnConstruction(Xf);
	if (Definition) DisplayName = Definition->DisplayName;
}

FText ANpcTalk::GetInteractLabel() const
{
	return FText::Format(NSLOCTEXT("Aerondel", "TalkTo", "Talk-to {0}"), Definition ? Definition->DisplayName : DisplayName);
}

void ANpcTalk::Interact(AActor* InInstigator)
{
	if (!Definition) return;
	// Face the player.
	if (InInstigator)
	{
		FVector To = InInstigator->GetActorLocation() - GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) SetActorRotation(To.Rotation());
	}
	AAerondelPlayerState* S = PS(InInstigator);
	if (S && S->Quests->HandleTalk(Definition)) return;
	if (!Definition->Service.IsNone())
		if (UAerondelEvents* Ev = UAerondelEvents::Get(this))
		{
			if (Ev->OnServiceRequested.IsBound()) { Ev->OnServiceRequested.Broadcast(Definition->Service, this); return; }
		}
	if (Definition->IdleLines.Num() > 0)
		if (UAerondelEvents* Ev = UAerondelEvents::Get(this))
			Ev->OnDialogue.Broadcast(Definition->DisplayName, { Definition->IdleLines[LineIndex++ % Definition->IdleLines.Num()] });
}
