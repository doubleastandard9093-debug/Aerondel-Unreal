#include "Features/WildsFeature.h"
#include "Features/StandingFeature.h"
#include "Features/ArenaFeature.h"
#include "Combat/GielinorMob.h"
#include "World/GielinorScalability.h"
#include "Dom/JsonValue.h"

namespace WildsData
{
	static const FVector2D Center(122, -122); static constexpr float Near = 70.f;
	static const FVector2D Flags[3] = { FVector2D(-11, -6), FVector2D(10, -8), FVector2D(0, 11) };
	static const TCHAR* Rival = TEXT("Ashen Company"); static const TCHAR* Mon = TEXT("Ashen raider");
	static constexpr int32 CapTicks = 8, PayEvery = 100, PayTokens = 2, PayRep = 5, RaidEvery = 60, CaptureRep = 15;
	static constexpr float ClearR = 6.f, CaptureR = 3.f, Retake = 0.4f, GuardR = 15.f;
}

FVector2D UWildsFeature::BannerPos(int32 i) { return WildsData::Center + WildsData::Flags[i]; }

int32 UWildsFeature::BotCap()
{
	const EGielinorTier T = UGielinorScalability::GetSavedTier();
	return (T == EGielinorTier::Quest || T == EGielinorTier::Low) ? 3 : 6;
}

int32 UWildsFeature::HeldCount() const { int32 N = 0; for (bool H : Held) N += H ? 1 : 0; return N; }

TArray<FWildsBanner> UWildsFeature::GetBanners() const
{
	TArray<FWildsBanner> Out;
	for (int32 i = 0; i < 3; ++i) { FWildsBanner B; const FVector2D P = BannerPos(i); B.Location = WebToWorld(P.X, P.Y); B.bYours = Held[i]; B.Progress = (float)Prog[i] / WildsData::CapTicks; Out.Add(B); }
	return Out;
}

void UWildsFeature::DespawnRaiders()
{
	for (const TWeakObjectPtr<AGielinorMob>& M : Raiders) if (M.IsValid()) M->Destroy();
	Raiders.Reset();
}

void UWildsFeature::OnGameTick()
{
	using namespace WildsData;
	++Tick;
	FVector2D P;
	if (!PlayerWebPos(P) || FVector2D::Distance(P, Center) > Near) { if (Raiders.Num()) DespawnRaiders(); return; }
	UStandingFeature* St = Feature<UStandingFeature>();
	Raiders.RemoveAll([](const TWeakObjectPtr<AGielinorMob>& M) { return !M.IsValid() || M->IsDead(); });
	// Raids: retake unguarded banners, refill the raider pool at rival banners.
	if (Tick - LastRaid >= RaidEvery || Raiders.Num() == 0)
	{
		LastRaid = Tick;
		for (int32 i = 0; i < 3; ++i)
			if (Held[i] && FVector2D::Distance(P, BannerPos(i)) > GuardR && FMath::FRand() < Retake)
			{ Held[i] = false; OnBannerChanged.Broadcast(i, false); Say(FString::Printf(TEXT("The %s retake a banner in the wilds!"), Rival)); }
		TArray<int32> RivalFlags; for (int32 i = 0; i < 3; ++i) if (!Held[i]) RivalFlags.Add(i);
		for (int32 k = Raiders.Num(); RivalFlags.Num() && k < BotCap(); ++k)
		{
			const float A = FMath::FRand() * 2.f * PI;
			const FVector2D At = BannerPos(RivalFlags[k % RivalFlags.Num()]) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 2.5f;
			if (AGielinorMob* M = SpawnTunedMob(Mon, At, Mon, 420, 38, 32, 28, 40)) Raiders.Add(M);
		}
	}
	// Capture.
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector2D F = BannerPos(i);
		const bool bNear = FVector2D::Distance(P, F) < CaptureR;
		bool bRivals = false;
		for (const TWeakObjectPtr<AGielinorMob>& M : Raiders)
			if (M.IsValid() && !M->IsDead()) { const FVector L = M->GetActorLocation(); if (FVector2D::Distance(FVector2D(L.X / 100.0, L.Y / 100.0), F) < ClearR) bRivals = true; }
		if (bNear && bRivals && !Held[i] && Tick % 5 == 0) Say(FString::Printf(TEXT("Clear the %s from the banner to claim it."), Rival));
		if (Held[i] || !bNear || bRivals) { Prog[i] = 0; continue; }
		if (++Prog[i] >= CapTicks)
		{
			Held[i] = true; ++Captures; Prog[i] = 0; OnBannerChanged.Broadcast(i, true);
			Say(FString::Printf(TEXT("You raise the Watch's colours over a banner! (%d/3 held)"), HeldCount()));
			if (St) St->AddRep(TEXT("watch"), CaptureRep);
		}
	}
	// Pay.
	if (Tick - LastPay >= PayEvery)
	{
		LastPay = Tick;
		if (const int32 N = HeldCount())
		{
			if (UArenaFeature* A = Feature<UArenaFeature>()) A->AddTokens(N * PayTokens);
			if (St) St->AddRep(TEXT("watch"), N * PayRep);
			Say(FString::Printf(TEXT("Your %d wilds banner%s pay %d arena tokens and %d Watch standing."), N, N > 1 ? TEXT("s") : TEXT(""), N * PayTokens, N * PayRep));
		}
	}
}

void UWildsFeature::SaveState(FJsonObject& Out) const
{
	TArray<TSharedPtr<FJsonValue>> A;
	for (bool H : Held) A.Add(MakeShared<FJsonValueString>(H ? TEXT("you") : TEXT("rival")));
	Out.SetArrayField(TEXT("held"), A); Out.SetNumberField(TEXT("captures"), Captures);
}

void UWildsFeature::LoadState(const FJsonObject& In)
{
	const TArray<FString> H = JStrArr(In, TEXT("held"));
	for (int32 i = 0; i < 3; ++i) Held[i] = H.IsValidIndex(i) && H[i] == TEXT("you");
	Captures = FMath::Max<int32>(0, (int32)JInt(In, TEXT("captures")));
}
