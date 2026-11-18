#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/EquipmentComponent.h"
#include "Combat/AerondelMob.h"
#include "Rules/AerondelRules.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"

// Ventures group 33 (web adapt/shadework.js): after you leave a foe's notice you are unseen for a
// short window. The first strike in that window is a Shade hit: a modest named extra (20%, less on
// a boss) and a large fill of their Guardbreak bar. Ranged and melee both work, at different ranges.
// Bosses and Colossi only feel a little of it, so you cannot lock them. This is not a sneak through
// town, and it does not replace a pickpocket, Stillwatch or True Aim.

namespace VenturesShade
{
	static constexpr float ShadeDmg = 0.2f, ShadeDmgBoss = 0.08f, PoiseFrac = 0.55f, PoiseFracBoss = 0.15f;
	static constexpr float NoticeR = 9.f, NoticeRHush = 7.f, MeleeR = 2.4f, RangeR = 8.f;
	static constexpr int64 WindowMs = 2500;
	static void VAct33(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
	static bool Faces(const AAerondelMob* M, const FVector& PlayerLoc)
	{
		if (!M) return false;
		const FVector ToP = (PlayerLoc - M->GetActorLocation()).GetSafeNormal2D();
		return FVector::DotProduct(M->GetActorForwardVector().GetSafeNormal2D(), ToP) > 0.35f;
	}
	static bool Occluded(UWorld* W, const AAerondelMob* M, AActor* Player)
	{
		if (!W || !M || !Player) return false;
		FHitResult Hit;
		FCollisionQueryParams P(SCENE_QUERY_STAT(ShadeOcclude), false);
		P.AddIgnoredActor(M);
		P.AddIgnoredActor(Player);
		return W->LineTraceSingleByChannel(Hit, M->GetActorLocation() + FVector(0, 0, 80.f), Player->GetActorLocation() + FVector(0, 0, 80.f), ECC_Visibility, P);
	}
}
using namespace VenturesShade;

void UVenturesFeature::ResetShade()
{
	ShadeHits = ShadeUnseenSecs = 0;
	bShadeHidePip = false;
	bShadeUnseen = true;
	ShadeUntilMs = 0;
	// MarkNoticeMul is owned by later systems; leave at 1 on a fresh character.
	MarkNoticeMul = 1.f;
}

void UVenturesFeature::SaveShade(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("hits"), ShadeHits);
	S->SetNumberField(TEXT("unseenTime"), ShadeUnseenSecs);
	S->SetBoolField(TEXT("hidePip"), bShadeHidePip);
	A.SetObjectField(TEXT("shade"), S);
}

void UVenturesFeature::LoadShade(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("shade"));
	if (!S) return;
	ShadeHits = (int32)JInt(*S, TEXT("hits"));
	ShadeUnseenSecs = (int32)JInt(*S, TEXT("unseenTime"));
	bShadeHidePip = JBool(*S, TEXT("hidePip"));
	bShadeUnseen = true; // mid-fight unseen is not saved
	ShadeUntilMs = 0;
}

void UVenturesFeature::ShadeSetHidePip(bool bHide) { bShadeHidePip = bHide; }

float UVenturesFeature::ShadeNoticeRange() const
{
	const bool bHush = KitHush() || VeilHush();
	return (bHush ? NoticeRHush : NoticeR) * FMath::Max(0.2f, MarkNoticeMul);
}

int64 UVenturesFeature::ShadeWindowMs() const
{
	const bool bHush = KitHush() || VeilHush();
	return WindowMs + (int64)FMath::RoundToInt((float)(SkillLevel(TEXT("Agility")) + SkillLevel(TEXT("Thieving"))) * 12.f) + (bHush ? 400 : 0);
}

TArray<AAerondelMob*> UVenturesFeature::ShadeFoesNear(float RadiusM) const
{
	TArray<AAerondelMob*> Out;
	const UGameInstance* GI = GetGameInstance();
	UWorld* W = GI ? GI->GetWorld() : nullptr;
	FVector2D P;
	if (!W || !PlayerWebPos(P)) return Out;
	const float Rcm = RadiusM * 100.f;
	const AAerondelPlayerState* PS = GetPS();
	AActor* Pawn = PS ? PS->GetPawn() : nullptr;
	for (TActorIterator<AAerondelMob> It(W); It; ++It)
	{
		AAerondelMob* M = *It;
		if (!M || M->IsDead()) continue;
		if (Pawn && FVector::Dist2D(M->GetActorLocation(), Pawn->GetActorLocation()) <= Rcm) Out.Add(M);
	}
	return Out;
}

AAerondelMob* UVenturesFeature::ShadeSeer() const
{
	const AAerondelPlayerState* PS = GetPS();
	AActor* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Pawn) return nullptr;
	UWorld* W = Pawn->GetWorld();
	const float R = ShadeNoticeRange();
	for (AAerondelMob* M : ShadeFoesNear(R + 1.f))
	{
		const float D = FVector::Dist2D(M->GetActorLocation(), Pawn->GetActorLocation()) / 100.f;
		if (D > R) continue;
		if (Occluded(W, M, Pawn)) continue;
		const bool bFight = M->IsEngaged();
		if ((bFight || D < R * 0.7f) && Faces(M, Pawn->GetActorLocation())) return M;
	}
	return nullptr;
}

bool UVenturesFeature::ShadeCloseFight() const
{
	for (AAerondelMob* M : ShadeFoesNear(14.f)) if (M && M->IsEngaged()) return true;
	return false;
}

bool UVenturesFeature::ShadeEnter()
{
	if (bShadeUnseen) return false;
	bShadeUnseen = true;
	ShadeUntilMs = NowMs() + ShadeWindowMs();
	ShadeUnseenSecs += (int32)FMath::RoundToInt((float)ShadeWindowMs() / 1000.f);
	return true;
}

bool UVenturesFeature::ShadeNotice()
{
	if (!bShadeUnseen) return false;
	bShadeUnseen = false;
	ShadeUntilMs = 0;
	return true;
}

bool UVenturesFeature::ShadeBreakLine() { ShadeEnter(); return true; }

bool UVenturesFeature::ShadeArmed(const AAerondelMob* Mob) const
{
	if (!bShadeUnseen || !Mob) return false;
	const AAerondelPlayerState* PS = GetPS();
	AActor* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Pawn) return false;
	const float D = FVector::Dist2D(Mob->GetActorLocation(), Pawn->GetActorLocation()) / 100.f;
	const bool bRanged = UAerondelRules::IsRanged(PS);
	return D <= (bRanged ? RangeR : MeleeR) + 0.4f;
}

int32 UVenturesFeature::HitShade(AAerondelMob* Mob, int32 Damage)
{
	if (!Mob || Damage <= 0 || !ShadeArmed(Mob)) return Damage;
	const bool bBoss = IsBossy(Mob);
	Damage = FMath::Max(1, FMath::FloorToInt(Damage * (1.f + (bBoss ? ShadeDmgBoss : ShadeDmg))));
	FGuardPoise& P = GuardPoise.FindOrAdd(Mob);
	const int32 Max = GuardPoiseMax(Mob);
	P.Poise = FMath::Min((float)Max, P.Poise + (float)Max * (bBoss ? PoiseFracBoss : PoiseFrac));
	if (!bBoss) { P.RiposteUntil = FMath::Max(P.RiposteUntil, TickNow() + 3); P.bUsed = false; }
	++ShadeHits;
	const FString Name = Mob->GetDisplayName().ToString();
	Say(bBoss
		? FString::Printf(TEXT("Shade hit on the %s. The opening is smaller on a boss — this will not lock them."), *Name)
		: FString::Printf(TEXT("Shade hit! The %s takes a first strike from unseen, and their poise bar fills."), *Name));
	ShadeNotice();
	return Damage;
}

void UVenturesFeature::TickShade()
{
	if (ShadeSeer()) ShadeNotice();
	else if (!bShadeUnseen && !ShadeCloseFight()) ShadeEnter();
	if (bShadeUnseen && ShadeUntilMs && NowMs() > ShadeUntilMs && ShadeSeer()) ShadeNotice();
}

void UVenturesFeature::TabShade(FGPanelModel& M)
{
	M.Title = TEXT("Shadework");
	M.Lines.Add(TEXT("After you leave a foe's notice you are unseen for a short window. The first strike in that window is a Shade hit: a modest named extra (20%, less on a boss) and a large fill of their Guardbreak bar. Ranged and melee both work, at different ranges. Bosses cannot be locked by repeating this. It does not replace pickpocket, Stillwatch or True Aim."));
	M.Lines.Add(FString::Printf(TEXT("You are %s. Shade hits landed: %d."), bShadeUnseen ? TEXT("unseen") : TEXT("noticed"), ShadeHits));
	VAct33(M, bShadeHidePip ? TEXT("Show the eye pip") : TEXT("Hide the eye pip"), [this] { ShadeSetHidePip(!bShadeHidePip); });
	M.Lines.Add(TEXT("A hush-string kit or a quiet-step veil shortens a foe's notice range a little. Settings can also hide the pip (bind a widget to ShadeUnseen / ShadeHidePip)."));
}
