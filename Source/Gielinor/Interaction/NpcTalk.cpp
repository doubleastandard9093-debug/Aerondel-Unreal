#include "Interaction/NpcTalk.h"
#include "Data/NpcDef.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Quests/QuestComponent.h"

void ANpcTalk::OnConstruction(const FTransform& Xf)
{
	Super::OnConstruction(Xf);
	if (Definition) DisplayName = Definition->DisplayName;
}

FText ANpcTalk::GetInteractLabel() const
{
	return FText::Format(NSLOCTEXT("Gielinor", "TalkTo", "Talk-to {0}"), Definition ? Definition->DisplayName : DisplayName);
}

void ANpcTalk::Interact(AActor* Instigator)
{
	if (!Definition) return;
	// Face the player.
	if (Instigator)
	{
		FVector To = Instigator->GetActorLocation() - GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) SetActorRotation(To.Rotation());
	}
	AGielinorPlayerState* S = PS(Instigator);
	if (S && S->Quests->HandleTalk(Definition)) return;
	if (Definition->IdleLines.Num() > 0)
		if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
			Ev->OnDialogue.Broadcast(Definition->DisplayName, { Definition->IdleLines[LineIndex++ % Definition->IdleLines.Num()] });
}
