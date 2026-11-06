#include "UI/AerondelPanels.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "World/AerondelRelevance.h"
#include "Models/AerondelModelLibrary.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
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
#include "Features/RelicryFeature.h"
#include "Features/RelicryData.h"
#include "Features/DelvingFeature.h"
#include "Features/GuildFeature.h"
#include "Features/SeasonalEventsFeature.h"
#include "Features/VenturesFeature.h"
#include "Activities/ActivityActors.h"
#include "Interaction/AerondelInteractable.h"
#include "Interaction/NpcTalk.h"
#include "Core/AerondelPlayerState.h"
#include "Core/AerondelEvents.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/SkillsComponent.h"
#include "Player/AerondelGamepadComponent.h"
#include "World/AerondelScalability.h"
#include "Data/ItemDef.h"
#include "Data/NpcDef.h"
#include "Data/QuestDef.h"
#include "Quests/QuestComponent.h"
#include "Data/ContentDefs.h"
#include "Data/GameDatabase.h"
#include "Features/AerondelFeature.h"
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
	/** Runs a feature call and shows its reply in the message log. */
	TFunction<void()> Say(UWorld* W, TFunction<FString()> F)
	{ return [W, F] { const FString R = F(); if (!R.IsEmpty()) if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); }; }
	const TCHAR* ToneName(EWheelTone T) { return T == EWheelTone::Kind ? TEXT("Kind") : T == EWheelTone::Firm ? TEXT("Firm") : TEXT("Cunning"); }
	FString CardStr(const FPlayingCard& C)
	{
		if (C.Rank < 0) return TEXT("[?]");
		static const TCHAR* R[13] = { TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("J"), TEXT("Q"), TEXT("K"), TEXT("A") };
		static const TCHAR* S[4] = { TEXT("Anchor"), TEXT("Shell"), TEXT("Skull"), TEXT("Compass") };
		return FString::Printf(TEXT("[%s %s]"), R[FMath::Clamp(C.Rank, 0, 12)], S[FMath::Clamp(C.Suit, 0, 3)]);
	}
	// Quest journal paging (web quest tab: 1,245 quests are paged, not listed at once).
	int32 GQuestPage = 0, GQuestFilter = 0, GQuestTown = -1;
	FString Hand(const TArray<FPlayingCard>& H) { FString S; for (const FPlayingCard& C : H) S += CardStr(C) + TEXT(" "); return S; }
}

void FAerondelPanels::Build(UWorld* W, FName Panel, AActor* Ctx, FGPanelModel& M)
{
	M = FGPanelModel();
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(W, 0);
	AAerondelPlayerState* PS = Pawn ? Pawn->GetPlayerState<AAerondelPlayerState>() : nullptr;
	const FString P = Panel.ToString();
	const ANpcTalk* Npc = Cast<ANpcTalk>(Ctx);
	const FString NpcName = Npc && Npc->Definition ? Npc->Definition->DisplayName.ToString() : FString();
	const FName NpcId = Npc && Npc->Definition ? Npc->Definition->Id : NAME_None;

	// Ventures (J): every ventures* panel and the Ventures NPC services.
	if (UVenturesFeature::IsVenturePanel(P)) if (UVenturesFeature* V = Sub<UVenturesFeature>(W)) if (V->BuildPanel(P, M)) return;

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
		auto Go = [W](const TCHAR* Panel) { const FName N(Panel); return [W, N] { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->OnServiceRequested.Broadcast(N, nullptr); }; };
		Act(M, TEXT("Quest journal"), Go(TEXT("quests")));
		Act(M, TEXT("Companions"), Go(TEXT("companions")));
		Act(M, TEXT("Relicry"), Go(TEXT("relicry")));
		Act(M, TEXT("Delving"), Go(TEXT("delving")));
		Act(M, TEXT("Guild"), Go(TEXT("guild")));
		Act(M, TEXT("Ventures (J)"), Go(TEXT("ventures")));
		if (USeasonalEventsFeature::ActiveIndices().Num() > 0) Act(M, TEXT("Festival"), Go(TEXT("festival")));
		if (UHearthFeature* H = Sub<UHearthFeature>(W)) if (H->HasDeed()) Act(M, TEXT("Hearthstead"), Go(TEXT("hearth")));
		Close(M); return;
	}
	if (P == TEXT("pad_settings"))
	{
		M.Title = TEXT("Settings");
		const EAerondelTier Tier = UAerondelScalability::GetSavedTier();
		static const TCHAR* TierNames[4] = { TEXT("Quest"), TEXT("Low"), TEXT("High"), TEXT("Ultra") };
		M.Lines.Add(FString::Printf(TEXT("Graphics: %s. Low never loads High assets; Lumen, sky and foliage upgrades are High/Ultra only."), TierNames[(int32)Tier]));
		{
			const AerondelRelevance::FBands Bd = AerondelRelevance::ForTier(Tier);
			M.Lines.Add(FString::Printf(TEXT("Characters and resources: full detail within %.0f m, reduced to %.0f m, asleep beyond (anything you fight or use stays awake)."), Bd.Near / 100.f, Bd.Mid / 100.f));
		}
		for (EAerondelTier T : { EAerondelTier::Low, EAerondelTier::High, EAerondelTier::Ultra })
			Act(M, FString::Printf(TEXT("%sGraphics: %s"), T == Tier ? TEXT("> ") : TEXT(""), TierNames[(int32)T]), [W, T] { UAerondelScalability::SetAndSaveTier(W, T); });
		if (Tier >= EAerondelTier::High)
		{
			const bool bNanite = UAerondelScalability::GetNaniteOptIn();
			Act(M, FString::Printf(TEXT("Nanite (experimental, restart): %s"), bNanite ? TEXT("on") : TEXT("off")), [bNanite] { UAerondelScalability::SetNaniteOptIn(!bNanite); });
		}
		// Window mode + resolution (engine GameUserSettings; saved to GameUserSettings.ini, applied immediately).
		if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			const EWindowMode::Type Mode = GUS->GetFullscreenMode();
			const FIntPoint Res = GUS->GetScreenResolution();
			static const TCHAR* ModeNames[3] = { TEXT("Fullscreen"), TEXT("Borderless window"), TEXT("Window") };
			M.Lines.Add(FString::Printf(TEXT("Display: %s, %dx%d (F10 / Esc opens this menu)"), ModeNames[FMath::Clamp((int32)Mode, 0, 2)], Res.X, Res.Y));
			Act(M, FString::Printf(TEXT("Window mode: %s (change)"), ModeNames[FMath::Clamp((int32)Mode, 0, 2)]), [GUS, Mode]
			{
				const EWindowMode::Type Next = Mode == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen : Mode == EWindowMode::WindowedFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed;
				GUS->SetFullscreenMode(Next);
				if (Next == EWindowMode::WindowedFullscreen) GUS->SetScreenResolution(GUS->GetDesktopResolution());
				GUS->ApplySettings(false);
			});
			static const FIntPoint Presets[] = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
			const FIntPoint Desk = GUS->GetDesktopResolution();
			for (const FIntPoint& R : Presets)
			{
				if (Desk.X > 0 && (R.X > Desk.X || R.Y > Desk.Y)) continue; // never offer more than the monitor
				Act(M, FString::Printf(TEXT("%sResolution %dx%d"), R == Res ? TEXT("> ") : TEXT(""), R.X, R.Y), [GUS, R]
				{
					if (GUS->GetFullscreenMode() == EWindowMode::WindowedFullscreen) GUS->SetFullscreenMode(EWindowMode::Windowed);
					GUS->SetScreenResolution(R);
					GUS->ApplySettings(false);
				});
			}
		}
		M.Lines.Add(TEXT("Camera: hold right (or middle) mouse and move to look around; wheel zooms."));
		{
			const int32 BP = UAerondelModelLibrary::GetBindPoseMode();
			ACharacter* PlayerChar = Cast<ACharacter>(Pawn);
			USkeletalMeshComponent* PlayerMesh = PlayerChar ? PlayerChar->GetMesh() : nullptr;
			Act(M, FString::Printf(TEXT("Debug: player bind pose, no animation (pose test): %s"), BP >= 1 ? TEXT("on") : TEXT("off")), [BP, WeakMesh = TWeakObjectPtr<USkeletalMeshComponent>(PlayerMesh)] { UAerondelModelLibrary::SetBindPoseMode(BP >= 1 ? 0 : 1, WeakMesh.Get()); });
		}
		if (UTrueAimFeature* Aim = Sub<UTrueAimFeature>(W))
			Act(M, FString::Printf(TEXT("True Aim: %s"), Aim->IsOn() ? TEXT("on") : TEXT("off")), [Aim] { Aim->Toggle(); });
		if (UAerondelGamepadComponent* Pad = Pawn ? Pawn->FindComponentByClass<UAerondelGamepadComponent>() : nullptr)
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
		// Towns come from world.json (Db->Towns); the sea/feature sites are not towns.
		static const FPlace Sites[] = { { TEXT("The Wilds"), 122, -122 }, { TEXT("Gullrest"), -70, 960 }, { TEXT("Brineholt"), 80, 1030 }, { TEXT("Wreckers' Reef"), 10, 1120 } };
		static const TCHAR* Dirs[8] = { TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW"), TEXT("N"), TEXT("NE") }; // web z grows south
		auto Line = [&](const FString& N, float X, float Z)
		{
			const float Dx = X - L.X / 100.f, Dz = Z - L.Y / 100.f;
			const int32 Oct = (FMath::RoundToInt(FMath::Atan2(Dz, Dx) / (PI / 4.f)) + 8) % 8;
			M.Lines.Add(FString::Printf(TEXT("%s - %.0f m %s"), *N, FMath::Sqrt(Dx * Dx + Dz * Dz), Dirs[Oct]));
		};
		if (const UGameDatabase* Db = UGameDatabase::Get())
			for (const FTownInfo& Town : Db->Towns) Line(Town.DisplayName.ToString(), (float)Town.Position.X, (float)Town.Position.Y);
		for (const FPlace& Pl : Sites) Line(Pl.Name, Pl.X, Pl.Z);
		M.Lines.Add(TEXT("Waystones in every town square carry you to any other town."));
		if (USailingFeature* Sea = Sub<USailingFeature>(W)) for (const FTreasureChart& C : Sea->GetCharts()) Line(TEXT("Treasure X"), C.X, C.Z);
		Close(M); return;
	}
	if (P == TEXT("pad_context"))
	{
		M.Title = TEXT("Nearby");
		const FVector L = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		TArray<TPair<float, AActor*>> Near;
		for (TActorIterator<AActor> It(W); It; ++It)
			if (*It != Pawn && !It->IsHidden() && It->Implements<UAerondelInteractable>())
			{
				const float D = FVector::Dist2D(It->GetActorLocation(), L);
				if (D < 800.f) Near.Add({ D, *It });
			}
		Near.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B) { return A.Key < B.Key; });
		for (int32 i = 0; i < FMath::Min(8, Near.Num()); ++i)
		{
			AActor* A = Near[i].Value; IAerondelInteractable* I = Cast<IAerondelInteractable>(A);
			if (!I) continue;
			TWeakObjectPtr<AActor> WA = A; TWeakObjectPtr<APawn> WP = Pawn;
			Act(M, FString::Printf(TEXT("%s (%.0f m)"), *I->GetInteractLabel().ToString(), Near[i].Key / 100.f),
				[WA, WP] { if (WA.IsValid() && WP.IsValid()) if (IAerondelInteractable* X = Cast<IAerondelInteractable>(WA.Get())) X->Interact(WP.Get()); }, true);
		}
		if (!Near.Num()) M.Lines.Add(TEXT("Nothing within reach."));
		Close(M); return;
	}
	if (P == TEXT("quests"))
	{
		static const TCHAR* Filters[4] = { TEXT("All"), TEXT("In progress"), TEXT("Not started"), TEXT("Completed") };
		static constexpr int32 PerPage = 12;
		M.Title = TEXT("Quest journal");
		const UGameDatabase* Db = UGameDatabase::Get();
		UQuestComponent* QC = PS ? PS->Quests : nullptr;
		if (!Db || !QC) { M.Lines.Add(TEXT("No quest data loaded.")); Close(M); return; }
		TArray<FName> Towns;
		for (const UQuestDef* Q : Db->Quests) if (Q && !Q->Town.IsNone()) Towns.AddUnique(Q->Town);
		if (GQuestTown >= Towns.Num()) GQuestTown = -1;
		TArray<const UQuestDef*> List; int32 Done = 0;
		for (const UQuestDef* Q : Db->Quests)
		{
			if (!Q) continue;
			const EQuestState St = QC->GetState(Q);
			if (St == EQuestState::Completed) ++Done;
			if (GQuestTown >= 0 && Q->Town != Towns[GQuestTown]) continue;
			if ((GQuestFilter == 1 && St != EQuestState::InProgress) || (GQuestFilter == 2 && St != EQuestState::NotStarted) || (GQuestFilter == 3 && St != EQuestState::Completed)) continue;
			List.Add(Q);
		}
		const int32 Pages = FMath::Max(1, FMath::DivideAndRoundUp(List.Num(), PerPage));
		GQuestPage = FMath::Clamp(GQuestPage, 0, Pages - 1);
		M.Lines.Add(FString::Printf(TEXT("%d/%d quests done, %d quest points. Showing %s%s%s: page %d/%d (%d quests)."), Done, Db->Quests.Num(), QC->QuestPoints(),
			Filters[GQuestFilter], GQuestTown >= 0 ? TEXT(" in ") : TEXT(""), GQuestTown >= 0 ? *Towns[GQuestTown].ToString() : TEXT(""), GQuestPage + 1, Pages, List.Num()));
		for (int32 I = GQuestPage * PerPage; I < FMath::Min(List.Num(), (GQuestPage + 1) * PerPage); ++I)
		{
			const UQuestDef* Q = List[I];
			const EQuestState St = QC->GetState(Q);
			FString L = FString::Printf(TEXT("%s %s (%s, %d QP, %s)"), St == EQuestState::Completed ? TEXT("[done]") : St == EQuestState::InProgress ? TEXT("[>>]") : TEXT("[  ]"),
				*Q->DisplayName.ToString(), Q->Difficulty.IsEmpty() ? TEXT("Novice") : *Q->Difficulty.ToString(), Q->QuestPoints, Q->Town.IsNone() ? TEXT("Aerondel") : *Q->Town.ToString());
			if (St == EQuestState::InProgress) L += TEXT(" - ") + QC->CurrentStepText(Q).ToString();
			else if (St == EQuestState::NotStarted && !Q->StartText.IsEmpty()) L += TEXT(" - ") + Q->StartText.ToString();
			if (!Q->PermanentBonusText.IsEmpty()) L += TEXT(" Bonus: ") + Q->PermanentBonusText.ToString(); // most generated quests have no bonus
			M.Lines.Add(L);
		}
		Act(M, TEXT("Previous page"), [] { --GQuestPage; }, false, GQuestPage > 0);
		Act(M, TEXT("Next page"), [] { ++GQuestPage; }, false, GQuestPage < Pages - 1);
		Act(M, FString::Printf(TEXT("Show: %s"), Filters[(GQuestFilter + 1) % 4]), [] { GQuestFilter = (GQuestFilter + 1) % 4; GQuestPage = 0; });
		const int32 NT = Towns.Num();
		Act(M, FString::Printf(TEXT("Town: %s"), GQuestTown + 1 < NT ? *Towns[GQuestTown + 1].ToString() : TEXT("all towns")), [NT] { GQuestTown = GQuestTown + 1 < NT ? GQuestTown + 1 : -1; GQuestPage = 0; });
		Close(M); return;
	}
	if (P == TEXT("relicry"))
	{
		M.Title = TEXT("Relicry");
		URelicryFeature* R = Sub<URelicryFeature>(W);
		if (!R) { Close(M); return; }
		if (R->IsInRun())
		{
			M.Lines.Add(TEXT("Relic dungeon run: pass the gates in order, beat the Relic colossus, open the reliquary."));
			const TArray<FRelicGate> G = R->GetGates();
			for (int32 I = 0; I < G.Num(); ++I)
			{
				M.Lines.Add(FString::Printf(TEXT("%s (%s %d)%s"), *G[I].Name, *G[I].Skill, G[I].Need, G[I].bOpen ? TEXT(" - open") : TEXT("")));
				if (!G[I].bOpen) { Act(M, FString::Printf(TEXT("Pass: %s"), *G[I].Name), Say(W, [R, I] { return R->TryGate(I); })); Act(M, FString::Printf(TEXT("Use a key: %s"), *G[I].Name), Say(W, [R, I] { return R->TryGate(I, true); })); }
			}
			Act(M, TEXT("Open the reliquary"), Say(W, [R] { return R->OpenChest(); }));
			Act(M, TEXT("Leave the dungeon"), [R] { R->Leave(); }, true);
			Close(M); return;
		}
		M.Lines.Add(FString::Printf(TEXT("Relicry level %d, %d dungeon runs."), PS && PS->Skills ? PS->Skills->GetLevelById(TEXT("relicry")) : 1, R->GetRuns()));
		const int32 Near = R->NearestSite();
		const int32 NS = (int32)UE_ARRAY_COUNT(RelicryData::Sites);
		for (int32 I = 0; I < NS; ++I) M.Lines.Add(FString::Printf(TEXT("%s (lvl %d): %d collections%s"), RelicryData::Sites[I].Name, RelicryData::Sites[I].Level, R->GetCollections(RelicryData::Sites[I].Name), I == Near ? TEXT(" - you are here") : TEXT("")));
		if (Near < 0) M.Lines.Add(TEXT("Walk to a dig site to excavate, restore and enter its vault."));
		else
		{
			Act(M, TEXT("Excavate"), Say(W, [R, Near] { return R->StartDig(Near); }), true);
			for (int32 A = 0; A < 3; ++A) Act(M, FString::Printf(TEXT("Restore %s"), RelicryData::Sites[Near].Arts[A].Name), Say(W, [R, Near, A] { return R->Restore(Near, A); }));
			Act(M, TEXT("Turn in a collection"), Say(W, [R, Near] { return R->TurnIn(Near); }));
			Act(M, FString::Printf(TEXT("Enter the vault (dungeon level %d)"), R->DungeonLevel(Near)), Say(W, [R, Near] { return R->EnterVault(Near); }), true);
			for (const RelicryData::FReforge& F : RelicryData::Reforges)
				if (FString(F.Site) == RelicryData::Sites[Near].Name) { const FString G = F.Gear; Act(M, FString::Printf(TEXT("Reforge %s (%d fragments, lvl %d)"), F.Gear, F.Frags, F.Level), Say(W, [R, G] { return R->Reforge(G); })); }
		}
		Close(M); return;
	}
	if (P == TEXT("delving"))
	{
		M.Title = TEXT("Delver's Descent");
		UDelvingFeature* D = Sub<UDelvingFeature>(W);
		if (!D) { Close(M); return; }
		M.Lines.Add(FString::Printf(TEXT("%d delving tokens. Difficulty up to %d unlocked."), D->GetTokens(), D->Unlocked()));
		if (D->IsInRun())
		{
			for (const TCHAR* Sk : { TEXT("Mining"), TEXT("Woodcutting"), TEXT("Fishing") }) { const FString S = Sk; Act(M, FString::Printf(TEXT("Gather (%s)"), Sk), Say(W, [D, S] { return D->Gather(S); }), true); }
			const TArray<FString> Rec = D->GetRecipes();
			for (int32 I = 0; I < Rec.Num(); ++I) Act(M, TEXT("Forge: ") + Rec[I], Say(W, [D, I] { return D->Forge(I); }));
			Act(M, TEXT("Cook at the campfire"), Say(W, [D] { return D->Cook(); }));
			const TArray<FString> Keys = D->GetKeysOnFloor();
			for (int32 I = 0; I < Keys.Num(); ++I) Act(M, FString::Printf(TEXT("Take the %s key"), *Keys[I]), Say(W, [D, I] { return D->TakeKey(I); }));
			const TArray<FDelveDoor> Doors = D->GetDoors();
			for (int32 I = 0; I < Doors.Num(); ++I) if (!Doors[I].bOpen)
			{
				const FString L = Doors[I].Kind == TEXT("key") ? FString::Printf(TEXT("Unlock the %s door"), *Doors[I].Key) : Doors[I].Kind == TEXT("gate") ? FString::Printf(TEXT("%s (%s %d)"), *Doors[I].Name, *Doors[I].Skill, Doors[I].Need) : FString(TEXT("Open the guardian's door"));
				Act(M, L, Say(W, [D, I] { return D->OpenDoor(I); }));
			}
			Act(M, TEXT("Leave the floor"), [D] { D->Leave(); }, true);
		}
		else
		{
			for (const TCHAR* Sz : { TEXT("small"), TEXT("medium"), TEXT("large") })
				for (int32 Df : TSet<int32>({ 1, FMath::Max(1, D->Unlocked() / 2), D->Unlocked() }))
				{ const FName S(Sz); Act(M, FString::Printf(TEXT("Enter a %s floor, difficulty %d"), Sz, Df), Say(W, [D, S, Df] { return D->Enter(S, Df); }), true); }
			const TArray<FString> Shop = D->GetShop();
			for (int32 I = 0; I < Shop.Num(); ++I) Act(M, TEXT("Buy ") + Shop[I], Say(W, [D, I] { return D->Buy(I); }));
		}
		Close(M); return;
	}
	if (P == TEXT("guild"))
	{
		M.Title = TEXT("Guildmaster Halvard");
		UGuildFeature* G = Sub<UGuildFeature>(W);
		if (!G) { Close(M); return; }
		M.Lines.Add(G->Status());
		if (!G->HasGuild())
		{
			const TArray<FString> Names = UGuildFeature::CharterNames();
			for (int32 I = 0; I < Names.Num(); ++I) Act(M, FString::Printf(TEXT("Found \"%s\" (5,000 coins)"), *Names[I]), Say(W, [G, I] { return G->Found(I); }));
			Close(M); return;
		}
		for (int32 C : { 1000, 10000 }) Act(M, FString::Printf(TEXT("Deposit %d coins"), C), Say(W, [G, C] { return G->Deposit(C); }));
		for (int32 I = 0; I < UGuildFeature::NumRooms(); ++I) Act(M, G->RoomLabel(I), Say(W, [G, I] { return G->UpgradeRoom(I); }));
		for (int32 I = 0; I < UGuildFeature::NumBoosts(); ++I) Act(M, G->BoostLabel(I), Say(W, [G, I] { return G->RaiseBoost(I); }));
		Act(M, TEXT("Hire an adventurer"), Say(W, [G] { return G->Hire(); }));
		Act(M, TEXT("Hand in guild crests"), Say(W, [G] { return G->TurnInCrests(); }));
		Close(M); return;
	}
	if (P == TEXT("festival"))
	{
		USeasonalEventsFeature* F = Sub<USeasonalEventsFeature>(W);
		M.Title = TEXT("Festival Herald Odelyn");
		if (!F) { Close(M); return; }
		M.Lines.Add(F->Greeting());
		const TArray<int32> L = USeasonalEventsFeature::ActiveIndices();
		if (L.Num() == 0) { Close(M); return; }
		M.Lines.Add(TEXT("Today's task: ") + F->TaskText());
		Act(M, TEXT("I finished the festival task."), Say(W, [F] { return F->TurnInTask(); }));
		for (int32 E : L)
		{
			const FString Tok = USeasonalEventsFeature::EventToken(E);
			M.Lines.Add(FString::Printf(TEXT("%s stall: you have %d %ss"), *USeasonalEventsFeature::EventName(E), F->TokenCount(E), *Tok.ToLower()));
			for (int32 I = 0; I < USeasonalEventsFeature::NumShopItems(E); ++I) Act(M, F->ShopLabel(E, I), Say(W, [F, E, I] { return F->Buy(E, I); }));
			Act(M, FString::Printf(TEXT("Blessing: 25 %ss for XP in your weakest skill"), *Tok.ToLower()), Say(W, [F, E] { return F->Blessing(E); }));
		}
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
			if (S.bRecruited && !bActive) { const FName Id = D.Id; Act(M, (C->InParty(Id) ? FString::Printf(TEXT("Party: dismiss %s"), *D.Name) : FString::Printf(TEXT("Party: bring %s along"), *D.Name)), [C, Id] { C->TogglePartyMember(Id); }); }
		}
		if (!C->GetActive().IsNone()) Act(M, TEXT("Send companion home"), [C] { C->SendHome(); });
		Act(M, TEXT("Call the whole party"), [C, W] { const FString R = C->CallWholeParty(); if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); });
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
			if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->OnDialogue.Broadcast(FText::FromString(TEXT("Chronicler Corwin")), T); });
		if (S->ChaptersDone() == 0)
			for (const TCHAR* Id : { TEXT("warden"), TEXT("arcanist"), TEXT("ranger") })
			{ const FName N(Id); Act(M, FString::Printf(TEXT("Calling: %s"), Id), [S, N] { S->ChooseCalling(N); }); }
		if (S->ReadyToFinish())
			for (EWheelTone T : { EWheelTone::Kind, EWheelTone::Firm, EWheelTone::Cunning })
				Act(M, FString::Printf(TEXT("Finish the chapter (%s)"), ToneName(T)), [S, T, W] { const FString R = S->Finish(T);
					if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); });
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
		auto Say = [W](const FString& R) { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); };
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
		auto Say = [W](const FString& R) { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->OnDialogue.Broadcast(FText::FromString(TEXT("Mama Sorrel")), { FText::FromString(R) }); };
		if (H)
		{
			Act(M, TEXT("Brew Bog runes (Glyph essence + grimy herbs)"), [H, Say] { Say(H->BrewBog()); });
			Act(M, TEXT("Brew Bone runes (Glyph essence + Bones)"), [H, Say] { Say(H->BrewBone()); });
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
		auto Say = [W](const FString& R) { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); };
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
		auto Say = [W](const FString& R) { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		for (const TCHAR* Id : { TEXT("cape"), TEXT("band"), TEXT("watch"), TEXT("tutor") }) { const FName N(Id); Act(M, FString::Printf(TEXT("Token shop: %s"), Id), [A, Say, N] { Say(A->Buy(N)); }); }
		Close(M); return;
	}
	if (P == TEXT("quirks"))
	{
		UQuirksFeature* Q = Sub<UQuirksFeature>(W);
		M.Title = TEXT("Apothecary Ysolde");
		if (!Q) { Close(M); return; }
		auto Say = [W](const FString& R) { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); };
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
		auto Say = [W](const FString& R) { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); };
		Act(M, TEXT("Enter the Undercroft Sewers (combat 20)"), [D, Say] { Say(D->Enter(TEXT("sewers"))); }, true);
		Act(M, TEXT("Enter the Drowned Keep (combat 35)"), [D, Say] { Say(D->Enter(TEXT("keep"))); }, true);
		Close(M); return;
	}
	if (P == TEXT("paths"))
	{
		UPathsFeature* Pa = Sub<UPathsFeature>(W);
		M.Title = TEXT("Pathwarden Idris - Combat Paths");
		if (!Pa) { Close(M); return; }
		auto Say = [W](const FString& R) { if (UAerondelEvents* Ev = UAerondelEvents::Get(W)) Ev->Message(FText::FromString(R)); };
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
	if (P == TEXT("travel"))
	{
		// Town waystone (NPC service "travel"): free fast travel to any other town's waystone.
		const UGameDatabase* Db = UGameDatabase::Get();
		const FTownInfo* Here = nullptr; float HereD = 1e12f;
		const FVector At = Ctx ? Ctx->GetActorLocation() : (Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector);
		if (Db)
			for (const FTownInfo& Town : Db->Towns)
			{
				const float D = (float)FVector2D::DistSquared(Town.Position, FVector2D(At.X / 100.0, At.Y / 100.0));
				if (D < HereD) { HereD = D; Here = &Town; }
			}
		M.Title = Here ? FString::Printf(TEXT("%s Waystone"), *Here->DisplayName.ToString()) : TEXT("Waystone");
		if (Here && !Here->Blurb.IsEmpty()) M.Lines.Add(Here->Blurb.ToString());
		M.Lines.Add(TEXT("Touch the stone and name a town."));
		if (Db && Pawn)
		{
			const TWeakObjectPtr<APawn> WP = Pawn;
			for (const FTownInfo& Town : Db->Towns)
			{
				if (&Town == Here) continue;
				const FVector2D Dest = Town.TravelPoint;
				const FString Name = Town.DisplayName.ToString();
				Act(M, FString::Printf(TEXT("Travel to %s"), *Name), [WP, Dest]
				{
					APawn* Pw = WP.Get();
					if (!Pw) return;
					Pw->TeleportTo(UAerondelFeature::WebToWorld(Dest.X, Dest.Y + 2.0, 1.2), Pw->GetActorRotation());
				}, true);
			}
		}
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
