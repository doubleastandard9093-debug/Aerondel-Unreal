#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "ArenaFeature.generated.h"

USTRUCT(BlueprintType)
struct FLadderEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Rating = 1000;
};

/** Bot duellist stats copied from the player's loadout and scaled by rating. */
USTRUCT(BlueprintType)
struct FDuellistStats
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) int32 Hp = 10;
	UPROPERTY(BlueprintReadWrite) int32 MaxHit = 6;
	UPROPERTY(BlueprintReadWrite) int32 Att = 5;
	UPROPERTY(BlueprintReadWrite) int32 Def = 5;
	UPROPERTY(BlueprintReadWrite) int32 Level = 3;
	UPROPERTY(BlueprintReadWrite) bool bRanged = false;
};

/** The Proving Grounds: offline duels vs bot duellists, Elo (K 32), tokens, weekly ladder payout, token shop. */
UCLASS()
class GIELINOR_API UArenaFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	virtual FString GetSaveKey() const override { return TEXT("arena"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Rating = 1000; Tokens = 0; Wins = 0; Losses = 0; Best = 1000; Week = 0; }

	static int64 WeekNo(int64 NowMs) { return NowMs / (7LL * 24 * 3600 * 1000); }
	static TArray<FLadderEntry> Ladder(int64 WeekNumber);
	UFUNCTION(BlueprintPure, Category="Gielinor|Arena") TArray<FLadderEntry> ThisWeeksLadder() const { return Ladder(WeekNo(NowMs())); }
	UFUNCTION(BlueprintPure, Category="Gielinor|Arena") int32 Rank() const;
	/** Pays last week's ladder tokens and halves rating drift. Call when entering the arena. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Arena") FString WeekRoll();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Arena") FLadderEntry PickOpponent() const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Arena") static FDuellistStats DuellistStats(const FDuellistStats& Me, int32 OppRating);
	/** Record a duel result; returns the rating change. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Arena") int32 RateDuel(int32 OppRating, bool bWin);
	/** cape | band | watch | tutor */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Arena") FString Buy(FName ShopId);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Arena") void AddTokens(int32 N) { Tokens = FMath::Clamp(Tokens + N, 0, 1000000); }
	UFUNCTION(BlueprintPure, Category="Gielinor|Arena") int32 GetTokens() const { return Tokens; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Arena") int32 GetRating() const { return Rating; }
private:
	int32 Rating = 1000, Tokens = 0, Wins = 0, Losses = 0, Best = 1000;
	int64 Week = 0;
};
