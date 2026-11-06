#include "World/AerondelRelevance.h"
#include "Combat/AerondelMob.h"
#include "Interaction/NpcTalk.h"
#include "Interaction/ResourceNode.h"
#include "Player/AerondelCharacter.h"
#include "Combat/PlayerCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

using namespace AerondelRelevance;

bool UAerondelRelevanceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UAerondelRelevanceSubsystem::Tick(float Dt)
{
	Accum += Dt;
	if (Accum < 0.5f) return; // twice a second is plenty; ~600 actors in loaded cells
	Accum = 0.f;
	UWorld* W = GetWorld();
	APawn* P = W ? UGameplayStatics::GetPlayerPawn(W, 0) : nullptr;
	if (!P) return;
	const FVector Me = P->GetActorLocation();
	const FBands B = ForTier(UAerondelScalability::GetAppliedTier());
	AActor* Pending = nullptr; AActor* Target = nullptr;
	if (AAerondelCharacter* C = Cast<AAerondelCharacter>(P)) { Pending = C->GetPendingInteraction(); if (C->Combat) Target = C->Combat->GetTarget(); }
	int32 Count[3] = { 0, 0, 0 };
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		const AAerondelMob* Mob = Cast<AAerondelMob>(A);
		if (!Mob && !A->IsA<ANpcTalk>() && !A->IsA<AResourceNode>()) continue;
		const bool bPinned = A == Pending || A == Target || (Mob && Mob->IsEngaged());
		const EBand Band = Classify(FVector::Dist(Me, A->GetActorLocation()), B, bPinned);
		++Count[(int32)Band];
		uint8* Old = Bands.Find(A);
		if (Old && *Old == (uint8)Band) continue;
		Apply(A, Band);
		Bands.Add(A, (uint8)Band);
	}
	if (Count[0] != LastLog[0] || Count[1] != LastLog[1] || Count[2] != LastLog[2])
	{
		if (FMath::Abs(Count[2] - LastLog[2]) > 10 || LastLog[0] < 0)
			UE_LOG(LogTemp, Log, TEXT("[Aerondel] Relevance: %d near, %d mid, %d far (tier bands %.0f / %.0f m)"), Count[0], Count[1], Count[2], B.Near / 100.f, B.Mid / 100.f);
		LastLog[0] = Count[0]; LastLog[1] = Count[1]; LastLog[2] = Count[2];
	}
}

void UAerondelRelevanceSubsystem::Apply(AActor* A, EBand Band)
{
	if (!BaseTick.Contains(A)) BaseTick.Add(A, A->GetActorTickInterval());
	const float Base = BaseTick.FindRef(A);
	const bool bFar = Band == EBand::Far;
	A->SetActorHiddenInGame(bFar);
	if (A->PrimaryActorTick.bCanEverTick)
	{
		A->SetActorTickEnabled(!bFar);
		A->SetActorTickInterval(Band == EBand::Mid ? FMath::Max(Base, 0.5f) : Base);
	}
	TArray<USkeletalMeshComponent*> Sks; A->GetComponents(Sks);
	for (USkeletalMeshComponent* Sk : Sks)
	{
		Sk->SetComponentTickEnabled(!bFar);
		Sk->bEnableUpdateRateOptimizations = Band != EBand::Near; // URO: far-ish rigs animate at a lower rate
		Sk->SetComponentTickInterval(Band == EBand::Mid ? 0.1f : 0.f);
	}
}
