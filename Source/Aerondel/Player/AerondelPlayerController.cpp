#include "Player/AerondelPlayerController.h"
#include "UI/AerondelHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Player/AerondelCharacter.h"
#include "Core/AerondelInputConfig.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Core/AerondelEvents.h"
#include "Combat/HealthComponent.h"
#include "Combat/AerondelMob.h"
#include "Interaction/AerondelInteractable.h"
#include "EnhancedInputComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "XR/AerondelVRPawn.h"
#include "Engine/HitResult.h"

AAerondelPlayerController::AAerondelPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AAerondelPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass())) RespawnPoint = Start->GetActorLocation();
	FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	// Camera pitch: -75 (looking down from above) .. +35 (camera low, looking up at the sky / tall things).
	if (PlayerCameraManager) { PlayerCameraManager->ViewPitchMin = -75.f; PlayerCameraManager->ViewPitchMax = 35.f; }
}

void AAerondelPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	const bool bVR = InPawn && InPawn->IsA<AAerondelVRPawn>();
	bShowMouseCursor = !bVR;
	if (AAerondelPlayerState* PS = GetPlayerState<AAerondelPlayerState>())
	{
		PS->Health->OnDied.RemoveDynamic(this, &AAerondelPlayerController::OnPlayerDied);
		PS->Health->OnDied.AddDynamic(this, &AAerondelPlayerController::OnPlayerDied);
	}
}

void AAerondelPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		if (UInputAction* A = UAerondelInputConfig::Get()->Click.LoadSynchronous())
			EIC->BindAction(A, ETriggerEvent::Started, this, &AAerondelPlayerController::OnClick);
}

void AAerondelPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	// Mouse moved after gamepad use: bring the cursor back (desktop pawn only; VR keeps it hidden).
	if (!bShowMouseCursor && GetPawn<AAerondelCharacter>())
	{
		float Mx = 0.f, My = 0.f; GetInputMouseDelta(Mx, My);
		if (!FMath::IsNearlyZero(Mx) || !FMath::IsNearlyZero(My)) bShowMouseCursor = true;
	}
	if (!bShowMouseCursor) { HoverText = FText::GetEmpty(); return; }
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		if (IAerondelInteractable* I = Cast<IAerondelInteractable>(Hit.GetActor())) { HoverText = I->GetInteractLabel(); return; }
	HoverText = FText::GetEmpty();
}

void AAerondelPlayerController::OnClick(const FInputActionValue&)
{
	AAerondelCharacter* Ch = GetPawn<AAerondelCharacter>();
	if (!Ch) return;
	float Mx = 0.f, My = 0.f;
	if (AAerondelHUD* Hud = Cast<AAerondelHUD>(GetHUD())) if (GetMousePosition(Mx, My) && Hud->HandleHudClick(FVector2D(Mx, My))) return;
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return;
	AActor* A = Hit.GetActor();
	if (Cast<IAerondelInteractable>(A))
	{
		if (AAerondelMob* Mob = Cast<AAerondelMob>(A)) { Ch->CancelInteraction(); Mob->Interact(Ch); return; }
		Ch->QueueInteraction(A);
		return;
	}
	Ch->CancelInteraction();
	Ch->MoveToWorld(Hit.ImpactPoint); // NavMesh when present, else a straight walk
}

void AAerondelPlayerController::OnPlayerDied(AActor*)
{
	// Health::OnDied fires in the middle of ApplyDamage, before UPlayerStatsComponent::ApplyPlayerDamage
	// can see IsDead(). Reviving here used to skip HandleDeath (prayers, poison, stun, feature notify).
	// If something damaged Health directly, call HandleDeath ourselves; if ApplyPlayerDamage is about
	// to call it, HandleDeath is a no-op once HP is already refilled.
	if (AAerondelPlayerState* PS = GetPlayerState<AAerondelPlayerState>())
		if (PS->Health && PS->Health->IsDead() && PS->Stats) PS->Stats->HandleDeath();
}
