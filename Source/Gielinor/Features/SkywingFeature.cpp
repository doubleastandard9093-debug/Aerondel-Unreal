#include "Features/SkywingFeature.h"
#include "Features/StandingFeature.h"
#include "Core/GielinorPlayerState.h"
#include "Core/PlayerStatsComponent.h"

bool USkywingFeature::CheckUnlock()
{
	if (bUnlocked) return true;
	const UStandingFeature* St = Feature<UStandingFeature>();
	if (SkillLevel(TEXT("Agility")) >= AgilityReq && St && St->BestTier() >= 4)
	{
		bUnlocked = true; Say(TEXT("A Skywing lands beside you. Press Y to take flight.")); return true;
	}
	return false;
}

bool USkywingFeature::ToggleFlight()
{
	if (!CheckUnlock()) { Say(TEXT("Skywing needs Agility 70 and Exalted standing with a faction.")); return false; }
	bFlying = !bFlying;
	if (bFlying) { ++Flights; HeightM = CruiseM; }
	return true;
}

bool USkywingFeature::Dive()
{
	AGielinorPlayerState* PS = GetPS();
	if (!bFlying || !PS || !PS->Stats || PS->Stats->RunEnergy < DiveCost) return false;
	PS->Stats->RunEnergy -= DiveCost;
	HeightM = FMath::Max(MinM, HeightM - StepM);
	return true;
}
