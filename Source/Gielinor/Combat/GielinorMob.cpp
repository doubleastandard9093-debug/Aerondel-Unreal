#include "Combat/GielinorMob.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatMath.h"
#include "Combat/PlayerCombatComponent.h"
#include "Data/MobDef.h"
#include "Data/ItemDef.h"
#include "Core/GielinorEvents.h"
#include "Core/GielinorPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Interaction/GroundItem.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AGielinorMob::AGielinorMob()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // AI at 10 Hz is plenty and cheap on Quest.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	// Body: no physics; blocks Visibility (cursor clicks) and overlaps WorldDynamic (VR hands/tools).
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Body->SetGenerateOverlapEvents(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Body->SetStaticMesh(Cube.Object);
	GetCharacterMovement()->MaxWalkSpeed = 250.f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationYaw = false;
}

void AGielinorMob::OnConstruction(const FTransform& Xf)
{
	Super::OnConstruction(Xf);
	if (!Definition) return;
	// Placeholder body: a tinted box sized from the data (cube mesh is 100 cm).
	const FVector S = Definition->BodySize;
	GetCapsuleComponent()->SetCapsuleSize(FMath::Min(S.X, S.Y) * 0.5f, FMath::Max(S.Z * 0.5f, FMath::Min(S.X, S.Y) * 0.5f));
	Body->SetRelativeScale3D(FVector(S.Y, S.X, S.Z) / 100.f);
	// M_Tint (Vector param "Color") is created by generate_starter_data.py; quietly skip if it doesn't exist yet.
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Gielinor/Materials/M_Tint.M_Tint"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Definition->Tint);
		Body->SetMaterial(0, M);
	}
}

void AGielinorMob::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	Health->SetMax(Definition ? Definition->Hitpoints : 5, true);
	Health->OnDied.AddDynamic(this, &AGielinorMob::HandleDied);
}

AAIController* AGielinorMob::AI() const { return Cast<AAIController>(GetController()); }

FText AGielinorMob::GetInteractLabel() const
{
	return Definition ? FText::Format(NSLOCTEXT("Gielinor", "Attack", "Attack {0} (level {1})"), Definition->DisplayName, Definition->CombatLevel) : FText::GetEmpty();
}

void AGielinorMob::Interact(AActor* InInstigator)
{
	if (UPlayerCombatComponent* C = InInstigator ? InInstigator->FindComponentByClass<UPlayerCombatComponent>() : nullptr) C->SetTarget(this);
}

void AGielinorMob::OnToolHit(AActor* InInstigator, EToolType, float Power) { ReceivePlayerHit(InInstigator, Power); }

void AGielinorMob::ReceivePlayerHit(AActor* Attacker, float Power)
{
	if (State == EMobState::Dead || !Definition) return;
	AGielinorPlayerState* PS = AGielinorPlayerState::FromActor(Attacker);
	if (!PS) return;
	const int32 Att = UCombatMath::AttackRoll(PS->Skills->GetLevelById(TEXT("attack")), PS->Equipment->TotalAttackBonus());
	const int32 Def_ = UCombatMath::DefenceRoll(Definition->DefenceLevel, 0);
	const int32 Max = UCombatMath::MaxHit(PS->Skills->GetLevelById(TEXT("strength")), PS->Equipment->TotalStrengthBonus());
	const int32 Dmg = FMath::Min(UCombatMath::RollDamage(Att, Def_, Max, Power), Health->GetCurrent());
	if (Dmg > 0)
	{
		// Controlled-style split: attack/strength/defence get 4/3 each, Hitpoints 4/3.
		if (UPlayerCombatComponent* C = Attacker->FindComponentByClass<UPlayerCombatComponent>()) C->AwardXp(PS, Dmg);
	}
	Target = Attacker;
	if (State != EMobState::Attack) SetState(EMobState::Chase);
	Health->ApplyDamage(Dmg, Attacker);
}

AActor* AGielinorMob::FindPlayerInAggro() const
{
	if (!Definition || !Definition->bAggressive) return nullptr;
	APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	if (P && FVector::Dist(P->GetActorLocation(), GetActorLocation()) < Definition->AggroRadius && FVector::Dist(P->GetActorLocation(), Home) < Definition->LeashRadius) return P;
	return nullptr;
}

void AGielinorMob::SetState(EMobState S) { State = S; StateTime = 0.f; }

void AGielinorMob::Tick(float Dt)
{
	Super::Tick(Dt);
	StateTime += Dt; AttackCooldown -= Dt;
	AAIController* C = AI();
	if (!C || !Definition) return;
	AActor* T = Target.Get();

	switch (State)
	{
	case EMobState::Idle:
		if ((Target = FindPlayerInAggro()).IsValid()) { SetState(EMobState::Chase); break; }
		if (StateTime > FMath::FRandRange(3.f, 7.f))
		{
			FNavLocation Out;
			if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
				if (Nav->GetRandomReachablePointInRadius(Home, WanderRadius, Out)) C->MoveToLocation(Out.Location, 30.f);
			SetState(EMobState::Wander);
		}
		break;
	case EMobState::Wander:
		if ((Target = FindPlayerInAggro()).IsValid()) { SetState(EMobState::Chase); break; }
		if (C->GetMoveStatus() == EPathFollowingStatus::Idle || StateTime > 8.f) SetState(EMobState::Idle);
		break;
	case EMobState::Chase:
		if (!T || FVector::Dist(GetActorLocation(), Home) > Definition->LeashRadius) { Target = nullptr; SetState(EMobState::Return); C->MoveToLocation(Home, 30.f); break; }
		if (FVector::Dist2D(T->GetActorLocation(), GetActorLocation()) < 170.f) { C->StopMovement(); SetState(EMobState::Attack); break; }
		C->MoveToActor(T, 120.f);
		break;
	case EMobState::Attack:
		if (!T || FVector::Dist2D(T->GetActorLocation(), GetActorLocation()) > 220.f) { SetState(EMobState::Chase); break; }
		{ FVector To = T->GetActorLocation() - GetActorLocation(); To.Z = 0; if (!To.IsNearlyZero()) SetActorRotation(To.Rotation()); }
		if (AttackCooldown <= 0.f) { TryAttack(); AttackCooldown = Definition->AttackSeconds; }
		break;
	case EMobState::Return:
		if (FVector::Dist2D(GetActorLocation(), Home) < 100.f || StateTime > 10.f)
		{
			Health->Heal(Health->GetMax()); SetState(EMobState::Idle);
		}
		break;
	case EMobState::Dead: break;
	}
}

void AGielinorMob::TryAttack()
{
	AGielinorPlayerState* PS = AGielinorPlayerState::FromActor(Target.Get());
	if (!PS || !Definition) return;
	const int32 A = UCombatMath::AttackRoll(Definition->AttackLevel, 0);
	const int32 D = UCombatMath::DefenceRoll(PS->Skills->GetLevelById(TEXT("defence")), PS->Equipment->TotalDefenceBonus());
	const int32 Dmg = UCombatMath::RollDamage(A, D, Definition->MaxHit);
	PS->Health->ApplyDamage(Dmg, this);
	// Auto-retaliate.
	if (UPlayerCombatComponent* PC = Target->FindComponentByClass<UPlayerCombatComponent>()) PC->OnAttackedBy(this);
}

void AGielinorMob::HandleDied(AActor* Killer)
{
	SetState(EMobState::Dead);
	if (AAIController* C = AI()) C->StopMovement();
	if (Definition)
	{
		for (const FDropEntry& D : Definition->Drops)
			if (D.Item && FMath::FRand() <= D.Chance)
				AGroundItem::Spawn(GetWorld(), D.Item, FMath::RandRange(D.Min, D.Max), GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 15.f) + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 0));
		if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnMobKilled.Broadcast(Definition);
	}
	Target = nullptr;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AGielinorMob::Respawn, Definition ? Definition->RespawnSeconds : 20.f, false);
}

void AGielinorMob::Respawn()
{
	SetActorLocation(Home, false, nullptr, ETeleportType::TeleportPhysics);
	Health->SetMax(Definition ? Definition->Hitpoints : 5, true);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetState(EMobState::Idle);
}
