#include "Player/GielinorPlayerController.h"
#include "Player/GielinorCharacter.h"
#include "Core/GielinorInputConfig.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Combat/HealthComponent.h"
#include "Combat/GielinorMob.h"
#include "Interaction/GielinorInteractable.h"
#include "EnhancedInputComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "XR/GielinorVRPawn.h"

AGielinorPlayerController::AGielinorPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AGielinorPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass())) RespawnPoint = Start->GetActorLocation();
	FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void AGielinorPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	const bool bVR = InPawn && InPawn->IsA<AGielinorVRPawn>();
	bShowMouseCursor = !bVR;
	if (AGielinorPlayerState* PS = GetPlayerState<AGielinorPlayerState>())
	{
		PS->Health->OnDied.RemoveDynamic(this, &AGielinorPlayerController::OnPlayerDied);
		PS->Health->OnDied.AddDynamic(this, &AGielinorPlayerController::OnPlayerDied);
	}
}

void AGielinorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		if (UInputAction* A = UGielinorInputConfig::Get()->Click.LoadSynchronous())
			EIC->BindAction(A, ETriggerEvent::Started, this, &AGielinorPlayerController::OnClick);
}

void AGielinorPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	// Mouse moved after gamepad use: bring the cursor back (desktop pawn only; VR keeps it hidden).
	if (!bShowMouseCursor && GetPawn<AGielinorCharacter>())
	{
		float Mx = 0.f, My = 0.f; GetInputMouseDelta(Mx, My);
		if (!FMath::IsNearlyZero(Mx) || !FMath::IsNearlyZero(My)) bShowMouseCursor = true;
	}
	if (!bShowMouseCursor) { HoverText = FText::GetEmpty(); return; }
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		if (IGielinorInteractable* I = Cast<IGielinorInteractable>(Hit.GetActor())) { HoverText = I->GetInteractLabel(); return; }
	HoverText = FText::GetEmpty();
}

void AGielinorPlayerController::OnClick(const FInputActionValue&)
{
	AGielinorCharacter* Ch = GetPawn<AGielinorCharacter>();
	if (!Ch) return;
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return;
	AActor* A = Hit.GetActor();
	if (Cast<IGielinorInteractable>(A))
	{
		if (AGielinorMob* Mob = Cast<AGielinorMob>(A)) { Ch->CancelInteraction(); Mob->Interact(Ch); return; }
		Ch->QueueInteraction(A);
		return;
	}
	Ch->CancelInteraction();
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Hit.ImpactPoint); // click-to-move on the NavMesh
}

void AGielinorPlayerController::OnPlayerDied(AActor*)
{
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(NSLOCTEXT("Gielinor", "Died", "Oh dear, you are dead!"));
	if (APawn* P = GetPawn()) P->TeleportTo(RespawnPoint + FVector(0, 0, 100), P->GetActorRotation());
	if (AGielinorPlayerState* PS = GetPlayerState<AGielinorPlayerState>()) PS->Health->SetMax(PS->Health->GetMax(), true);
}
