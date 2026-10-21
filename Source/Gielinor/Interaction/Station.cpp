#include "Interaction/Station.h"
#include "Data/RecipeDef.h"
#include "Data/ItemDef.h"
#include "Data/GameDatabase.h"
#include "Core/GielinorPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"

#define LOCTEXT_NAMESPACE "GielinorStation"

FText AStation::GetInteractLabel() const
{
	switch (StationType)
	{
	case EStationType::Furnace: return LOCTEXT("Smelt", "Smelt Furnace");
	case EStationType::Anvil: return LOCTEXT("Smith", "Smith Anvil");
	default: return LOCTEXT("Cook", "Cook-at Range");
	}
}

TArray<URecipeDef*> AStation::GetRecipes() const
{
	TArray<URecipeDef*> Out;
	if (const UGameDatabase* Db = UGameDatabase::Get())
		for (URecipeDef* R : Db->Recipes) if (R && R->Station == StationType) Out.Add(R);
	return Out;
}

bool AStation::CanCraft(AActor* Instigator, const URecipeDef* R, bool bTell) const
{
	AGielinorPlayerState* S = PS(Instigator);
	if (!S || !R) return false;
	if (S->Skills->GetLevel(R->Skill) < R->LevelRequired) { if (bTell) Msg(FText::Format(LOCTEXT("Lvl", "You need level {0}."), R->LevelRequired)); return false; }
	if (R->ToolRequired != EToolType::None && !S->Inventory->BestTool(R->ToolRequired) && !S->Equipment->EquippedTool(R->ToolRequired))
	{ if (bTell) Msg(LOCTEXT("Tool", "You need a tool for that.")); return false; }
	for (const FItemAmount& In : R->Inputs) if (!S->Inventory->Has(In.Item, In.Amount)) return false;
	return true;
}

void AStation::Interact(AActor* Instigator)
{
	for (URecipeDef* R : GetRecipes())
	{
		if (CanCraft(Instigator, R, false)) { CraftRecipe(Instigator, R); return; }
	}
	Msg(LOCTEXT("Nothing", "You don't have anything to use here."));
}

void AStation::CraftRecipe(AActor* Instigator, URecipeDef* Recipe)
{
	if (!CanCraft(Instigator, Recipe, true)) return;
	Active = Recipe;
	StartLoop(Instigator, Recipe->Seconds);
}

bool AStation::OnLoopTick(AActor* Instigator)
{
	AGielinorPlayerState* S = PS(Instigator);
	if (!S || !CanCraft(Instigator, Active, false)) return false;
	for (const FItemAmount& In : Active->Inputs) S->Inventory->Remove(In.Item, In.Amount);
	const int32 Lvl = S->Skills->GetLevel(Active->Skill);
	if (Active->FailItem && FMath::FRand() < Active->FailChance(Lvl))
	{
		S->Inventory->Add(Active->FailItem, 1);
		Msg(LOCTEXT("Burn", "You accidentally burn it."));
	}
	else
	{
		S->Inventory->Add(Active->Output, Active->OutputAmount);
		S->Skills->AddXp(Active->Skill, Active->Xp);
	}
	return CanCraft(Instigator, Active, false);
}

#undef LOCTEXT_NAMESPACE
