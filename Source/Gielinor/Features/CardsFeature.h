#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "CardsFeature.generated.h"

USTRUCT(BlueprintType)
struct FPlayingCard
{
	GENERATED_BODY()
	/** 0 = '2' ... 8 = '10', 9 J, 10 Q, 11 K, 12 A. */
	UPROPERTY(BlueprintReadOnly) int32 Rank = 0;
	/** 0 anchor, 1 shell, 2 skull, 3 compass (shell and compass print red). */
	UPROPERTY(BlueprintReadOnly) int32 Suit = 0;
};

UENUM(BlueprintType)
enum class ECardGame : uint8 { Tides, Crown };

/** Table state for the UMG card widget. */
USTRUCT(BlueprintType)
struct FCardTable
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bOpen = false;
	UPROPERTY(BlueprintReadOnly) ECardGame Game = ECardGame::Tides;
	UPROPERTY(BlueprintReadOnly) FString Title;
	UPROPERTY(BlueprintReadOnly) FString Dealer;
	UPROPERTY(BlueprintReadOnly) int32 Bet = 50;
	/** Stake on the current hand (doubles on Double). */
	UPROPERTY(BlueprintReadOnly) int32 Stake = 0;
	UPROPERTY(BlueprintReadOnly) bool bLive = false;
	UPROPERTY(BlueprintReadOnly) TArray<FPlayingCard> You;
	/** While live, hidden house cards are reported with Rank -1 (Tides shows the first card). */
	UPROPERTY(BlueprintReadOnly) TArray<FPlayingCard> House;
	UPROPERTY(BlueprintReadOnly) TArray<bool> Held;
	UPROPERTY(BlueprintReadOnly) int32 YouTotal = 0;
	UPROPERTY(BlueprintReadOnly) int32 HouseTotal = 0;
	UPROPERTY(BlueprintReadOnly) FString Result;
	UPROPERTY(BlueprintReadOnly) FString Msg;
};

/** Brineholt card tables at the Salted Lantern (Sails of Saltreach), played for coins only. Twenty-One Tides: blackjack-style,
 *  house stands on 17, twenty-one on the deal pays 3:2, Double on two cards. Crown & Anchor: five-card draw against the house,
 *  hold then draw once; the house keeps pairs or better (all five on a straight or better), else its top two; a full house or
 *  better pays double. Bets 10-500. Wins and losses are tallied on the vessel save (USailingFeature). */
UCLASS()
class GIELINOR_API UCardsFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 BetMin = 10, BetMax = 500;

	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void Open(ECardGame Game, const FString& DealerName);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void Close();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void BetDown();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void BetUp();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void Deal();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void Hit();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void Stand();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void Double();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void ToggleHold(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Cards") void Draw();
	UFUNCTION(BlueprintPure, Category="Gielinor|Cards") FCardTable GetTable() const;

	static int32 TideTotal(const TArray<FPlayingCard>& Hand);
	/** [category, tiebreakers...]: 8 straight flush ... 0 high card. */
	static TArray<int32> CrownRank(const TArray<FPlayingCard>& Hand);
	static int32 CmpRank(const TArray<int32>& A, const TArray<int32>& B);
	static TArray<int32> DealerHolds(const TArray<FPlayingCard>& Hand);
	static const TCHAR* CrownName(int32 Cat);

private:
	FPlayingCard Pop() { return Deck.Pop(); }
	void Settle(float Mult, const FString& Why);
	void HouseTides();
	FCardTable T;
	TArray<FPlayingCard> Deck;
};
