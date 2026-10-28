#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "FlowFeature.generated.h"

UENUM(BlueprintType)
enum class EFlowFinisher : uint8 { None, Bleeding, Piercing, Chain };

USTRUCT(BlueprintType)
struct FFlowHit
{
	GENERATED_BODY()
	/** Extra damage fraction for this hit (+8% per link). */
	UPROPERTY(BlueprintReadOnly) float Bonus = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Links = 0;
	UPROPERTY(BlueprintReadOnly) EFlowFinisher Finisher = EFlowFinisher::None;
	UPROPERTY(BlueprintReadOnly) bool bRepeat = false;
};

/** Flow Strikes + dodge-roll (Phase 2, deepened in the mechanics pass): chaining different abilities within 6 ticks
 *  builds links; the 3rd link fires a style finisher (melee bleed, ranged pierce, magic chain). A roll costs 15 run energy
 *  and has i-frames; rolling through a hit (perfect dodge) adds a link and feeds Stillwatch. Abilities/combat call these. */
UCLASS()
class GIELINOR_API UFlowFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 WindowTicks = 6;
	static constexpr float PerLink = 0.08f;
	static constexpr int32 MaxLinks = 3;
	static constexpr float FinisherBonus = 0.5f;
	static constexpr float DodgeCost = 15.f;
	static constexpr int64 DodgeCdMs = 2000;
	static constexpr int64 IframeMs = 420;
	static constexpr float DodgeDistCm = 420.f;

	virtual FString GetSaveKey() const override { return TEXT("flowBest"); }
	virtual void SaveState(FJsonObject& Out) const override { Out.SetNumberField(TEXT("best"), Best); }
	virtual void LoadState(const FJsonObject& In) override { Best = FMath::Clamp((int32)JInt(In, TEXT("best")), 0, MaxLinks); }
	virtual void ResetState() override { Links = 0; LastTick = -99; Best = 0; DodgeReadyAt = IframeUntil = 0; Used.Reset(); }

	/** Call when a combat ability fires. Style: melee | ranged | magic. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Flow") FFlowHit OnSkillHit(int32 Tick, FName AbilityId, FName Style);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Flow") int32 OnPerfectDodge(int32 Tick);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Flow") void Decay(int32 Tick) { if (Links && Tick - LastTick > WindowTicks) Links = 0; }
	/** Spend run energy and start i-frames. BonusMs: extra i-frame time (Agility synergy, quirks). Returns false if refused. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Flow") bool TryDodge(int32 BonusMs = 0);
	UFUNCTION(BlueprintPure, Category="Gielinor|Flow") bool IsDodging() const { return NowMs() < IframeUntil; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Flow") int32 GetLinks() const { return Links; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Flow") int32 GetBest() const { return Best; }

private:
	int32 Links = 0, LastTick = -99, Best = 0;
	int64 DodgeReadyAt = 0, IframeUntil = 0;
	TArray<FName> Used;
};
