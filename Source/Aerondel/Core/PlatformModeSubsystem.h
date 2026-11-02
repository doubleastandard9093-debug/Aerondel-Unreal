#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "PlatformModeSubsystem.generated.h"

UENUM(BlueprintType)
enum class EAerondelPlayMode : uint8 { Desktop, VR };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayModeChanged, EAerondelPlayMode, NewMode);

/**
 * Detects a connected/active HMD and possesses the matching pawn (desktop Character or VR pawn).
 * Progression stays on AAerondelPlayerState, so swapping pawns keeps all state.
 * Polls twice a second so plugging in / taking off a headset (Link, SteamVR) switches live.
 * Launch flags: -vr forces VR, -novr forces desktop.
 */
UCLASS()
class AERONDEL_API UPlatformModeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Aerondel|XR") bool IsVR() const { return Mode == EAerondelPlayMode::VR; }
	UFUNCTION(BlueprintPure, Category="Aerondel|XR") EAerondelPlayMode GetMode() const { return Mode; }
	UFUNCTION(BlueprintCallable, Category="Aerondel|XR") void ForceMode(EAerondelPlayMode NewMode);
	UFUNCTION(BlueprintCallable, Category="Aerondel|XR") void ClearForcedMode() { bForced = false; }

	UPROPERTY(BlueprintAssignable) FOnPlayModeChanged OnModeChanged;

	static bool DetectHMD();

private:
	bool Tick(float DeltaTime);
	void ApplyMode(EAerondelPlayMode NewMode);
	void SwapPawn();

	EAerondelPlayMode Mode = EAerondelPlayMode::Desktop;
	bool bForced = false;
	FTSTicker::FDelegateHandle TickHandle;
	float Accum = 0.f;
};
