#include "Core/AerondelPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Quests/QuestComponent.h"
#include "Combat/HealthComponent.h"
#include "Core/PlayerStatsComponent.h"
#include "Rules/AerondelRules.h"
#include "Save/AerondelSaveSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/GameInstance.h"

AAerondelPlayerState::AAerondelPlayerState()
{
	Skills = CreateDefaultSubobject<USkillsComponent>(TEXT("Skills"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
	Bank = CreateDefaultSubobject<UBankComponent>(TEXT("Bank"));
	Quests = CreateDefaultSubobject<UQuestComponent>(TEXT("Quests"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Stats = CreateDefaultSubobject<UPlayerStatsComponent>(TEXT("Stats"));
}

void AAerondelPlayerState::BeginPlay()
{
	Super::BeginPlay();
	bool bLoaded = false;
	if (UAerondelSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UAerondelSaveSubsystem>())
	{
		bLoaded = Save->LoadInto(this);
		if (!bLoaded)
		{
			Stats->ResetRuntime();
			Inventory->GiveStarterKit();
		}
	}
	Stats->RefreshMaxHp(!bLoaded);
	Stats->PrayerPoints = FMath::Min(Stats->PrayerPoints, (float)UAerondelRules::MaxPrayerPoints(this));
}

AAerondelPlayerState* AAerondelPlayerState::FromActor(const AActor* Actor)
{
	if (!Actor) return nullptr;
	if (const AAerondelPlayerState* PS = Cast<AAerondelPlayerState>(Actor)) return const_cast<AAerondelPlayerState*>(PS);
	if (const APawn* Pawn = Cast<APawn>(Actor)) return Pawn->GetPlayerState<AAerondelPlayerState>();
	if (const AController* C = Cast<AController>(Actor)) return C->GetPlayerState<AAerondelPlayerState>();
	return FromActor(Actor->GetOwner());
}
