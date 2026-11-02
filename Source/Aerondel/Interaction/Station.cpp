#include "Interaction/Station.h"
#include "Data/RecipeDef.h"
#include "Data/ItemDef.h"
#include "Data/GameDatabase.h"
#include "Core/AerondelPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Core/PlayerStatsComponent.h"
#include "Rules/AerondelRules.h"
#include "Data/SkillDef.h"

#define LOCTEXT_NAMESPACE "AerondelStation"

FText AStation::GetInteractLabel() const
{
	switch (StationType)
	{
	case EStationType::Furnace: return LOCTEXT("Smelt", "Smelt Furnace");
	case EStationType::Anvil: return LOCTEXT("Smith", "Smith Anvil");
	case EStationType::Fire: return LOCTEXT("CookFire", "Cook-at Fire");
	case EStationType::Hand: return LOCTEXT("Craft", "Craft");
	default: return LOCTEXT("Cook", "Cook-at Range");
	}
}

TArray<URecipeDef*> AStation::GetRecipes() const
{
	TArray<URecipeDef*> Out;
	if (const UGameDatabase* Db = UGameDatabase::Get())
		for (URecipeDef* R : Db->Recipes)
		{
			// A campfire cooks everything a range does.
			if (R && (R->Station == StationType || (StationType == EStationType::Fire && R->Station == EStationType::Range))) Out.Add(R);
		}
	return Out;
}

bool AStation::CanCraft(AActor* InInstigator, const URecipeDef* R, bool bTell) const
{
	AAerondelPlayerState* S = PS(InInstigator);
	if (!S || !R) return false;
	if (S->Skills->GetLevel(R->Skill) < R->LevelRequired) { if (bTell) Msg(FText::Format(LOCTEXT("Lvl", "You need level {0}."), R->LevelRequired)); return false; }
	if (R->ToolRequired != EToolType::None && !UAerondelRules::OwnsToolType(S, R->ToolRequired))
	{ if (bTell) Msg(LOCTEXT("Tool", "You need a tool for that.")); return false; }
	for (const FItemAmount& In : R->Inputs) if (!S->Inventory->Has(In.Item, In.Amount)) return false;
	return true;
}

void AStation::Interact(AActor* InInstigator)
{
	for (URecipeDef* R : GetRecipes())
	{
		if (CanCraft(InInstigator, R, false)) { CraftRecipe(InInstigator, R); return; }
	}
	Msg(LOCTEXT("Nothing", "You don't have anything to use here."));
}

void AStation::CraftRecipe(AActor* InInstigator, URecipeDef* Recipe)
{
	if (!CanCraft(InInstigator, Recipe, true)) return;
	Active = Recipe;
	StartLoop(InInstigator, Recipe->Seconds);
}

bool AStation::OnLoopTick(AActor* InInstigator)
{
	AAerondelPlayerState* S = PS(InInstigator);
	if (!S || !CanCraft(InInstigator, Active, false)) return false;
	for (const FItemAmount& In : Active->Inputs) S->Inventory->Remove(In.Item, In.Amount);
	const bool bCooking = Active->Station == EStationType::Range || Active->Station == EStationType::Fire;
	const float Fail = bCooking ? UAerondelRules::BurnChance(S, Active->StopFailLevel, Active->LevelRequired) : Active->FailChance(S->Skills->GetLevel(Active->Skill));
	if (Active->FailItem && FMath::FRand() < Fail)
	{
		S->Inventory->Add(Active->FailItem, 1);
		Msg(LOCTEXT("Burn", "You accidentally burn it."));
	}
	else
	{
		S->Inventory->Add(Active->Output, Active->OutputAmount);
		S->Skills->AddXp(Active->Skill, Active->Xp * UAerondelRules::XpMultiplier(S, Active->Skill ? Active->Skill->Id : NAME_None));
	}
	return CanCraft(InInstigator, Active, false);
}

#undef LOCTEXT_NAMESPACE
