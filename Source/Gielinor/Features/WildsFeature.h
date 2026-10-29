#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "WildsFeature.generated.h"

class AGielinorMob;

USTRUCT(BlueprintType)
struct FWildsBanner
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector; // world cm (ground Z left at 0; snap in the level)
	UPROPERTY(BlueprintReadOnly) bool bYours = false;
	/** Capture progress 0..1 while you stand at it with no rivals near. */
	UPROPERTY(BlueprintReadOnly) float Progress = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWildsBanner, int32, Index, bool, bYours);

/** Contested Wilds: three banners in the scorched wilderness fought over with the rival Ashen Company (offline bots).
 *  Stand within 3 m of a banner for 8 ticks with no raider within 6 m to raise the Watch's colours (+15 Watch).
 *  Every 100 ticks each held banner pays 2 arena tokens and 5 Watch standing. Every 60 ticks raiders respawn at rival
 *  banners and may retake (40%) a banner you are more than 15 m from. Raider pool: 3 on Quest/Low, 6 otherwise. */
UCLASS()
class GIELINOR_API UWildsFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	virtual FString GetSaveKey() const override { return TEXT("wilds"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { for (bool& H : Held) H = false; Captures = 0; }
	virtual void OnGameTick() override;
	virtual void Deinitialize() override { DespawnRaiders(); Super::Deinitialize(); }

	UFUNCTION(BlueprintPure, Category="Gielinor|Wilds") TArray<FWildsBanner> GetBanners() const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Wilds") int32 HeldCount() const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Wilds") int32 GetCaptures() const { return Captures; }
	/** 3 on Quest/Low tiers (phones/standalone VR), 6 on High/Ultra. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Wilds") static int32 BotCap();
	UPROPERTY(BlueprintAssignable, Category="Gielinor|Wilds") FOnWildsBanner OnBannerChanged;
private:
	static FVector2D BannerPos(int32 i);
	void DespawnRaiders();
	bool Held[3] = { false, false, false };
	int32 Prog[3] = { 0, 0, 0 };
	int32 Captures = 0, Tick = 0, LastRaid = -100000, LastPay = 0;
	TArray<TWeakObjectPtr<AGielinorMob>> Raiders;
};
