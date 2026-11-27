#include "Bank/BankComponent.h"
#include "Data/ItemDef.h"
#include "GameFramework/Actor.h"

UBankComponent::UBankComponent()
{
	Capacity = 400; bStackEverything = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UBankComponent::Open()
{
	bOpen = true;
	BankUser = nullptr; BankSource = nullptr;
	SetComponentTickEnabled(false);
	OnOpenChanged.Broadcast(true);
}

void UBankComponent::OpenFrom(AActor* User, AActor* Source, float RangeCm)
{
	bOpen = true;
	BankUser = User; BankSource = Source; CloseRangeCm = RangeCm;
	SetComponentTickEnabled(User && Source);
	OnOpenChanged.Broadcast(true);
}

void UBankComponent::Close()
{
	if (!bOpen) return;
	bOpen = false;
	BankUser = nullptr; BankSource = nullptr;
	SetComponentTickEnabled(false);
	OnOpenChanged.Broadcast(false);
}

void UBankComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	AActor* U = BankUser.Get();
	AActor* S = BankSource.Get();
	if (!bOpen || !U || !S) { if (bOpen && (!U || !S)) Close(); return; }
	if (FVector::Dist2D(U->GetActorLocation(), S->GetActorLocation()) > CloseRangeCm) Close();
}

int32 UBankComponent::Deposit(UInventoryComponent* From, UItemDef* Item, int32 Count)
{
	if (!From || !Item) return 0;
	const int32 Moved = From->Remove(Item, FMath::Min(Count, From->CountOf(Item)));
	const int32 Left = Add(Item, Moved, false);
	if (Left > 0) From->Add(Item, Left, false); // bank full: give back
	return Moved - Left;
}

void UBankComponent::DepositAll(UInventoryComponent* From)
{
	if (!From) return;
	TArray<UItemDef*> Seen;
	for (const FItemStack& S : From->GetSlots()) if (S.Item) Seen.AddUnique(S.Item);
	for (UItemDef* I : Seen) Deposit(From, I, MAX_int32);
}

int32 UBankComponent::Withdraw(UInventoryComponent* To, UItemDef* Item, int32 Count)
{
	if (!To || !Item) return 0;
	int32 Want = FMath::Min(Count, CountOf(Item));
	if (!Item->bStackable) Want = FMath::Min(Want, To->FreeSlots());
	if (Want <= 0) return 0;
	Remove(Item, Want);
	const int32 Left = To->Add(Item, Want, false);
	if (Left > 0) Add(Item, Left, false);
	return Want - Left;
}
