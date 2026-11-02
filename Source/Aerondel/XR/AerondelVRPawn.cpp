#include "XR/AerondelVRPawn.h"
#include "XR/VRHandComponent.h"
#include "Core/AerondelInputConfig.h"
#include "Combat/PlayerCombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AAerondelVRPawn::AAerondelVRPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled; // UPlatformModeSubsystem decides.

	Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
	Body->InitCapsuleSize(25.f, 88.f);
	Body->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = Body;

	// Origin sits at the floor (capsule centre is HalfHeight above it).
	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	VROrigin->SetupAttachment(Body);
	VROrigin->SetRelativeLocation(FVector(0, 0, -88.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
	Camera->bLockToHmd = true;

	auto MakeMC = [this](const TCHAR* Name, FName Source) {
		UMotionControllerComponent* MC = CreateDefaultSubobject<UMotionControllerComponent>(Name);
		MC->SetupAttachment(VROrigin);
		MC->SetTrackingMotionSource(Source);
		return MC;
	};
	LeftGrip = MakeMC(TEXT("LeftGrip"), FName(TEXT("Left")));
	RightGrip = MakeMC(TEXT("RightGrip"), FName(TEXT("Right")));
	LeftAim = MakeMC(TEXT("LeftAim"), FName(TEXT("LeftAim")));
	RightAim = MakeMC(TEXT("RightAim"), FName(TEXT("RightAim")));

	LeftHand = CreateDefaultSubobject<UVRHandComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(LeftGrip);
	LeftHand->Hand = EControllerHand::Left;
	RightHand = CreateDefaultSubobject<UVRHandComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(RightGrip);
	RightHand->Hand = EControllerHand::Right;

	Laser = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("Laser"));
	Laser->SetupAttachment(RightAim);
	Laser->InteractionDistance = 500.f;
	Laser->bShowDebug = false;
	Laser->PointerIndex = 0;

	WristPanel = CreateDefaultSubobject<UWidgetComponent>(TEXT("WristPanel"));
	WristPanel->SetupAttachment(LeftGrip);
	WristPanel->SetRelativeLocation(FVector(5, 8, 6));
	WristPanel->SetRelativeRotation(FRotator(0, 180, 70));
	WristPanel->SetDrawSize(FVector2D(600, 800));
	WristPanel->SetRelativeScale3D(FVector(0.03f));
	WristPanel->SetWidgetSpace(EWidgetSpace::World);
	WristPanel->SetVisibility(false);

	Vignette = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Vignette"));
	Vignette->SetupAttachment(Body);
	Vignette->bUnbound = true;
	Vignette->Settings.bOverride_VignetteIntensity = true;
	Vignette->Settings.VignetteIntensity = 0.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ArcDots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArcDots"));
	ArcDots->SetupAttachment(RootComponent);
	ArcDots->SetUsingAbsoluteLocation(true); ArcDots->SetUsingAbsoluteRotation(true); ArcDots->SetUsingAbsoluteScale(true);
	ArcDots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArcDots->SetCastShadow(false);
	if (Sphere.Succeeded()) ArcDots->SetStaticMesh(Sphere.Object);
	TeleportMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportMarker"));
	TeleportMarker->SetupAttachment(RootComponent);
	TeleportMarker->SetUsingAbsoluteLocation(true); TeleportMarker->SetUsingAbsoluteRotation(true);
	TeleportMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TeleportMarker->SetCastShadow(false);
	if (Cyl.Succeeded()) TeleportMarker->SetStaticMesh(Cyl.Object);
	TeleportMarker->SetWorldScale3D(FVector(0.6f, 0.6f, 0.02f));
	TeleportMarker->SetVisibility(false);

	Combat = CreateDefaultSubobject<UPlayerCombatComponent>(TEXT("Combat"));
	Combat->bAutoApproach = false; // never move the player's view for them
}

void AAerondelVRPawn::BeginPlay()
{
	Super::BeginPlay();
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
	if (WristWidgetClass) WristPanel->SetWidgetClass(WristWidgetClass);
}

void AAerondelVRPawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->ClearAllMappings();
			if (UInputMappingContext* IMC = UAerondelInputConfig::Get()->XRContext.LoadSynchronous()) Sub->AddMappingContext(IMC, 0);
		}
}

void AAerondelVRPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	UEnhancedInputComponent* E = Cast<UEnhancedInputComponent>(Input);
	const UAerondelInputConfig* C = UAerondelInputConfig::Get();
	if (!E || !C) return;
	if (UInputAction* A = C->XRMove.LoadSynchronous()) { E->BindAction(A, ETriggerEvent::Triggered, this, &AAerondelVRPawn::OnMove); E->BindAction(A, ETriggerEvent::Completed, this, &AAerondelVRPawn::OnMoveDone); }
	if (UInputAction* A = C->XRTurn.LoadSynchronous()) { E->BindAction(A, ETriggerEvent::Triggered, this, &AAerondelVRPawn::OnTurn); E->BindAction(A, ETriggerEvent::Completed, this, &AAerondelVRPawn::OnTurnDone); }
	if (UInputAction* A = C->XRTeleport.LoadSynchronous()) { E->BindAction(A, ETriggerEvent::Triggered, this, &AAerondelVRPawn::OnTeleportAim); E->BindAction(A, ETriggerEvent::Completed, this, &AAerondelVRPawn::OnTeleportRelease); }
	if (UInputAction* A = C->XRGrabLeft.LoadSynchronous()) { E->BindAction(A, ETriggerEvent::Started, this, &AAerondelVRPawn::OnGripL); E->BindAction(A, ETriggerEvent::Completed, this, &AAerondelVRPawn::OnGripLEnd); }
	if (UInputAction* A = C->XRGrabRight.LoadSynchronous()) { E->BindAction(A, ETriggerEvent::Started, this, &AAerondelVRPawn::OnGripR); E->BindAction(A, ETriggerEvent::Completed, this, &AAerondelVRPawn::OnGripREnd); }
	if (UInputAction* A = C->XRTriggerLeft.LoadSynchronous()) E->BindAction(A, ETriggerEvent::Started, this, &AAerondelVRPawn::OnTriggerL);
	if (UInputAction* A = C->XRTriggerRight.LoadSynchronous()) { E->BindAction(A, ETriggerEvent::Started, this, &AAerondelVRPawn::OnTriggerR); E->BindAction(A, ETriggerEvent::Completed, this, &AAerondelVRPawn::OnTriggerREnd); }
	if (UInputAction* A = C->XRMenu.LoadSynchronous()) E->BindAction(A, ETriggerEvent::Started, this, &AAerondelVRPawn::OnMenu);
}

void AAerondelVRPawn::OnMove(const FInputActionValue& V) { MoveInput = V.Get<FVector2D>(); }

void AAerondelVRPawn::OnTurn(const FInputActionValue& V)
{
	const float X = V.Get<float>();
	if (bSnapTurn)
	{
		if (bSnapArmed && FMath::Abs(X) > 0.7f) { RotateAroundHead(FMath::Sign(X) * SnapAngle); bSnapArmed = false; }
		else if (FMath::Abs(X) < 0.3f) bSnapArmed = true;
	}
	else TurnInput = X;
}

void AAerondelVRPawn::RotateAroundHead(float Degrees)
{
	// Rotate the pawn so the HMD position stays fixed in the world.
	const FVector Head = Camera->GetComponentLocation();
	const FRotator NewRot = GetActorRotation() + FRotator(0, Degrees, 0);
	const FVector Offset = Head - GetActorLocation();
	const FVector Rotated = FRotator(0, Degrees, 0).RotateVector(Offset);
	SetActorLocationAndRotation(Head - Rotated, NewRot);
}

void AAerondelVRPawn::OnTeleportRelease(const FInputActionValue&)
{
	bAimingTeleport = false;
	ArcDots->ClearInstances();
	TeleportMarker->SetVisibility(false);
	if (!bTeleportValid) return;
	// Keep the player's head over the target: offset by the HMD's horizontal offset from the capsule.
	FVector HeadOffset = Camera->GetComponentLocation() - GetActorLocation(); HeadOffset.Z = 0;
	SetActorLocation(TeleportTarget + FVector(0, 0, Body->GetScaledCapsuleHalfHeight()) - HeadOffset, false, nullptr, ETeleportType::TeleportPhysics);
	bTeleportValid = false;
}

void AAerondelVRPawn::UpdateTeleportArc()
{
	FPredictProjectilePathParams P(4.f, RightAim->GetComponentLocation(), RightAim->GetForwardVector() * TeleportLaunchSpeed, 2.f, ECC_WorldStatic, this);
	P.SimFrequency = 15.f;
	FPredictProjectilePathResult R;
	const bool bHit = UGameplayStatics::PredictProjectilePath(this, P, R);
	ArcDots->ClearInstances();
	for (int32 i = 0; i < R.PathData.Num(); i += 2)
		ArcDots->AddInstance(FTransform(FRotator::ZeroRotator, R.PathData[i].Location, FVector(0.05f)), true);

	bTeleportValid = false;
	if (bHit)
	{
		FNavLocation Nav;
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (NavSys && NavSys->ProjectPointToNavigation(R.HitResult.Location, Nav, FVector(50, 50, 100)))
		{
			bTeleportValid = true;
			TeleportTarget = Nav.Location;
		}
	}
	TeleportMarker->SetVisibility(bTeleportValid);
	if (bTeleportValid) TeleportMarker->SetWorldLocation(TeleportTarget + FVector(0, 0, 2));
}

void AAerondelVRPawn::ApplyVignette(float Dt, bool bMoving)
{
	const float Target = (bVignette && bMoving) ? VignetteMax : 0.f;
	VignetteCurrent = FMath::FInterpTo(VignetteCurrent, Target, Dt, 8.f);
	Vignette->Settings.VignetteIntensity = VignetteCurrent;
}

void AAerondelVRPawn::Tick(float Dt)
{
	Super::Tick(Dt);
	bool bMoving = false;
	if (!MoveInput.IsNearlyZero(0.15f))
	{
		// Head-relative smooth locomotion, swept so walls stop you (no tunnelling: small sub-steps).
		const FRotator Yaw(0, Camera->GetComponentRotation().Yaw, 0);
		const FVector Dir = Yaw.RotateVector(FVector(MoveInput.Y, MoveInput.X, 0)).GetClampedToMaxSize(1.f);
		FVector Delta = Dir * MoveSpeed * Dt;
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Delta.Size() / 10.f));
		for (int32 i = 0; i < Steps; ++i)
		{
			FHitResult Hit;
			AddActorWorldOffset(Delta / Steps, true, &Hit);
			if (Hit.bBlockingHit) { AddActorWorldOffset(FVector::VectorPlaneProject(Delta / Steps, Hit.Normal) * (1.f - Hit.Time), true); }
		}
		// Snap to ground below (simple gravity substitute; NavMesh/terrain stays walkable).
		FHitResult Ground;
		const FVector Start = GetActorLocation();
		if (GetWorld()->LineTraceSingleByChannel(Ground, Start, Start - FVector(0, 0, 400), ECC_Visibility, FCollisionQueryParams(NAME_None, false, this)))
			SetActorLocation(Ground.Location + FVector(0, 0, Body->GetScaledCapsuleHalfHeight()));
		bMoving = true;
	}
	if (!bSnapTurn && FMath::Abs(TurnInput) > 0.2f) { RotateAroundHead(TurnInput * SmoothTurnDegPerSec * Dt); bMoving = true; }
	ApplyVignette(Dt, bMoving);
	if (bAimingTeleport) UpdateTeleportArc();
}

void AAerondelVRPawn::OnGripL(const FInputActionValue&) { LeftHand->GripPressed(); }
void AAerondelVRPawn::OnGripLEnd(const FInputActionValue&) { LeftHand->GripReleased(); }
void AAerondelVRPawn::OnGripR(const FInputActionValue&) { RightHand->GripPressed(); }
void AAerondelVRPawn::OnGripREnd(const FInputActionValue&) { RightHand->GripReleased(); }
void AAerondelVRPawn::OnTriggerL(const FInputActionValue&) { LeftHand->TriggerPressed(); }

void AAerondelVRPawn::OnTriggerR(const FInputActionValue&)
{
	if (Laser->IsOverInteractableWidget()) Laser->PressPointerKey(EKeys::LeftMouseButton);
	else RightHand->TriggerPressed();
}

void AAerondelVRPawn::OnTriggerREnd(const FInputActionValue&) { Laser->ReleasePointerKey(EKeys::LeftMouseButton); }

void AAerondelVRPawn::OnMenu(const FInputActionValue&) { WristPanel->SetVisibility(!WristPanel->IsVisible()); }
