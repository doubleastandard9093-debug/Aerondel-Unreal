#include "Features/FlowFeature.h"
#include "Features/StillwatchFeature.h"
#include "Features/VenturesFeature.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"

FFlowHit UFlowFeature::OnSkillHit(int32 Tick, FName AbilityId, FName Style)
{
	FFlowHit Out;
	const bool bLive = Tick - LastTick <= WindowTicks && Links > 0;
	if (!bLive) Used.Reset();
	if (bLive && !AbilityId.IsNone() && Used.Contains(AbilityId))
	{
		LastTick = Tick; Out.Bonus = (Links - 1) * PerLink; Out.Links = Links; Out.bRepeat = true; return Out;
	}
	Links = bLive ? Links + 1 : 1;
	if (!AbilityId.IsNone()) Used.Add(AbilityId);
	LastTick = Tick;
	Best = FMath::Max(Best, Links);
	Out.Bonus = (Links - 1) * PerLink; Out.Links = Links;
	if (Links >= MaxLinks)
	{
		Out.Finisher = Style == TEXT("ranged") ? EFlowFinisher::Piercing : Style == TEXT("magic") ? EFlowFinisher::Chain : EFlowFinisher::Bleeding;
		Links = 0; Used.Reset();
		if (UStillwatchFeature* S = Feature<UStillwatchFeature>()) S->AddFocus(UStillwatchFeature::FocusFinisher);
	}
	return Out;
}

int32 UFlowFeature::OnPerfectDodge(int32 Tick)
{
	const bool bLive = Tick - LastTick <= WindowTicks + 2;
	Links = FMath::Min(MaxLinks - 1, (bLive ? Links : 0) + 1); LastTick = Tick;
	if (!bLive) Used.Reset();
	Best = FMath::Max(Best, Links);
	if (UStillwatchFeature* S = Feature<UStillwatchFeature>()) S->AddFocus(UStillwatchFeature::FocusPerfect);
	return Links;
}

bool UFlowFeature::TryDodge(int32 BonusMs)
{
	const int64 Now = NowMs();
	if (Now < DodgeReadyAt) return false; // ignore mashing
	AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Stats) return false;
	if (PS->Stats->RunEnergy < DodgeCost) { Say(TEXT("You are too tired to roll.")); return false; }
	PS->Stats->RunEnergy -= DodgeCost;
	DodgeReadyAt = Now + DodgeCdMs; IframeUntil = Now + IframeMs + BonusMs;
	if (UVenturesFeature* V = UVenturesFeature::Get(this)) V->ShadeBreakLine();
	return true;
}
