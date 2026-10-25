#include "Combat/GielinorMob.h"
#include "Combat/HealthComponent.h"
#include "Rules/GielinorRules.h"
#include "Core/PlayerStatsComponent.h"
#include "Data/GameDatabase.h"
#include "Data/SkillDef.h"
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

float AGielinorMob::GetInteractRange() const
{
	// Let ranged / magic users click from their weapon range; the combat component walks into range itself.
	return 2000.f;
}

int32 AGielinorMob::GetHp() const { return Health ? Health->GetCurrent() : 0; }
int32 AGielinorMob::GetMaxHp() const { return Health ? Health->GetMax() : 1; }
FText AGielinorMob::GetDisplayName() const { return Definition ? Definition->DisplayName : FText::GetEmpty(); }

void AGielinorMob::ReceivePlayerHit(AActor* Attacker, float Power)
{
	if (State == EMobState::Dead || !Definition || !Attacker) return;
	if (UPlayerCombatComponent* C = Attacker->FindComponentByClass<UPlayerCombatComponent>()) C->PlayerAttack(this, FMath::Clamp(Power, 0.f, 1.f));
}

void AGielinorMob::TakePlayerDamage(int32 Damage, AActor* Attacker)
{
	if (State == EMobState::Dead) return;
	Target = Attacker;
	if (State != EMobState::Attack) { SetState(EMobState::Chase); AttackCooldown = FMath::Min(AttackCooldown, UGielinorRules::TickSeconds); }
	Health->ApplyDamage(FMath::Clamp(Damage, 0, Health->GetCurrent()), Attacker);
}

bool AGielinorMob::ApplyVenom(int32 Damage)
{
	if (Damage <= 0 || VenomDamage >= Damage) return false;
	VenomDamage = Damage; VenomNext = UGielinorRules::PoisonEvery;
	return true;
}

void AGielinorMob::GameTick()
{
	if (StunTicks > 0) --StunTicks;
	if (VenomDamage > 0 && --VenomNext <= 0)
	{
		VenomNext = UGielinorRules::PoisonEvery;
		const int32 D = FMath::Min(VenomDamage, Health->GetCurrent());
		VenomDamage = FMath::Max(0, VenomDamage - 2);
		Health->ApplyDamage(D, Target.Get());
	}
}

AActor* AGielinorMob::FindPlayerInAggro() const
{
	if (!Definition || !Definition->bAggressive) return nullptr;
	APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const AGielinorPlayerState* PS = AGielinorPlayerState::FromActor(P);
	if (PS && PS->Health && PS->Health->IsDead()) return nullptr;
	if (P && FVector::Dist(P->GetActorLocation(), GetActorLocation()) < Definition->AggroRadius && FVector::Dist(P->GetActorLocation(), Home) < Definition->LeashRadius) return P;
	return nullptr;
}

void AGielinorMob::SetState(EMobState S) { State = S; StateTime = 0.f; }

void AGielinorMob::Tick(float Dt)
{
	Super::Tick(Dt);
	StateTime += Dt; AttackCooldown -= Dt;
	if (State != EMobState::Dead) { TickAcc += Dt; while (TickAcc >= UGielinorRules::TickSeconds) { TickAcc -= UGielinorRules::TickSeconds; GameTick(); } }
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
		if (!T || IsLeashed(T)) { Target = nullptr; SetState(EMobState::Return); C->MoveToLocation(Home, 30.f); break; }
		if (StunTicks > 0) { C->StopMovement(); break; }
		if (FVector::Dist2D(T->GetActorLocation(), GetActorLocation()) < Definition->AttackRange) { C->StopMovement(); SetState(EMobState::Attack); break; }
		C->MoveToActor(T, 120.f);
		break;
	case EMobState::Attack:
		if (!T || IsLeashed(T)) { Target = nullptr; SetState(EMobState::Return); C->MoveToLocation(Home, 30.f); break; }
		if (FVector::Dist2D(T->GetActorLocation(), GetActorLocation()) > Definition->AttackRange + 50.f) { SetState(EMobState::Chase); break; }
		if (StunTicks > 0) break;
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

bool AGielinorMob::IsLeashed(const AActor* T) const
{
	const AGielinorPlayerState* PS = AGielinorPlayerState::FromActor(T);
	if (PS && PS->Health && PS->Health->IsDead()) return true;
	// Web build: give up beyond 13 tiles from the player or 16 tiles from spawn.
	return FVector::Dist2D(T->GetActorLocation(), GetActorLocation()) > 13.f * UGielinorRules::TileCm
		|| FVector::Dist2D(GetActorLocation(), Home) > FMath::Max(16.f * UGielinorRules::TileCm, Definition ? Definition->LeashRadius : 0.f);
}

void AGielinorMob::TryAttack()
{
	AActor* T = Target.Get();
	AGielinorPlayerState* PS = AGielinorPlayerState::FromActor(T);
	if (!PS || !PS->Stats || !Definition || PS->Health->IsDead()) return;
	UPlayerStatsComponent* St = PS->Stats;
	UGielinorEvents* Ev = UGielinorEvents::Get(this);
	const int32 Raw = UGielinorRules::RollMonsterHit(Definition, PS);
	int32 Dmg = UGielinorRules::ReduceIncoming(PS, Raw, St->IsFortified(), St->IsRaging());
	if (Dmg > 0 && FMath::FRand() < UGielinorRules::Perk(PS, TEXT("defence"), TEXT("block")))
	{
		Dmg = 0;
		if (Ev) Ev->Message(NSLOCTEXT("Gielinor", "Parry", "Parry: you block the attack!"));
	}
	// Retribution: reflect some damage back.
	const int32 Refl = Dmg > 0 ? FMath::FloorToInt(Dmg * FMath::Min(0.6f, UGielinorRules::Perk(PS, TEXT("defence"), TEXT("reflect")) + UGielinorRules::GearFx(PS, TEXT("reflect")))) : 0;
	if (Refl > 0) TakePlayerDamage(Refl, T);
	if (State == EMobState::Dead) return;
	UPlayerCombatComponent* PC = T->FindComponentByClass<UPlayerCombatComponent>();
	// Riposte: counter when a melee monster misses and you're fighting it up close.
	if (Raw == 0 && !Definition->bRanged && PC && PC->GetTarget() == this && !UGielinorRules::IsRanged(PS)
		&& FMath::FRand() < UGielinorRules::Perk(PS, TEXT("attack"), TEXT("counter")))
	{
		PC->BonusHit(this, NSLOCTEXT("Gielinor", "Riposte", "Riposte"));
		if (State == EMobState::Dead) return;
	}
	if (Dmg > 0 && Definition->PoisonChance > 0.f && FMath::FRand() < Definition->PoisonChance && St->ApplyPoison(Definition->PoisonDamage) && Ev)
		Ev->Message(FText::Format(NSLOCTEXT("Gielinor", "Poisoned", "You have been poisoned by the {0}!"), Definition->DisplayName));
	St->ApplyPlayerDamage(Dmg, this);
	if (PC && !PS->Health->IsDead()) PC->OnAttackedBy(this);
}

void AGielinorMob::HandleDied(AActor* Killer)
{
	SetState(EMobState::Dead);
	if (AAIController* C = AI()) C->StopMovement();
	if (Definition)
	{
		if (AGielinorPlayerState* PS = AGielinorPlayerState::FromActor(Killer))
		{
			if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(FText::Format(NSLOCTEXT("Gielinor", "Defeated", "You have defeated the {0}."), Definition->DisplayName));
			const float Kh = UGielinorRules::Perk(PS, TEXT("hitpoints"), TEXT("killHeal"));
			if (Kh > 0.f && !PS->Health->IsDead() && PS->Health->GetCurrent() < PS->Health->GetMax())
				PS->Health->Heal(FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * Kh)));
			if (PS->Stats) PS->Stats->OnMobKilledForBounty(Definition);
		}
		for (const FDropEntry& D : Definition->Drops)
		{
			UItemDef* DropItem = D.OneOf.Num() > 0 ? D.OneOf[FMath::RandHelper(D.OneOf.Num())].Get() : D.Item.Get();
			if (DropItem && FMath::FRand() < D.Chance)
				AGroundItem::Spawn(GetWorld(), DropItem, FMath::RandRange(D.Min, D.Max), GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 15.f) + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 0));
		}
		if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->OnMobKilled.Broadcast(Definition);
	}
	Target = nullptr;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AGielinorMob::Respawn, Definition ? Definition->RespawnSeconds : 20.f, false);
}

void AGielinorMob::Respawn()
{
	StunTicks = 0; VenomDamage = 0;
	SetActorLocation(Home, false, nullptr, ETeleportType::TeleportPhysics);
	Health->SetMax(Definition ? Definition->Hitpoints : 5, true);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetState(EMobState::Idle);
}
