#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GielinorTypes.h"
#include "EquipmentComponent.generated.h"

class UItemDef;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentChanged);

UCLASS(ClassGroup=(Gielinor), meta=(BlueprintSpawnableComponent))
class GIELINOR_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnEquipmentChanged OnChanged;

	/** Moves an item from the backpack slot into its equip slot, swapping out what was there. */
	UFUNCTION(BlueprintCallable, Category="Equipment") bool EquipFromInventory(int32 InventorySlot);
	UFUNCTION(BlueprintCallable, Category="Equipment") bool Unequip(EEquipSlot Slot);
	UFUNCTION(BlueprintPure, Category="Equipment") UItemDef* GetEquipped(EEquipSlot Slot) const;
	UFUNCTION(BlueprintPure, Category="Equipment") int32 TotalAttackBonus() const;
	UFUNCTION(BlueprintPure, Category="Equipment") int32 TotalStrengthBonus() const;
	UFUNCTION(BlueprintPure, Category="Equipment") int32 TotalDefenceBonus() const;
	/** Best equipped tool of a type (e.g. an axe wielded as a weapon), or null. */
	UFUNCTION(BlueprintPure, Category="Equipment") UItemDef* EquippedTool(EToolType Type) const;

	void SetRaw(EEquipSlot Slot, UItemDef* Item) { if (Item) Equipped.Add(Slot, Item); else Equipped.Remove(Slot); }
	const TMap<EEquipSlot, TObjectPtr<UItemDef>>& GetAll() const { return Equipped; }
	void ClearAll() { Equipped.Reset(); OnChanged.Broadcast(); }

private:
	UPROPERTY() TMap<EEquipSlot, TObjectPtr<UItemDef>> Equipped;
};
