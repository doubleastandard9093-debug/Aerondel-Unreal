#include "Features/VenturesFeature.h"
#include "Features/SkywingFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/HealthComponent.h"
#include "Rules/AerondelRules.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"

// Ventures group 21 (web adapt/freeclimb.js): climb most cliffs, walls and big trees, spending run energy.
// Jump off into a Glidewalk glide. Tetherline and Skywing stay as they are.

namespace VenturesClimb
{
	static void VAct21(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesClimb;

void UVenturesFeature::ResetClimb()
{
	if (bClimbing) ClimbEnd(FString(), true);
	bClimbUsed = false;
}

void UVenturesFeature::SaveClimb(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetBoolField(TEXT("used"), bClimbUsed);
	A.SetObjectField(TEXT("climb"), S);
}

void UVenturesFeature::LoadClimb(const FJsonObject& A)
{
	if (const TSharedPtr<FJsonObject> S = JObj(A, TEXT("climb"))) bClimbUsed = JBool(*S, TEXT("used"));
}

bool UVenturesFeature::Climbing() const { return bClimbing; }

bool UVenturesFeature::ClimbProbe(FHitResult& Out) const
{
	const AAerondelPlayerState* PS = GetPS();
	const ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	UWorld* W = C ? C->GetWorld() : nullptr;
	if (!W) return false;
	const FVector P = C->GetActorLocation();
	const FVector Fwd = C->GetActorForwardVector().GetSafeNormal2D();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VenturesClimb), false, C);
	FHitResult H;
	if (!W->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, 40.f), P + FVector(0.f, 0.f, 40.f) + Fwd * 180.f, ECC_Visibility, Q)) return false;
	if (!H.bBlockingHit) return false;
	const int32 Ag = SkillLevel(TEXT("Agility"));
	const float MaxNz = 0.55f + Ag / 400.f; // higher Agility takes a slightly steeper / more tilted face
	if (H.Normal.Z > MaxNz || H.Normal.Z < -0.25f) return false;
	Out = H;
	return true;
}

bool UVenturesFeature::ClimbBegin()
{
	if (OathOn()) { Say(TEXT("The Oathframe cannot climb a steep face. Step out first.")); return false; }
	if (bClimbing) { ClimbEnd(TEXT("You let go."), true); return true; }
	if (const USkywingFeature* Sk = Feature<USkywingFeature>()) if (Sk->IsFlying()) { Say(TEXT("Land first.")); return false; }
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	if (!C) return false;
	if (!PS->Stats || PS->Stats->RunEnergy < 6.f) { Say(TEXT("Too tired to climb.")); return false; }
	FHitResult H;
	if (!ClimbProbe(H)) { Say(TEXT("There is no climbable face ahead.")); return false; }
	bClimbing = true;
	bClimbUsed = true;
	ScoreOn(TEXT("climb"));
	ClimbNormal = H.Normal;
	if (UCharacterMovementComponent* M = C->GetCharacterMovement())
	{
		ClimbGrav = M->GravityScale;
		M->StopMovementImmediately();
		M->GravityScale = 0.f;
		M->SetMovementMode(MOVE_Flying);
	}
	Say(TEXT("You start climbing. Walk up the face; jump off (C) to glide. Press , to climb."));
	return true;
}

void UVenturesFeature::ClimbEnd(const FString& Why, bool bSafe)
{
	if (!bClimbing) return;
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	float DropM = 0.f;
	if (C)
	{
		UWorld* W = C->GetWorld();
		if (W)
		{
			FCollisionQueryParams Q(SCENE_QUERY_STAT(VenturesClimbEnd), false, C);
			FHitResult G;
			if (W->LineTraceSingleByChannel(G, C->GetActorLocation(), C->GetActorLocation() - FVector(0.f, 0.f, 20000.f), ECC_Visibility, Q) && G.bBlockingHit)
				DropM = (C->GetActorLocation().Z - G.ImpactPoint.Z) / 100.f;
		}
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->GravityScale = ClimbGrav > 0.f ? ClimbGrav : 1.f;
			M->SetMovementMode(MOVE_Walking);
		}
	}
	bClimbing = false;
	ClimbNormal = FVector::ZeroVector;
	if (!bSafe && DropM > 10.f && PS && PS->Health && PS->Health->GetCurrent() > 1)
	{
		const int32 Hurt = FMath::Max(1, FMath::RoundToInt(FMath::Min(PS->Health->GetMax() * 0.08f, DropM - 8.f)));
		HurtPlayer((float)Hurt, TEXT("climb-fall"));
	}
	if (!Why.IsEmpty()) Say(Why);
}

bool UVenturesFeature::ClimbLaunch()
{
	if (!bClimbing) return false;
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	const FVector Fwd = C ? C->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	const float Drop = GlideDrop();
	ClimbEnd(FString(), true);
	if (PS && PS->Stats) PS->Stats->RunEnergy = FMath::Max(0.f, PS->Stats->RunEnergy - 4.f);
	if (C)
	{
		const int32 Ag = SkillLevel(TEXT("Agility"));
		const bool bGlide = Drop > 3.f;
		const float Dist = (bGlide ? 14.f : 6.f) * (1.f + Ag / 400.f);
		const float Secs = bGlide ? 0.9f : 0.22f;
		C->LaunchCharacter(Fwd * (Dist * 100.f / Secs) + FVector(0.f, 0.f, bGlide ? 250.f : 60.f), true, true);
	}
	Say(Drop > 3.f ? TEXT("You push off and glide.") : TEXT("You drop off the face."));
	return true;
}

void UVenturesFeature::TickClimb(float Dt)
{
	if (!bClimbing) return;
	Dt = FMath::Clamp(Dt, 0.f, 0.05f);
	if (const USkywingFeature* Sk = Feature<USkywingFeature>()) if (Sk->IsFlying()) { ClimbEnd(TEXT("You leave the face."), true); return; }
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	if (!C || !PS->Stats) { ClimbEnd(FString(), true); return; }
	if (PS->Stats->RunEnergy <= 0.f) { ClimbEnd(TEXT("You run out of energy and fall — but you land safely enough."), true); return; }
	const int32 Ag = SkillLevel(TEXT("Agility"));
	const float Drain = FMath::Max(3.5f, 16.f - Ag / 80.f - UAerondelRules::Perk(PS, FName(TEXT("Agility")), FName(TEXT("climbDrain"))) * 10.f) * Dt * ClimbDrainMul();
	PS->Stats->RunEnergy = FMath::Max(0.f, PS->Stats->RunEnergy - Drain);
	const float Speed = (1.7f + Ag / 200.f) * 100.f;
	FVector Delta = FVector(0.f, 0.f, Speed * Dt);
	if (!ClimbNormal.IsNearlyZero()) Delta += (-ClimbNormal.GetSafeNormal2D()) * 20.f * Dt; // stay on the face
	C->SetActorLocation(C->GetActorLocation() + Delta, true);
	UWorld* W = C->GetWorld();
	if (!W) return;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VenturesClimbTick), false, C);
	// Mantle: if the face eases just ahead, step onto the top.
	const FVector Fwd = C->GetActorForwardVector().GetSafeNormal2D();
	FHitResult Ahead, Top;
	const FVector Head = C->GetActorLocation() + FVector(0.f, 0.f, 90.f);
	const bool bWall = W->LineTraceSingleByChannel(Ahead, Head, Head + Fwd * 160.f, ECC_Visibility, Q) && Ahead.bBlockingHit;
	if (!bWall && W->LineTraceSingleByChannel(Top, Head + Fwd * 80.f + FVector(0.f, 0.f, 40.f), Head + Fwd * 80.f - FVector(0.f, 0.f, 250.f), ECC_Visibility, Q) && Top.bBlockingHit)
	{
		if (FMath::Abs(Top.ImpactPoint.Z - C->GetActorLocation().Z) < 120.f)
		{
			C->SetActorLocation(Top.ImpactPoint + FVector(0.f, 0.f, 90.f), false);
			ClimbEnd(TEXT("You pull yourself over the top."), true);
		}
	}
}

void UVenturesFeature::TabClimb(FGPanelModel& M)
{
	const int32 Ag = SkillLevel(TEXT("Agility"));
	const float En = (GetPS() && GetPS()->Stats) ? GetPS()->Stats->RunEnergy : 0.f;
	M.Title = FString::Printf(TEXT("Freeclimb · Agility %d"), Ag);
	M.Lines.Add(TEXT("Press , (Climb) at a steep cliff, wall or big tree to climb it. It spends run energy; Agility lowers the drain and lets you take steeper faces. Jump off with Dash (C) to glide (Glidewalk). Tetherline and Skywing stay as they are — this is for mid-height travel, not a replacement for flight."));
	M.Lines.Add(bClimbing ? TEXT("You are on a face now. Walk up, or jump off (C) to glide.") : TEXT("Find a cliff or wall and press , to climb."));
	M.Lines.Add(FString::Printf(TEXT("Energy %d."), FMath::RoundToInt(En)));
	VAct21(M, bClimbing ? TEXT("Let go") : TEXT("Climb ahead"), [this] { if (bClimbing) ClimbEnd(TEXT("You let go."), true); else ClimbBegin(); });
}
