#include "Features/BossMechFeature.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/AerondelMob.h"
#include "Data/MobDef.h"
#include "Rules/AerondelRules.h"
#include "EngineUtils.h"
#include "UObject/Package.h"

namespace BossData
{
	static constexpr int32 SlamEvery = 9, SlamWarn = 3, SwitchEvery = 14;
	static constexpr float SlamR = 3.2f, SlamPct = 0.3f, EnrageAt = 0.3f, EnrageHit = 1.3f, ProtectBoss = 0.6f;
	static bool IsDataBoss(const AAerondelMob* M) { return M && M->Definition && M->Definition->bBoss && M->Definition->GetOuter() != GetTransientPackage(); }
}

FName UBossMechFeature::PrayerFor(EBossStyle S) { return S == EBossStyle::Melee ? FName(TEXT("protect")) : S == EBossStyle::Ranged ? FName(TEXT("protectr")) : FName(TEXT("protectm")); }

void UBossMechFeature::OnGameTick()
{
	using namespace BossData;
	++Tick;
	UGameInstance* GI = GetGameInstance(); UWorld* W = GI ? GI->GetWorld() : nullptr;
	const AAerondelPlayerState* PS = GetPS();
	if (!W || !PS || !PS->Health) return;
	// Forget bosses that died or stopped fighting (back to their base style next time).
	for (auto It = States.CreateIterator(); It; ++It)
	{
		const AAerondelMob* M = It.Key().Get();
		if (!M || M->IsDead() || (M->GetState() != EMobState::Chase && M->GetState() != EMobState::Attack)) { if (TeleBoss == It.Key()) TeleBoss = nullptr; It.RemoveCurrent(); }
	}
	for (TActorIterator<AAerondelMob> It(W); It; ++It)
	{
		AAerondelMob* M = *It;
		if (!IsDataBoss(M) || M->IsDead() || (M->GetState() != EMobState::Chase && M->GetState() != EMobState::Attack)) continue;
		FState* S = States.Find(M);
		if (!S) { S = &States.Add(M); S->Style = M->Definition->bRanged ? EBossStyle::Ranged : EBossStyle::Melee; S->NextSwitch = Tick + SwitchEvery; S->NextSlam = Tick + 5; }
		const FString Name = M->GetDisplayName().ToString();
		if (!S->bEnraged && M->GetHp() > 0 && M->GetHp() < M->GetMaxHp() * EnrageAt) { S->bEnraged = true; Say(FString::Printf(TEXT("%s is enraged! It hits harder and faster."), *Name)); }
		if (Tick >= S->NextSwitch)
		{
			const EBossStyle Opts[3] = { EBossStyle::Melee, EBossStyle::Ranged, EBossStyle::Magic };
			EBossStyle N; do { N = Opts[FMath::RandHelper(3)]; } while (N == S->Style);
			S->Style = N; S->NextSwitch = Tick + SwitchEvery - (S->bEnraged ? 4 : 0);
			OnStyleChanged.Broadcast(M, N);
			Say(FString::Printf(TEXT("%s switches to %s! Pray %s."), *Name, N == EBossStyle::Melee ? TEXT("melee") : N == EBossStyle::Ranged ? TEXT("ranged") : TEXT("magic"),
				N == EBossStyle::Melee ? TEXT("Ward against Melee") : N == EBossStyle::Ranged ? TEXT("Ward against Missiles") : TEXT("Ward against Magic")));
		}
		FVector2D P;
		if (!TeleBoss.IsValid() && Tick >= S->NextSlam && PlayerWebPos(P))
		{
			TeleBoss = M; TeleAt = P; TeleR = SlamR * (S->bEnraged ? 1.25f : 1.f); TeleStart = Tick; TeleHit = Tick + SlamWarn;
			S->NextSlam = Tick + SlamEvery - (S->bEnraged ? 3 : 0);
			Say(FString::Printf(TEXT("%s winds up a ground slam. Roll out of the red ring!"), *Name));
		}
	}
	if (TeleBoss.IsValid() && Tick >= TeleHit)
	{
		const FState* S = States.Find(TeleBoss);
		FVector2D P;
		if (PlayerWebPos(P) && FVector2D::Distance(P, TeleAt) <= TeleR)
			HurtPlayer(PS->Health->GetMax() * SlamPct * (S && S->bEnraged ? 1.25f : 1.f), TeleBoss->GetDisplayName().ToString() + TEXT("'s slam"));
		TeleBoss = nullptr;
	}
}

float UBossMechFeature::DamageTakenMult(const UMobDef* Source, float HpFrac) const
{
	using namespace BossData;
	if (!Source) return 1.f;
	for (const TPair<TWeakObjectPtr<AAerondelMob>, FState>& P : States)
	{
		const AAerondelMob* M = P.Key.Get();
		if (!M || M->Definition != Source) continue;
		const AAerondelPlayerState* PS = GetPS();
		const bool bRight = PS && PS->Stats && PS->Stats->IsPrayerActive(PrayerFor(P.Value.Style));
		// ReduceIncoming already applied the generic Ward against Melee (40%); swap it for the boss rule.
		const float Generic = FMath::Clamp(UAerondelRules::PrayerFx(PS, TEXT("protect")), 0.f, 0.9f);
		const float Want = bRight ? 1.f - ProtectBoss : 1.f;
		return Want / (1.f - Generic) * (P.Value.bEnraged ? EnrageHit : 1.f);
	}
	return 1.f;
}

bool UBossMechFeature::GetTelegraph(FBossTelegraph& Out) const
{
	if (!TeleBoss.IsValid()) return false;
	Out.Location = WebToWorld(TeleAt.X, TeleAt.Y); Out.RadiusCm = TeleR * 100.f;
	Out.Fill = FMath::Clamp((float)(Tick - TeleStart) / FMath::Max(1, TeleHit - TeleStart), 0.f, 1.f);
	return true;
}

bool UBossMechFeature::GetBossStyle(const AAerondelMob* Boss, EBossStyle& Out) const
{
	for (const TPair<TWeakObjectPtr<AAerondelMob>, FState>& P : States) if (P.Key.Get() == Boss) { Out = P.Value.Style; return true; }
	return false;
}
