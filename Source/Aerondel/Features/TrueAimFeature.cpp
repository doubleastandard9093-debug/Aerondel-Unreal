#include "Features/TrueAimFeature.h"
#include "Features/PathsFeature.h"
#include "Combat/AerondelMob.h"
#include "Combat/PlayerCombatComponent.h"
#include "Core/AerondelPlayerState.h"
#include "Rules/AerondelRules.h"
#include "Data/GameDatabase.h"
#include "Data/ContentDefs.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Dom/JsonObject.h"

void UTrueAimFeature::SaveState(FJsonObject& Out) const
{
	Out.SetBoolField(TEXT("on"), bOn); Out.SetNumberField(TEXT("strikes"), Strikes); Out.SetNumberField(TEXT("cancels"), Cancels);
}

void UTrueAimFeature::LoadState(const FJsonObject& In)
{
	ResetState();
	bOn = JBool(In, TEXT("on")); Strikes = FMath::Max(0, (int32)JInt(In, TEXT("strikes"))); Cancels = FMath::Max(0, (int32)JInt(In, TEXT("cancels")));
}

void UTrueAimFeature::SetOn(bool bNewOn)
{
	bOn = bNewOn;
	Say(bOn ? TEXT("True Aim on: skills fire where you face and hit everything in their shape.") : TEXT("True Aim off: classic targeting."));
}

bool UTrueAimFeature::Toggle() { SetOn(!bOn); return bOn; }

EAimShape UTrueAimFeature::ShapeFor(FName Id)
{
	static const TMap<FName, EAimShape> M = {
		{ TEXT("power"), EAimShape::Line }, { TEXT("snipe"), EAimShape::Line }, { TEXT("rend"), EAimShape::Line }, { TEXT("puncture"), EAimShape::Line },
		{ TEXT("volley"), EAimShape::Cone }, { TEXT("flurry"), EAimShape::Cone }, { TEXT("bash"), EAimShape::Circle }, { TEXT("challenge"), EAimShape::Circle } };
	const EAimShape* S = M.Find(Id);
	return S ? *S : EAimShape::Cone;
}

bool UTrueAimFeature::InShape(EAimShape Shape, float Ox, float Oz, float Yaw, float X, float Z, float R)
{
	const float Fx = FMath::Sin(Yaw), Fz = FMath::Cos(Yaw), Dx = X - Ox, Dz = Z - Oz;
	if (Shape == EAimShape::Circle) return FMath::Sqrt(FMath::Square(X - (Ox + Fx * 2.5f)) + FMath::Square(Z - (Oz + Fz * 2.5f))) <= 3.f + R;
	const float Along = Dx * Fx + Dz * Fz, Side = FMath::Abs(Dx * Fz - Dz * Fx);
	if (Shape == EAimShape::Line) return Along >= -R && Along <= 8.f + R && Side <= 0.7f + R;
	const float D = FMath::Sqrt(Dx * Dx + Dz * Dz);
	if (D > 4.2f + R) return false;
	if (D < 0.8f) return true;
	return FMath::Acos(FMath::Clamp(Along / D, -1.f, 1.f)) <= PI / 3.f + FMath::Atan2(R, D);
}

int32 UTrueAimFeature::CancelWindowMs() const
{
	const UPathsFeature* P = Feature<UPathsFeature>();
	const FName Path = P ? P->GetPath() : NAME_None;
	return Path == FName(TEXT("bulwark")) ? 200 : Path == FName(TEXT("mender")) ? 260 : Path == FName(TEXT("striker")) ? 360 : 240;
}

bool UTrueAimFeature::TryCancel()
{
	if (!bOn || !LastStrikeMs || NowMs() - LastStrikeMs >= CancelWindowMs()) return false;
	++Cancels; LastStrikeMs = 0;
	return true;
}

int32 UTrueAimFeature::Strike(FName AbilityId)
{
	AAerondelPlayerState* PS = GetPS();
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PS || !Pawn) return 0;
	const EAimShape Shape = ShapeFor(AbilityId);
	const FVector P = Pawn->GetActorLocation();
	const float Ox = P.X / 100.f, Oz = P.Y / 100.f;
	// UE yaw 0 faces +X (web +x); web yaw 0 faces +z (UE +Y): web yaw = 90 deg - UE yaw.
	// Aim with the camera (control rotation), web aim.js uses the camera heading; fall back to the pawn's facing.
	const AController* Ctl = Pawn->GetController();
	const float UeYaw = Ctl ? Ctl->GetControlRotation().Yaw : Pawn->GetActorRotation().Yaw;
	if (Ctl) Pawn->SetActorRotation(FRotator(0.f, UeYaw, 0.f)); // turn to face the strike
	const float Yaw = FMath::DegreesToRadians(90.f - UeYaw);
	TArray<AAerondelMob*> Hits;
	for (TActorIterator<AAerondelMob> It(GetWorld()); It; ++It)
	{
		AAerondelMob* M = *It;
		if (M->IsDead() || M->GetHp() <= 0 || M->IsHidden()) continue;
		const FVector L = M->GetActorLocation();
		const float R = FMath::Max(0.5f, M->GetSimpleCollisionRadius() / 100.f);
		if (InShape(Shape, Ox, Oz, Yaw, L.X / 100.f, L.Y / 100.f, R)) Hits.Add(M);
	}
	Hits.Sort([&P](const AAerondelMob& A, const AAerondelMob& B) { return FVector::DistSquared(A.GetActorLocation(), P) < FVector::DistSquared(B.GetActorLocation(), P); });
	if (Hits.Num() > MaxTargets) Hits.SetNum(MaxTargets);
	const float BaseDmg = AbilityId == FName(TEXT("snipe")) || AbilityId == FName(TEXT("rend")) ? 0.6f : AbilityId == FName(TEXT("power")) ? 0.4f : 0.1f;
	int32 Total = 0;
	for (int32 i = 0; i < Hits.Num(); ++i)
	{
		AAerondelMob* M = Hits[i];
		FSwingMods Mods; Mods.Accuracy = 0.15f; Mods.Damage = BaseDmg - i * 0.08f;
		const int32 D = UAerondelRules::RollPlayerHit(PS, M->Definition, M->GetHp(), M->GetMaxHp(), Mods);
		Total += D;
		M->TakePlayerDamage(D, Pawn); // also pulls it into the fight
	}
	if (Hits.Num())
		if (UPlayerCombatComponent* C = Pawn->FindComponentByClass<UPlayerCombatComponent>())
			if (!C->GetTarget()) C->SetTarget(Hits[0]);
	for (const auto& X : UAerondelRules::CombatXp(PS, Total)) GiveXp(X.Key.ToString(), X.Value);
	++Strikes; LastStrikeMs = NowMs();
	OnStrike.Broadcast(Shape, P, UeYaw, Hits.Num());
	FString Name = AbilityId.ToString();
	if (const UGameDatabase* Db = UGameDatabase::Get()) if (const UAbilityDef* A = Db->FindAbility(AbilityId)) Name = A->DisplayName.ToString();
	static const TCHAR* ShapeNames[3] = { TEXT("cone"), TEXT("line"), TEXT("circle") };
	Say(FString::Printf(TEXT("%s (%s): %d hit%s, %d damage."), *Name, ShapeNames[(int32)Shape], Hits.Num(), Hits.Num() == 1 ? TEXT("") : TEXT("s"), Total));
	return Hits.Num();
}
