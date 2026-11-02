#include "Interaction/BankChest.h"
#include "Core/AerondelPlayerState.h"
#include "Bank/BankComponent.h"

ABankChest::ABankChest() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = 0.25f; }

void ABankChest::Interact(AActor* InInstigator)
{
	if (AAerondelPlayerState* S = PS(InInstigator)) { S->Bank->Open(); User = InInstigator; }
}

void ABankChest::Tick(float Dt)
{
	Super::Tick(Dt);
	AActor* U = User.Get();
	if (U && !InRange(U, this, 2.f))
	{
		if (AAerondelPlayerState* S = PS(U)) S->Bank->Close();
		User = nullptr;
	}
}
