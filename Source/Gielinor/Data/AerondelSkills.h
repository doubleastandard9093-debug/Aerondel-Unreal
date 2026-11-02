#pragma once
#include "CoreMinimal.h"

/**
 * Skill ids added with the Aerondel climate-region update (web js/skills.js, Content/Data/skills.json).
 * Skills are data-driven FNames (GielinorId::FromName), so these are just shared constants for C++ callers,
 * e.g. Skills->GetLevelById(AerondelSkills::Relicry). Gameplay data lives in relicry.json / items.json.
 */
namespace AerondelSkills
{
	inline const FName Relicry(TEXT("relicry"));
	inline const FName Delving(TEXT("delving"));

	/** Perk effect keys used by the two skills (skills.json "perks" / "caps"). */
	inline const FName RareFind(TEXT("rareFind"));
	inline const FName GateEase(TEXT("gateEase"));
	inline const FName TokenBonus(TEXT("tokenBonus"));
	inline const FName FloorHeal(TEXT("floorHeal"));
}
