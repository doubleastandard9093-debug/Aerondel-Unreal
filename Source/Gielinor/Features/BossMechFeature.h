#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "BossMechFeature.generated.h"

class AGielinorMob;

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossStyle, AGielinorMob*, Boss, EBossStyle, Style);

/** Boss mechanics for data bosses (UMobDef::bBoss, not the one-off siege/colossus copies) while they fight you:
 *  telegraphed ground slams every 9 ticks (3-tick warning, r 3.2 m, 30% max HP; roll out or through),
 *  style switches every 14 ticks that call for Protect from Melee / Missiles / Magic (60% less damage from that boss),
 *  and an enrage under 30% HP (hits x1.3, slams every 6 ticks and 25% bigger, switches every 10). */
UCLASS()
class GIELINOR_API UBossMechFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	virtual void OnGameTick() override;
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const override;
	UFUNCTION(BlueprintPure, Category="Gielinor|Boss") bool GetTelegraph(FBossTelegraph& Out) const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Boss") bool GetBossStyle(const AGielinorMob* Boss, EBossStyle& Out) const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Boss") static FName PrayerFor(EBossStyle Style);
	UPROPERTY(BlueprintAssignable, Category="Gielinor|Boss") FOnBossStyle OnStyleChanged;
private:
	struct FState { EBossStyle Style = EBossStyle::Melee; int32 NextSwitch = 0, NextSlam = 0; bool bEnraged = false; };
	TMap<TWeakObjectPtr<AGielinorMob>, FState> States;
	TWeakObjectPtr<AGielinorMob> TeleBoss;
	FVector2D TeleAt = FVector2D::ZeroVector; float TeleR = 0.f; int32 TeleStart = 0, TeleHit = 0;
	int32 Tick = 0;
};
