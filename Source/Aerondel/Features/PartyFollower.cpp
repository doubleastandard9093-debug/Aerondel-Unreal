#include "Features/PartyFollower.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Models/AerondelModelLibrary.h"
#include "UObject/ConstructorHelpers.h"

APartyFollower::APartyFollower()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) Body->SetStaticMesh(Cyl.Object);
	Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.7f));
	Rig = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Rig"));
	Rig->SetupAttachment(Body);
	Rig->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APartyFollower::Setup(FName InId, const FString& InName, const FString& Style)
{
	Id = InId; DisplayName = InName;
	// Rigged model for the companion's role (Lite rigs.js npc mapping); stays a capsule when models are not imported yet.
	const bool bRig = UAerondelModelLibrary::ApplyRig(Rig, Style == TEXT("magic") ? TEXT("npc_mage") : Style == TEXT("ranged") ? TEXT("npc_ranger") : TEXT("npc_knight"));
	UE_LOG(LogTemp, Log, TEXT("[Aerondel] Spawn companion %s (%s): rig %s"), *InId.ToString(), *Style, bRig ? TEXT("applied") : TEXT("FAILED, capsule"));
	if (bRig)
	{
		Body->SetVisibility(false);
		Rig->SetUsingAbsoluteScale(true); Rig->SetWorldScale3D(FVector(1.f));
		UAerondelModelLibrary::FitRig(Rig, 172.f, 50.f); // body is ~100 cm tall: feet 50 cm below its centre
	}
}

void APartyFollower::Tick(float Dt)
{
	Super::Tick(Dt);
	AActor* L = Leader.Get(); if (!L) return;
	const FVector F = L->GetActorForwardVector(), R = L->GetActorRightVector();
	const FVector Goal = L->GetActorLocation() + R * Slot.X + F * Slot.Y;
	const FVector P = GetActorLocation(); const FVector To = Goal - P;
	const float D = To.Size2D();
	if (D > 2500.f) { SetActorLocation(Goal); return; } // teleported / fell behind: snap
	if (D < 30.f) { UAerondelModelLibrary::SetAnimState(Rig, TEXT("idle")); return; }
	const float Speed = D > 400.f ? 700.f : 420.f;
	const FVector Step = To.GetSafeNormal2D() * FMath::Min(D, Speed * Dt);
	SetActorLocation(FVector(P.X + Step.X, P.Y + Step.Y, FMath::FInterpTo(P.Z, Goal.Z, Dt, 6.f)));
	SetActorRotation(FRotator(0.f, Step.Rotation().Yaw, 0.f));
	UAerondelModelLibrary::SetAnimState(Rig, D > 400.f ? TEXT("run") : TEXT("walk"));
}
