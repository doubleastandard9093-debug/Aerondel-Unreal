#include "Features/VenturesFeature.h"
#include "Features/SkywingFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"

// Ventures group 35 (web adapt/oathframe.js): a heavy, enterable suit powered by a hearth-core.
// You walk slowly, you cannot Skywing or climb well, and Dash (C) becomes a shoulder-check that
// fills a foe's Guardbreak bar. Not a damage percent, not a Path, not a Vocation. When the core
// runs dry you step out safely — you do not die. Load always steps you out, core intact.
// Shops and houses ask you to step out. The town road is fine.

namespace VenturesOath
{
	static constexpr int32 CoreMax = 100, PoiseBonus = 36;
	static constexpr int64 DrainMs = 4000, SlamCd = 8000;
	static constexpr float Speed = 0.55f, SlamR = 2.8f;
	static void VAct35(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesOath;

void UVenturesFeature::ResetOath()
{
	bOathAssembled = false;
	OathCoreAmt = 0;
	OathSlamAt = 0;
	if (bOathOn) OathStepOut(TEXT("You step out of the Oathframe. The core is intact."));
	bOathOn = false;
	OathLastDrain = 0;
}

void UVenturesFeature::SaveOath(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Owned;
	if (bOathAssembled) Owned.Add(MakeShared<FJsonValueString>(TEXT("oath")));
	S->SetArrayField(TEXT("owned"), Owned);
	S->SetBoolField(TEXT("assembled"), bOathAssembled);
	S->SetNumberField(TEXT("core"), OathCoreAmt);
	A.SetObjectField(TEXT("oath"), S);
}

void UVenturesFeature::LoadOath(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("oath"));
	if (!S) return;
	bOathAssembled = JBool(*S, TEXT("assembled")) || JStrArr(*S, TEXT("owned")).Contains(TEXT("oath"));
	OathCoreAmt = FMath::Clamp((int32)JInt(*S, TEXT("core")), 0, CoreMax);
	bOathOn = false; // never persist "on"
}

void UVenturesFeature::OathApplySpeed(bool bOn)
{
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr;
	if (!M) return;
	if (bOn && !bOathSpeedOn) { OathSavedWalk = M->MaxWalkSpeed; M->MaxWalkSpeed = OathSavedWalk * Speed; bOathSpeedOn = true; }
	else if (bOn && bOathSpeedOn) M->MaxWalkSpeed = OathSavedWalk * Speed;
	else if (!bOn && bOathSpeedOn) { M->MaxWalkSpeed = OathSavedWalk; bOathSpeedOn = false; }
}

AAerondelMob* UVenturesFeature::OathNearest(float RadiusM) const
{
	const AAerondelPlayerState* PS = GetPS();
	AActor* Pawn = PS ? PS->GetPawn() : nullptr;
	UWorld* W = Pawn ? Pawn->GetWorld() : nullptr;
	if (!W || !Pawn) return nullptr;
	AAerondelMob* Best = nullptr;
	float Bd = RadiusM * 100.f;
	for (TActorIterator<AAerondelMob> It(W); It; ++It)
	{
		AAerondelMob* M = *It;
		if (!M || M->IsDead()) continue;
		const float D = FVector::Dist2D(M->GetActorLocation(), Pawn->GetActorLocation());
		if (D < Bd) { Bd = D; Best = M; }
	}
	return Best;
}

bool UVenturesFeature::OathAssemble()
{
	if (bOathAssembled) { Say(TEXT("You already have an Oathframe.")); return false; }
	if (SkillLevel(TEXT("Construction")) < 20 || SkillLevel(TEXT("Smithing")) < 20)
	{ Say(TEXT("You need Construction 20 and Smithing 20 to assemble an Oathframe.")); return false; }
	if (CountItem(TEXT("Iron bar")) < 8 || CountItem(TEXT("Logs")) < 6 || CountItem(TEXT("Coal")) < 2)
	{ Say(TEXT("You need 8 Iron bars, 6 Logs and 2 Coal.")); return false; }
	TakeItem(TEXT("Iron bar"), 8); TakeItem(TEXT("Logs"), 6); TakeItem(TEXT("Coal"), 2);
	bOathAssembled = true;
	if (OathCoreAmt < 20) OathCoreAmt = 20;
	GiveXp(TEXT("Construction"), 80.f); GiveXp(TEXT("Smithing"), 80.f);
	Say(TEXT("You assemble an Oathframe. Craft a hearth-core to power it, then step in."));
	return true;
}

bool UVenturesFeature::OathCraftCore()
{
	if (SkillLevel(TEXT("Smithing")) < 10) { Say(TEXT("You need Smithing 10 to craft a hearth-core.")); return false; }
	if (CountItem(TEXT("Iron bar")) < 2 || CountItem(TEXT("Coal")) < 4) { Say(TEXT("You need 2 Iron bars and 4 Coal.")); return false; }
	TakeItem(TEXT("Iron bar"), 2); TakeItem(TEXT("Coal"), 4);
	OathCoreAmt = FMath::Min(CoreMax, OathCoreAmt + 40);
	GiveXp(TEXT("Smithing"), 30.f);
	Say(FString::Printf(TEXT("You stoke a hearth-core. Core %d/%d."), OathCoreAmt, CoreMax));
	return true;
}

bool UVenturesFeature::OathStepOut(const FString& Why)
{
	if (!bOathOn) return false;
	bOathOn = false;
	OathApplySpeed(false);
	Say(Why.IsEmpty() ? TEXT("You step out of the Oathframe. The core is intact.") : Why);
	return true;
}

bool UVenturesFeature::OathStepIn()
{
	if (bOathOn) { Say(TEXT("You are already inside the Oathframe.")); return false; }
	if (!bOathAssembled) { Say(TEXT("Assemble an Oathframe at the frame bench first.")); return false; }
	if (OathCoreAmt <= 0) { Say(TEXT("The hearth-core is dry. Craft a core and stoke it first. You will not die when it runs out — you will just step out.")); return false; }
	if (USkywingFeature* Sk = Feature<USkywingFeature>()) if (Sk->IsFlying()) Sk->ToggleFlight();
	if (bClimbing) ClimbEnd(TEXT("You step off the face into the Oathframe."), true);
	bOathOn = true;
	OathLastDrain = NowMs();
	OathApplySpeed(true);
	Say(TEXT("You step into the Oathframe. You walk slowly, you cannot Skywing or climb well, and a slam (Dash / C) fills a foe's Guardbreak bar. The hearth-core drains while you are inside; at zero you step out safely."));
	return true;
}

bool UVenturesFeature::OathSlam()
{
	if (!bOathOn) { Say(TEXT("Step into the Oathframe first.")); return false; }
	if (NowMs() < OathSlamAt) { Say(TEXT("The slam is still recovering.")); return false; }
	OathSlamAt = NowMs() + SlamCd;
	AAerondelMob* Best = OathNearest(SlamR);
	if (!Best) { Say(TEXT("The shoulder-check finds no one close enough.")); return true; }
	const bool bBoss = IsBossy(Best);
	FGuardPoise& P = GuardPoise.FindOrAdd(Best);
	const int32 Max = GuardPoiseMax(Best);
	P.Poise = FMath::Min((float)Max, P.Poise + (float)Max * (bBoss ? 0.18f : 0.45f));
	const FString Name = Best->GetDisplayName().ToString();
	Say(bBoss
		? FString::Printf(TEXT("The Oathframe slams the %s. A boss only feels a little of it."), *Name)
		: FString::Printf(TEXT("The Oathframe slams the %s. Their Guardbreak bar fills. This is not a damage percent."), *Name));
	return true;
}

void UVenturesFeature::TickOath()
{
	if (!bOathOn) { if (bOathSpeedOn) OathApplySpeed(false); return; }
	OathApplySpeed(true);
	const int64 Now = NowMs();
	if (Now - OathLastDrain >= DrainMs)
	{
		const int32 Steps = FMath::Max(1, (int32)((Now - OathLastDrain) / DrainMs));
		OathLastDrain += (int64)Steps * DrainMs;
		OathCoreAmt = FMath::Max(0, OathCoreAmt - Steps);
		if (OathCoreAmt <= 0) OathStepOut(TEXT("The hearth-core runs dry. You step out safely — you do not fall."));
	}
}

void UVenturesFeature::TabOath(FGPanelModel& M)
{
	M.Title = TEXT("Oathframe");
	M.Lines.Add(TEXT("A heavy, enterable suit. You walk slowly, you cannot Skywing or climb well, Dash (C) is a shoulder-check, and a slam fills a foe's Guardbreak bar. A hearth-core drains while you are inside; at zero you step out safely. Not a damage percent, not a Path, not a Vocation. Town roads are fine; shops and houses ask you to step out. Load always steps you out, core intact."));
	M.Lines.Add(FString::Printf(TEXT("Core: %d/%d · %s"), OathCoreAmt, CoreMax, bOathOn ? TEXT("You are inside.") : (bOathAssembled ? TEXT("You have a frame ready.") : TEXT("Not yet assembled."))));
	if (!bOathAssembled) VAct35(M, TEXT("Assemble (8 Iron bars, 6 Logs, 2 Coal, Construction 20, Smithing 20)"), [this] { OathAssemble(); });
	if (bOathAssembled) VAct35(M, TEXT("Craft a hearth-core (2 Iron bars, 4 Coal, Smithing 10)"), [this] { OathCraftCore(); });
	if (bOathAssembled && !bOathOn) VAct35(M, TEXT("Step in"), [this] { OathStepIn(); });
	if (bOathOn)
	{
		VAct35(M, TEXT("Step out"), [this] { OathStepOut(); });
		VAct35(M, TEXT("Slam (or press C / Dash)"), [this] { OathSlam(); });
	}
}
