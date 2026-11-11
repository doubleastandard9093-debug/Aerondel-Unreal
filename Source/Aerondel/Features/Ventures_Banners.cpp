#include "Features/VenturesFeature.h"
#include "Features/StandingFeature.h"
#include "UI/AerondelPanels.h"
#include "Dom/JsonObject.h"

// Ventures group 27 (web adapt/banners.js): a late writ for a Trusted (or higher) banner can
// cool its opposite. A Wary banner will not honor writs and holds back some specialty stock.
// Larkhaven, the bank, everyday stalls and the Wanderer's Saga are never locked.
// Mend a Wary banner for 250 coins.

namespace VenturesBanners
{
	static constexpr int32 MendCost = 250, Trusted = 2, Specialty = 200;
	static const TPair<const TCHAR*, const TCHAR*> Pairs[] = {
		{ TEXT("watch"), TEXT("mariners") }, { TEXT("mariners"), TEXT("watch") },
		{ TEXT("circle"), TEXT("lantern") }, { TEXT("lantern"), TEXT("circle") }
	};
	static const TCHAR* SafeShops[] = { TEXT("shop"), TEXT("tools"), TEXT("apothecary"), TEXT("magic0"), TEXT("herb") };
	struct FShopFac { const TCHAR* Shop; const TCHAR* Fac; };
	static const FShopFac ShopFac[] = {
		{ TEXT("emberfold"), TEXT("watch") },
		{ TEXT("general2"), TEXT("mariners") }, { TEXT("magic"), TEXT("mariners") }, { TEXT("chandlery"), TEXT("mariners") }, { TEXT("armsBrine"), TEXT("mariners") },
		{ TEXT("weapons"), TEXT("circle") }, { TEXT("general3"), TEXT("circle") }, { TEXT("runes"), TEXT("circle") },
		{ TEXT("armour"), TEXT("lantern") }, { TEXT("herb2"), TEXT("lantern") }
	};
	struct FTown { const TCHAR* Name; float X, Z; };
	static const FTown Towns[] = {
		{ TEXT("Larkhaven"), 0.f, 0.f }, { TEXT("Emberfold"), 50.f, -24.f }, { TEXT("Saltreach"), -80.f, 60.f },
		{ TEXT("Highcrest"), 120.f, -40.f }, { TEXT("Whitecairn"), -60.f, -80.f }
	};
	static bool IsSafe(FName ShopId)
	{
		const FString S = ShopId.ToString();
		for (const TCHAR* K : SafeShops) if (S.Equals(K, ESearchCase::IgnoreCase)) return true;
		return false;
	}
	static void VAct27(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesBanners;

FString UVenturesFeature::BannerTownAt(float X, float Z) const
{
	const TCHAR* Best = TEXT("Larkhaven");
	float Bd = 1e12f;
	for (const FTown& T : Towns)
	{
		const float D = FMath::Hypot(X - T.X, Z - T.Z);
		if (D < Bd) { Bd = D; Best = T.Name; }
	}
	return Best;
}

FName UVenturesFeature::BannerOpposite(FName Faction) const
{
	const FString Id = Faction.ToString().ToLower();
	for (const auto& P : Pairs) if (Id == P.Key) return FName(P.Value);
	return NAME_None;
}

void UVenturesFeature::ResetBanners()
{
	BannerCrosses.Reset();
	BannerWarySet.Reset();
}

void UVenturesFeature::SaveBanners(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Cross;
	for (const FString& C : BannerCrosses) Cross.Add(MakeShared<FJsonValueString>(C));
	S->SetArrayField(TEXT("crosses"), Cross);
	TSharedRef<FJsonObject> W = MakeShared<FJsonObject>();
	for (const FName& F : BannerWarySet) W->SetNumberField(F.ToString(), 1);
	S->SetObjectField(TEXT("wary"), W);
	A.SetObjectField(TEXT("banners"), S);
}

void UVenturesFeature::LoadBanners(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("banners"));
	if (!S) return; // old saves are not made Wary
	BannerCrosses = JStrArr(*S, TEXT("crosses"));
	if (const TSharedPtr<FJsonObject> W = JObj(*S, TEXT("wary")))
		for (const auto& P : W->Values) if (JInt(*W, *P.Key) > 0) BannerWarySet.Add(FName(*P.Key));
}

bool UVenturesFeature::BannerWary(FName Faction) const
{
	return BannerWarySet.Contains(Faction) || BannerWarySet.Contains(FName(*Faction.ToString().ToLower()));
}

bool UVenturesFeature::BannerMend(FName Faction)
{
	if (!BannerWary(Faction)) { Say(TEXT("That banner is already in good courtesy.")); return false; }
	if (!TakeCoins(MendCost)) { Say(FString::Printf(TEXT("Mending a banner costs %d coins."), MendCost)); return false; }
	BannerWarySet.Remove(Faction);
	BannerWarySet.Remove(FName(*Faction.ToString().ToLower()));
	Say(TEXT("The banner is mended. Writs will be honored again, and specialty stock returns."));
	return true;
}

void UVenturesFeature::BannerOnWrit(FName Faction)
{
	const UStandingFeature* St = Feature<UStandingFeature>();
	if (!St) return;
	if (St->TierFor(Faction) < Trusted) return;
	const FName Opp = BannerOpposite(Faction);
	if (Opp.IsNone() || BannerWary(Opp)) return;
	BannerWarySet.Add(Opp);
	const FString Cross = Faction.ToString() + TEXT("_") + Opp.ToString();
	BannerCrosses.AddUnique(Cross);
	Say(TEXT("A late deed has cooled the opposite banner. They will not honor writs and will hold back some specialty stock until you mend the banner (250 coins). Larkhaven, the bank and the Saga stay open."));
}

bool UVenturesFeature::BannerHideStock(FName ShopId, const AActor* Shop, int32 Price) const
{
	if (IsSafe(ShopId)) return false;
	if (Price < Specialty) return false;
	FVector2D Web(0.f, 0.f);
	if (Shop)
	{
		const FVector L = Shop->GetActorLocation();
		Web = FVector2D(L.X / 100.f, L.Y / 100.f);
	}
	const FString Town = BannerTownAt(Web.X, Web.Y);
	if (Town == TEXT("Larkhaven")) return false; // Larkhaven is never locked
	FName Fac = NAME_None;
	const FString Sid = ShopId.ToString();
	for (const FShopFac& E : ShopFac) if (Sid.Equals(E.Shop, ESearchCase::IgnoreCase)) { Fac = FName(E.Fac); break; }
	if (Fac.IsNone())
		if (const UStandingFeature* St = Feature<UStandingFeature>()) Fac = St->FactionForTown(Town);
	if (Fac.IsNone()) return false;
	return BannerWary(Fac);
}

void UVenturesFeature::TabBanners(FGPanelModel& M)
{
	M.Title = TEXT("Crossed Banners");
	M.Lines.Add(TEXT("Standing still rises as before. A late writ for a Trusted (or higher) banner can cool its opposite. A Wary banner will not honor writs and holds back some specialty stock. Larkhaven, the bank, everyday stalls and the Wanderer's Saga are never locked. Mend a Wary banner for 250 coins."));
	if (const UStandingFeature* St = Feature<UStandingFeature>())
	{
		for (const FFactionDef& F : UStandingFeature::Factions())
		{
			if (F.Id == FName(TEXT("drowned"))) continue; // no opposite pair
			const bool bWary = BannerWary(F.Id);
			M.Lines.Add(FString::Printf(TEXT("%s — %s%s"), *F.Name, *UStandingFeature::TierName(St->TierFor(F.Id)), bWary ? TEXT("  (Wary)") : TEXT("")));
			if (bWary) VAct27(M, FString::Printf(TEXT("Mend %s (%d coins)"), *F.Name, MendCost), [this, Id = F.Id] { BannerMend(Id); });
		}
	}
	if (BannerCrosses.Num())
	{
		FString Line = TEXT("Crossed deeds:");
		for (const FString& C : BannerCrosses) Line += TEXT(" ") + C;
		M.Lines.Add(Line);
	}
	else M.Lines.Add(TEXT("No crossed deeds yet. Completing a writ for a Trusted (or higher) banner can cool its opposite."));
	VAct27(M, TEXT("Claim standing rewards"), [this] { if (UStandingFeature* St = Feature<UStandingFeature>()) St->ClaimTierRewards(); });
}
