#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GielinorTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContainerChanged);

/** Fixed-size slot container. Used for the 28-slot backpack; the bank uses its own larger instance. */
UCLASS(ClassGroup=(Gielinor), meta=(BlueprintSpawnableComponent))
class GIELINOR_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UInventoryComponent();
	virtual void InitializeComponent() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory") int32 Capacity = 28;
	/** Bank-style: every item stacks regardless of UItemDef::bStackable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory") bool bStackEverything = true;
	UPROPERTY(BlueprintAssignable) FOnContainerChanged OnChanged;

	/** Adds as many as fit. Returns how many were NOT added. */
	UFUNCTION(BlueprintCallable, Category="Inventory") int32 Add(UItemDef* Item, int32 Count = 1, bool bAnnounce = true);
	/** Removes up to Count. Returns removed count. */
	UFUNCTION(BlueprintCallable, Category="Inventory") int32 Remove(UItemDef* Item, int32 Count = 1);
	UFUNCTION(BlueprintCallable, Category="Inventory") bool RemoveFromSlot(int32 Slot, int32 Count = 1);
	UFUNCTION(BlueprintPure, Category="Inventory") int32 CountOf(const UItemDef* Item) const;
	UFUNCTION(BlueprintPure, Category="Inventory") bool Has(const UItemDef* Item, int32 Count = 1) const { return CountOf(Item) >= Count; }
	UFUNCTION(BlueprintPure, Category="Inventory") bool CanAdd(const UItemDef* Item, int32 Count = 1) const;
	UFUNCTION(BlueprintPure, Category="Inventory") int32 FreeSlots() const;
	UFUNCTION(BlueprintPure, Category="Inventory") FItemStack GetSlot(int32 Index) const { return Slots.IsValidIndex(Index) ? Slots[Index] : FItemStack(); }
	UFUNCTION(BlueprintPure, Category="Inventory") const TArray<FItemStack>& GetSlots() const { return Slots; }
	UFUNCTION(BlueprintCallable, Category="Inventory") void Swap(int32 A, int32 B);
	/** Best tool of the type in the backpack (equipment is checked by the caller). */
	UFUNCTION(BlueprintPure, Category="Inventory") UItemDef* BestTool(EToolType Type) const;

	void ClearAll();
	void SetSlotRaw(int32 Index, UItemDef* Item, int32 Count);
	void GiveStarterKit();

private:
	bool Stacks(const UItemDef* Item) const;
	UPROPERTY() TArray<FItemStack> Slots;
};
