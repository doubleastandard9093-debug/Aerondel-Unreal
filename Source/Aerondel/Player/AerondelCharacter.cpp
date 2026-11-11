#include "Player/AerondelCharacter.h"
#include "Core/AerondelInputConfig.h"
#include "Interaction/AerondelInteractable.h"
#include "Interaction/InteractableActor.h"
#include "Combat/PlayerCombatComponent.h"
#include "Models/AerondelModelLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/AerondelGamepadComponent.h"
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
#include "TimerManager.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/ItemDef.h"
#include "Features/VenturesFeature.h"
#include "UI/AerondelHUD.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"

AAerondelCharacter::AAerondelCharacter()
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
	// Pivot at head height so tilting the camera up has room before the boom hits the ground and pulls in.
	SpringArm->SetRelativeLocation(FVector(0, 0, 60.f));
	SpringArm->ProbeSize = 8.f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.f;
	SpringArm->bDoCollisionTest = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	Combat = CreateDefaultSubobject<UPlayerCombatComponent>(TEXT("Combat"));
	Gamepad = CreateDefaultSubobject<UAerondelGamepadComponent>(TEXT("Gamepad"));

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Capsule(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Capsule.Succeeded()) PlaceholderBody->SetStaticMesh(Capsule.Object);
	PlaceholderBody->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
}

void AAerondelCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->ClearAllMappings();
			if (UInputMappingContext* IMC = UAerondelInputConfig::Get()->DesktopContext.LoadSynchronous()) Sub->AddMappingContext(IMC, 0);
			// Gamepad layout sits above the desktop context so its keys consume any old gamepad rows there.
			if (Gamepad) if (UInputMappingContext* Pad = Gamepad->GetContext()) Sub->AddMappingContext(Pad, 1);
			BuildVentureKeys();
			if (VentureContext) Sub->AddMappingContext(VentureContext, 2);
		}
}

void AAerondelCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Input);
	const UAerondelInputConfig* Cfg = UAerondelInputConfig::Get();
	if (!EIC || !Cfg) return;
	if (UInputAction* A = Cfg->Move.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Triggered, this, &AAerondelCharacter::OnMove);
	if (UInputAction* A = Cfg->Look.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Triggered, this, &AAerondelCharacter::OnLook);
	if (UInputAction* A = Cfg->Zoom.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Triggered, this, &AAerondelCharacter::OnZoom);
	if (UInputAction* A = Cfg->OrbitHold.LoadSynchronous())
	{
		EIC->BindAction(A, ETriggerEvent::Started, this, &AAerondelCharacter::OnOrbitStart);
		EIC->BindAction(A, ETriggerEvent::Completed, this, &AAerondelCharacter::OnOrbitEnd);
	}
	if (UInputAction* A = Cfg->Interact.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Started, this, &AAerondelCharacter::OnInteract);
	if (UInputAction* A = Cfg->Jump.LoadSynchronous()) EIC->BindAction(A, ETriggerEvent::Started, this, &ACharacter::Jump);
	if (Gamepad) Gamepad->Bind(EIC);
	if (EIC)
	{
		BuildVentureKeys();
		static const TCHAR* Names[] = { TEXT("overflare"), TEXT("ventures"), TEXT("tether"), TEXT("super"), TEXT("dash"), TEXT("settings"), TEXT("ashkin"), TEXT("wordcry"), TEXT("kinbeast") };
		for (int32 i = 0; i < VentureActions.Num() && i < UE_ARRAY_COUNT(Names); ++i)
		{
			const FName K(Names[i]);
			EIC->BindAction(VentureActions[i].Get(), ETriggerEvent::Started, this, &AAerondelCharacter::OnVentureKey, K);
		}
	}
	// Click is bound by AAerondelPlayerController (needs the cursor).
}

void AAerondelCharacter::OnMove(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	if (In.IsNearlyZero()) return;
	CancelInteraction();
	if (AController* C = GetController()) C->StopMovement();
	const FRotator Yaw(0, GetControlRotation().Yaw, 0);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), In.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), In.X);
}

void AAerondelCharacter::OnLook(const FInputActionValue& V)
{
	// Mouse look only while orbiting (RMB held) so the cursor stays free for clicking; gamepad always orbits.
	const FVector2D In = V.Get<FVector2D>();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->bShowMouseCursor && !bOrbiting) return;
	AddControllerYawInput(In.X);
	AddControllerPitchInput(-In.Y * 1.5f); // pitch was much slower than yaw to drag; limits are set on the camera manager
}

void AAerondelCharacter::OnZoom(const FInputActionValue& V)
{
	TargetArm = FMath::Clamp(TargetArm - V.Get<float>() * ZoomStep, MinZoom, MaxZoom);
}

void AAerondelCharacter::OnInteract(const FInputActionValue&)
{
	if (AActor* T = FindNearestInteractable()) QueueInteraction(T);
}

AActor* AAerondelCharacter::FindNearestInteractable(float Radius) const
{
	TArray<FOverlapResult> Hits;
	GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeSphere(Radius));
	AActor* Best = nullptr; float BestScore = TNumericLimits<float>::Max();
	for (const FOverlapResult& H : Hits)
	{
		AActor* A = H.GetActor();
		IAerondelInteractable* I = Cast<IAerondelInteractable>(A);
		if (!I || A == this || !I->CanInteract(const_cast<AAerondelCharacter*>(this))) continue;
		const FVector To = I->GetInteractPoint() - GetActorLocation();
		const float Score = To.Size() * (1.5f - 0.5f * FVector::DotProduct(To.GetSafeNormal2D(), GetActorForwardVector()));
		if (Score < BestScore) { BestScore = Score; Best = A; }
	}
	return Best;
}

void AAerondelCharacter::QueueInteraction(AActor* Target)
{
	IAerondelInteractable* I = Cast<IAerondelInteractable>(Target);
	if (!I) return;
	if (IAerondelInteractable* Old = Cast<IAerondelInteractable>(Pending.Get())) Old->StopInteract(this);
	Combat->ClearTarget();
	Pending = Target;
	if (!AInteractableActor::InRange(this, I, 1.f) && GetController())
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(GetController(), I->GetInteractPoint());
}

void AAerondelCharacter::CancelInteraction()
{
	if (IAerondelInteractable* I = Cast<IAerondelInteractable>(Pending.Get())) I->StopInteract(this);
	Pending = nullptr;
}

void AAerondelCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bRigged && !UAerondelModelLibrary::IsBusy(GetMesh()))
	{
		const float V = GetVelocity().Size2D();
		UAerondelModelLibrary::SetAnimState(GetMesh(), V > 330.f ? FName(TEXT("run")) : V > 20.f ? FName(TEXT("walk")) : FName(TEXT("idle")));
	}
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArm, Dt, 10.f);
	AActor* T = Pending.Get();
	IAerondelInteractable* I = Cast<IAerondelInteractable>(T);
	if (I && AInteractableActor::InRange(this, I, 1.f))
	{
		if (AController* C = GetController()) C->StopMovement();
		FVector To = I->GetInteractPoint() - GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) SetActorRotation(To.Rotation());
		Pending = nullptr; // the interactable now owns any repeating loop
		I->Interact(this);
	}
}

void AAerondelCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Lite rigs.js: the Adventurer stays animated on every tier (Low only drops its shadow / off-screen ticking).
	if (USkeletalMeshComponent* Sk = GetMesh())
	{
		bRigged = UAerondelModelLibrary::ApplyRig(Sk, TEXT("player"));
		UE_LOG(LogTemp, Log, TEXT("[Aerondel] Spawn player %s: rig %s"), *GetName(), bRigged ? TEXT("applied") : TEXT("FAILED, placeholder body"));
		if (bRigged)
		{
			UAerondelModelLibrary::FitRig(Sk, 178.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			Sk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (PlaceholderBody) PlaceholderBody->SetVisibility(false);
			GetWorldTimerManager().SetTimer(GearTintTimer, this, &AAerondelCharacter::RefreshGearTint, 1.f, true);
		}
	}
}

void AAerondelCharacter::RefreshGearTint()
{
	const AAerondelPlayerState* S = GetPlayerState<AAerondelPlayerState>();
	const UItemDef* Body = S && S->Equipment ? S->Equipment->GetEquipped(EEquipSlot::Body) : nullptr;
	FLinearColor Want = Body ? Body->Tint : FLinearColor::White;
	float Blend = 0.35f;
	FLinearColor Dye;
	if (Body && UVenturesFeature::BodyDye(this, Dye)) { Want = Dye; Blend = 0.55f; } // Kitbash Wardrobe body dye
	if (Body == TintedBody.Get() && Want.Equals(AppliedTint)) return;
	TintedBody = Body; AppliedTint = Want;
	const bool bTinted = !Want.Equals(FLinearColor::White);
	// Web gearKind/metalColor tints the armour pieces; one rig mesh here, so a light whole-body blend (0 restores the original colours).
	if (bTinted || UntintedColors.Num()) UAerondelModelLibrary::ApplyTint(GetMesh(), bTinted ? Want : FLinearColor::White, bTinted ? Blend : 0.f, &UntintedColors);
}

void AAerondelCharacter::BuildVentureKeys()
{
	if (VentureContext) return;
	VentureContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Ventures"));
	static const TCHAR* Names[] = { TEXT("IA_VentureFlare"), TEXT("IA_VentureHub"), TEXT("IA_VentureTether"), TEXT("IA_VentureSuper"), TEXT("IA_VentureDash") };
	const FKey Keys[] = { EKeys::U, EKeys::J, EKeys::L, EKeys::N, EKeys::C };
	for (int32 i = 0; i < 5; ++i)
	{
		UInputAction* A = NewObject<UInputAction>(this, FName(Names[i]));
		A->ValueType = EInputActionValueType::Boolean;
		VentureActions.Add(A);
		VentureContext->MapKey(A, Keys[i]);
	}
	// Settings on the keyboard: bound here in C++ so it works without re-running Create Input Assets.
	UInputAction* S = NewObject<UInputAction>(this, FName(TEXT("IA_Settings")));
	S->ValueType = EInputActionValueType::Boolean;
	VentureActions.Add(S);
	VentureContext->MapKey(S, EKeys::F10);
	VentureContext->MapKey(S, EKeys::Escape);
	// Ashkin (H): bound here in C++ so it works without re-running Create Input Assets.
	UInputAction* Ash = NewObject<UInputAction>(this, FName(TEXT("IA_Ashkin")));
	Ash->ValueType = EInputActionValueType::Boolean;
	VentureActions.Add(Ash);
	VentureContext->MapKey(Ash, EKeys::H);
	UInputAction* Cry = NewObject<UInputAction>(this, FName(TEXT("IA_Wordcry")));
	Cry->ValueType = EInputActionValueType::Boolean;
	VentureActions.Add(Cry);
	VentureContext->MapKey(Cry, EKeys::Semicolon);
	// Kinbeasts (K): bound here in C++ so it works without re-running Create Input Assets.
	UInputAction* Kin = NewObject<UInputAction>(this, FName(TEXT("IA_Kinbeast")));
	Kin->ValueType = EInputActionValueType::Boolean;
	VentureActions.Add(Kin);
	VentureContext->MapKey(Kin, EKeys::K);
}

void AAerondelCharacter::OnVentureKey(FName Key)
{
	if (Key == FName(TEXT("settings")))
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (AAerondelHUD* Hud = PC ? Cast<AAerondelHUD>(PC->GetHUD()) : nullptr) Hud->ToggleSettings();
		return;
	}
	if (Key == FName(TEXT("ventures")))
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		AAerondelHUD* Hud = PC ? Cast<AAerondelHUD>(PC->GetHUD()) : nullptr;
		if (!Hud) return;
		if (Hud->GetOpenPanel().ToString().StartsWith(TEXT("ventures"))) Hud->ClosePanel(); else Hud->OpenPanel(TEXT("ventures"), nullptr);
		return;
	}
	if (UVenturesFeature* V = UVenturesFeature::Get(this)) V->OnKey(Key);
}

void AAerondelCharacter::PlayBodyAnim(FName State) { if (bRigged) UAerondelModelLibrary::SetAnimState(GetMesh(), State); }
