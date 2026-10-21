#include "Inventory/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/SkillsComponent.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Data/ItemDef.h"

bool UEquipmentComponent::EquipFromInventory(int32 InventorySlot)
{
	AGielinorPlayerState* PS = Cast<AGielinorPlayerState>(GetOwner());
	if (!PS) return false;
	const FItemStack Stack = PS->Inventory->GetSlot(InventorySlot);
	UItemDef* Item = Stack.Item;
	if (!Item || Item->EquipSlot == EEquipSlot::None) return false;
	const FName Req = Item->EquipSlot == EEquipSlot::Weapon ? FName(TEXT("attack")) : FName(TEXT("defence"));
	if (PS->Skills->GetLevelById(Req) < Item->RequiredLevel)
	{
		if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(FText::Format(NSLOCTEXT("Gielinor", "EquipReq", "You need level {0} to equip that."), Item->RequiredLevel));
		return false;
	}
	PS->Inventory->RemoveFromSlot(InventorySlot, 1);
	if (UItemDef* Old = GetEquipped(Item->EquipSlot)) PS->Inventory->Add(Old, 1, false);
	Equipped.Add(Item->EquipSlot, Item);
	OnChanged.Broadcast();
	return true;
}

bool UEquipmentComponent::Unequip(EEquipSlot Slot)
{
	AGielinorPlayerState* PS = Cast<AGielinorPlayerState>(GetOwner());
	UItemDef* Item = GetEquipped(Slot);
	if (!PS || !Item || !PS->Inventory->CanAdd(Item)) return false;
	Equipped.Remove(Slot);
	PS->Inventory->Add(Item, 1, false);
	OnChanged.Broadcast();
	return true;
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
