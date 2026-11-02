#include "Player/AerondelGamepadComponent.h"
#include "Features/TrueAimFeature.h"
#include "Player/AerondelCharacter.h"
#include "Combat/PlayerCombatComponent.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Core/AerondelEvents.h"
#include "Features/FlowFeature.h"
#include "Features/StillwatchFeature.h"
#include "Features/SteedsFeature.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "EngineUtils.h"

static const TCHAR* PadSection = TEXT("Gielinor.Gamepad");

UAerondelGamepadComponent::UAerondelGamepadComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	AbilitySlots = { TEXT("power"), TEXT("puncture"), TEXT("bash"), TEXT("snipe"),
	                 TEXT("volley"), TEXT("fortify"), TEXT("rage"), TEXT("rejuv") };
}

void UAerondelGamepadComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GConfig)
	{
		GConfig->GetFloat(PadSection, TEXT("LookSensitivity"), LookSensitivity, GGameUserSettingsIni);
		GConfig->GetBool(PadSection, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
		GConfig->GetFloat(PadSection, TEXT("Deadzone"), Deadzone, GGameUserSettingsIni);
		GConfig->GetBool(PadSection, TEXT("Rumble"), bRumble, GGameUserSettingsIni);
	}
	LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 3.f);
	Deadzone = FMath::Clamp(Deadzone, 0.f, 0.6f);
}

void UAerondelGamepadComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AAerondelPlayerState* PS = AAerondelPlayerState::FromActor(GetOwner()))
		if (PS->Health) PS->Health->OnHealthChanged.RemoveDynamic(this, &UAerondelGamepadComponent::OnHealth);
	if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(StartHoldTimer);
	Super::EndPlay(Reason);
}

void UAerondelGamepadComponent::SaveSettings() const
{
	if (!GConfig) return;
	GConfig->SetFloat(PadSection, TEXT("LookSensitivity"), LookSensitivity, GGameUserSettingsIni);
	GConfig->SetBool(PadSection, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->SetFloat(PadSection, TEXT("Deadzone"), Deadzone, GGameUserSettingsIni);
	GConfig->SetBool(PadSection, TEXT("Rumble"), bRumble, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

AAerondelCharacter* UAerondelGamepadComponent::Ch() const { return Cast<AAerondelCharacter>(GetOwner()); }
APlayerController* UAerondelGamepadComponent::PC() const
{
	const APawn* P = Cast<APawn>(GetOwner());
	return P ? Cast<APlayerController>(P->GetController()) : nullptr;
}

void UAerondelGamepadComponent::MarkPad()
{
	// First pad input hides the cursor; the controller shows it again when the mouse moves.
	bUsingPad = true;
	if (APlayerController* C = PC()) C->bShowMouseCursor = false;
}

void UAerondelGamepadComponent::Rumble(float Intensity, float Seconds)
{
	if (!bRumble || !bUsingPad) return;
	if (APlayerController* C = PC()) C->PlayDynamicForceFeedback(Intensity, Seconds, true, true, true, true);
}

UInputAction* UAerondelGamepadComponent::MakeAction(const TCHAR* Name, bool bAxis2D, bool bAxis1D)
{
	UInputAction* A = NewObject<UInputAction>(this, FName(Name));
	A->ValueType = bAxis2D ? EInputActionValueType::Axis2D : bAxis1D ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean;
	A->bConsumeInput = true; // blocks old gamepad rows in the lower-priority IMC_Desktop
	Actions.Add(FName(Name), A);
	return A;
}

void UAerondelGamepadComponent::BuildAssets()
{
	if (Context) return;
	Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Gamepad_Runtime"));
	auto Map = [this](const TCHAR* Name, const FKey& Key, bool b2D = false, bool b1D = false)
	{
		UInputAction* A = MakeAction(Name, b2D, b1D);
		Context->MapKey(A, Key);
	};
	Map(TEXT("Pad_Move"), EKeys::Gamepad_Left2D, true);
	Map(TEXT("Pad_Look"), EKeys::Gamepad_Right2D, true);
	Map(TEXT("Pad_ZoomOut"), EKeys::Gamepad_LeftTriggerAxis, false, true);
	Map(TEXT("Pad_ZoomIn"), EKeys::Gamepad_RightTriggerAxis, false, true);
	Map(TEXT("Pad_RT"), EKeys::Gamepad_RightTrigger);
	Map(TEXT("Pad_A"), EKeys::Gamepad_FaceButton_Bottom);
	Map(TEXT("Pad_B"), EKeys::Gamepad_FaceButton_Right);
	Map(TEXT("Pad_X"), EKeys::Gamepad_FaceButton_Left);
	Map(TEXT("Pad_Y"), EKeys::Gamepad_FaceButton_Top);
	Map(TEXT("Pad_LB"), EKeys::Gamepad_LeftShoulder);
	Map(TEXT("Pad_RB"), EKeys::Gamepad_RightShoulder);
	Map(TEXT("Pad_DUp"), EKeys::Gamepad_DPad_Up);
	Map(TEXT("Pad_DRight"), EKeys::Gamepad_DPad_Right);
	Map(TEXT("Pad_DDown"), EKeys::Gamepad_DPad_Down);
	Map(TEXT("Pad_DLeft"), EKeys::Gamepad_DPad_Left);
	Map(TEXT("Pad_L3"), EKeys::Gamepad_LeftThumbstick);
	Map(TEXT("Pad_R3"), EKeys::Gamepad_RightThumbstick);
	Map(TEXT("Pad_View"), EKeys::Gamepad_Special_Left);
	Map(TEXT("Pad_Start"), EKeys::Gamepad_Special_Right);
}

UInputMappingContext* UAerondelGamepadComponent::GetContext() { BuildAssets(); return Context; }

void UAerondelGamepadComponent::Bind(UEnhancedInputComponent* EIC)
{
	if (!EIC) return;
	BuildAssets();
	// Called after possession, so the PlayerState (and its Health) exists now.
	if (AAerondelPlayerState* PS = AAerondelPlayerState::FromActor(GetOwner()))
		if (PS->Health) PS->Health->OnHealthChanged.AddUniqueDynamic(this, &UAerondelGamepadComponent::OnHealth);
	using T = ETriggerEvent;
	auto B = [&](const TCHAR* Name, T Ev, void (UAerondelGamepadComponent::*Fn)(const FInputActionValue&))
	{
		if (TObjectPtr<UInputAction>* A = Actions.Find(FName(Name))) EIC->BindAction(A->Get(), Ev, this, Fn);
	};
	B(TEXT("Pad_Move"), T::Triggered, &UAerondelGamepadComponent::OnMove);
	B(TEXT("Pad_Look"), T::Triggered, &UAerondelGamepadComponent::OnLook);
	B(TEXT("Pad_ZoomOut"), T::Triggered, &UAerondelGamepadComponent::OnZoomOut);
	B(TEXT("Pad_ZoomIn"), T::Triggered, &UAerondelGamepadComponent::OnZoomIn);
	B(TEXT("Pad_RT"), T::Started, &UAerondelGamepadComponent::OnRTStart);
	B(TEXT("Pad_A"), T::Started, &UAerondelGamepadComponent::OnA);
	B(TEXT("Pad_B"), T::Started, &UAerondelGamepadComponent::OnB);
	B(TEXT("Pad_X"), T::Started, &UAerondelGamepadComponent::OnX);
	B(TEXT("Pad_Y"), T::Started, &UAerondelGamepadComponent::OnY);
	B(TEXT("Pad_LB"), T::Started, &UAerondelGamepadComponent::OnLBDown);
	B(TEXT("Pad_LB"), T::Completed, &UAerondelGamepadComponent::OnLBUp);
	B(TEXT("Pad_RB"), T::Started, &UAerondelGamepadComponent::OnRB);
	B(TEXT("Pad_DUp"), T::Started, &UAerondelGamepadComponent::OnDUp);
	B(TEXT("Pad_DRight"), T::Started, &UAerondelGamepadComponent::OnDRight);
	B(TEXT("Pad_DDown"), T::Started, &UAerondelGamepadComponent::OnDDown);
	B(TEXT("Pad_DLeft"), T::Started, &UAerondelGamepadComponent::OnDLeft);
	B(TEXT("Pad_L3"), T::Started, &UAerondelGamepadComponent::OnL3);
	B(TEXT("Pad_R3"), T::Started, &UAerondelGamepadComponent::OnR3);
	B(TEXT("Pad_View"), T::Started, &UAerondelGamepadComponent::OnView);
	B(TEXT("Pad_Start"), T::Started, &UAerondelGamepadComponent::OnStartDown);
	B(TEXT("Pad_Start"), T::Completed, &UAerondelGamepadComponent::OnStartUp);
}

static FVector2D RadialDeadzone(FVector2D In, float Dz)
{
	const float Len = In.Size();
	if (Len <= Dz) return FVector2D::ZeroVector;
	return In / Len * FMath::Min(1.f, (Len - Dz) / FMath::Max(0.01f, 1.f - Dz));
}

void UAerondelGamepadComponent::OnMove(const FInputActionValue& V)
{
	AAerondelCharacter* C = Ch();
	const FVector2D In = RadialDeadzone(V.Get<FVector2D>(), Deadzone);
	if (!C || In.IsNearlyZero()) return;
	MarkPad();
	C->CancelInteraction();
	if (AController* Ctl = C->GetController()) Ctl->StopMovement();
	const FRotator Yaw(0, C->GetControlRotation().Yaw, 0);
	C->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), In.Y);
	C->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), In.X);
}

void UAerondelGamepadComponent::OnLook(const FInputActionValue& V)
{
	AAerondelCharacter* C = Ch();
	const FVector2D In = RadialDeadzone(V.Get<FVector2D>(), Deadzone);
	if (!C || In.IsNearlyZero()) return;
	MarkPad();
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const float K = LookRate * LookSensitivity * Dt;
	C->AddControllerYawInput(In.X * K);
	C->AddControllerPitchInput((bInvertY ? In.Y : -In.Y) * K);
}

void UAerondelGamepadComponent::OnZoomOut(const FInputActionValue& V)
{
	if (AAerondelCharacter* C = Ch()) { MarkPad(); C->AddZoom(V.Get<float>() * ZoomRate * GetWorld()->GetDeltaSeconds()); }
}

void UAerondelGamepadComponent::OnZoomIn(const FInputActionValue& V)
{
	if (bLBHeld) return; // LB+RT is True Aim, not zoom
	if (AAerondelCharacter* C = Ch()) { MarkPad(); C->AddZoom(-V.Get<float>() * ZoomRate * GetWorld()->GetDeltaSeconds()); }
}

void UAerondelGamepadComponent::OnRTStart(const FInputActionValue&)
{
	if (!bLBHeld) return;
	MarkPad();
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UTrueAimFeature* Aim = GI ? GI->GetSubsystem<UTrueAimFeature>() : nullptr) bTrueAim = Aim->Toggle(); // saved, logs the hint
	else bTrueAim = !bTrueAim;
	OnTrueAimToggled.Broadcast(bTrueAim);
}

void UAerondelGamepadComponent::OnA(const FInputActionValue&)
{
	MarkPad();
	if (AAerondelCharacter* C = Ch()) if (AActor* T = C->FindNearestInteractable()) C->QueueInteraction(T);
}

void UAerondelGamepadComponent::OnX(const FInputActionValue&)
{
	MarkPad();
	AAerondelCharacter* C = Ch();
	if (!C || !GetWorld()) return;
	AAerondelMob* Best = nullptr; float BestD = AttackRadiusCm * AttackRadiusCm;
	for (TActorIterator<AAerondelMob> It(GetWorld()); It; ++It)
	{
		AAerondelMob* M = *It;
		if (!M || !M->CanInteract(C)) continue;
		const float D = FVector::DistSquared(M->GetActorLocation(), C->GetActorLocation());
		if (D < BestD) { BestD = D; Best = M; }
	}
	if (Best) { C->CancelInteraction(); Best->Interact(C); }
}

void UAerondelGamepadComponent::OnB(const FInputActionValue&)
{
	MarkPad();
	AAerondelCharacter* C = Ch();
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFlowFeature* Flow = GI ? GI->GetSubsystem<UFlowFeature>() : nullptr;
	if (!C || !Flow || !Flow->TryDodge()) return;
	if (UTrueAimFeature* Aim = GI->GetSubsystem<UTrueAimFeature>()) if (Aim->TryCancel()) C->StopAnimMontage(); // dodge-cancel an aimed strike
	FVector Dir = C->GetLastMovementInputVector().GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = C->GetActorForwardVector().GetSafeNormal2D();
	// ~0.25 s roll covering DodgeDistCm.
	C->LaunchCharacter(Dir * (UFlowFeature::DodgeDistCm / 0.25f), true, false);
	Rumble(0.35f, 0.12f);
}

void UAerondelGamepadComponent::OnY(const FInputActionValue&) { MarkPad(); OnMenuRequested.Broadcast(EAerondelPadMenu::ContextMenu); }
void UAerondelGamepadComponent::OnView(const FInputActionValue&) { MarkPad(); OnMenuRequested.Broadcast(EAerondelPadMenu::WorldMap); }

void UAerondelGamepadComponent::OnRB(const FInputActionValue&)
{
	MarkPad();
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UStillwatchFeature* S = GI ? GI->GetSubsystem<UStillwatchFeature>() : nullptr)
		if (!S->Trigger(false))
			if (UAerondelEvents* Ev = UAerondelEvents::Get(this)) Ev->Message(NSLOCTEXT("Aerondel", "StillNotReady", "Stillwatch isn't ready."));
}

void UAerondelGamepadComponent::UseSlot(int32 Base)
{
	MarkPad();
	const int32 Idx = Base + (bLBHeld ? 4 : 0);
	AAerondelCharacter* C = Ch();
	if (!C || !C->Combat || !AbilitySlots.IsValidIndex(Idx) || AbilitySlots[Idx].IsNone()) return;
	const FText Msg = C->Combat->UseAbility(AbilitySlots[Idx]);
	if (!Msg.IsEmpty()) if (UAerondelEvents* Ev = UAerondelEvents::Get(this)) Ev->Message(Msg);
}

void UAerondelGamepadComponent::OnL3(const FInputActionValue&)
{
	MarkPad();
	if (AAerondelPlayerState* PS = AAerondelPlayerState::FromActor(GetOwner()))
		if (PS->Stats) PS->Stats->bRunning = !PS->Stats->bRunning;
}

void UAerondelGamepadComponent::OnR3(const FInputActionValue&)
{
	MarkPad();
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	USteedsFeature* S = GI ? GI->GetSubsystem<USteedsFeature>() : nullptr;
	if (!S) return;
	if (S->IsMounted()) { S->Dismount(); return; }
	const TArray<FName> Owned = S->GetOwned();
	if (Owned.Num() == 0 || !S->Summon(Owned.Last()))
		if (UAerondelEvents* Ev = UAerondelEvents::Get(this)) Ev->Message(NSLOCTEXT("Aerondel", "NoSteed", "You have no steed to summon."));
}

void UAerondelGamepadComponent::OnStartDown(const FInputActionValue&)
{
	MarkPad();
	bStartHoldFired = false;
	if (UWorld* W = GetWorld())
		W->GetTimerManager().SetTimer(StartHoldTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bStartHoldFired = true;
			OnMenuRequested.Broadcast(EAerondelPadMenu::Settings);
		}), 0.5f, false);
}

void UAerondelGamepadComponent::OnStartUp(const FInputActionValue&)
{
	if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(StartHoldTimer);
	if (!bStartHoldFired) OnMenuRequested.Broadcast(EAerondelPadMenu::Inventory);
}

void UAerondelGamepadComponent::OnHealth(int32 Cur, int32 Max, int32 Delta)
{
	if (Delta < 0) Rumble(FMath::Clamp(0.3f + 0.7f * (-Delta) / FMath::Max(1, Max) * 4.f, 0.3f, 1.f), 0.18f);
}
