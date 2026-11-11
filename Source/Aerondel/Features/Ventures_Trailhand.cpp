#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Dom/JsonObject.h"

// Ventures group 25 (web adapt/trailhand.js): a build-a-path card game taught by Trailhand Kes.
// Play onto three trails. Ranks must rise, and a card must match suit or step exactly one rank.
// Not Twenty-One Tides or Crown & Anchor — those stay at the Salted Lantern.

namespace VenturesTrail
{
	static constexpr int32 BetMin = 10, BetMax = 500, Len = 3, Rows = 3, HandN = 5, DeckN = 36;
	static const TCHAR* Suits[] = { TEXT("Sun"), TEXT("Moon"), TEXT("Star"), TEXT("Spark") };
	static const TCHAR* Marks[] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9") };
	static void VAct25(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesTrail;

FString UVenturesFeature::TrailCardLabel(int32 Rank, int32 Suit) const
{
	const int32 R = FMath::Clamp(Rank, 0, 8), S = FMath::Clamp(Suit, 0, 3);
	return FString(Marks[R]) + TEXT("-") + Suits[S];
}

bool UVenturesFeature::TrailLegal(const TArray<FIntPoint>& Row, int32 Rank, int32 Suit)
{
	if (Rank < 0 || Rank > 8 || Suit < 0 || Suit > 3) return false;
	if (Row.Num() <= 0) return true;
	if (Row.Num() >= Len) return false;
	const FIntPoint Last = Row.Last();
	if (Rank <= Last.X) return false;
	return Suit == Last.Y || Rank == Last.X + 1;
}

void UVenturesFeature::ResetTrail()
{
	TrailWins = 0; TrailBest = 0; TrailTokens = 0; TrailBet = 50; TrailSel = -1;
	bTrailLive = false; TrailDone.Reset(); TrailHand.Reset(); TrailRows.Reset();
}

void UVenturesFeature::SaveTrail(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("wins"), TrailWins);
	S->SetNumberField(TEXT("best"), TrailBest);
	S->SetNumberField(TEXT("tokens"), TrailTokens);
	A.SetObjectField(TEXT("trail"), S);
}

void UVenturesFeature::LoadTrail(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("trail"));
	if (!S) return;
	TrailWins = FMath::Max(0, (int32)JInt(*S, TEXT("wins")));
	TrailBest = FMath::Max(0, (int32)JInt(*S, TEXT("best")));
	TrailTokens = FMath::Max(0, (int32)JInt(*S, TEXT("tokens")));
}

void UVenturesFeature::TrailClose()
{
	bTrailLive = false; TrailSel = -1; TrailHand.Reset(); TrailRows.Reset(); TrailDone.Reset();
}

bool UVenturesFeature::TrailDeal()
{
	if (bTrailLive) { Say(TEXT("Finish the hand first.")); return false; }
	TrailBet = FMath::Clamp(TrailBet, BetMin, BetMax);
	if (!TakeCoins(TrailBet)) { Say(FString::Printf(TEXT("Trailhand Kes: A stake of %d coins, or we cannot deal."), TrailBet)); return false; }
	TArray<FIntPoint> Deck;
	for (int32 S = 0; S < 4; ++S) for (int32 R = 0; R < 9; ++R) Deck.Add(FIntPoint(R, S));
	for (int32 i = Deck.Num() - 1; i > 0; --i) { const int32 j = FMath::RandRange(0, i); Deck.Swap(i, j); }
	TrailHand.Reset();
	for (int32 i = 0; i < HandN; ++i) TrailHand.Add(Deck[i]);
	TrailRows.Reset();
	for (int32 i = 0; i < Rows; ++i) TrailRows.AddDefaulted();
	bTrailLive = true; TrailSel = -1; TrailDone.Reset();
	Say(TEXT("Trailhand Kes deals. Ranks must rise, and a card must match the suit or step exactly one rank."));
	return true;
}

void UVenturesFeature::TrailSettle(int32 Mult, const FString& Msg)
{
	const int32 Won = FMath::RoundToInt((float)TrailBet * (float)Mult);
	if (Won > 0)
	{
		GiveCoins(TrailBet + Won);
		TrailWins++;
		ScoreOn(TEXT("trail"));
		TrailTokens++;
		TrailBest = FMath::Max(TrailBest, Won);
		TrailDone = Msg + FString::Printf(TEXT(" You win %d coins."), Won);
	}
	else if (Mult == 0)
	{
		GiveCoins(TrailBet);
		TrailDone = Msg + TEXT(" Your stake comes back.");
	}
	else
	{
		TrailDone = Msg + FString::Printf(TEXT(" You lose %d coins."), TrailBet);
	}
	bTrailLive = false;
	Say(TEXT("Trailhand: ") + TrailDone);
}

bool UVenturesFeature::TrailPlay(int32 HandIndex, int32 Row)
{
	if (!bTrailLive) return false;
	if (!TrailHand.IsValidIndex(HandIndex) || !TrailRows.IsValidIndex(Row)) return false;
	const FIntPoint C = TrailHand[HandIndex];
	if (!TrailLegal(TrailRows[Row], C.X, C.Y)) { Say(TEXT("That card cannot go on that trail.")); return false; }
	TrailRows[Row].Add(C);
	TrailHand.RemoveAt(HandIndex);
	TrailSel = -1;
	bool bFull = true;
	for (const TArray<FIntPoint>& R : TrailRows) if (R.Num() < Len) bFull = false;
	if (bFull)
	{
		bool bSuited = true;
		for (const TArray<FIntPoint>& R : TrailRows)
			for (const FIntPoint& X : R) if (X.Y != R[0].Y) bSuited = false;
		TrailSettle(bSuited ? 2 : 1, bSuited ? TEXT("All three trails suited.") : TEXT("You fill all three trails."));
		return true;
	}
	bool bAny = false;
	for (const FIntPoint& H : TrailHand)
		for (const TArray<FIntPoint>& R : TrailRows)
			if (TrailLegal(R, H.X, H.Y)) bAny = true;
	if (!bAny && TrailHand.Num() > 0) TrailSettle(-1, TEXT("No legal play remains."));
	return true;
}

void UVenturesFeature::TabTrail(FGPanelModel& M)
{
	M.Title = TEXT("Trailhand");
	M.Lines.Add(TEXT("A build-a-path card game taught by Trailhand Kes in Emberfold. Play cards onto three trails. Ranks must rise, and a card must match the suit or step exactly one rank. Fill all three trails to win your stake; all three trails suited pays double. Coins only. This is not Twenty-One Tides or Crown & Anchor."));
	M.Lines.Add(FString::Printf(TEXT("Wins: %d · best payout: %d · hand tokens: %d · stake: %d"), TrailWins, TrailBest, TrailTokens, TrailBet));
	if (!TrailDone.IsEmpty()) M.Lines.Add(TrailDone);
	if (bTrailLive)
	{
		for (int32 r = 0; r < TrailRows.Num(); ++r)
		{
			FString Line = FString::Printf(TEXT("Trail %d (%d/%d):"), r + 1, TrailRows[r].Num(), Len);
			for (const FIntPoint& C : TrailRows[r]) Line += TEXT(" ") + TrailCardLabel(C.X, C.Y);
			if (TrailRows[r].Num() == 0) Line += TEXT(" (empty)");
			M.Lines.Add(Line);
			const int32 Row = r;
			const bool bCan = TrailSel >= 0 && TrailHand.IsValidIndex(TrailSel) && TrailLegal(TrailRows[r], TrailHand[TrailSel].X, TrailHand[TrailSel].Y);
			VAct25(M, FString::Printf(TEXT("Play on trail %d"), r + 1), [this, Row] { TrailPlay(TrailSel, Row); }, bCan);
		}
		FString Hand = TEXT("Hand:");
		for (int32 i = 0; i < TrailHand.Num(); ++i)
		{
			Hand += FString::Printf(TEXT(" [%s%s]"), *TrailCardLabel(TrailHand[i].X, TrailHand[i].Y), i == TrailSel ? TEXT("*") : TEXT(""));
			const int32 Ix = i;
			VAct25(M, FString::Printf(TEXT("Select %s"), *TrailCardLabel(TrailHand[i].X, TrailHand[i].Y)), [this, Ix] { TrailSel = Ix; });
		}
		M.Lines.Add(Hand);
		VAct25(M, TEXT("Fold this hand"), [this] { TrailSettle(-1, TEXT("You fold.")); });
	}
	else
	{
		VAct25(M, TEXT("Lower stake"), [this] { TrailBet = FMath::Max(BetMin, TrailBet - 10); });
		VAct25(M, TEXT("Raise stake"), [this] { TrailBet = FMath::Min(BetMax, TrailBet + 10); });
		VAct25(M, FString::Printf(TEXT("Deal a hand (%d coins)"), TrailBet), [this] { TrailDeal(); });
	}
}
