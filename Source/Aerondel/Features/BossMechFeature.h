#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "BossMechFeature.generated.h"

class AAerondelMob;

UENUM(BlueprintType)
enum class EBossStyle : uint8 { Melee, Ranged, Magic };

/** A telegraphed ground slam for the UI/VFX to draw (red ring; on Low just the ring, no filling disc). */
USTRUCT(BlueprintType)
struct FBossTelegraph
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector; // world cm
	UPROPERTY(BlueprintReadOnly) float RadiusCm = 0.f;
	/** 0 at wind-up, 1 when it lands. */
	UPROPERTY(BlueprintReadOnly) float Fill = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossStyle, AAerondelMob*, Boss, EBossStyle, Style);

/** Boss mechanics for data bosses (UMobDef::bBoss, not the one-off siege/colossus copies) while they fight you:
 *  telegraphed ground slams every 9 ticks (3-tick warning, r 3.2 m, 30% max HP; roll out or through),
 *  style switches every 14 ticks that call for Ward against Melee / Missiles / Magic (60% less damage from that boss),
 *  and an enrage under 30% HP (hits x1.3, slams every 6 ticks and 25% bigger, switches every 10). */
UCLASS()
class AERONDEL_API UBossMechFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual void OnGameTick() override;
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const override;
	UFUNCTION(BlueprintPure, Category="Aerondel|Boss") bool GetTelegraph(FBossTelegraph& Out) const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Boss") bool GetBossStyle(const AAerondelMob* Boss, EBossStyle& Out) const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Boss") static FName PrayerFor(EBossStyle Style);
	UPROPERTY(BlueprintAssignable, Category="Aerondel|Boss") FOnBossStyle OnStyleChanged;
private:
	struct FState { EBossStyle Style = EBossStyle::Melee; int32 NextSwitch = 0, NextSlam = 0; bool bEnraged = false; };
	TMap<TWeakObjectPtr<AAerondelMob>, FState> States;
	TWeakObjectPtr<AAerondelMob> TeleBoss;
	FVector2D TeleAt = FVector2D::ZeroVector; float TeleR = 0.f; int32 TeleStart = 0, TeleHit = 0;
	int32 Tick = 0;
};
