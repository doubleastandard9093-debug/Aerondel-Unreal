#include "Inventory/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/SkillsComponent.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Data/ItemDef.h"
#include "Core/PlayerStatsComponent.h"
#include "Quests/QuestComponent.h"

bool UEquipmentComponent::MeetsRequirements(const UItemDef* Item, FText& OutReason) const
{
	const AGielinorPlayerState* PS = Cast<AGielinorPlayerState>(GetOwner());
	if (!PS || !Item) return false;
	const FName Legacy = Item->EquipSlot == EEquipSlot::Weapon ? FName(TEXT("attack")) : FName(TEXT("defence"));
	if (Item->RequiredLevel > 1 && !Item->SkillRequirements.Contains(Legacy) && PS->Skills->GetLevelById(Legacy) < Item->RequiredLevel)
	{
		OutReason = FText::Format(NSLOCTEXT("Gielinor", "EquipReqL", "You need a {0} level of {1} to wear this."), FText::FromName(Legacy), Item->RequiredLevel);
		return false;
	}
	for (const auto& R : Item->SkillRequirements)
	{
		if (PS->Skills->GetLevelById(R.Key) < R.Value)
		{
			OutReason = FText::Format(NSLOCTEXT("Gielinor", "EquipReq", "You need a {0} level of {1} to wear this."), FText::FromName(R.Key), R.Value);
			return false;
		}
	}
	if (Item->bQuestCape && !(PS->Quests && PS->Quests->QuestCapeAllowed()))
	{
		OutReason = NSLOCTEXT("Gielinor", "QuestCape", "You need to complete every quest to wear this cape.");
		return false;
	}
	return true;
}

bool UEquipmentComponent::EquipFromInventory(int32 InventorySlot)
{
	AGielinorPlayerState* PS = Cast<AGielinorPlayerState>(GetOwner());
	if (!PS) return false;
	const FItemStack Stack = PS->Inventory->GetSlot(InventorySlot);
	UItemDef* Item = Stack.Item;
	if (!Item || Item->EquipSlot == EEquipSlot::None) return false;
	UGielinorEvents* Ev = UGielinorEvents::Get(this);
	FText Why;
	if (!MeetsRequirements(Item, Why)) { if (Ev) Ev->Message(Why); return false; }

	const EEquipSlot Slot = Item->EquipSlot;
	const bool bAmmo = Slot == EEquipSlot::Ammo;
	const int32 Qty = bAmmo ? Stack.Count : 1;
	UItemDef* Old = GetEquipped(Slot);
	int32& Ammo = PS->Stats->AmmoCount;
	PS->Inventory->RemoveFromSlot(InventorySlot, Qty);
	if (Old && Old != Item)
	{
		PS->Inventory->Add(Old, bAmmo ? FMath::Max(1, Ammo) : 1, false);
		if (bAmmo) Ammo = 0;
	}
	if (bAmmo) Ammo = (Old == Item ? Ammo : 0) + Qty;
	Equipped.Add(Slot, Item);
	if (Ev) Ev->Message(FText::Format(NSLOCTEXT("Gielinor", "Wear", "You {0} the {1}."), Slot == EEquipSlot::Weapon ? NSLOCTEXT("Gielinor", "Wield", "wield") : NSLOCTEXT("Gielinor", "WearV", "wear"), Item->DisplayName));
	OnChanged.Broadcast();
	if (PS->Stats) PS->Stats->RefreshMaxHp(false);
	return true;
}

bool UEquipmentComponent::Unequip(EEquipSlot Slot)
{
	AGielinorPlayerState* PS = Cast<AGielinorPlayerState>(GetOwner());
	UItemDef* Item = GetEquipped(Slot);
	if (!PS || !Item) return false;
	if (!PS->Inventory->CanAdd(Item))
	{
		if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(NSLOCTEXT("Gielinor", "NoSpace", "You don't have enough free inventory space."));
		return false;
	}
	const int32 Qty = Slot == EEquipSlot::Ammo ? FMath::Max(1, PS->Stats->AmmoCount) : 1;
	Equipped.Remove(Slot);
	if (Slot == EEquipSlot::Ammo) PS->Stats->AmmoCount = 0;
	PS->Inventory->Add(Item, Qty, false);
	OnChanged.Broadcast();
	if (PS->Stats) PS->Stats->RefreshMaxHp(false);
	return true;
}

FItemBonuses UEquipmentComponent::TotalBonuses() const
{
	FItemBonuses T;
	for (const auto& P : Equipped)
	{
		if (!P.Value) continue;
		T += P.Value->Bonuses;
		T.Attack += P.Value->AttackBonus; T.Strength += P.Value->StrengthBonus; T.Defence += P.Value->DefenceBonus;
	}
	return T;
}

float UEquipmentComponent::TotalEffect(FName Key) const
{
	float T = 0.f; for (const auto& P : Equipped) if (P.Value) T += P.Value->GetEffect(Key); return T;
}

UItemDef* UEquipmentComponent::GetEquipped(EEquipSlot Slot) const
{
	const TObjectPtr<UItemDef>* F = Equipped.Find(Slot); return F ? F->Get() : nullptr;
}

int32 UEquipmentComponent::TotalAttackBonus() const { int32 T = 0; for (const auto& P : Equipped) if (P.Value) T += P.Value->AttackBonus; return T; }
int32 UEquipmentComponent::TotalStrengthBonus() const { int32 T = 0; for (const auto& P : Equipped) if (P.Value) T += P.Value->StrengthBonus; return T; }
int32 UEquipmentComponent::TotalDefenceBonus() const { int32 T = 0; for (const auto& P : Equipped) if (P.Value) T += P.Value->DefenceBonus; return T; }

UItemDef* UEquipmentComponent::EquippedTool(EToolType Type) const
{
	for (const auto& P : Equipped) if (P.Value && P.Value->ToolType == Type) return P.Value;
	return nullptr;
}
