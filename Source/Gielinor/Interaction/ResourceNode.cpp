#include "Interaction/ResourceNode.h"
#include "Data/ResourceNodeDef.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Core/GielinorPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "GielinorResource"

AResourceNode::AResourceNode()
{
	DepletedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DepletedMesh"));
	DepletedMesh->SetupAttachment(Root);
	DepletedMesh->SetVisibility(false);
	DepletedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AResourceNode::OnConstruction(const FTransform& Xf)
{
	Super::OnConstruction(Xf);
	if (Definition) DisplayName = Definition->DisplayName;
}

FText AResourceNode::GetInteractLabel() const
{
	if (!Definition) return DisplayName;
	switch (Definition->Kind)
	{
	case EResourceKind::Tree: return FText::Format(LOCTEXT("Chop", "Chop down {0}"), Definition->DisplayName);
	case EResourceKind::Rock: return FText::Format(LOCTEXT("Mine", "Mine {0}"), Definition->DisplayName);
	default: return FText::Format(LOCTEXT("Net", "Net {0}"), Definition->DisplayName);
	}
}

UItemDef* AResourceNode::FindTool(AActor* InInstigator) const
{
	AGielinorPlayerState* S = PS(InInstigator);
	if (!S || !Definition || Definition->ToolRequired == EToolType::None) return nullptr;
	UItemDef* A = S->Equipment->EquippedTool(Definition->ToolRequired);
	UItemDef* B = S->Inventory->BestTool(Definition->ToolRequired);
	if (A && B) return A->ToolTier >= B->ToolTier ? A : B;
	return A ? A : B;
}

bool AResourceNode::Validate(AActor* InInstigator, UItemDef*& OutTool) const
{
	AGielinorPlayerState* S = PS(InInstigator);
	if (!S || !Definition || bDepleted) return false;
	OutTool = FindTool(InInstigator);
	if (Definition->ToolRequired != EToolType::None && !OutTool)
	{
		Msg(LOCTEXT("NoTool", "You need the right tool for that.")); return false;
	}
	if (S->Skills->GetLevel(Definition->Skill) < Definition->LevelRequired)
	{
		Msg(FText::Format(LOCTEXT("Lvl", "You need level {0} to do that."), Definition->LevelRequired)); return false;
	}
	if (!S->Inventory->CanAdd(Definition->Product))
	{
		Msg(LOCTEXT("Full", "Your inventory is too full.")); return false;
	}
	return true;
}

void AResourceNode::Interact(AActor* InInstigator)
{
	UItemDef* Tool = nullptr;
	if (!Validate(InInstigator, Tool)) return;
	Msg(LOCTEXT("Begin", "You begin gathering..."));
	StartLoop(InInstigator, Definition->TickSeconds);
}

bool AResourceNode::Roll(AActor* InInstigator, float Bonus)
{
	UItemDef* Tool = nullptr;
	if (!Validate(InInstigator, Tool)) return false;
	AGielinorPlayerState* S = PS(InInstigator);
	const int32 Lvl = S->Skills->GetLevel(Definition->Skill);
	const float Tier = Tool ? Tool->ToolTier : 1;
	const float Chance = FMath::Clamp(Definition->BaseSuccess + 0.01f * (Lvl - Definition->LevelRequired) + 0.05f * (Tier - 1) + Bonus, 0.05f, 0.95f);
	if (FMath::FRand() > Chance) return true; // keep trying
	S->Inventory->Add(Definition->Product, 1);
	S->Skills->AddXp(Definition->Skill, Definition->Xp);
	if (Definition->DepleteChance > 0.f && FMath::FRand() <= Definition->DepleteChance)
	{
		SetDepleted(true);
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AResourceNode::Respawn, Definition->RespawnSeconds, false);
		return false;
	}
	return S->Inventory->CanAdd(Definition->Product);
}

bool AResourceNode::OnLoopTick(AActor* InInstigator) { return Roll(InInstigator, 0.f); }

void AResourceNode::OnToolHit(AActor* InInstigator, EToolType Tool, float Power)
{
	// A VR swing with the right tool is one immediate gather roll; a hard swing earns a bonus.
	if (!Definition || (Definition->ToolRequired != EToolType::None && Tool != Definition->ToolRequired)) return;
	Roll(InInstigator, 0.25f * FMath::Clamp(Power, 0.f, 1.f));
}

void AResourceNode::SetDepleted(bool b)
{
	bDepleted = b;
	Mesh->SetVisibility(!b);
	Mesh->SetCollisionEnabled(b ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	DepletedMesh->SetVisibility(b);
	DepletedMesh->SetCollisionEnabled(b ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (b) StopInteract(nullptr);
}

#undef LOCTEXT_NAMESPACE
