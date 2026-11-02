#pragma once
#include "CoreMinimal.h"
#include "Inventory/InventoryComponent.h"
#include "BankComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBankOpen, bool, bOpen);

/** 400-slot, everything-stacks container plus deposit/withdraw against the backpack. */
UCLASS(ClassGroup=(Aerondel), meta=(BlueprintSpawnableComponent))
class AERONDEL_API UBankComponent : public UInventoryComponent
{
	GENERATED_BODY()
public:
	UBankComponent();
	UPROPERTY(BlueprintAssignable) FOnBankOpen OnOpenChanged;

	UFUNCTION(BlueprintCallable, Category="Bank") void Open() { bOpen = true; OnOpenChanged.Broadcast(true); }
	UFUNCTION(BlueprintCallable, Category="Bank") void Close() { bOpen = false; OnOpenChanged.Broadcast(false); }
	UFUNCTION(BlueprintPure, Category="Bank") bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintCallable, Category="Bank") int32 Deposit(UInventoryComponent* From, UItemDef* Item, int32 Count);
	UFUNCTION(BlueprintCallable, Category="Bank") void DepositAll(UInventoryComponent* From);
	UFUNCTION(BlueprintCallable, Category="Bank") int32 Withdraw(UInventoryComponent* To, UItemDef* Item, int32 Count);

private:
	bool bOpen = false;
};
