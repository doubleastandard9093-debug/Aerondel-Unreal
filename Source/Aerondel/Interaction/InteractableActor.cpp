#include "Interaction/InteractableActor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/AerondelEvents.h"
#include "Core/AerondelPlayerState.h"
#include "TimerManager.h"
#include "Engine/World.h"

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetGenerateOverlapEvents(true); // VR hands/tools overlap interactables
}

bool AInteractableActor::InRange(const AActor* InInstigator, const IAerondelInteractable* Target, float Slack)
{
	if (!InInstigator || !Target) return false;
	const FVector A = InInstigator->GetActorLocation();
	const FVector B = Target->GetInteractPoint();
	return FVector::Dist2D(A, B) <= Target->GetInteractRange() * Slack;
}

AAerondelPlayerState* AInteractableActor::PS(AActor* InInstigator) { return AAerondelPlayerState::FromActor(InInstigator); }

void AInteractableActor::Msg(const FText& T) const
{
	if (UAerondelEvents* Ev = UAerondelEvents::Get(this)) Ev->Message(T);
}

void AInteractableActor::StartLoop(AActor* InInstigator, float Seconds)
{
	Current = InInstigator;
	GetWorldTimerManager().SetTimer(LoopTimer, this, &AInteractableActor::LoopTick, FMath::Max(0.1f, Seconds), true);
}

void AInteractableActor::LoopTick()
{
	AActor* I = Current.Get();
	if (!I || !InRange(I, this) || !OnLoopTick(I)) StopInteract(I);
}

void AInteractableActor::StopInteract(AActor*)
{
	GetWorldTimerManager().ClearTimer(LoopTimer);
	Current = nullptr;
}
