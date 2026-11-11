#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"

// Ventures group 38 (web adapt/wildmarks.js): up to three held marks that change how you see,
// climb, swim, or handle ley-fire. They are not Quirks (Ysolde's remedy will not clear them),
// not Leychemistry, and not a damage percent. Cap 3, two is the comfortable number.

namespace VenturesMarks
{
	static constexpr int32 Cap = 3;
	static constexpr int64 HushMs = 10 * 60000;
	struct FMark { const TCHAR* Id; const TCHAR* Name; const TCHAR* Line; };
	static const FMark Marks[] = {
		{ TEXT("nighteye"), TEXT("Night-eye"), TEXT("A lamp pip, and Shadework notice is a little wider.") },
		{ TEXT("kindled"), TEXT("Fire-kindled"), TEXT("Ley-fire spreads a little farther and rain puts it out more slowly.") },
		{ TEXT("deeplung"), TEXT("Deep-lung"), TEXT("Climbing costs a little less run energy; you swim and wade a little slower.") },
	};
	static const FMark* Find(const FString& Id) { for (const FMark& M : Marks) if (Id == M.Id) return &M; return nullptr; }
	static void VAct38(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesMarks;

void UVenturesFeature::ResetMark()
{
	MarkHeld.Reset();
	MarkHushUntil = 0;
	MarkNoticeMul = 1.f;
	if (bMarkSwimOn)
	{
		// speed is restored on the next tick if a pawn is still around; clear the flag so we do not stack
		bMarkSwimOn = false;
		MarkSwimSaved = 0.f;
	}
}

void UVenturesFeature::SaveMark(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	PutStrArr(*S, TEXT("held"), MarkHeld);
	S->SetNumberField(TEXT("hushUntil"), (double)MarkHushUntil);
	A.SetObjectField(TEXT("blight"), S);
}

void UVenturesFeature::LoadMark(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("blight"));
	if (!S) return;
	MarkHeld = JStrArr(*S, TEXT("held"));
	while (MarkHeld.Num() > Cap) MarkHeld.RemoveAt(MarkHeld.Num() - 1);
	MarkHushUntil = (int64)JInt(*S, TEXT("hushUntil"));
}

bool UVenturesFeature::MarkActive(const FString& Id) const
{
	if (!MarkHeld.Contains(Id)) return false;
	if (MarkHushUntil > 0 && NowMs() < MarkHushUntil) return false;
	return Find(Id) != nullptr;
}

bool UVenturesFeature::MarkGrant(const FString& Id)
{
	if (!Find(Id)) return false;
	if (MarkHeld.Contains(Id)) { Say(TEXT("You already carry that Wildmark.")); return false; }
	if (MarkHeld.Num() >= Cap) { Say(TEXT("You already carry three Wildmarks. Clear one first. Two is the comfortable number.")); return false; }
	MarkHeld.Add(Id);
	const FMark* M = Find(Id);
	Say(FString::Printf(TEXT("A Wildmark settles: %s. %s This is not a Quirk, and it is not a damage percent."), M->Name, M->Line));
	if (MarkHeld.Num() == 2) Say(TEXT("Two Wildmarks is a comfortable load."));
	if (MarkHeld.Num() == 3) Say(TEXT("Three is the cap. A Mark-clear tonic will lift one."));
	return true;
}

bool UVenturesFeature::MarkHush()
{
	if (!MarkHeld.Num()) { Say(TEXT("You are not carrying a Wildmark.")); return false; }
	if (CountItem(TEXT("Mark-hush tonic")) < 1) { Say(TEXT("You need a Mark-hush tonic.")); return false; }
	if (!TakeItem(TEXT("Mark-hush tonic"), 1)) return false;
	MarkHushUntil = NowMs() + HushMs;
	Say(TEXT("The marks go quiet for ten minutes. They are still on you."));
	return true;
}

bool UVenturesFeature::MarkClear(const FString& Id)
{
	if (!MarkHeld.Contains(Id)) { Say(TEXT("You are not carrying that mark.")); return false; }
	if (CountItem(TEXT("Mark-clear tonic")) < 1) { Say(TEXT("You need a Mark-clear tonic. Ysolde's quirk remedy will not do this.")); return false; }
	if (!TakeItem(TEXT("Mark-clear tonic"), 1)) return false;
	MarkHeld.Remove(Id);
	Say(TEXT("The Wildmark lifts. Ysolde's quirk remedy cannot do this."));
	return true;
}

bool UVenturesFeature::MarkDraft(const FString& Id)
{
	if (CountItem(TEXT("Wildmark tonic")) < 1) { Say(TEXT("You need a Wildmark tonic to draft a mark.")); return false; }
	if (MarkHeld.Contains(Id)) { Say(TEXT("You already carry that Wildmark.")); return false; }
	if (MarkHeld.Num() >= Cap) { Say(TEXT("You already carry three Wildmarks. Clear one first.")); return false; }
	if (!TakeItem(TEXT("Wildmark tonic"), 1)) return false;
	return MarkGrant(Id);
}

bool UVenturesFeature::MarkBrew(const FString& Item)
{
	struct FBrew { const TCHAR* Item; const TCHAR* Skill; int32 Lvl; const TCHAR* A; int32 Na; const TCHAR* B; int32 Nb; const TCHAR* C; int32 Nc; int32 Xp; };
	static const FBrew Brews[] = {
		{ TEXT("Wildmark tonic"), TEXT("Herbalism"), 20, TEXT("Harrowbloom"), 1, TEXT("Vial of water"), 1, TEXT("White berries"), 1, 50 },
		{ TEXT("Mark-hush tonic"), TEXT("Herbalism"), 18, TEXT("Marshmint"), 1, TEXT("Vial of water"), 1, nullptr, 0, 40 },
		{ TEXT("Mark-clear tonic"), TEXT("Herbalism"), 28, TEXT("Ravenweed"), 1, TEXT("Vial of water"), 1, TEXT("Unicorn horn dust"), 1, 70 },
	};
	const FBrew* B = nullptr; for (const FBrew& X : Brews) if (Item == X.Item) { B = &X; break; }
	if (!B) return false;
	if (SkillLevel(B->Skill) < B->Lvl) { Say(FString::Printf(TEXT("You need %s %d to brew that."), B->Skill, B->Lvl)); return false; }
	if (CountItem(B->A) < B->Na || CountItem(B->B) < B->Nb || (B->C && CountItem(B->C) < B->Nc)) { Say(TEXT("You do not have the ingredients.")); return false; }
	if (!TakeItem(B->A, B->Na) || !TakeItem(B->B, B->Nb) || (B->C && !TakeItem(B->C, B->Nc))) return false;
	GiveItem(B->Item, 1);
	GiveXp(B->Skill, (float)B->Xp);
	Say(FString::Printf(TEXT("You brew a %s."), B->Item));
	return true;
}

float UVenturesFeature::LeySpreadMul() const { return MarkActive(TEXT("kindled")) ? 1.25f : 1.f; }
float UVenturesFeature::LeyRainMul() const { return MarkActive(TEXT("kindled")) ? 0.55f : 1.f; }

void UVenturesFeature::TickMark()
{
	MarkNoticeMul = MarkActive(TEXT("nighteye")) ? 1.25f : 1.f;
	AAerondelPlayerState* PS = GetPS();
	ACharacter* C = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
	UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr;
	if (!M) { bMarkSwimOn = false; return; }
	const bool bWater = M->IsSwimming() || (M->GetPhysicsVolume() && M->GetPhysicsVolume()->bWaterVolume);
	const bool bSlow = bWater && MarkActive(TEXT("deeplung"));
	if (!bSlow)
	{
		if (bMarkSwimOn)
		{
			if (!bOathOn) M->MaxWalkSpeed = MarkSwimSaved;
			bMarkSwimOn = false;
		}
		return;
	}
	if (!bMarkSwimOn) { MarkSwimSaved = M->MaxWalkSpeed; bMarkSwimOn = true; }
	const float Base = (bOathOn && bOathSpeedOn) ? OathSavedWalk * 0.55f : MarkSwimSaved;
	M->MaxWalkSpeed = Base * 0.8f;
}

void UVenturesFeature::TabMark(FGPanelModel& M)
{
	M.Title = TEXT("Wildmarks");
	M.Lines.Add(TEXT("A Wildmark is a held change to how you see, climb, swim or handle ley-fire. It is not a Quirk — Ysolde's remedy will not clear it — not Leychemistry, and not a damage percent. You can carry up to three; two is the comfortable number. A Mark-hush tonic quiets them for ten minutes without lifting them."));
	const bool bHush = MarkHushUntil > 0 && NowMs() < MarkHushUntil;
	if (bHush) M.Lines.Add(FString::Printf(TEXT("Hushed for about %d more minutes. The marks are still on you."), FMath::Max(1, (int32)((MarkHushUntil - NowMs() + 59999) / 60000))));
	M.Lines.Add(FString::Printf(TEXT("Carrying %d/%d"), MarkHeld.Num(), Cap));
	for (const FString& Id : MarkHeld)
	{
		const FMark* K = Find(Id);
		M.Lines.Add(FString::Printf(TEXT("• %s%s — %s"), K ? K->Name : *Id, (bHush ? TEXT(" (hushed)") : TEXT("")), K ? K->Line : TEXT("")));
		VAct38(M, FString::Printf(TEXT("Clear %s (tonic)"), K ? K->Name : *Id), [this, Id] { MarkClear(Id); }, CountItem(TEXT("Mark-clear tonic")) > 0);
	}
	if (MarkHeld.Num()) VAct38(M, TEXT("Hush all (tonic)"), [this] { MarkHush(); }, CountItem(TEXT("Mark-hush tonic")) > 0 && !bHush);
	M.Lines.Add(TEXT("Draft a mark"));
	M.Lines.Add(TEXT("A Wildmark tonic lets you take one, if you have room."));
	for (const FMark& K : Marks)
	{
		if (MarkHeld.Contains(K.Id)) continue;
		VAct38(M, FString::Printf(TEXT("Draft %s"), K.Name), [this, Id = FString(K.Id)] { MarkDraft(Id); }, CountItem(TEXT("Wildmark tonic")) > 0 && MarkHeld.Num() < Cap);
	}
	M.Lines.Add(TEXT("Brew"));
	VAct38(M, TEXT("Brew Wildmark tonic (Herbalism 20)"), [this] { MarkBrew(TEXT("Wildmark tonic")); });
	VAct38(M, TEXT("Brew Mark-hush tonic (Herbalism 18)"), [this] { MarkBrew(TEXT("Mark-hush tonic")); });
	VAct38(M, TEXT("Brew Mark-clear tonic (Herbalism 28)"), [this] { MarkBrew(TEXT("Mark-clear tonic")); });
}
