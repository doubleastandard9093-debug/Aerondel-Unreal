#include "XR/VRInteractionBridge.h"
#include "Interaction/GielinorInteractable.h"
#include "Interaction/ResourceNode.h"
#include "Data/ResourceNodeDef.h"
#include "Combat/GielinorMob.h"
#include "Combat/PlayerCombatComponent.h"

bool UVRInteractionBridge::GrabOrPoke(AActor* Pawn, AActor* Target)
{
	IGielinorInteractable* I = Cast<IGielinorInteractable>(Target);
	if (!I || !I->CanInteract(Pawn)) return false;
	I->Interact(Pawn); // VR is already in reach, so no walk-to step.
	return true;
}

bool UVRInteractionBridge::SwingHit(AActor* Pawn, AActor* Target, EToolType Tool, float Power)
{
	IGielinorInteractable* I = Cast<IGielinorInteractable>(Target);
	if (!I || !I->CanInteract(Pawn)) return false;
	if (Target->IsA<AGielinorMob>())
	{
		UPlayerCombatComponent* C = Pawn ? Pawn->FindComponentByClass<UPlayerCombatComponent>() : nullptr;
		if (C && !C->ConsumeSwing()) return false;
	}
	I->OnToolHit(Pawn, Tool, Power);
	return true;
}

EToolType UVRInteractionBridge::ContextTool(const AActor* Target)
{
	if (const AResourceNode* N = Cast<AResourceNode>(Target)) return N->Definition ? N->Definition->ToolRequired : EToolType::None;
	return EToolType::None;
}
