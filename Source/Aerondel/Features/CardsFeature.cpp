#include "Features/CardsFeature.h"
#include "Features/SailingFeature.h"

static const TCHAR* GCrownNames[9] = { TEXT("High card"), TEXT("A pair"), TEXT("Two pair"), TEXT("Three of a kind"), TEXT("A straight"), TEXT("A flush"), TEXT("A full house"), TEXT("Four of a kind"), TEXT("A straight flush") };
const TCHAR* UCardsFeature::CrownName(int32 Cat) { return GCrownNames[FMath::Clamp(Cat, 0, 8)]; }

int32 UCardsFeature::TideTotal(const TArray<FPlayingCard>& Hand)
{
	int32 Total = 0, Aces = 0;
	for (const FPlayingCard& C : Hand) { if (C.Rank == 12) { ++Aces; Total += 11; } else Total += C.Rank >= 8 ? 10 : C.Rank + 2; }
	while (Total > 21 && Aces) { Total -= 10; --Aces; }
	return Total;
}

TArray<int32> UCardsFeature::CrownRank(const TArray<FPlayingCard>& Hand)
{
	TArray<int32> Rs; for (const FPlayingCard& C : Hand) Rs.Add(C.Rank);
	Rs.Sort([](int32 A, int32 B) { return A > B; });
	TMap<int32, int32> Cnt; for (int32 R : Rs) Cnt.FindOrAdd(R)++;
	TArray<TPair<int32, int32>> Groups; for (const auto& P : Cnt) Groups.Add(TPair<int32, int32>(P.Value, P.Key));
	Groups.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Key != B.Key ? A.Key > B.Key : A.Value > B.Value; });
	bool bFlush = true; for (const FPlayingCard& C : Hand) if (C.Suit != Hand[0].Suit) bFlush = false;
	int32 StraightHi = -1;
	if (Cnt.Num() == 5 && Rs.Num() == 5) { if (Rs[0] - Rs[4] == 4) StraightHi = Rs[0]; else if (Rs[0] == 12 && Rs[1] == 3 && Rs[2] == 2 && Rs[3] == 1 && Rs[4] == 0) StraightHi = 3; }
	TArray<int32> Kick; for (const auto& G : Groups) Kick.Add(G.Value);
	auto With = [](int32 Cat, const TArray<int32>& Rest) { TArray<int32> O; O.Add(Cat); O.Append(Rest); return O; };
	const int32 G0 = Groups.Num() > 0 ? Groups[0].Key : 0, G1 = Groups.Num() > 1 ? Groups[1].Key : 0;
	if (StraightHi >= 0 && bFlush) return { 8, StraightHi };
	if (G0 == 4) return With(7, Kick);
	if (G0 == 3 && G1 == 2) return With(6, Kick);
	if (bFlush) return With(5, Rs);
	if (StraightHi >= 0) return { 4, StraightHi };
	if (G0 == 3) return With(3, Kick);
	if (G0 == 2 && G1 == 2) return With(2, Kick);
	if (G0 == 2) return With(1, Kick);
	return With(0, Rs);
}

int32 UCardsFeature::CmpRank(const TArray<int32>& A, const TArray<int32>& B)
{
	for (int32 i = 0; i < FMath::Max(A.Num(), B.Num()); ++i)
	{
		const int32 D = (A.IsValidIndex(i) ? A[i] : 0) - (B.IsValidIndex(i) ? B[i] : 0);
		if (D) return D;
	}
	return 0;
}

TArray<int32> UCardsFeature::DealerHolds(const TArray<FPlayingCard>& Hand)
{
	const TArray<int32> Rk = CrownRank(Hand);
	TArray<int32> Out;
	if (Rk[0] >= 4) { for (int32 i = 0; i < Hand.Num(); ++i) Out.Add(i); return Out; }
	TMap<int32, int32> Cnt; for (const FPlayingCard& C : Hand) Cnt.FindOrAdd(C.Rank)++;
	if (Rk[0] >= 1) { for (int32 i = 0; i < Hand.Num(); ++i) if (Cnt[Hand[i].Rank] > 1) Out.Add(i); return Out; }
	TArray<int32> Idx; for (int32 i = 0; i < Hand.Num(); ++i) Idx.Add(i);
	Idx.Sort([&Hand](int32 A, int32 B) { return Hand[A].Rank > Hand[B].Rank; });
	Out.Add(Idx[0]); if (Idx.Num() > 1) Out.Add(Idx[1]);
	return Out;
}

void UCardsFeature::Open(ECardGame Game, const FString& DealerName)
{
	const int32 Bet = T.Bet;
	T = FCardTable(); T.bOpen = true; T.Game = Game; T.Dealer = DealerName; T.Bet = Bet;
	T.Title = Game == ECardGame::Tides ? TEXT("Twenty-One Tides") : TEXT("Crown & Anchor");
	T.Msg = Game == ECardGame::Tides
		? TEXT("Get closer to 21 than the house without going over. The house stands on 17. Twenty-one on the deal pays 3 to 2.")
		: TEXT("Five-card draw against the house. Hold what you like, draw once; best hand wins. A full house or better pays double.");
}

void UCardsFeature::Close() { const int32 Bet = T.Bet; T = FCardTable(); T.Bet = Bet; Deck.Reset(); }
void UCardsFeature::BetDown() { if (!T.bLive) T.Bet = FMath::Max(BetMin, T.Bet - (T.Bet > 100 ? 50 : 10)); }
void UCardsFeature::BetUp() { if (!T.bLive) T.Bet = FMath::Min(BetMax, T.Bet + (T.Bet >= 100 ? 50 : 10)); }

void UCardsFeature::Settle(float Mult, const FString& Why)
{
	const int32 Won = FMath::RoundToInt(T.Stake * Mult);
	if (Won > 0) GiveCoins(T.Stake + Won); else if (Mult == 0.f) GiveCoins(T.Stake);
	if (USailingFeature* S = Feature<USailingFeature>()) { if (Mult > 0.f) S->AddCardResult(true); else if (Mult < 0.f) S->AddCardResult(false); }
	T.bLive = false;
	T.Result = Why + (Mult > 0.f ? FString::Printf(TEXT(" You win %d coins."), Won) : Mult == 0.f ? FString(TEXT(" Push: your stake comes back.")) : FString::Printf(TEXT(" You lose %d coins."), T.Stake));
	T.YouTotal = TideTotal(T.You); T.HouseTotal = TideTotal(T.House);
	Say(T.Title + TEXT(": ") + T.Result);
}

void UCardsFeature::Deal()
{
	if (!T.bOpen || T.bLive) return;
	T.Msg.Reset(); T.Result.Reset();
	if (!TakeCoins(T.Bet)) { T.Msg = FString::Printf(TEXT("You need %d coins to sit in."), T.Bet); return; }
	Deck.Reset();
	for (int32 s = 0; s < 4; ++s) for (int32 r = 0; r < 13; ++r) { FPlayingCard C; C.Rank = r; C.Suit = s; Deck.Add(C); }
	for (int32 i = Deck.Num() - 1; i > 0; --i) Deck.Swap(i, FMath::RandRange(0, i));
	T.Stake = T.Bet; T.bLive = true; T.You.Reset(); T.House.Reset(); T.Held.Init(false, 5);
	if (T.Game == ECardGame::Tides)
	{
		T.You = { Pop(), Pop() }; T.House = { Pop(), Pop() };
		if (TideTotal(T.You) == 21) { const bool bH = TideTotal(T.House) == 21; Settle(bH ? 0.f : 1.5f, bH ? TEXT("Both of you hold twenty-one.") : TEXT("Twenty-one on the deal!")); }
	}
	else for (int32 i = 0; i < 5; ++i) { T.You.Add(Pop()); T.House.Add(Pop()); }
	T.YouTotal = TideTotal(T.You);
}

void UCardsFeature::HouseTides()
{
	while (TideTotal(T.House) < 17) T.House.Add(Pop());
	const int32 Y = TideTotal(T.You), H = TideTotal(T.House);
	if (H > 21) Settle(1.f, FString::Printf(TEXT("The house busts on %d."), H));
	else if (Y > H) Settle(1.f, FString::Printf(TEXT("%d beats %d."), Y, H));
	else if (Y == H) Settle(0.f, FString::Printf(TEXT("Both on %d."), Y));
	else Settle(-1.f, FString::Printf(TEXT("The house's %d beats your %d."), H, Y));
}

void UCardsFeature::Hit()
{
	if (!T.bLive || T.Game != ECardGame::Tides) return;
	T.You.Add(Pop()); T.YouTotal = TideTotal(T.You);
	if (T.YouTotal > 21) Settle(-1.f, FString::Printf(TEXT("Bust on %d."), T.YouTotal));
	else if (T.YouTotal == 21) HouseTides();
}

void UCardsFeature::Stand() { if (T.bLive && T.Game == ECardGame::Tides) HouseTides(); }

void UCardsFeature::Double()
{
	if (!T.bLive || T.Game != ECardGame::Tides || T.You.Num() != 2) return;
	if (!TakeCoins(T.Stake)) return;
	T.Stake *= 2; T.You.Add(Pop()); T.YouTotal = TideTotal(T.You);
	if (T.YouTotal > 21) Settle(-1.f, FString::Printf(TEXT("Bust on %d."), T.YouTotal)); else HouseTides();
}

void UCardsFeature::ToggleHold(int32 i) { if (T.bLive && T.Game == ECardGame::Crown && T.Held.IsValidIndex(i)) T.Held[i] = !T.Held[i]; }

void UCardsFeature::Draw()
{
	if (!T.bLive || T.Game != ECardGame::Crown) return;
	for (int32 i = 0; i < 5; ++i) if (!T.Held[i]) T.You[i] = Pop();
	const TArray<int32> Keep = DealerHolds(T.House);
	for (int32 i = 0; i < 5; ++i) if (!Keep.Contains(i)) T.House[i] = Pop();
	const TArray<int32> Y = CrownRank(T.You), H = CrownRank(T.House);
	const int32 D = CmpRank(Y, H);
	Settle(D > 0 ? (Y[0] >= 6 ? 2.f : 1.f) : D == 0 ? 0.f : -1.f, FString::Printf(TEXT("%s against the house's %s."), CrownName(Y[0]), *FString(CrownName(H[0])).ToLower()));
}

FCardTable UCardsFeature::GetTable() const
{
	FCardTable Out = T;
	if (T.bLive)
		for (int32 i = 0; i < Out.House.Num(); ++i)
			if (T.Game == ECardGame::Crown || i > 0) Out.House[i].Rank = -1;
	if (T.bLive) Out.HouseTotal = 0;
	return Out;
}
