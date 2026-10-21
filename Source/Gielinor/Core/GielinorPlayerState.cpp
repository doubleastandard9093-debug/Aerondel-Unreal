#include "Core/GielinorPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Quests/QuestComponent.h"
#include "Combat/HealthComponent.h"
#include "Save/GielinorSaveSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/GameInstance.h"

AGielinorPlayerState::AGielinorPlayerState()
{
	Skills = CreateDefaultSubobject<USkillsComponent>(TEXT("Skills"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
	Bank = CreateDefaultSubobject<UBankComponent>(TEXT("Bank"));
	Quests = CreateDefaultSubobject<UQuestComponent>(TEXT("Quests"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
}

void AGielinorPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (UGielinorSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UGielinorSaveSubsystem>())
	{
		if (!Save->LoadInto(this))
		{
			Inventory->GiveStarterKit();
		}
	}
	Health->SetMax(Skills->GetLevelById(TEXT("hitpoints")), true);
}

AGielinorPlayerState* AGielinorPlayerState::FromActor(const AActor* Actor)
{
	if (!Actor) return nullptr;
	if (const AGielinorPlayerState* PS = Cast<AGielinorPlayerState>(Actor)) return const_cast<AGielinorPlayerState*>(PS);
	if (const APawn* Pawn = Cast<APawn>(Actor)) return Pawn->GetPlayerState<AGielinorPlayerState>();
	if (const AController* C = Cast<AController>(Actor)) return C->GetPlayerState<AGielinorPlayerState>();
	return FromActor(Actor->GetOwner());
}
