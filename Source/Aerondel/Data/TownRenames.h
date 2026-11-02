#pragma once
#include "CoreMinimal.h"

/**
 * Town rename map (save v5). The web-port town names were replaced with original ones:
 *   Brightwater -> Larkhaven, Ashford -> Emberfold, Port Sarim -> Saltreach, Varrock -> Highcrest,
 *   Falador -> Whitecairn, Wellmere -> Thistlewell.
 * Old ids/names still found in older saves or hand-placed actors are mapped to the new ones here.
 */
namespace AerondelTowns
{
	struct FRename { const TCHAR* OldName; const TCHAR* NewName; const TCHAR* OldId; const TCHAR* NewId; };

	inline TArrayView<const FRename> Renames()
	{
		// OldId/NewId cover town ids (AerondelId::FromName) and agility course ids (activities.json keys).
		static const FRename R[] = {
			{ TEXT("Port Sarim"),  TEXT("Saltreach"),   TEXT("port_sarim"),  TEXT("saltreach") },
			{ TEXT("Port Sarim"),  TEXT("Saltreach"),   TEXT("portsarim"),   TEXT("saltreach") },
			{ TEXT("Brightwater"), TEXT("Larkhaven"),   TEXT("brightwater"), TEXT("larkhaven") },
			{ TEXT("Ashford"),     TEXT("Emberfold"),   TEXT("ashford"),     TEXT("emberfold") },
			{ TEXT("Varrock"),     TEXT("Highcrest"),   TEXT("varrock"),     TEXT("highcrest") },
			{ TEXT("Falador"),     TEXT("Whitecairn"),  TEXT("falador"),     TEXT("whitecairn") },
			{ TEXT("Wellmere"),    TEXT("Thistlewell"), TEXT("wellmere"),    TEXT("thistlewell") },
			{ TEXT("Lumbridge"),   TEXT("Larkhaven"),   TEXT("lumbridge"),   TEXT("larkhaven") },
		};
		return TArrayView<const FRename>(R, UE_ARRAY_COUNT(R));
	}

	/** Old town / course id -> new id (unknown ids pass through unchanged). */
	inline FName MigrateId(FName Id)
	{
		if (Id.IsNone()) return Id;
		const FString S = Id.ToString();
		for (const FRename& E : Renames()) if (S.Equals(E.OldId, ESearchCase::IgnoreCase)) return FName(E.NewId);
		return Id;
	}

	/** Old town display name -> new display name (exact match, case-insensitive). */
	inline FString MigrateName(const FString& Name)
	{
		for (const FRename& E : Renames()) if (Name.Equals(E.OldName, ESearchCase::IgnoreCase)) return E.NewName;
		return Name;
	}

	/** Replace old town names inside free text (saved feature JSON blocks from before v5). */
	inline FString MigrateText(const FString& Text)
	{
		FString Out = Text;
		for (const FRename& E : Renames()) Out.ReplaceInline(E.OldName, E.NewName, ESearchCase::CaseSensitive);
		return Out;
	}
}
