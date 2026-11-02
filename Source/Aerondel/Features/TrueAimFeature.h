#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "TrueAimFeature.generated.h"

UENUM(BlueprintType)
enum class EAimShape : uint8 { Cone, Line, Circle };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnAimStrike, EAimShape, Shape, FVector, Origin, float, YawDeg, int32, Hits);

/** True Aim (web aim.js): optional free-aim mode. With it on, attack abilities fire as a shape where you face and hit every foe
 *  inside (max 6, nearest first, each further target -8% damage): cone 4.2 m / 120 degrees, line 8 m x 1.4 m, circle r 3 m
 *  centred 2.5 m ahead. power/snipe/rend/puncture = line, volley/flurry = cone, bash/challenge = circle, others cone.
 *  Strikes roll +15% accuracy and +60% (snipe, rend) / +40% (power) / +10% damage. A dodge-roll inside the cancel window after a
 *  strike (bulwark 200 ms, mender 260, striker 360, no path 240) cancels its recovery. Classic tab-target stays the default.
 *  Plain 2D math, no physics. OnStrike tells the UI / VFX where to flash the ground marker (Low tier: decal only). */
UCLASS()
class AERONDEL_API UTrueAimFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxTargets = 6;

	virtual FString GetSaveKey() const override { return TEXT("aim"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { bOn = false; Strikes = Cancels = 0; LastStrikeMs = 0; }

	UPROPERTY(BlueprintAssignable, Category="Aerondel|Aim") FOnAimStrike OnStrike;

	UFUNCTION(BlueprintCallable, Category="Aerondel|Aim") bool Toggle();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Aim") void SetOn(bool bNewOn);
	UFUNCTION(BlueprintPure, Category="Aerondel|Aim") bool IsOn() const { return bOn; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Aim") static EAimShape ShapeFor(FName AbilityId);
	/** Pure shape test in web metres; Yaw = facing (forward = (sin yaw, cos yaw)); R = target radius. */
	static bool InShape(EAimShape Shape, float Ox, float Oz, float Yaw, float X, float Z, float R = 0.5f);
	/** Fires an aimed ability now. Returns the number of foes hit. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Aim") int32 Strike(FName AbilityId);
	/** Dodge-cancel window for the current Combat Path, ms. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Aim") int32 CancelWindowMs() const;
	/** Call on a dodge-roll: true (and counted) when it cancels a strike's recovery. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Aim") bool TryCancel();
	UFUNCTION(BlueprintPure, Category="Aerondel|Aim") int32 GetStrikes() const { return Strikes; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Aim") int32 GetCancels() const { return Cancels; }

private:
	bool bOn = false;
	int32 Strikes = 0, Cancels = 0;
	int64 LastStrikeMs = 0;
};
