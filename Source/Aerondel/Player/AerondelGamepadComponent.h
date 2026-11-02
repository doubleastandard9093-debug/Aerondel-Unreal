#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AerondelGamepadComponent.generated.h"

class UInputAction; class UInputMappingContext; class UEnhancedInputComponent; class AAerondelCharacter;
struct FInputActionValue;

/** Menus the pad asks the UI to open. UMG listens on OnMenuRequested and calls SetUserFocus on its first button. */
UENUM(BlueprintType)
enum class EAerondelPadMenu : uint8 { Inventory, Settings, WorldMap, ContextMenu };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPadMenu, EAerondelPadMenu, Menu);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPadBool, bool, bOn);

/**
 * Xbox / XInput gamepad layout, mirroring the web build. Builds its own Input Actions + Mapping Context in C++
 * at runtime (no editor assets needed) and adds it ABOVE IMC_Desktop, so keyboard/mouse and the VR context are
 * untouched and these gamepad keys win over any old gamepad rows left in IMC_Desktop.
 *
 * LS move | RS camera | A interact | X attack nearest | B dodge-roll | Y context menu
 * D-pad ability slots 1-4 (hold LB: 5-8) | RB Stillwatch | LT/RT zoom out/in | LB+RT True Aim
 * L3 run toggle | R3 mount/dismount | View world map | Start inventory (hold 0.5 s: settings)
 * Rumble on taking a hit and on a dodge. Sensitivity / invert Y / deadzone / rumble persist in GameUserSettings.ini.
 */
UCLASS(ClassGroup=(Aerondel), meta=(BlueprintSpawnableComponent))
class AERONDEL_API UAerondelGamepadComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAerondelGamepadComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gamepad") float LookSensitivity = 1.f;  // 0.2 .. 3
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gamepad") bool bInvertY = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gamepad") float Deadzone = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gamepad") bool bRumble = true;
	/** Yaw/pitch degrees per second at full stick and sensitivity 1. */
	UPROPERTY(EditAnywhere, Category="Gamepad") float LookRate = 160.f;
	UPROPERTY(EditAnywhere, Category="Gamepad") float ZoomRate = 900.f;
	UPROPERTY(EditAnywhere, Category="Gamepad") float AttackRadiusCm = 1500.f;
	/** Quick-bar ability ids for slots 1-8 (same order as the web quick bar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gamepad") TArray<FName> AbilitySlots;

	UPROPERTY(BlueprintAssignable, Category="Gamepad") FOnPadMenu OnMenuRequested;
	/** True Aim has no C++ system yet: the UI / aim code listens here. */
	UPROPERTY(BlueprintAssignable, Category="Gamepad") FOnPadBool OnTrueAimToggled;
	UFUNCTION(BlueprintPure, Category="Gamepad") bool IsTrueAim() const { return bTrueAim; }
	UFUNCTION(BlueprintPure, Category="Gamepad") bool IsUsingGamepad() const { return bUsingPad; }
	UFUNCTION(BlueprintCallable, Category="Gamepad") void SaveSettings() const;
	UFUNCTION(BlueprintCallable, Category="Gamepad") void Rumble(float Intensity, float Seconds);

	UInputMappingContext* GetContext();
	void Bind(UEnhancedInputComponent* EIC);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UInputAction* MakeAction(const TCHAR* Name, bool bAxis2D, bool bAxis1D = false);
	void BuildAssets();
	AAerondelCharacter* Ch() const;
	class APlayerController* PC() const;
	void MarkPad();

	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnZoomIn(const FInputActionValue& V);
	void OnZoomOut(const FInputActionValue& V);
	void OnRTStart(const FInputActionValue& V);
	void OnA(const FInputActionValue& V);
	void OnX(const FInputActionValue& V);
	void OnB(const FInputActionValue& V);
	void OnY(const FInputActionValue& V);
	void OnLBDown(const FInputActionValue& V) { bLBHeld = true; }
	void OnLBUp(const FInputActionValue& V) { bLBHeld = false; }
	void OnRB(const FInputActionValue& V);
	void OnDUp(const FInputActionValue& V) { UseSlot(0); }
	void OnDRight(const FInputActionValue& V) { UseSlot(1); }
	void OnDDown(const FInputActionValue& V) { UseSlot(2); }
	void OnDLeft(const FInputActionValue& V) { UseSlot(3); }
	void OnL3(const FInputActionValue& V);
	void OnR3(const FInputActionValue& V);
	void OnView(const FInputActionValue& V);
	void OnStartDown(const FInputActionValue& V);
	void OnStartUp(const FInputActionValue& V);
	void UseSlot(int32 Base);
	UFUNCTION() void OnHealth(int32 Cur, int32 Max, int32 Delta);

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Context;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputAction>> Actions;
	bool bLBHeld = false, bTrueAim = false, bUsingPad = false, bStartHoldFired = false;
	double StartDownAt = 0.0;
	FTimerHandle StartHoldTimer;
};
