#include "Combat/AerondelMobRegistry.h"
#include "Combat/AerondelMob.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool UAerondelMobRegistry::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UAerondelMobRegistry::RegisterMob(AAerondelMob* Mob)
{
	if (Mob) Mobs.AddUnique(TWeakObjectPtr<AAerondelMob>(Mob));
}

void UAerondelMobRegistry::UnregisterMob(AAerondelMob* Mob)
{
	if (Mob) Mobs.Remove(TWeakObjectPtr<AAerondelMob>(Mob));
	// drop anything that went away without EndPlay (keeps the list from growing across long sessions)
	Mobs.RemoveAll([](const TWeakObjectPtr<AAerondelMob>& P) { return !P.IsValid(); });
}

void UAerondelMobRegistry::SnapshotMobs(FAerondelMobList& Out) const
{
	Out.Reset();
	for (const TWeakObjectPtr<AAerondelMob>& P : Mobs)
		if (AAerondelMob* M = P.Get()) Out.Add(M);
}

UAerondelMobRegistry* UAerondelMobRegistry::GetFor(const UWorld* World)
{
	return World ? World->GetSubsystem<UAerondelMobRegistry>() : nullptr;
}

void AerondelMobs::Live(UWorld* World, FAerondelMobList& Out)
{
	Out.Reset();
	if (!World) return;
	if (World->HasBegunPlay())
		if (const UAerondelMobRegistry* R = UAerondelMobRegistry::GetFor(World)) { R->SnapshotMobs(Out); return; }
	for (TActorIterator<AAerondelMob> It(World); It; ++It) Out.Add(*It);
}
