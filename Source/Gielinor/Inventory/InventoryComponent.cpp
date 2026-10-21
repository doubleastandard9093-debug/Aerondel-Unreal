#include "Inventory/InventoryComponent.h"
#include "Data/ItemDef.h"
#include "Data/GameDatabase.h"
#include "Core/GielinorEvents.h"

UInventoryComponent::UInventoryComponent() { bWantsInitializeComponent = true; }

void UInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();
	Slots.SetNum(Capacity);
}

bool UInventoryComponent::Stacks(const UItemDef* Item) const { return bStackEverything || (Item && Item->bStackable); }

bool UInventoryComponent::CanAdd(const UItemDef* Item, int32 Count) const
{
	if (!Item || Count <= 0) return false;
	if (Stacks(Item)) return CountOf(Item) > 0 || FreeSlots() > 0;
	return FreeSlots() >= Count;
}

int32 UInventoryComponent::Add(UItemDef* Item, int32 Count, bool bAnnounce)
{
	if (!Item || Count <= 0) return Count;
	int32 Left = Count;
	if (Stacks(Item))
	{
		for (FItemStack& S : Slots) if (S.Item == Item) { S.Count += Left; Left = 0; break; }
		if (Left > 0) for (FItemStack& S : Slots) if (S.IsEmpty()) { S.Item = Item; S.Count = Left; Left = 0; break; }
	}
	else
	{
		for (FItemStack& S : Slots) { if (Left == 0) break; if (S.IsEmpty()) { S.Item = Item; S.Count = 1; --Left; } }
	}
	const int32 Added = Count - Left;
	if (Added > 0)
	{
		OnChanged.Broadcast();
		if (bAnnounce) if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnItemGained.Broadcast(Item, Added);
	}
	if (Left > 0) if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(NSLOCTEXT("Gielinor", "InvFull", "You don't have enough inventory space."));
	return Left;
}

int32 UInventoryComponent::Remove(UItemDef* Item, int32 Count)
{
	int32 Removed = 0;
	for (int32 i = Slots.Num() - 1; i >= 0 && Removed < Count; --i)
	{
		FItemStack& S = Slots[i];
		if (S.Item != Item) continue;
		const int32 Take = FMath::Min(S.Count, Count - Removed);
		S.Count -= Take; Removed += Take;
		if (S.Count <= 0) S = FItemStack();
	}
	if (Removed > 0) OnChanged.Broadcast();
	return Removed;
}

bool UInventoryComponent::RemoveFromSlot(int32 Slot, int32 Count)
{
	if (!Slots.IsValidIndex(Slot) || Slots[Slot].IsEmpty()) return false;
	Slots[Slot].Count -= Count;
	if (Slots[Slot].Count <= 0) Slots[Slot] = FItemStack();
	OnChanged.Broadcast();
	return true;
}

int32 UInventoryComponent::CountOf(const UItemDef* Item) const
{
	int32 N = 0; for (const FItemStack& S : Slots) if (S.Item == Item) N += S.Count; return N;
}

int32 UInventoryComponent::FreeSlots() const
{
	int32 N = 0; for (const FItemStack& S : Slots) if (S.IsEmpty()) ++N; return N;
}

void UInventoryComponent::Swap(int32 A, int32 B)
{
	if (Slots.IsValidIndex(A) && Slots.IsValidIndex(B)) { Slots.Swap(A, B); OnChanged.Broadcast(); }
}

UItemDef* UInventoryComponent::BestTool(EToolType Type) const
{
	UItemDef* Best = nullptr;
	for (const FItemStack& S : Slots)
		if (S.Item && S.Item->ToolType == Type && (!Best || S.Item->ToolTier > Best->ToolTier)) Best = S.Item;
	return Best;
}

void UInventoryComponent::ClearAll() { Slots.Reset(); Slots.SetNum(Capacity); OnChanged.Broadcast(); }

void UInventoryComponent::SetSlotRaw(int32 Index, UItemDef* Item, int32 Count)
{
	if (Index >= Slots.Num()) Slots.SetNum(Index + 1);
	Slots[Index].Item = Item; Slots[Index].Count = Count;
}

void UInventoryComponent::GiveStarterKit()
{
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!Db) return;
	for (const TCHAR* Id : { TEXT("bronze_axe"), TEXT("bronze_pickaxe"), TEXT("small_fishing_net"), TEXT("hammer"), TEXT("tinderbox"), TEXT("bronze_sword") })
		if (UItemDef* I = Db->FindItem(Id)) Add(I, 1, false);
	if (UItemDef* Coins = Db->FindItem(TEXT("coins"))) Add(Coins, 25, false);
}
