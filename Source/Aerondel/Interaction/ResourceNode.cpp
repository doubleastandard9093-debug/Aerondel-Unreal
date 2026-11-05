#include "Interaction/ResourceNode.h"
#include "Player/AerondelCharacter.h"
#include "Data/ResourceNodeDef.h"
#include "Data/GameDatabase.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Core/AerondelPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Core/AerondelEvents.h"
#include "Rules/AerondelRules.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "AerondelResource"

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

void AResourceNode::BeginPlay()
{
	Super::BeginPlay();
	if (!Definition && !DefinitionId.IsNone())
	{
		if (UGameDatabase* Db = UGameDatabase::Get()) Definition = Db->FindResourceNode(DefinitionId);
		if (Definition) DisplayName = Definition->DisplayName;
		else UE_LOG(LogTemp, Warning, TEXT("[Aerondel] Placement: resource node %s has unknown id %s"), *GetName(), *DefinitionId.ToString());
	}
}

FText AResourceNode::GetInteractLabel() const
{
	if (!Definition) return DisplayName;
	switch (Definition->Kind)
	{
	case EResourceKind::Tree: return FText::Format(LOCTEXT("Chop", "Chop down {0}"), Definition->DisplayName);
	case EResourceKind::Rock: return FText::Format(LOCTEXT("Mine", "Mine {0}"), Definition->DisplayName);
	case EResourceKind::HerbPatch: return FText::Format(LOCTEXT("Pick", "Pick {0}"), Definition->DisplayName);
	default: return FText::Format(LOCTEXT("Fish", "Fish {0}"), Definition->DisplayName);
	}
}

UItemDef* AResourceNode::FindTool(AActor* InInstigator) const
{
	AAerondelPlayerState* S = PS(InInstigator);
	if (!S || !Definition || Definition->ToolRequired == EToolType::None) return nullptr;
	return UAerondelRules::BestTool(S, Definition->ToolRequired);
}

bool AResourceNode::Validate(AActor* InInstigator, UItemDef*& OutTool) const
{
	AAerondelPlayerState* S = PS(InInstigator);
	if (!S || !Definition || bDepleted) return false;
	OutTool = FindTool(InInstigator);
	return true;
}

void AResourceNode::Interact(AActor* InInstigator)
{
	AAerondelPlayerState* S = PS(InInstigator);
	if (!S || !Definition) return;
	// Dry run for level / tool / space errors before starting the loop (a failed roll is fine).
	const FGatherResult R = UAerondelRules::GatherAttempt(S, Definition, -10.f);
	if (!R.Error.IsEmpty()) { Msg(R.Error); return; }
	Msg(LOCTEXT("Begin", "You begin gathering..."));
	StartLoop(InInstigator, Definition->TickSeconds);
}

bool AResourceNode::Roll(AActor* InInstigator, float Bonus)
{
	AAerondelPlayerState* S = PS(InInstigator);
	if (!S || !Definition || bDepleted) return false;
	const FGatherResult R = UAerondelRules::GatherAttempt(S, Definition, Bonus);
	if (!R.Error.IsEmpty()) { Msg(R.Error); return false; }
	if (!R.Item) return true; // missed: keep trying
	const FName SkillId = Definition->Skill ? Definition->Skill->Id : NAME_None;
	if (R.bToBank) { S->Bank->Add(R.Item, R.Count, false); Msg(FText::Format(LOCTEXT("ToBank", "Your {0} is sent to the bank."), R.Item->DisplayName)); }
	else S->Inventory->Add(R.Item, R.Count);
	if (R.Count > 1) Msg(LOCTEXT("Double", "You get an extra one!"));
	S->Skills->AddXp(Definition->Skill, R.Xp * UAerondelRules::XpMultiplier(S, SkillId));
	// Web build: resources never run out (SetDepleted stays for Blueprint use).
	return true;
}

bool AResourceNode::OnLoopTick(AActor* InInstigator)
{
	if (AAerondelCharacter* Ch = Cast<AAerondelCharacter>(InInstigator)) Ch->PlayBodyAnim(TEXT("gather")); // web rigs.js "interact" clip while skilling
	return Roll(InInstigator, 0.f);
}

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
