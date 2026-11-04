#include "Interaction/NpcTalk.h"
#include "Data/NpcDef.h"
#include "Core/AerondelPlayerState.h"
#include "Core/AerondelEvents.h"
#include "Quests/QuestComponent.h"
#include "Data/GameDatabase.h"
#include "Models/AerondelModelLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ANpcTalk::ANpcTalk()
{
	Rig = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Rig"));
	Rig->SetupAttachment(RootComponent);
	Rig->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rig->SetGenerateOverlapEvents(false);
}

void ANpcTalk::BeginPlay()
{
	Super::BeginPlay();
	if (!Definition && !NpcId.IsNone())
		if (const UGameDatabase* Db = UGameDatabase::Get()) Definition = Db->FindNpc(NpcId);
	if (Definition) DisplayName = Definition->DisplayName;
	const bool bRig = Definition && UAerondelModelLibrary::ApplyRig(Rig, TEXT("npc:") + Definition->Id.ToString());
	UE_LOG(LogTemp, Log, TEXT("[Aerondel] Spawn npc %s (%s): rig %s"), *GetName(), Definition ? *Definition->Id.ToString() : TEXT("no definition"), bRig ? TEXT("applied") : TEXT("FAILED, placeholder mesh"));
	if (bRig)
	{
		UAerondelModelLibrary::FitRig(Rig, 172.f, 0.f);
		if (Mesh) Mesh->SetVisibility(false);
	}
}

void ANpcTalk::OnConstruction(const FTransform& Xf)
{
	Super::OnConstruction(Xf);
	if (Definition) DisplayName = Definition->DisplayName;
}

FText ANpcTalk::GetInteractLabel() const
{
	return FText::Format(NSLOCTEXT("Aerondel", "TalkTo", "Talk-to {0}"), Definition ? Definition->DisplayName : DisplayName);
}

void ANpcTalk::Interact(AActor* InInstigator)
{
	if (!Definition) return;
	// Face the player.
	if (InInstigator)
	{
		FVector To = InInstigator->GetActorLocation() - GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) SetActorRotation(To.Rotation());
	}
	AAerondelPlayerState* S = PS(InInstigator);
	if (S && S->Quests->HandleTalk(Definition)) return;
	if (!Definition->Service.IsNone())
		if (UAerondelEvents* Ev = UAerondelEvents::Get(this))
		{
			if (Ev->OnServiceRequested.IsBound()) { Ev->OnServiceRequested.Broadcast(Definition->Service, this); return; }
		}
	if (Definition->IdleLines.Num() > 0)
		if (UAerondelEvents* Ev = UAerondelEvents::Get(this))
			Ev->OnDialogue.Broadcast(Definition->DisplayName, { Definition->IdleLines[LineIndex++ % Definition->IdleLines.Num()] });
}
