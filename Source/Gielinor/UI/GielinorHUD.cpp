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
	}
}

void AGielinorHUD::OnMsg(const FText& T) { Messages.Add(T); if (Messages.Num() > 6) Messages.RemoveAt(0); }

void AGielinorHUD::OnDlg(const FText& Speaker, const TArray<FText>& Lines)
{
	DialogueSpeaker = Speaker; DialogueLines = Lines; DialogueUntil = GetWorld()->GetTimeSeconds() + 4.f + 2.f * Lines.Num();
}

void AGielinorHUD::DrawHUD()
{
	Super::DrawHUD();
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

	// Dialogue + chat box.
	float MY = Canvas->ClipY - 140.f;
	if (GetWorld()->GetTimeSeconds() < DialogueUntil)
	{
		DrawText(DialogueSpeaker.ToString(), Yellow, 20, MY - 60, Font, 1.1f);
		float DY = MY - 40; for (const FText& L : DialogueLines) { DrawText(L.ToString(), White, 20, DY, Font); DY += 16; }
	}
	for (const FText& M : Messages) { DrawText(M.ToString(), White, 20, MY + 40, Font); MY += 16; }
}
