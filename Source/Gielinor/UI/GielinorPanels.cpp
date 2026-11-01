#include "UI/GielinorPanels.h"
#include "Features/CompanionsFeature.h"
#include "Features/SagaFeature.h"
#include "Features/HearthFeature.h"
#include "Features/SailingFeature.h"
#include "Features/CardsFeature.h"
#include "Features/HexerFeature.h"
#include "Features/StableCodexFeature.h"
#include "Features/CharterFeature.h"
#include "Features/SiegeFeature.h"
#include "Features/TasksFeature.h"
#include "Features/ArenaFeature.h"
#include "Features/QuirksFeature.h"
#include "Features/WritsFeature.h"
#include "Features/DelvesFeature.h"
#include "Features/PathsFeature.h"
#include "Features/ErrandsFeature.h"
#include "Features/TrueAimFeature.h"
#include "Activities/ActivityActors.h"
#include "Interaction/GielinorInteractable.h"
#include "Interaction/NpcTalk.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Inventory/InventoryComponent.h"
#include "Player/GielinorGamepadComponent.h"
#include "World/GielinorScalability.h"
#include "Data/ItemDef.h"
#include "Data/NpcDef.h"
#include "Data/ContentDefs.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	template<class T> T* Sub(UWorld* W) { UGameInstance* GI = W ? W->GetGameInstance() : nullptr; return GI ? GI->GetSubsystem<T>() : nullptr; }
	void Act(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bClose = false, bool bEnabled = true)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bClose = bClose; A.bEnabled = bEnabled; M.Actions.Add(MoveTemp(A)); }
	void Close(FGPanelModel& M) { Act(M, TEXT("Close"), [] {}, true); }
	const TCHAR* ToneName(EWheelTone T) { return T == EWheelTone::Kind ? TEXT("Kind") : T == EWheelTone::Firm ? TEXT("Firm") : TEXT("Cunning"); }
	FString CardStr(const FPlayingCard& C)
	{
		if (C.Rank < 0) return TEXT("[?]");
		static const TCHAR* R[13] = { TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("J"), TEXT("Q"), TEXT("K"), TEXT("A") };
		static const TCHAR* S[4] = { TEXT("Anchor"), TEXT("Shell"), TEXT("Skull"), TEXT("Compass") };
		return FString::Printf(TEXT("[%s %s]"), R[FMath::Clamp(C.Rank, 0, 12)], S[FMath::Clamp(C.Suit, 0, 3)]);
	}
	FString Hand(const TArray<FPlayingCard>& H) { FString S; for (const FPlayingCard& C : H) S += CardStr(C) + TEXT(" "); return S; }
}

void FGielinorPanels::Build(UWorld* W, FName Panel, AActor* Ctx, FGPanelModel& M)
{
	M = FGPanelModel();
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(W, 0);
	AGielinorPlayerState* PS = Pawn ? Pawn->GetPlayerState<AGielinorPlayerState>() : nullptr;
	const FString P = Panel.ToString();
	const ANpcTalk* Npc = Cast<ANpcTalk>(Ctx);
	const FString NpcName = Npc && Npc->Definition ? Npc->Definition->DisplayName.ToString() : FString();
	const FName NpcId = Npc && Npc->Definition ? Npc->Definition->Id : NAME_None;

	if (P == TEXT("pad_inventory"))
	{
		M.Title = TEXT("Inventory");
		if (PS && PS->Inventory)
		{
			int32 Used = 0;
			for (const FItemStack& S : PS->Inventory->GetSlots()) if (!S.IsEmpty()) { ++Used; M.Lines.Add(FString::Printf(TEXT("%s x%d"), *S.Item->DisplayName.ToString(), S.Count)); }
			M.Lines.Add(FString::Printf(TEXT("%d / %d slots"), Used, PS->Inventory->GetSlots().Num()));
		}
		if (USailingFeature* Sea = Sub<USailingFeature>(W))
		{
			Act(M, TEXT("Read a Treasure chart"), [Sea] { Sea->ReadChart(); });
			Act(M, TEXT("Dig here (Spade)"), [Sea] { Sea->DigHere(); });
		}
		auto Go = [W](const TCHAR* Panel) { const FName N(Panel); return [W, N] { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->OnServiceRequested.Broadcast(N, nullptr); }; };
		Act(M, TEXT("Companions"), Go(TEXT("companions")));
		if (UHearthFeature* H = Sub<UHearthFeature>(W)) if (H->HasDeed()) Act(M, TEXT("Hearthstead"), Go(TEXT("hearth")));
		Close(M); return;
	}
	if (P == TEXT("pad_settings"))
	{
		M.Title = TEXT("Settings");
		const EGielinorTier Tier = UGielinorScalability::GetSavedTier();
		static const TCHAR* TierNames[4] = { TEXT("Quest"), TEXT("Low"), TEXT("High"), TEXT("Ultra") };
		M.Lines.Add(FString::Printf(TEXT("Graphics: %s. Low never loads High assets; Lumen, sky and foliage upgrades are High/Ultra only."), TierNames[(int32)Tier]));
		for (EGielinorTier T : { EGielinorTier::Low, EGielinorTier::High, EGielinorTier::Ultra })
			Act(M, FString::Printf(TEXT("%sGraphics: %s"), T == Tier ? TEXT("> ") : TEXT(""), TierNames[(int32)T]), [W, T] { UGielinorScalability::SetAndSaveTier(W, T); });
		if (Tier >= EGielinorTier::High)
		{
			const bool bNanite = UGielinorScalability::GetNaniteOptIn();
			Act(M, FString::Printf(TEXT("Nanite (experimental, restart): %s"), bNanite ? TEXT("on") : TEXT("off")), [bNanite] { UGielinorScalability::SetNaniteOptIn(!bNanite); });
		}
		if (UTrueAimFeature* Aim = Sub<UTrueAimFeature>(W))
			Act(M, FString::Printf(TEXT("True Aim: %s"), Aim->IsOn() ? TEXT("on") : TEXT("off")), [Aim] { Aim->Toggle(); });
		if (UGielinorGamepadComponent* Pad = Pawn ? Pawn->FindComponentByClass<UGielinorGamepadComponent>() : nullptr)
		{
			M.Lines.Add(FString::Printf(TEXT("Look sensitivity %.1f, deadzone %.2f"), Pad->LookSensitivity, Pad->Deadzone));
			Act(M, TEXT("Sensitivity -"), [Pad] { Pad->LookSensitivity = FMath::Max(0.2f, Pad->LookSensitivity - 0.1f); Pad->SaveSettings(); });
			Act(M, TEXT("Sensitivity +"), [Pad] { Pad->LookSensitivity = FMath::Min(3.f, Pad->LookSensitivity + 0.1f); Pad->SaveSettings(); });
			Act(M, FString::Printf(TEXT("Invert Y: %s"), Pad->bInvertY ? TEXT("on") : TEXT("off")), [Pad] { Pad->bInvertY = !Pad->bInvertY; Pad->SaveSettings(); });
			Act(M, FString::Printf(TEXT("Rumble: %s"), Pad->bRumble ? TEXT("on") : TEXT("off")), [Pad] { Pad->bRumble = !Pad->bRumble; Pad->SaveSettings(); });
		}
		Close(M); return;
	}
	if (P == TEXT("pad_map"))
	{
		const FVector L = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		M.Title = FString::Printf(TEXT("World map - you are at (%.0f, %.0f)"), L.X / 100.f, L.Y / 100.f);
		struct FPlace { const TCHAR* Name; float X, Z; };
		static const FPlace Places[] = { { TEXT("Larkhaven"), 0, 0 }, { TEXT("Emberfold"), 50, -24 }, { TEXT("Saltreach"), -80, 60 }, { TEXT("Highcrest"), 120, -40 },
			{ TEXT("Whitecairn"), -60, -80 }, { TEXT("Thistlewell"), 40, 90 }, { TEXT("The Wilds"), 122, -122 }, { TEXT("Gullrest"), -70, 960 }, { TEXT("Brineholt"), 80, 1030 }, { TEXT("Wreckers' Reef"), 10, 1120 } };
		static const TCHAR* Dirs[8] = { TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW"), TEXT("N"), TEXT("NE") }; // web z grows south
		auto Line = [&](const FString& N, float X, float Z)
		{
			const float Dx = X - L.X / 100.f, Dz = Z - L.Y / 100.f;
			const int32 Oct = (FMath::RoundToInt(FMath::Atan2(Dz, Dx) / (PI / 4.f)) + 8) % 8;
			M.Lines.Add(FString::Printf(TEXT("%s - %.0f m %s"), *N, FMath::Sqrt(Dx * Dx + Dz * Dz), Dirs[Oct]));
		};
		for (const FPlace& Pl : Places) Line(Pl.Name, Pl.X, Pl.Z);
		if (USailingFeature* Sea = Sub<USailingFeature>(W)) for (const FTreasureChart& C : Sea->GetCharts()) Line(TEXT("Treasure X"), C.X, C.Z);
		Close(M); return;
	}
	if (P == TEXT("pad_context"))
	{
		M.Title = TEXT("Nearby");
		const FVector L = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		TArray<TPair<float, AActor*>> Near;
		for (TActorIterator<AActor> It(W); It; ++It)
			if (*It != Pawn && !It->IsHidden() && It->Implements<UGielinorInteractable>())
			{
				const float D = FVector::Dist2D(It->GetActorLocation(), L);
				if (D < 800.f) Near.Add({ D, *It });
			}
		Near.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B) { return A.Key < B.Key; });
		for (int32 i = 0; i < FMath::Min(8, Near.Num()); ++i)
		{
			AActor* A = Near[i].Value; IGielinorInteractable* I = Cast<IGielinorInteractable>(A);
			if (!I) continue;
			TWeakObjectPtr<AActor> WA = A; TWeakObjectPtr<APawn> WP = Pawn;
			Act(M, FString::Printf(TEXT("%s (%.0f m)"), *I->GetInteractLabel().ToString(), Near[i].Key / 100.f),
				[WA, WP] { if (WA.IsValid() && WP.IsValid()) if (IGielinorInteractable* X = Cast<IGielinorInteractable>(WA.Get())) X->Interact(WP.Get()); }, true);
		}
		if (!Near.Num()) M.Lines.Add(TEXT("Nothing within reach."));
		Close(M); return;
	}
	if (P == TEXT("companions"))
	{
		M.Title = TEXT("Sworn Companions");
		UCompanionsFeature* C = Sub<UCompanionsFeature>(W);
		if (!C) { Close(M); return; }
		for (const FCompanionDef& D : UCompanionsFeature::Defs())
		{
			const FCompanionState S = C->GetState(D.Id);
			const bool bActive = C->GetActive() == D.Id;
			M.Lines.Add(FString::Printf(TEXT("%s (%s): %s, bond %d%s"), *D.Name, *D.Role, S.bRecruited ? TEXT("sworn") : S.bStarted ? TEXT("recruiting") : TEXT("not met"), S.Bond, bActive ? TEXT(", travelling with you") : TEXT("")));
			if (S.bRecruited && !bActive) { const FName Id = D.Id; Act(M, FString::Printf(TEXT("Travel with %s"), *D.Name), [C, Id] { C->SetActive(Id); }); }
		}
		if (!C->GetActive().IsNone()) Act(M, TEXT("Send companion home"), [C] { C->SendHome(); });
		if (UErrandsFeature* E = Sub<UErrandsFeature>(W)) Act(M, TEXT("Collect finished errands"), [E] { E->CollectFinished(); });
		Close(M); return;
	}
	if (P == TEXT("saga"))
	{
		USagaFeature* S = Sub<USagaFeature>(W);
		M.Title = TEXT("Chronicler Corwin - The Wanderer's Saga");
		if (!S) { Close(M); return; }
		if (!S->GetTitle().IsEmpty()) M.Lines.Add(FString::Printf(TEXT("Known as %s"), *S->GetTitle()));
		const FString Gate = S->GateText(); if (!Gate.IsEmpty()) M.Lines.Add(Gate);
		const FString Obj = S->ObjectiveText(); if (!Obj.IsEmpty()) M.Lines.Add(FString::Printf(TEXT("%s: %d/%d"), *Obj, FMath::Min(S->Progress(), S->Need()), S->Need()));
		Act(M, TEXT("Talk"), [S, W] { const TArray<FString> L = S->Talk(); TArray<FText> T; for (const FString& X : L) T.Add(FText::FromString(X));
			if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->OnDialogue.Broadcast(FText::FromString(TEXT("Chronicler Corwin")), T); });
		if (S->ChaptersDone() == 0)
			for (const TCHAR* Id : { TEXT("warden"), TEXT("arcanist"), TEXT("ranger") })
			{ const FName N(Id); Act(M, FString::Printf(TEXT("Calling: %s"), Id), [S, N] { S->ChooseCalling(N); }); }
		if (S->ReadyToFinish())
			for (EWheelTone T : { EWheelTone::Kind, EWheelTone::Firm, EWheelTone::Cunning })
				Act(M, FString::Printf(TEXT("Finish the chapter (%s)"), ToneName(T)), [S, T, W] { const FString R = S->Finish(T);
					if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->Message(FText::FromString(R)); });
		Close(M); return;
	}
	if (P == TEXT("hearth"))
	{
		UHearthFeature* H = Sub<UHearthFeature>(W);
		M.Title = TEXT("Hearthstead");
		if (!H) { Close(M); return; }
		if (!H->HasDeed()) { M.Lines.Add(TEXT("A plot behind Larkhaven: 25,000 coins.")); Act(M, TEXT("Buy the deed (25,000 coins)"), [H] { H->BuyDeed(); }); Close(M); return; }
		struct FR { const TCHAR* Id; const TCHAR* Name; };
		static const FR Rooms[] = { { TEXT("workshop"), TEXT("Workshop (15k, 20 Oak logs, 10 Iron bars)") }, { TEXT("garden"), TEXT("Herb garden (10k, 20 Logs)") },
			{ TEXT("trophy"), TEXT("Trophy hall (30k, 10 Steel bars)") }, { TEXT("quarters"), TEXT("Companion quarters (20k, 30 Oak logs)") } };
		static const FR Furn[] = { { TEXT("bench"), TEXT("Oak workbench") }, { TEXT("anvil"), TEXT("Anvil stand") }, { TEXT("planter"), TEXT("Herb planter") }, { TEXT("trough"), TEXT("Watering trough") },
			{ TEXT("plinth"), TEXT("Mounting plinth") }, { TEXT("banners"), TEXT("Banner rack") }, { TEXT("bunks"), TEXT("Bunks") }, { TEXT("stove"), TEXT("Hearth stove") } };
		for (const FR& R : Rooms) { const FName Id(R.Id); if (!H->HasRoom(Id)) Act(M, FString::Printf(TEXT("Build %s"), R.Name), [H, Id] { H->BuildRoom(Id); }); else M.Lines.Add(FString::Printf(TEXT("Built: %s"), R.Name)); }
		for (const FR& F : Furn) { const FName Id(F.Id); if (!H->HasFurniture(Id)) Act(M, FString::Printf(TEXT("Build %s"), F.Name), [H, Id] { H->BuildFurniture(Id); }); }
		Act(M, TEXT("Harvest the garden"), [H] { H->TakeHarvest(); });
		Act(M, TEXT("Companion day at home"), [H] { H->TakeBondDay(); });
		for (const FString& T : H->GetTrophies()) M.Lines.Add(FString::Printf(TEXT("Trophy: %s"), *T));
		Close(M); return;
	}
	if (P == TEXT("shipwright"))
	{
		USailingFeature* S = Sub<USailingFeature>(W);
		M.Title = TEXT("Shipwright Halvard");
		if (!S) { Close(M); return; }
		M.Lines.Add(S->ShipwrightLine());
		auto Say = [W](const FString& R) { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		Act(M, S->GetHull() ? TEXT("Upgrade the hull") : TEXT("Build a Sloop"), [S, Say] { Say(S->BuildOrUpgrade()); });
		if (S->GetHull()) Act(M, TEXT("Patch the hull"), [S, Say] { Say(S->PatchHull()); });
		if (S->GetHull()) Act(M, TEXT("Hire a deckhand (150 coins)"), [S, Say] { Say(S->HireDeckhand()); });
		if (UCompanionsFeature* C = Sub<UCompanionsFeature>(W))
			for (const FCompanionDef& D : UCompanionsFeature::Defs())
				if (C->GetState(D.Id).bRecruited) { const FName Id = D.Id; Act(M, FString::Printf(TEXT("Officer: %s"), *D.Name), [S, Say, Id] { Say(S->ToggleOfficer(Id)); }); }
		if (S->GetHull()) Act(M, TEXT("Cast off into the Sound"), [S] { S->SetSail(); }, true);
		Close(M); return;
	}
	if (P == TEXT("sea"))
	{
		USailingFeature* S = Sub<USailingFeature>(W);
		M.bLive = true;
		M.Title = TEXT("At sea");
		if (!S) { Close(M); return; }
		const FSeaBoat B = S->GetShip();
		M.Lines.Add(FString::Printf(TEXT("%s  hull %d/%d  speed %.1f  foes %d"), *B.Name, B.Hp, B.MaxHp, B.Speed, S->GetEnemies().Num()));
		M.Lines.Add(FString::Printf(TEXT("Port %s   Starboard %s   (steer with move, A/D)"), S->ReloadLeft(true) > 0 ? *FString::Printf(TEXT("%.1fs"), S->ReloadLeft(true)) : TEXT("ready"),
			S->ReloadLeft(false) > 0 ? *FString::Printf(TEXT("%.1fs"), S->ReloadLeft(false)) : TEXT("ready")));
		if (S->IsAtSea())
		{
			Act(M, TEXT("Fire port broadside"), [S] { S->Fire(true); });
			Act(M, TEXT("Fire starboard broadside"), [S] { S->Fire(false); });
			Act(M, TEXT("Full sail"), [S] { S->Helm(0.f, 1.f); });
			Act(M, TEXT("Strike sail"), [S] { S->Helm(0.f, 0.f); });
		}
		else if (!S->GetMoored().IsNone()) Act(M, TEXT("Reboard your ship"), [S] { S->Reboard(); });
		Close(M); return;
	}
	if (P == TEXT("tides") || P == TEXT("crown"))
	{
		UCardsFeature* C = Sub<UCardsFeature>(W);
		if (!C) { Close(M); return; }
		FCardTable T = C->GetTable();
		if (!T.bOpen) { C->Open(P == TEXT("tides") ? ECardGame::Tides : ECardGame::Crown, NpcName.IsEmpty() ? TEXT("The dealer") : NpcName); T = C->GetTable(); }
		M.bLive = true;
		M.Title = FString::Printf(TEXT("%s - %s"), *T.Title, *T.Dealer);
		M.Lines.Add(FString::Printf(TEXT("Bet %d coins%s"), T.Bet, T.bLive ? *FString::Printf(TEXT(", stake %d"), T.Stake) : TEXT("")));
		M.Lines.Add(FString::Printf(TEXT("House: %s %s"), *Hand(T.House), T.Game == ECardGame::Tides && !T.bLive && T.HouseTotal ? *FString::Printf(TEXT("(%d)"), T.HouseTotal) : TEXT("")));
		M.Lines.Add(FString::Printf(TEXT("You:   %s %s"), *Hand(T.You), T.Game == ECardGame::Tides && T.YouTotal ? *FString::Printf(TEXT("(%d)"), T.YouTotal) : TEXT("")));
		if (!T.Result.IsEmpty()) M.Lines.Add(T.Result);
		if (!T.Msg.IsEmpty()) M.Lines.Add(T.Msg);
		if (!T.bLive)
		{
			Act(M, TEXT("Bet -"), [C] { C->BetDown(); }); Act(M, TEXT("Bet +"), [C] { C->BetUp(); }); Act(M, TEXT("Deal"), [C] { C->Deal(); });
		}
		else if (T.Game == ECardGame::Tides)
		{
			Act(M, TEXT("Hit"), [C] { C->Hit(); }); Act(M, TEXT("Stand"), [C] { C->Stand(); }); Act(M, TEXT("Double"), [C] { C->Double(); }, false, T.You.Num() == 2);
		}
		else
		{
			for (int32 i = 0; i < T.You.Num(); ++i) Act(M, FString::Printf(TEXT("%s %s"), T.Held.IsValidIndex(i) && T.Held[i] ? TEXT("Held") : TEXT("Hold"), *CardStr(T.You[i])), [C, i] { C->ToggleHold(i); });
			Act(M, TEXT("Draw"), [C] { C->Draw(); });
		}
		Act(M, TEXT("Leave the table"), [C] { C->Close(); }, true);
		return;
	}
	if (P == TEXT("hexer"))
	{
		UHexerFeature* H = Sub<UHexerFeature>(W);
		M.Title = TEXT("Mama Sorrel");
		M.Lines.Add(UHexerFeature::Greeting);
		M.Lines.Add(TEXT("Trade at her stall beside her."));
		auto Say = [W](const FString& R) { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->OnDialogue.Broadcast(FText::FromString(TEXT("Mama Sorrel")), { FText::FromString(R) }); };
		if (H)
		{
			Act(M, TEXT("Brew Bog runes (Rune essence + grimy herbs)"), [H, Say] { Say(H->BrewBog()); });
			Act(M, TEXT("Brew Bone runes (Rune essence + Bones)"), [H, Say] { Say(H->BrewBone()); });
		}
		Close(M); return;
	}
	if (P == TEXT("roost"))
	{
		UStableCodexFeature* C = Sub<UStableCodexFeature>(W);
		M.Title = TEXT("Roostmaster Hale - Skyrook flights");
		if (C) for (const FString& R : UStableCodexFeature::Roosts()) Act(M, FString::Printf(TEXT("Fly to %s"), *R), [C, R] { C->StartFlight(R); }, true);
		Close(M); return;
	}
	if (P == TEXT("charter"))
	{
		UCharterFeature* C = Sub<UCharterFeature>(W);
		M.Title = NpcName;
		if (!C) { Close(M); return; }
		C->Meet();
		M.Lines.Add(C->Greeting(NpcId));
		if (C->GetSide().IsNone())
		{
			Act(M, TEXT("Side with the Charter Company"), [C] { C->Resolve(TEXT("charter")); });
			Act(M, TEXT("Side with the commons"), [C] { C->Resolve(TEXT("commons")); });
			Act(M, TEXT("Broker the Spring Accord (Persuade 40)"), [C] { C->Resolve(TEXT("deal")); });
			Act(M, TEXT("Sell the deeds to both (Lie 30)"), [C] { C->Resolve(TEXT("betray")); });
		}
		const TArray<FCharterStall> St = C->GetStall();
		for (int32 i = 0; i < St.Num(); ++i) Act(M, FString::Printf(TEXT("Buy %d %s (%d coins)"), St[i].Count, *St[i].Item, St[i].Cost), [C, i] { C->Buy(i); });
		Close(M); return;
	}
	if (P == TEXT("siege"))
	{
		USiegeFeature* S = Sub<USiegeFeature>(W);
		M.Title = TEXT("Warcaptain Hesk - Warband Siege");
		if (!S) { Close(M); return; }
		M.Lines.Add(S->Greeting()); M.Lines.Add(USiegeFeature::HowItWorks());
		M.Lines.Add(FString::Printf(TEXT("Bosses downed this week: %d. Clears: %d."), S->DownedThisWeek(), S->GetClears()));
		if (!S->IsRunning()) Act(M, TEXT("Lead the warband in"), [S] { S->Enter(); }, true); else Act(M, TEXT("Fall back"), [S] { S->Leave(); });
		Close(M); return;
	}
	if (P == TEXT("tasks"))
	{
		UTasksFeature* T = Sub<UTasksFeature>(W);
		M.Title = TEXT("Huntmaster Corvane");
		if (!T) { Close(M); return; }
		M.Lines.Add(T->CurrentLabel()); M.Lines.Add(FString::Printf(TEXT("Points: %d"), T->GetPoints()));
		auto Say = [W](const FString& R) { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		Act(M, TEXT("Give me a hunt"), [T, Say] { Say(T->Assign()); }); Act(M, TEXT("Skip (30 pts)"), [T, Say] { Say(T->Skip()); });
		Act(M, TEXT("Block this target"), [T, Say] { Say(T->Block()); }); Act(M, TEXT("Extend"), [T, Say] { Say(T->Extend()); });
		for (const TCHAR* Id : { TEXT("insight"), TEXT("ring"), TEXT("fire"), TEXT("lamp"), TEXT("reefward"), TEXT("reefbane"), TEXT("saltring") })
		{ const FName N(Id); Act(M, FString::Printf(TEXT("Points shop: %s"), Id), [T, Say, N] { Say(T->Buy(N)); }); }
		Close(M); return;
	}
	if (P == TEXT("arena"))
	{
		UArenaFeature* A = Sub<UArenaFeature>(W);
		M.Title = TEXT("Arenamaster Sefa - Proving Grounds");
		if (!A) { Close(M); return; }
		M.Lines.Add(FString::Printf(TEXT("Rating %d, rank %d, tokens %d"), A->GetRating(), A->Rank(), A->GetTokens()));
		const TArray<FLadderEntry> L = A->ThisWeeksLadder();
		for (int32 i = 0; i < FMath::Min(5, L.Num()); ++i) M.Lines.Add(FString::Printf(TEXT("%d. %s %d"), i + 1, *L[i].Name, L[i].Rating));
		auto Say = [W](const FString& R) { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		for (const TCHAR* Id : { TEXT("cape"), TEXT("band"), TEXT("watch"), TEXT("tutor") }) { const FName N(Id); Act(M, FString::Printf(TEXT("Token shop: %s"), Id), [A, Say, N] { Say(A->Buy(N)); }); }
		Close(M); return;
	}
	if (P == TEXT("quirks"))
	{
		UQuirksFeature* Q = Sub<UQuirksFeature>(W);
		M.Title = TEXT("Apothecary Ysolde");
		if (!Q) { Close(M); return; }
		auto Say = [W](const FString& R) { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		if (!Q->GetOffered().IsEmpty()) { M.Lines.Add(Q->Describe(Q->GetOffered())); Act(M, TEXT("Accept the quirk"), [Q] { Q->Accept(); }); Act(M, TEXT("Decline"), [Q] { Q->Decline(); }); }
		for (const FString& H : Q->GetHeld()) { M.Lines.Add(Q->Describe(H)); Act(M, FString::Printf(TEXT("Remedy: %s"), *H), [Q, Say, H] { Say(Q->Remedy(H)); }); }
		if (!Q->GetHeld().Num() && Q->GetOffered().IsEmpty()) M.Lines.Add(TEXT("No scars or quirks to treat."));
		Close(M); return;
	}
	if (P == TEXT("writs"))
	{
		UWritsFeature* Wr = Sub<UWritsFeature>(W);
		M.Title = TEXT("Writkeeper Maelis");
		if (Wr) { Wr->MaybeRoll(); for (const FWrit& X : Wr->GetDaily()) M.Lines.Add(UWritsFeature::Label(X)); FWrit Wk; if (Wr->GetWeekly(Wk)) M.Lines.Add(TEXT("Hearthboard: ") + UWritsFeature::Label(Wk)); }
		Close(M); return;
	}
	if (P == TEXT("delves"))
	{
		UDelvesFeature* D = Sub<UDelvesFeature>(W);
		M.Title = TEXT("Delvemaster Corvin - Delves of Three");
		if (!D) { Close(M); return; }
		for (const TCHAR* R : { TEXT("tank"), TEXT("healer"), TEXT("dps") }) { const FName N(R); Act(M, FString::Printf(TEXT("Role: %s"), R), [D, N] { D->SetRole(N); }); }
		auto Say = [W](const FString& R) { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		Act(M, TEXT("Enter the Undercroft Sewers (combat 20)"), [D, Say] { Say(D->Enter(TEXT("sewers"))); }, true);
		Act(M, TEXT("Enter the Drowned Keep (combat 35)"), [D, Say] { Say(D->Enter(TEXT("keep"))); }, true);
		Close(M); return;
	}
	if (P == TEXT("paths"))
	{
		UPathsFeature* Pa = Sub<UPathsFeature>(W);
		M.Title = TEXT("Pathwarden Idris - Combat Paths");
		if (!Pa) { Close(M); return; }
		auto Say = [W](const FString& R) { if (UGielinorEvents* Ev = UGielinorEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		M.Lines.Add(FString::Printf(TEXT("Path: %s"), Pa->GetPath().IsNone() ? TEXT("none") : *Pa->GetPath().ToString()));
		for (const TCHAR* Id : { TEXT("bulwark"), TEXT("mender"), TEXT("striker") }) { const FName N(Id); Act(M, FString::Printf(TEXT("Walk the %s path"), Id), [Pa, Say, N] { Say(Pa->Choose(N)); }); }
		for (int32 r = 0; r < Pa->RowsOpen(); ++r)
			for (int32 c = 0; c < 3; ++c) Act(M, FString::Printf(TEXT("Row %d: %s"), r + 1, *UPathsFeature::TalentName(r, c)), [Pa, Say, r, c] { Say(Pa->PickTalent(r, c)); });
		Close(M); return;
	}
	if (P == TEXT("harbour"))
	{
		M.Title = NpcName;
		M.Lines.Add(TEXT("Your ship is tied up at the dock. Reboard it to sail on."));
		if (USailingFeature* S = Sub<USailingFeature>(W)) if (!S->GetMoored().IsNone()) Act(M, TEXT("Reboard your ship"), [S] { S->Reboard(); }, true);
		Close(M); return;
	}
	if (P == TEXT("shop"))
	{
		AShopCounter* Shop = Cast<AShopCounter>(Ctx);
		UShopDef* Def = Shop ? Shop->GetShop() : nullptr;
		M.Title = Shop ? Shop->DisplayName.ToString() : TEXT("Shop");
		if (Def && Pawn)
		{
			TWeakObjectPtr<AShopCounter> WS = Shop; TWeakObjectPtr<APawn> WP = Pawn;
			for (int32 Idx : Shop->VisibleStock(Pawn))
			{
				const FShopStock& St = Def->Stock[Idx];
				if (!St.Item) continue;
				const FString N = St.Item->DisplayName.ToString(); const int32 Price = Shop->PriceOf(Pawn, Idx);
				Act(M, FString::Printf(TEXT("Buy %s (%d)"), *N, Price), [WS, WP, Idx] { if (WS.IsValid() && WP.IsValid()) WS->Buy(WP.Get(), Idx, 1); });
				Act(M, FString::Printf(TEXT("Buy 10 %s"), *N), [WS, WP, Idx] { if (WS.IsValid() && WP.IsValid()) WS->Buy(WP.Get(), Idx, 10); });
			}
			if (PS && PS->Inventory)
			{
				TArray<UItemDef*> Seen;
				for (const FItemStack& S : PS->Inventory->GetSlots())
					if (!S.IsEmpty() && !Seen.Contains(S.Item.Get()) && S.Item->DisplayName.ToString() != TEXT("Coins"))
					{ Seen.Add(S.Item.Get()); UItemDef* It = S.Item.Get(); Act(M, FString::Printf(TEXT("Sell %s"), *It->DisplayName.ToString()), [WS, WP, It] { if (WS.IsValid() && WP.IsValid()) WS->Sell(WP.Get(), It, 1); }); }
			}
		}
		Close(M); return;
	}
	// "say" and anything unknown: the NPC's line.
	M.Title = NpcName.IsEmpty() ? P : NpcName;
	if (Npc && Npc->Definition) for (const FText& L : Npc->Definition->IdleLines) M.Lines.Add(L.ToString());
	Close(M);
}
