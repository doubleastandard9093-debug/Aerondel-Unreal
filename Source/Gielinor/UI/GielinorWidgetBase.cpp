#include "UI/GielinorWidgetBase.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Player/GielinorPlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Combat/HealthComponent.h"
#include "Interaction/GroundItem.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "GameFramework/Pawn.h"

AGielinorPlayerState* UGielinorWidgetBase::GetGielinorPlayerState() const
{
	APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPlayerState<AGielinorPlayerState>() : nullptr;
}

UGielinorEvents* UGielinorWidgetBase::GetEvents() const { return UGielinorEvents::Get(this); }

void UGielinorWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	BindSources();
	Refresh();
}

// ---- HUD ----
FText UGielinorHudWidget::GetHoverText() const
{
	const AGielinorPlayerController* PC = Cast<AGielinorPlayerController>(GetOwningPlayer());
	return PC ? PC->GetHoverText() : FText::GetEmpty();
}

float UGielinorHudWidget::GetHealthPercent() const
{
	const AGielinorPlayerState* PS = GetGielinorPlayerState();
	return PS ? float(PS->Health->GetCurrent()) / FMath::Max(1, PS->Health->GetMax()) : 1.f;
}

void UGielinorHudWidget::BindSources()
{
	if (UGielinorEvents* Ev = GetEvents())
	{
		Ev->OnMessage.AddDynamic(this, &UGielinorHudWidget::OnMsg);
		Ev->OnDialogue.AddDynamic(this, &UGielinorHudWidget::OnDlg);
	}
	if (AGielinorPlayerState* PS = GetGielinorPlayerState()) PS->Health->OnHealthChanged.AddDynamic(this, &UGielinorHudWidget::OnHp);
}

void UGielinorHudWidget::OnMsg(const FText& T)
{
	Messages.Add(T);
	if (Messages.Num() > 6) Messages.RemoveAt(0);
	Refresh();
}

// ---- Inventory ----
void UGielinorInventoryWidget::BindSources()
{
	if (AGielinorPlayerState* PS = GetGielinorPlayerState())
	{
		PS->Inventory->OnChanged.AddDynamic(this, &UGielinorInventoryWidget::RequestRefresh);
		PS->Equipment->OnChanged.AddDynamic(this, &UGielinorInventoryWidget::RequestRefresh);
	}
}

void UGielinorInventoryWidget::UseSlot(int32 SlotIndex)
{
	AGielinorPlayerState* PS = GetGielinorPlayerState();
	if (!PS) return;
	const FItemStack S = PS->Inventory->GetSlot(SlotIndex);
	if (S.IsEmpty()) return;
	if (PS->Bank->IsOpen()) { PS->Bank->Deposit(PS->Inventory, S.Item, S.Item->bStackable ? S.Count : 1); return; }
	if (S.Item->EquipSlot != EEquipSlot::None) { PS->Equipment->EquipFromInventory(SlotIndex); return; }
	if (S.Item->HealAmount > 0) { PS->Inventory->RemoveFromSlot(SlotIndex, 1); PS->Health->Heal(S.Item->HealAmount); return; }
}

void UGielinorInventoryWidget::DropSlot(int32 SlotIndex)
{
	AGielinorPlayerState* PS = GetGielinorPlayerState();
	APawn* Pawn = GetOwningPlayerPawn();
	if (!PS || !Pawn) return;
	const FItemStack S = PS->Inventory->GetSlot(SlotIndex);
	if (S.IsEmpty()) return;
	PS->Inventory->RemoveFromSlot(SlotIndex, S.Count);
	AGroundItem::Spawn(Pawn->GetWorld(), S.Item, S.Count, Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 60.f - FVector(0, 0, 80));
}

// ---- Skills ----
void UGielinorSkillsWidget::BindSources()
{
	if (UGielinorEvents* Ev = GetEvents()) Ev->OnXpGained.AddDynamic(this, &UGielinorSkillsWidget::OnXp);
}

float UGielinorSkillsWidget::ProgressToNext(float Xp, int32 Level)
{
	const float A = UXpCurve::XpForLevel(Level), B = UXpCurve::XpForLevel(Level + 1);
	return B > A ? FMath::Clamp((Xp - A) / (B - A), 0.f, 1.f) : 1.f;
}

// ---- Quest log ----
void UGielinorQuestLogWidget::BindSources()
{
	if (UGielinorEvents* Ev = GetEvents()) Ev->OnQuestChanged.AddDynamic(this, &UGielinorQuestLogWidget::OnQuest);
}
