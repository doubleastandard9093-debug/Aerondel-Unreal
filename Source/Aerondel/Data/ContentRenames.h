#pragma once
#include "CoreMinimal.h"

/**
 * Content rename map (save v6). RuneScape-derived names were replaced with original ones
 * (skills, metal tiers, herbs, runes/altars, monsters, spells, prayers, unique items).
 * Ids are lower_snake_case of the display name (AerondelId::FromName), so one display-name table drives:
 *   - MigrateId:   old content id -> new id (token-wise, so "grimy_guam_leaf" and "herb_guam_leaf" both convert)
 *   - MigrateText: old display names / ids inside saved feature JSON -> new ones
 * NPC display names (save v7) are replaced case-sensitively in text only; NPC ids (e.g. "rovin") stay as they are.
 * Unknown ids and text pass through unchanged. Town renames (v5) live in Data/TownRenames.h.
 */
namespace AerondelContent
{
	struct FRename { const TCHAR* OldName; const TCHAR* NewName; };

	/** Display-name pairs, longest first so phrases win over single words. */
	inline TArrayView<const FRename> Renames()
	{
		static const FRename R[] = {
			{ TEXT("Protect from Missiles"), TEXT("Ward against Missiles") },
			{ TEXT("Dragon thrown dagger"), TEXT("Emberite thrown dagger") },
			{ TEXT("Dragon throwing axe"), TEXT("Emberite throwing axe") },
			{ TEXT("Superhuman Strength"), TEXT("Titan's Might") },
			{ TEXT("Incredible Reflexes"), TEXT("Lightning Wits") },
			{ TEXT("Rune thrown dagger"), TEXT("Cobalt thrown dagger") },
			{ TEXT("Clarity of Thought"), TEXT("Clear Mind") },
			{ TEXT("Protect from Magic"), TEXT("Ward against Magic") },
			{ TEXT("Protect from Melee"), TEXT("Ward against Melee") },
			{ TEXT("Slayer's Challenge"), TEXT("Hunter's Challenge") },
			{ TEXT("Rune throwing axe"), TEXT("Cobalt throwing axe") },
			{ TEXT("Burst of Strength"), TEXT("Surge of Might") },
			{ TEXT("Improved Reflexes"), TEXT("Quickened Wits") },
			{ TEXT("Ultimate Strength"), TEXT("Colossus Might") },
			{ TEXT("Dragon full helm"), TEXT("Emberite full helm") },
			{ TEXT("Dragon platelegs"), TEXT("Emberite platelegs") },
			{ TEXT("Dragon platebody"), TEXT("Emberite platebody") },
			{ TEXT("Dragon flintlock"), TEXT("Emberite flintlock") },
			{ TEXT("Dragon longsword"), TEXT("Emberite longsword") },
			{ TEXT("Dragon equipment"), TEXT("Emberite equipment") },
			{ TEXT("Dragon crossbow"), TEXT("Emberite crossbow") },
			{ TEXT("Amulet of glory"), TEXT("Amulet of renown") },
			{ TEXT("Wine of zamorak"), TEXT("Wine of the pyre") },
			{ TEXT("Rune full helm"), TEXT("Cobalt full helm") },
			{ TEXT("Rune platelegs"), TEXT("Cobalt platelegs") },
			{ TEXT("Rune platebody"), TEXT("Cobalt platebody") },
			{ TEXT("Rune flintlock"), TEXT("Cobalt flintlock") },
			{ TEXT("Rune longsword"), TEXT("Cobalt longsword") },
			{ TEXT("Rune equipment"), TEXT("Cobalt equipment") },
			{ TEXT("Dragon pickaxe"), TEXT("Emberite pickaxe") },
			{ TEXT("Dragon cutlass"), TEXT("Emberite cutlass") },
			{ TEXT("Ring of recoil"), TEXT("Ring of rebuke") },
			{ TEXT("Berserker ring"), TEXT("Ring of fury") },
			{ TEXT("Rune crossbow"), TEXT("Cobalt crossbow") },
			{ TEXT("Dragon shield"), TEXT("Emberite shield") },
			{ TEXT("Dragon arrows"), TEXT("Emberite arrows") },
			{ TEXT("Dragon dagger"), TEXT("Emberite dagger") },
			{ TEXT("Dragon armour"), TEXT("Emberite armour") },
			{ TEXT("Obsidian cape"), TEXT("Cinderglass cape") },
			{ TEXT("Limpwurt root"), TEXT("Bitterroot") },
			{ TEXT("Runecrafting"), TEXT("Glyphcraft") },
			{ TEXT("Rune pickaxe"), TEXT("Cobalt pickaxe") },
			{ TEXT("Rune cutlass"), TEXT("Cobalt cutlass") },
			{ TEXT("Dragon sword"), TEXT("Emberite sword") },
			{ TEXT("Dragon bolts"), TEXT("Emberite bolts") },
			{ TEXT("Dragon items"), TEXT("Emberite items") },
			{ TEXT("Dragon rocks"), TEXT("Emberite rocks") },
			{ TEXT("Granite maul"), TEXT("Basalt maul") },
			{ TEXT("Rune essence"), TEXT("Glyph essence") },
			{ TEXT("Pure essence"), TEXT("Clear essence") },
			{ TEXT("Black Knight"), TEXT("Ironveil Knight") },
			{ TEXT("Lesser Demon"), TEXT("Cinder Fiend") },
			{ TEXT("Water Strike"), TEXT("Water Dart") },
			{ TEXT("Earth Strike"), TEXT("Earth Dart") },
			{ TEXT("Runecrafter"), TEXT("Glyphcrafter") },
			{ TEXT("Rune shield"), TEXT("Cobalt shield") },
			{ TEXT("Rune arrows"), TEXT("Cobalt arrows") },
			{ TEXT("Rune dagger"), TEXT("Cobalt dagger") },
			{ TEXT("Rune armour"), TEXT("Cobalt armour") },
			{ TEXT("Dragon tier"), TEXT("Emberite tier") },
			{ TEXT("Dragon gear"), TEXT("Emberite gear") },
			{ TEXT("Dragon rock"), TEXT("Emberite rock") },
			{ TEXT("Snape grass"), TEXT("Marsh reed") },
			{ TEXT("Harralander"), TEXT("Harrowbloom") },
			{ TEXT("Ranarr weed"), TEXT("Ravenweed") },
			{ TEXT("Chaos altar"), TEXT("Havoc altar") },
			{ TEXT("Death altar"), TEXT("Grave altar") },
			{ TEXT("Blood altar"), TEXT("Crimson altar") },
			{ TEXT("Wind Strike"), TEXT("Wind Dart") },
			{ TEXT("Water Blast"), TEXT("Water Burst") },
			{ TEXT("Earth Blast"), TEXT("Earth Burst") },
			{ TEXT("Fire Strike"), TEXT("Fire Dart") },
			{ TEXT("Adamantite"), TEXT("Verdite") },
			{ TEXT("Rune sword"), TEXT("Cobalt sword") },
			{ TEXT("Rune bolts"), TEXT("Cobalt bolts") },
			{ TEXT("Rune items"), TEXT("Cobalt items") },
			{ TEXT("Rune rocks"), TEXT("Cobalt rocks") },
			{ TEXT("Dragon bar"), TEXT("Emberite bar") },
			{ TEXT("Dragon axe"), TEXT("Emberite axe") },
			{ TEXT("Dragon ore"), TEXT("Emberite ore") },
			{ TEXT("Dragon set"), TEXT("Emberite set") },
			{ TEXT("Marrentill"), TEXT("Marshmint") },
			{ TEXT("Dwarf weed"), TEXT("Stoneweed") },
			{ TEXT("Mind altar"), TEXT("Thought altar") },
			{ TEXT("Body altar"), TEXT("Vital altar") },
			{ TEXT("Chaos rune"), TEXT("Havoc rune") },
			{ TEXT("Death rune"), TEXT("Grave rune") },
			{ TEXT("Blood rune"), TEXT("Crimson rune") },
			{ TEXT("Moss Giant"), TEXT("Bog Giant") },
			{ TEXT("Hill Giant"), TEXT("Crag Giant") },
			{ TEXT("Wind Blast"), TEXT("Wind Burst") },
			{ TEXT("Water Bolt"), TEXT("Water Lance") },
			{ TEXT("Water Wave"), TEXT("Water Torrent") },
			{ TEXT("Earth Bolt"), TEXT("Earth Lance") },
			{ TEXT("Earth Wave"), TEXT("Earth Torrent") },
			{ TEXT("Fire Blast"), TEXT("Fire Burst") },
			{ TEXT("Thick Skin"), TEXT("Hardened Hide") },
			{ TEXT("Rapid Heal"), TEXT("Swift Mending") },
			{ TEXT("Steel Skin"), TEXT("Iron Hide") },
			{ TEXT("Rune tier"), TEXT("Cobalt tier") },
			{ TEXT("Rune gear"), TEXT("Cobalt gear") },
			{ TEXT("Rune rock"), TEXT("Cobalt rock") },
			{ TEXT("Guam leaf"), TEXT("Greenwort") },
			{ TEXT("Irit leaf"), TEXT("Ironleaf") },
			{ TEXT("Cadantine"), TEXT("Cindervine") },
			{ TEXT("Mind rune"), TEXT("Thought rune") },
			{ TEXT("Body rune"), TEXT("Vital rune") },
			{ TEXT("Wind Bolt"), TEXT("Wind Lance") },
			{ TEXT("Wind Wave"), TEXT("Wind Torrent") },
			{ TEXT("Fire Bolt"), TEXT("Fire Lance") },
			{ TEXT("Fire Wave"), TEXT("Fire Torrent") },
			{ TEXT("Sharp Eye"), TEXT("Keen Sight") },
			{ TEXT("Rock Skin"), TEXT("Stone Hide") },
			{ TEXT("Eagle Eye"), TEXT("Raptor Sight") },
			{ TEXT("Herblore"), TEXT("Herbalism") },
			{ TEXT("Rune bar"), TEXT("Cobalt bar") },
			{ TEXT("Rune axe"), TEXT("Cobalt axe") },
			{ TEXT("Rune ore"), TEXT("Cobalt ore") },
			{ TEXT("Rune set"), TEXT("Cobalt set") },
			{ TEXT("Dark bow"), TEXT("Dusk bow") },
			{ TEXT("Tarromin"), TEXT("Tallowroot") },
			{ TEXT("Hawk Eye"), TEXT("Falcon Sight") },
			{ TEXT("Mithril"), TEXT("Bluesteel") },
			{ TEXT("Adamant"), TEXT("Verdite") },
			{ TEXT("Avantoe"), TEXT("Amberleaf") },
			{ TEXT("Torstol"), TEXT("Thornstalk") },
			{ TEXT("Runite"), TEXT("Cobalt") },
			{ TEXT("Mystic"), TEXT("Starweave") },
			{ TEXT("Ranarr"), TEXT("Ravenweed") },
			{ TEXT("Kwuarm"), TEXT("Knotroot") },
			{ TEXT("Guam"), TEXT("Greenwort") },
		};
		return TArrayView<const FRename>(R, UE_ARRAY_COUNT(R));
	}

	/** Whole-id renames that are too generic to replace token-wise (rune altar keys, rock nodes). */
	inline TArrayView<const FRename> ExactIds()
	{
		static const FRename R[] = {
			{ TEXT("mind"), TEXT("thought") },
			{ TEXT("body"), TEXT("vital") },
			{ TEXT("chaos"), TEXT("havoc") },
			{ TEXT("death"), TEXT("grave") },
			{ TEXT("blood"), TEXT("crimson") },
			{ TEXT("rock_dragon"), TEXT("rock_emberite") },
			{ TEXT("rock_runite"), TEXT("rock_cobalt") },
		};
		return TArrayView<const FRename>(R, UE_ARRAY_COUNT(R));
	}

	/** NPC display names (v7, matches Aerondel-Lite js/renames.js). Text only, whole words, case-sensitive: NPC ids are unchanged. */
	inline TArrayView<const FRename> NpcNames()
	{
		static const FRename R[] = {
			{ TEXT("Juliet"), TEXT("Juniper") },
			{ TEXT("Mizgog"), TEXT("Mardoc") },
			{ TEXT("Horvik"), TEXT("Halvard") },
			{ TEXT("Gerrant"), TEXT("Garrow") },
			{ TEXT("Ellis"), TEXT("Ellard") },
			{ TEXT("Rovin"), TEXT("Roderic") },
		};
		return TArrayView<const FRename>(R, UE_ARRAY_COUNT(R));
	}

	/** Replace whole-word occurrences of Old (not inside a longer word). */
	inline void ReplaceWord(FString& Out, const TCHAR* Old, const TCHAR* New)
	{
		const int32 OldLen = FCString::Strlen(Old);
		int32 From = 0;
		while (true)
		{
			const int32 At = Out.Find(Old, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (At == INDEX_NONE) return;
			const bool bLeft = At == 0 || !FChar::IsAlpha(Out[At - 1]);
			const bool bRight = At + OldLen >= Out.Len() || !FChar::IsAlpha(Out[At + OldLen]);
			if (bLeft && bRight) { Out = Out.Left(At) + New + Out.Mid(At + OldLen); From = At + FCString::Strlen(New); }
			else From = At + 1;
		}
	}

	/** v7: NPC display names inside saved text. */
	inline FString MigrateNpcText(const FString& Text)
	{
		FString Out = Text;
		for (const FRename& E : NpcNames()) ReplaceWord(Out, E.OldName, E.NewName);
		return Out;
	}

	inline FString ToId(const TCHAR* Name)
	{
		FString Out; bool bUnderscore = false;
		for (const TCHAR* P = Name; *P; ++P)
		{
			const TCHAR C = *P;
			if (C == TEXT('\'')) continue;
			if (FChar::IsAlnum(C)) { Out.AppendChar(FChar::ToLower(C)); bUnderscore = false; }
			else if (!bUnderscore && Out.Len() > 0) { Out.AppendChar(TEXT('_')); bUnderscore = true; }
		}
		while (Out.EndsWith(TEXT("_"))) Out.LeftChopInline(1);
		return Out;
	}

	/** Old content id -> new id. Exact ids first, then token-wise phrase replacement. */
	inline FName MigrateId(FName Id)
	{
		if (Id.IsNone()) return Id;
		const FString S = Id.ToString().ToLower();
		for (const FRename& E : ExactIds()) if (S.Equals(E.OldName)) return FName(E.NewName);
		FString Out = TEXT("_") + S + TEXT("_");
		bool bChanged = false;
		for (const FRename& E : Renames())
		{
			const FString From = TEXT("_") + ToId(E.OldName) + TEXT("_");
			if (Out.Contains(From, ESearchCase::CaseSensitive))
			{
				Out.ReplaceInline(*From, *(TEXT("_") + ToId(E.NewName) + TEXT("_")), ESearchCase::CaseSensitive);
				bChanged = true;
			}
		}
		if (!bChanged) return Id;
		Out.MidInline(1, Out.Len() - 2);
		return FName(*Out);
	}

	/** Replace old display names (as written, and lower case) and old ids inside free text (saved feature JSON). */
	inline FString MigrateText(const FString& Text)
	{
		FString Out = Text;
		for (const FRename& E : Renames())
		{
			Out.ReplaceInline(E.OldName, E.NewName, ESearchCase::CaseSensitive);
			const FString OldLower = FString(E.OldName).ToLower();
			if (!OldLower.Equals(E.OldName)) Out.ReplaceInline(*OldLower, *FString(E.NewName).ToLower(), ESearchCase::CaseSensitive);
			const FString OldId = ToId(E.OldName);
			if (OldId.Contains(TEXT("_"))) Out.ReplaceInline(*OldId, *ToId(E.NewName), ESearchCase::CaseSensitive);
		}
		return Out;
	}
}
