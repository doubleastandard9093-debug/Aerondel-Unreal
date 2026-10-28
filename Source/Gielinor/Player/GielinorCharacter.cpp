#include "Player/GielinorCharacter.h"
#include "Core/GielinorInputConfig.h"
#include "Interaction/GielinorInteractable.h"
#include "Interaction/InteractableActor.h"
#include "Combat/PlayerCombatComponent.h"
#include "Player/GielinorGamepadComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AGielinorCharacter::AGielinorCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;
	auto* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0, 720, 0);
	Move->MaxWalkSpeed = 450.f;
	// Small max step + sub-stepping so walls can't be tunnelled through on hitches (lesson from the web build).
	Move->MaxSimulationTimeStep = 0.025f;
	Move->MaxSimulationIterations = 8;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = TargetArm;
	SpringArm->SetRelativeRotation(FRotator(-35, 0, 0));
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.f;
	SpringArm->bDoCollisionTest = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	Combat = CreateDefaultSubobject<UPlayerCombatComponent>(TEXT("Combat"));
	Gamepad = CreateDefaultSubobject<UGielinorGamepadComponent>(TEXT("Gamepad"));

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Capsule(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Capsule.Succeeded()) PlaceholderBody->SetStaticMesh(Capsule.Object);
	PlaceholderBody->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
}

void AGielinorCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->ClearAllMappings();
			if (UInputMappingContext* IMC = UGielinorInputConfig::Get()->DesktopContext.LoadSynchronous()) Sub->AddMappingContext(IMC, 0);
			// Gamepad layout sits above the desktop context so its keys consume any old gamepad rows there.
			if (Gamepad) if (UInputMappingContext* Pad = Gamepad->GetContext()) Sub->AddMappingContext(Pad, 1);
		}
}

void AGielinorCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Input);
	const UGielinorInputConfig* Cfg = UGielinorInputConfig::Get();
	if (!EIC || !Cfg) return;
	if (UInputAction* A = Cfg->Move.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Triggered, this, &AGielinorCharacter::OnMove);
	if (UInputAction* A = Cfg->Look.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Triggered, this, &AGielinorCharacter::OnLook);
	if (UInputAction* A = Cfg->Zoom.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Triggered, this, &AGielinorCharacter::OnZoom);
	if (UInputAction* A = Cfg->OrbitHold.LoadSynchronous())
	{
		EIC->BindAction(A, ETriggerEvent::Started, this, &AGielinorCharacter::OnOrbitStart);
		EIC->BindAction(A, ETriggerEvent::Completed, this, &AGielinorCharacter::OnOrbitEnd);
	}
	if (UInputAction* A = Cfg->Interact.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Started, this, &AGielinorCharacter::OnInteract);
	if (UInputAction* A = Cfg->Jump.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Started, this, &ACharacter::Jump);
	if (Gamepad) Gamepad->Bind(EIC);
	// Click is bound by AGielinorPlayerController (needs the cursor).
}

void AGielinorCharacter::OnMove(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	if (In.IsNearlyZero()) return;
	CancelInteraction();
	if (AController* C = GetController()) C->StopMovement();
	const FRotator Yaw(0, GetControlRotation().Yaw, 0);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), In.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), In.X);
}

void AGielinorCharacter::OnLook(const FInputActionValue& V)
{
	// Mouse look only while orbiting (RMB held) so the cursor stays free for clicking; gamepad always orbits.
	const FVector2D In = V.Get<FVector2D>();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->bShowMouseCursor && !bOrbiting) return;
	AddControllerYawInput(In.X);
	AddControllerPitchInput(-In.Y);
}

void AGielinorCharacter::OnZoom(const FInputActionValue& V)
{
	TargetArm = FMath::Clamp(TargetArm - V.Get<float>() * ZoomStep, MinZoom, MaxZoom);
}

void AGielinorCharacter::OnInteract(const FInputActionValue&)
{
	if (AActor* T = FindNearestInteractable()) QueueInteraction(T);
}

AActor* AGielinorCharacter::FindNearestInteractable(float Radius) const
{
	TArray<FOverlapResult> Hits;
	GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeSphere(Radius));
	AActor* Best = nullptr; float BestScore = TNumericLimits<float>::Max();
	for (const FOverlapResult& H : Hits)
	{
		AActor* A = H.GetActor();
		IGielinorInteractable* I = Cast<IGielinorInteractable>(A);
		if (!I || A == this || !I->CanInteract(const_cast<AGielinorCharacter*>(this))) continue;
		const FVector To = I->GetInteractPoint() - GetActorLocation();
		const float Score = To.Size() * (1.5f - 0.5f * FVector::DotProduct(To.GetSafeNormal2D(), GetActorForwardVector()));
		if (Score < BestScore) { BestScore = Score; Best = A; }
	}
	return Best;
}

void AGielinorCharacter::QueueInteraction(AActor* Target)
{
	IGielinorInteractable* I = Cast<IGielinorInteractable>(Target);
	if (!I) return;
	if (IGielinorInteractable* Old = Cast<IGielinorInteractable>(Pending.Get())) Old->StopInteract(this);
	Combat->ClearTarget();
	Pending = Target;
	if (!AInteractableActor::InRange(this, I, 1.f) && GetController())
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(GetController(), I->GetInteractPoint());
}

void AGielinorCharacter::CancelInteraction()
{
	if (IGielinorInteractable* I = Cast<IGielinorInteractable>(Pending.Get())) I->StopInteract(this);
	Pending = nullptr;
}

void AGielinorCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArm, Dt, 10.f);
	AActor* T = Pending.Get();
	IGielinorInteractable* I = Cast<IGielinorInteractable>(T);
	if (I && AInteractableActor::InRange(this, I, 1.f))
	{
		if (AController* C = GetController()) C->StopMovement();
		FVector To = I->GetInteractPoint() - GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) SetActorRotation(To.Rotation());
		Pending = nullptr; // the interactable now owns any repeating loop
		I->Interact(this);
	}
}
