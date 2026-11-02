#include "Combat/HealthComponent.h"

void UHealthComponent::SetMax(int32 NewMax, bool bRefill)
{
	Max = FMath::Max(1, NewMax);
	Current = bRefill ? Max : FMath::Min(Current, Max);
	OnHealthChanged.Broadcast(Current, Max, 0);
}

void UHealthComponent::ApplyDamage(int32 Amount, AActor* Source)
{
	if (IsDead()) return;
	Amount = FMath::Max(0, Amount);
	Current = FMath::Max(0, Current - Amount);
	OnHealthChanged.Broadcast(Current, Max, -Amount);
	if (Current == 0) OnDied.Broadcast(Source);
}

void UHealthComponent::Heal(int32 Amount)
{
	if (Amount <= 0) return;
	const int32 Old = Current;
	Current = FMath::Min(Max, Current + Amount);
	OnHealthChanged.Broadcast(Current, Max, Current - Old);
}
