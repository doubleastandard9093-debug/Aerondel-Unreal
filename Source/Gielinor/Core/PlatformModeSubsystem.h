#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "PlatformModeSubsystem.generated.h"

UENUM(BlueprintType)
enum class EGielinorPlayMode : uint8 { Desktop, VR };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayModeChanged, EGielinorPlayMode, NewMode);

/**
 * Detects a connected/active HMD and possesses the matching pawn (desktop Character or VR pawn).
 * Progression stays on AGielinorPlayerState, so swapping pawns keeps all state.
 * Polls twice a second so plugging in / taking off a headset (Link, SteamVR) switches live.
 * Launch flags: -vr forces VR, -novr forces desktop.
 */
UCLASS()
class GIELINOR_API UPlatformModeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Gielinor|XR") bool IsVR() const { return Mode == EGielinorPlayMode::VR; }
	UFUNCTION(BlueprintPure, Category="Gielinor|XR") EGielinorPlayMode GetMode() const { return Mode; }
	UFUNCTION(BlueprintCallable, Category="Gielinor|XR") void ForceMode(EGielinorPlayMode NewMode);
	UFUNCTION(BlueprintCallable, Category="Gielinor|XR") void ClearForcedMode() { bForced = false; }

	UPROPERTY(BlueprintAssignable) FOnPlayModeChanged OnModeChanged;

	static bool DetectHMD();

private:
	bool Tick(float DeltaTime);
	void ApplyMode(EGielinorPlayMode NewMode);
	void SwapPawn();

	EGielinorPlayMode Mode = EGielinorPlayMode::Desktop;
	bool bForced = false;
	FTSTicker::FDelegateHandle TickHandle;
	float Accum = 0.f;
};
