#pragma once
// Generated from Content/Data/relicry.json + Lite regiondata.js (dig positions). Do not edit by hand.
#include "CoreMinimal.h"
namespace RelicryData
{
	struct FArt { const TCHAR* Name; int32 Level; };
	struct FSite { const TCHAR* Name; int32 Level; float X; float Z; const TCHAR* Mat; const TCHAR* Reward; FArt Arts[3]; };
	struct FReforge { const TCHAR* Gear; const TCHAR* Site; int32 Frags; int32 Mat; int32 Level; };
	struct FGate { const TCHAR* Skill; const TCHAR* Name; const TCHAR* Kind; const TCHAR* Act; const TCHAR* Done; };
	static const FSite Sites[] = {
		{ TEXT("Drowned Chapel"), 1, -392.f, 165.f, TEXT("Bog-iron flake"), TEXT("Drowned reliquary staff"), { { TEXT("Chapel bell"), 1 }, { TEXT("Pilgrim's lamp"), 6 }, { TEXT("Drowned censer"), 12 } } },
		{ TEXT("Frozen Barrow"), 20, -284.f, -361.f, TEXT("Frostglass"), TEXT("Barrow-king's cuirass"), { { TEXT("Barrow-king's torc"), 20 }, { TEXT("Rime-carved horn"), 25 }, { TEXT("Frozen war-banner"), 30 } } },
		{ TEXT("Sky Shrine Ruins"), 35, 265.f, -372.f, TEXT("Skyslate"), TEXT("Skyreach warbow"), { { TEXT("Wind-chime crown"), 35 }, { TEXT("Feathered idol"), 40 }, { TEXT("Shrine keystone"), 44 } } },
		{ TEXT("Sunken Sun Temple"), 45, 392.f, 96.f, TEXT("Sunstone dust"), TEXT("Sun-disc ward"), { { TEXT("Sun-disc mirror"), 45 }, { TEXT("Scarab seal"), 48 }, { TEXT("Temple sundial"), 52 } } },
		{ TEXT("Overgrown Ziggurat"), 50, 180.f, 392.f, TEXT("Vine resin"), TEXT("Ziggurat scale body"), { { TEXT("Jade mask"), 50 }, { TEXT("Serpent sceptre"), 55 }, { TEXT("Calendar stone"), 58 } } },
		{ TEXT("Cinder Forge Vault"), 60, 341.f, -164.f, TEXT("Cinderglass"), TEXT("Sunforged khopesh"), { { TEXT("Forge-god anvil"), 60 }, { TEXT("Ember crown"), 66 }, { TEXT("First hammer"), 72 } } },
	};
	static const FReforge Reforges[] = {
		{ TEXT("Drowned reliquary staff"), TEXT("Drowned Chapel"), 14, 30, 50 },
		{ TEXT("Barrow-king's helm"), TEXT("Frozen Barrow"), 8, 25, 40 },
		{ TEXT("Barrow-king's greaves"), TEXT("Frozen Barrow"), 12, 30, 60 },
		{ TEXT("Barrow-king's cuirass"), TEXT("Frozen Barrow"), 16, 35, 65 },
		{ TEXT("Skyreach warbow"), TEXT("Sky Shrine Ruins"), 14, 30, 55 },
		{ TEXT("Sun-disc ward"), TEXT("Sunken Sun Temple"), 12, 30, 55 },
		{ TEXT("Mirage silk robe"), TEXT("Sunken Sun Temple"), 10, 25, 45 },
		{ TEXT("Ziggurat scale body"), TEXT("Overgrown Ziggurat"), 12, 30, 50 },
		{ TEXT("Sunforged khopesh"), TEXT("Cinder Forge Vault"), 16, 35, 60 },
	};
	static const FGate Gates[] = {
		{ TEXT("Attack"), TEXT("Warden Gate"), TEXT("fight"), TEXT("Challenge the warden"), TEXT("The warden falls and the gate grinds open.") },
		{ TEXT("Strength"), TEXT("Fallen Pillar"), TEXT(""), TEXT("Heave the pillar aside"), TEXT("You heave the pillar out of the way.") },
		{ TEXT("Defence"), TEXT("Spike Corridor"), TEXT(""), TEXT("Brace and push through"), TEXT("You weather the spikes and trip the release.") },
		{ TEXT("Hitpoints"), TEXT("Blood Seal"), TEXT("hp"), TEXT("Feed the seal"), TEXT("The seal drinks and the way opens.") },
		{ TEXT("Ranged"), TEXT("Distant Lever"), TEXT(""), TEXT("Shoot the lever"), TEXT("Your shot strikes the far lever. Clunk.") },
		{ TEXT("Magic"), TEXT("Arcane Ward"), TEXT(""), TEXT("Unpick the ward"), TEXT("The ward unravels in a spray of sparks.") },
		{ TEXT("Prayer"), TEXT("Desecrated Altar"), TEXT(""), TEXT("Reconsecrate the altar"), TEXT("Your prayer cleanses the altar.") },
		{ TEXT("Agility"), TEXT("Crumbling Ledge"), TEXT(""), TEXT("Leap the ledge"), TEXT("You leap across and lower the bridge.") },
		{ TEXT("Thieving"), TEXT("Trapped Lock"), TEXT(""), TEXT("Pick the lock"), TEXT("Click. The trap never fires.") },
		{ TEXT("Mining"), TEXT("Rubble Fall"), TEXT(""), TEXT("Dig through the rubble"), TEXT("You clear a path through the rubble.") },
		{ TEXT("Woodcutting"), TEXT("Root Wall"), TEXT(""), TEXT("Hack through the roots"), TEXT("The roots part under your blade.") },
		{ TEXT("Fishing"), TEXT("Flooded Channel"), TEXT(""), TEXT("Net the sluice key"), TEXT("You fish the sluice key out of the water and drain the channel.") },
		{ TEXT("Smithing"), TEXT("Seized Portcullis"), TEXT(""), TEXT("Re-forge the hinges"), TEXT("You hammer the hinges free.") },
		{ TEXT("Crafting"), TEXT("Mosaic Bridge"), TEXT(""), TEXT("Restore the mosaic"), TEXT("The mosaic is whole; the bridge rises.") },
		{ TEXT("Cooking"), TEXT("Hungry Guardian"), TEXT(""), TEXT("Cook a treat for it"), TEXT("The guardian hound munches happily and lets you by.") },
		{ TEXT("Fletching"), TEXT("Rope-Arrow Gap"), TEXT(""), TEXT("Fletch a rope arrow"), TEXT("You loose a rope arrow across the gap and climb over.") },
		{ TEXT("Herbalism"), TEXT("Spore Cloud"), TEXT(""), TEXT("Mix an antidote"), TEXT("Your antidote holds the spores at bay.") },
		{ TEXT("Glyphcraft"), TEXT("Rune Lock"), TEXT(""), TEXT("Trace the glyphs"), TEXT("The glyphs flare in sequence and the lock opens.") },
		{ TEXT("Relicry"), TEXT("Sealed Reliquary"), TEXT(""), TEXT("Read the reliquary"), TEXT("You read the old seal and it yields.") },
		{ TEXT("Delving"), TEXT("Shifting Maze"), TEXT(""), TEXT("Find the true path"), TEXT("You find the true path through the maze.") },
	};
	static const TCHAR* RelicGear[] = { TEXT("Sunforged khopesh"), TEXT("Skyreach warbow"), TEXT("Drowned reliquary staff"), TEXT("Barrow-king's helm"), TEXT("Barrow-king's cuirass"), TEXT("Barrow-king's greaves"), TEXT("Sun-disc ward"), TEXT("Ziggurat scale body"), TEXT("Mirage silk robe") };
	static const TCHAR* DelveGear[] = { TEXT("Deepwarden greatblade"), TEXT("Gloomstring longbow"), TEXT("Voidlantern staff"), TEXT("Delver's warhelm"), TEXT("Delver's warplate"), TEXT("Delver's bulwark"), TEXT("Shadowhide jerkin"), TEXT("Gloamweave robe"), TEXT("Delver's signet") };
}
