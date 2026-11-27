#pragma once
#include "CoreMinimal.h"

/**
 * One copy of the town table the Ventures files share (web world.json order, web metres; x east, z = UE Y / 100).
 * The first NumMain entries are the five main towns (web MAIN_TOWNS). Used by Ventures, Ashfall, Crossed Banners and Waycamp,
 * which each used to carry their own copy. Header-only, inline constexpr, so it is unity-build safe.
 */
namespace VenturesTowns
{
	struct FTownPt { const TCHAR* Name; float X; float Z; };

	inline constexpr FTownPt All[] = {
		{ TEXT("Larkhaven"), 0.f, 0.f }, { TEXT("Emberfold"), 50.f, -24.f }, { TEXT("Saltreach"), -80.f, 60.f }, { TEXT("Highcrest"), 120.f, -40.f },
		{ TEXT("Whitecairn"), -60.f, -80.f }, { TEXT("Thistlewell"), 40.f, 90.f }, { TEXT("Rimeholt"), -250.f, -355.f }, { TEXT("Cragmoor Lodge"), 235.f, -350.f },
		{ TEXT("Cinderpost"), 355.f, -140.f }, { TEXT("Mirage Well"), 360.f, 120.f }, { TEXT("Vinehollow"), 210.f, 360.f }, { TEXT("Bogwatch"), -360.f, 185.f },
		{ TEXT("Ashgate Post"), 540.f, 0.f }, { TEXT("Briarwatch"), 380.f, 400.f }, { TEXT("Thunderhold"), -390.f, 400.f }, { TEXT("Lanternmoor"), -545.f, -10.f },
		{ TEXT("Mirrorsand Rest"), -410.f, -440.f }, { TEXT("Slagwatch"), 0.f, -545.f }, { TEXT("Last Light Bastion"), 420.f, -430.f },
		{ TEXT("Snarlwick"), 190.f, 40.f }, { TEXT("Gorrumhold"), -200.f, -30.f }, { TEXT("Sylvaranth"), 30.f, 230.f }
	};
	inline constexpr int32 Num = (int32)UE_ARRAY_COUNT(All);
	/** Larkhaven, Emberfold, Saltreach, Highcrest, Whitecairn. */
	inline constexpr int32 NumMain = 5;

	/** Planar distance in web metres (same Sqrt form the per-file copies used, so results are bit-identical). */
	inline float Dist(const FTownPt& T, float X, float Z) { return FMath::Sqrt(FMath::Square(X - T.X) + FMath::Square(Z - T.Z)); }
}
