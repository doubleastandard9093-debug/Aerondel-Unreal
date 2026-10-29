#pragma once
#include "CoreMinimal.h"
#include "Templates/Function.h"

class UWorld; class AActor;

/** One button on a panel. Run executes on click, then the panel rebuilds (or closes when bClose). */
struct FGPanelAction
{
	FString Label;
	TFunction<void()> Run;
	bool bClose = false;
	bool bEnabled = true;
};

/** What a panel shows right now. Built fresh after every click and, for live panels (cards, sea), a few times a second. */
struct FGPanelModel
{
	FString Title;
	TArray<FString> Lines;
	TArray<FGPanelAction> Actions;
	bool bLive = false;
};

/** Content for every feature panel: pad menus (pad_inventory, pad_settings, pad_map, pad_context), companions, saga, house,
 *  shipwright, sea, cards (tides / crown), hexer, roost, charter, siege, tasks, arena, quirks, writs, delves, paths, harbour,
 *  say, shop. Pure C++ so it works without any Blueprint; UGielinorPanelWidget renders it as UMG. */
struct GIELINOR_API FGielinorPanels
{
	static void Build(UWorld* World, FName Panel, AActor* Context, FGPanelModel& Out);
};
