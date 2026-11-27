#include "Interaction/BankChest.h"
#include "Core/AerondelPlayerState.h"
#include "Bank/BankComponent.h"

ABankChest::ABankChest() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = 0.25f; }

void ABankChest::Interact(AActor* InInstigator)
{
	if (AAerondelPlayerState* S = PS(InInstigator)) { S->Bank->OpenFrom(InInstigator, this, GetInteractRange() * 2.f); User = InInstigator; }
}

void ABankChest::Tick(float Dt)
{
	Super::Tick(Dt);
	AActor* U = User.Get();
	if (U && !InRange(U, this, 2.f))
	{
		if (AAerondelPlayerState* S = PS(U)) if (S->Bank && S->Bank->IsOpen()) S->Bank->Close();
		User = nullptr;
	}
}
