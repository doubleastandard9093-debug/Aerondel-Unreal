#include "UI/GielinorHUD.h"
#include "UI/GielinorWidgetBase.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Player/GielinorPlayerController.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Bank/BankComponent.h"
#include "Quests/QuestComponent.h"
#include "Combat/HealthComponent.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Data/QuestDef.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Font.h"
#include "XR/GielinorVRPawn.h"
#include "Features/TrueAimFeature.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "UI/GielinorPanelWidget.h"
#include "Activities/ActivityActors.h"
#include "Features/SailingFeature.h"

void AGielinorHUD::BeginPlay()
{
	Super::BeginPlay();
	if (HudWidgetClass && PlayerOwner)
	{
		Widget = CreateWidget<UGielinorHudWidget>(PlayerOwner, HudWidgetClass);
		if (Widget) Widget->AddToViewport();
	}
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		Ev->OnMessage.AddDynamic(this, &AGielinorHUD::OnMsg);
		Ev->OnDialogue.AddDynamic(this, &AGielinorHUD::OnDlg);
		Ev->OnServiceRequested.AddDynamic(this, &AGielinorHUD::OnService);
	}
	if (UGameInstance* GI = GetGameInstance())
		if (UTrueAimFeature* Aim = GI->GetSubsystem<UTrueAimFeature>()) { Aim->OnStrike.AddDynamic(this, &AGielinorHUD::OnAimStrike); bAimBound = true; }
}

static FName PadPanelName(EGielinorPadMenu M)
{
	switch (M)
	{
	case EGielinorPadMenu::Settings: return TEXT("pad_settings");
	case EGielinorPadMenu::WorldMap: return TEXT("pad_map");
	case EGielinorPadMenu::ContextMenu: return TEXT("pad_context");
	default: return TEXT("pad_inventory");
	}
}

void AGielinorHUD::OpenPanel(FName PanelName, AActor* Context)
{
	if (!PlayerOwner || PlayerOwner->GetPawn<AGielinorVRPawn>()) return; // VR uses the wrist widget
	if (!Panel)
	{
		Panel = CreateWidget<UGielinorPanelWidget>(PlayerOwner, UGielinorPanelWidget::StaticClass());
		if (!Panel) return;
		Panel->OnClosed.BindUObject(this, &AGielinorHUD::ClosePanel);
	}
	if (!Panel->IsInViewport()) Panel->AddToViewport(50);
	Panel->Open(PanelName, Context);
}

void AGielinorHUD::ClosePanel()
{
	if (Panel && Panel->IsInViewport()) Panel->RemoveFromParent();
	if (bMenuOpen && Widget) Widget->ShowPadMenu(OpenMenu, false);
	bMenuOpen = false;
	if (PlayerOwner) { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); PlayerOwner->SetInputMode(Mode); }
}

void AGielinorHUD::OnService(FName Service, AActor* Source)
{
	FString S = Service.ToString();
	if (S.StartsWith(TEXT("shop:")))
	{
		// Shop NPCs stand beside their counter: open the nearest counter that sells that shop.
		const FName ShopId(*S.Mid(5)); AShopCounter* Best = nullptr; float BestD = 1e12f;
		for (TActorIterator<AShopCounter> It(GetWorld()); It; ++It)
			if (It->ShopId == ShopId)
			{
				const float D = Source ? FVector::DistSquared(It->GetActorLocation(), Source->GetActorLocation()) : 0.f;
				if (D < BestD) { BestD = D; Best = *It; }
			}
		if (Best) { OpenPanel(TEXT("shop"), Best); return; }
		S = TEXT("say");
	}
	OpenPanel(FName(*S), Source);
}

void AGielinorHUD::OnAimStrike(EAimShape Shape, FVector Origin, float YawDeg, int32 Hits)
{
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AGielinorAimMarker* M = GetWorld()->SpawnActor<AGielinorAimMarker>(Origin - FVector(0, 0, 85.f), FRotator::ZeroRotator, P))
		M->Setup((uint8)Shape, YawDeg, Hits);
}

void AGielinorHUD::BindPad()
{
	APawn* P = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	UGielinorGamepadComponent* Pad = P ? P->FindComponentByClass<UGielinorGamepadComponent>() : nullptr;
	if (!Pad || Pad == BoundPad.Get()) return; // pawn swaps (VR / respawn) rebind lazily
	Pad->OnMenuRequested.AddUniqueDynamic(this, &AGielinorHUD::TogglePadMenu);
	BoundPad = Pad;
}

void AGielinorHUD::TogglePadMenu(EGielinorPadMenu Menu)
{
	const bool bClose = bMenuOpen && OpenMenu == Menu;
	if (bMenuOpen && Widget) Widget->ShowPadMenu(OpenMenu, false);
	bMenuOpen = !bClose; OpenMenu = Menu;
	if (bMenuOpen && Widget) Widget->ShowPadMenu(Menu, true);
	if (bMenuOpen) OpenPanel(PadPanelName(Menu), nullptr); else ClosePanel();
}

void AGielinorHUD::ClosePadMenu() { ClosePanel(); }

void AGielinorHUD::DrawPadMenu(AGielinorPlayerState* PS)
{
	UFont* Font = GEngine->GetSmallFont();
	const float W = 420.f, H = 360.f, X0 = (Canvas->ClipX - W) * 0.5f, Y0 = (Canvas->ClipY - H) * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), X0, Y0, W, H);
	const FLinearColor Gold(1.f, 0.85f, 0.35f), White = FLinearColor::White;
	float Y = Y0 + 12.f; const float X = X0 + 16.f;
	auto Line = [&](const FString& S, const FLinearColor& C) { if (Y < Y0 + H - 30.f) { DrawText(S, C, X, Y, Font); Y += 17.f; } };
	APawn* P = PlayerOwner->GetPawn();
	const FVector L = P ? P->GetActorLocation() : FVector::ZeroVector;
	switch (OpenMenu)
	{
	case EGielinorPadMenu::Inventory:
	{
		Line(TEXT("Inventory"), Gold);
		const TArray<FItemStack>& Slots = PS->Inventory->GetSlots(); int32 Used = 0;
		for (int32 i = 0; i < Slots.Num(); ++i) if (!Slots[i].IsEmpty()) { ++Used; Line(FString::Printf(TEXT("%2d  %s x%d"), i + 1, *Slots[i].Item->DisplayName.ToString(), Slots[i].Count), White); }
		Line(FString::Printf(TEXT("%d / %d slots"), Used, Slots.Num()), Gold);
		break;
	}
	case EGielinorPadMenu::Settings:
	{
		Line(TEXT("Settings"), Gold);
		UGameInstance* GI = GetGameInstance();
		const UTrueAimFeature* Aim = GI ? GI->GetSubsystem<UTrueAimFeature>() : nullptr;
		Line(FString::Printf(TEXT("True Aim: %s   (LB + RT to toggle)"), Aim && Aim->IsOn() ? TEXT("on") : TEXT("off")), White);
		if (Aim) Line(FString::Printf(TEXT("Aimed strikes %d, dodge-cancels %d, window %d ms"), Aim->GetStrikes(), Aim->GetCancels(), Aim->CancelWindowMs()), White);
		Line(TEXT("Graphics tier, audio and bindings: Project Settings / BP settings widget."), White);
		break;
	}
	case EGielinorPadMenu::WorldMap:
	{
		Line(FString::Printf(TEXT("World map - you are at (%.0f, %.0f)"), L.X / 100.f, L.Y / 100.f), Gold);
		struct FPlace { const TCHAR* Name; float X, Z; };
		static const FPlace Places[] = { { TEXT("Larkhaven"), 0, 0 }, { TEXT("Thistlewell"), 40, 90 }, { TEXT("Saltreach docks"), -80, 76 },
			{ TEXT("The Wilds"), 122, -122 }, { TEXT("Siege fort"), -1000, -640 }, { TEXT("Brineholt"), 80, 1030 } };
		for (const FPlace& Pl : Places)
		{
			const float Dx = Pl.X - L.X / 100.f, Dz = Pl.Z - L.Y / 100.f;
			static const TCHAR* Dirs[8] = { TEXT("E"), TEXT("NE"), TEXT("N"), TEXT("NW"), TEXT("W"), TEXT("SW"), TEXT("S"), TEXT("SE") };
			const int32 Oct = ((int32)FMath::RoundToInt(FMath::Atan2(Dz, Dx) / (PI / 4.f)) + 8) % 8;
			Line(FString::Printf(TEXT("%-18s %5.0f m %s"), Pl.Name, FMath::Sqrt(Dx * Dx + Dz * Dz), Dirs[Oct]), White);
		}
		break;
	}
	case EGielinorPadMenu::ContextMenu:
	{
		Line(TEXT("Nearby"), Gold);
		const AGielinorPlayerController* PC = Cast<AGielinorPlayerController>(PlayerOwner);
		if (PC && !PC->GetHoverText().IsEmpty()) Line(FString::Printf(TEXT("Target: %s  (A to use)"), *PC->GetHoverText().ToString()), White);
		int32 N = 0;
		for (TActorIterator<AActor> It(GetWorld()); It && N < 10; ++It)
		{
			if (*It == P || It->IsHidden() || !It->GetRootComponent() || It->ActorHasTag(TEXT("NoContext"))) continue;
			if (!It->ActorHasTag(TEXT("Interactable")) && !It->IsA<APawn>()) continue;
			const float D = FVector::Dist2D(It->GetActorLocation(), L) / 100.f;
			if (D > 6.f) continue;
			Line(FString::Printf(TEXT("%s  %.1f m"), *It->GetActorNameOrLabel(), D), White); ++N;
		}
		if (!N) Line(TEXT("Nothing within reach."), White);
		break;
	}
	}
	DrawText(TEXT("Press the same button again to close"), Gold, X, Y0 + H - 22.f, Font);
}

void AGielinorHUD::OnMsg(const FText& T) { Messages.Add(T); if (Messages.Num() > 6) Messages.RemoveAt(0); }

void AGielinorHUD::OnDlg(const FText& Speaker, const TArray<FText>& Lines)
{
	DialogueSpeaker = Speaker; DialogueLines = Lines; DialogueUntil = GetWorld()->GetTimeSeconds() + 4.f + 2.f * Lines.Num();
}

void AGielinorHUD::DrawHUD()
{
	Super::DrawHUD();
	BindPad();
	if (!bAimBound) if (UGameInstance* GI = GetGameInstance()) if (UTrueAimFeature* Aim = GI->GetSubsystem<UTrueAimFeature>()) { Aim->OnStrike.AddDynamic(this, &AGielinorHUD::OnAimStrike); bAimBound = true; }
	// Sea panel follows the voyage: open on cast-off, close on landfall.
	if (UGameInstance* GI = GetGameInstance()) if (USailingFeature* Sea = GI->GetSubsystem<USailingFeature>())
	{
		const bool bShowing = Panel && Panel->IsInViewport() && Panel->GetPanel() == TEXT("sea");
		if (Sea->IsAtSea() && !(Panel && Panel->IsInViewport())) OpenPanel(TEXT("sea"), nullptr);
		else if (!Sea->IsAtSea() && bShowing) ClosePanel();
	}
	if (Widget || !Canvas || !PlayerOwner || PlayerOwner->GetPawn<AGielinorVRPawn>()) return;
	AGielinorPlayerState* PS = PlayerOwner->GetPlayerState<AGielinorPlayerState>();
	if (!PS) return;
	UFont* Font = GEngine->GetSmallFont();
	const FLinearColor Yellow(1.f, 0.9f, 0.3f), White = FLinearColor::White;
	float Y = 20.f;

	if (const AGielinorPlayerController* PC = Cast<AGielinorPlayerController>(PlayerOwner))
		if (!PC->GetHoverText().IsEmpty()) DrawText(PC->GetHoverText().ToString(), Yellow, 20, Y, Font, 1.2f);
	Y += 28;
	DrawText(FString::Printf(TEXT("HP %d/%d   Combat %d   Total %d   QP %d"), PS->Health->GetCurrent(), PS->Health->GetMax(),
		PS->Skills->CombatLevel(), PS->Skills->TotalLevel(), PS->Quests->QuestPoints()), White, 20, Y, Font);
	Y += 22;
	for (const FSkillProgress& S : PS->Skills->GetAll())
	{
		if (!S.Skill) continue;
		DrawText(FString::Printf(TEXT("%s %d (%.0f xp)"), *S.Skill->DisplayName.ToString(), S.Level, S.Xp), S.Skill->Color, 20, Y, Font);
		Y += 16;
	}

	// Inventory (right side).
	float IY = 20.f; const float X = Canvas->ClipX - 260.f;
	DrawText(PS->Bank->IsOpen() ? TEXT("Inventory (bank open)") : TEXT("Inventory"), Yellow, X, IY, Font); IY += 18;
	const TArray<FItemStack>& Slots = PS->Inventory->GetSlots();
	for (int32 i = 0; i < Slots.Num(); ++i)
		if (!Slots[i].IsEmpty()) { DrawText(FString::Printf(TEXT("%2d  %s x%d"), i + 1, *Slots[i].Item->DisplayName.ToString(), Slots[i].Count), White, X, IY, Font); IY += 16; }

	// Quest step.
	for (const FQuestProgress& Q : PS->Quests->GetAll())
		if (Q.Quest && Q.State == EQuestState::InProgress)
		{ DrawText(FString::Printf(TEXT("[%s] %s"), *Q.Quest->DisplayName.ToString(), *PS->Quests->CurrentStepText(Q.Quest).ToString()), Yellow, 20, Canvas->ClipY - 170, Font); break; }

	if (bMenuOpen && !Panel) DrawPadMenu(PS); // UMG panel normally covers this

	// Dialogue + chat box.
	float MY = Canvas->ClipY - 140.f;
	if (GetWorld()->GetTimeSeconds() < DialogueUntil)
	{
		DrawText(DialogueSpeaker.ToString(), Yellow, 20, MY - 60, Font, 1.1f);
		float DY = MY - 40; for (const FText& L : DialogueLines) { DrawText(L.ToString(), White, 20, DY, Font); DY += 16; }
	}
	for (const FText& M : Messages) { DrawText(M.ToString(), White, 20, MY + 40, Font); MY += 16; }
}
