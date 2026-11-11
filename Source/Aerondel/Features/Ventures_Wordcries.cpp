#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Rules/AerondelRules.h"
#include "GameFramework/Pawn.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 11 (web adapt/wordcries.js): short cries learned word-by-word from frontier cry-stones.
// Three words per cry, each making it stronger. Costs Prayer and a short cooldown.
// Not Overflare, not an Attunement super, not a new skill.

namespace VenturesCry
{
	struct FCryDef { const TCHAR* Id; const TCHAR* Name; const TCHAR* Words[3]; int32 Prayer[3]; int32 Cd = 18; float X = 0.f; float Z = 0.f; const TCHAR* Desc; };
	static const FCryDef Cries[] = {
		{ TEXT("stormheave"), TEXT("Stormheave"), { TEXT("Storm"), TEXT("Heave"), TEXT("Break") }, { 8, 12, 16 }, 18,  36.f, -70.f, TEXT("A force cry that shoves foes back.") },
		{ TEXT("rimehold"),   TEXT("Rimehold"),   { TEXT("Rime"), TEXT("Hold"), TEXT("Still") },  { 8, 12, 16 }, 18, -42.f, -92.f, TEXT("A frost cry that slows and chills.") },
		{ TEXT("windstride"), TEXT("Windstride"), { TEXT("Wind"), TEXT("Stride"), TEXT("Rush") }, { 6, 10, 14 }, 16,  88.f,  18.f, TEXT("A whirlwind rush that carries you forward.") },
		{ TEXT("heartencry"), TEXT("Heartencry"), { TEXT("Heart"), TEXT("Ward"), TEXT("Valor") }, { 8, 12, 16 }, 22,  48.f, -48.f, TEXT("A valor cry that steels you and your companion.") },
	};
	static const FCryDef* FindCry(const FString& Id) { for (const FCryDef& C : Cries) if (Id == C.Id) return &C; return nullptr; }
	static void VAct11(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesCry;

void UVenturesFeature::ResetCry()
{ CryWords.Reset(); CryEquipped.Reset(); CryUses = 0; CryLastMs = 0; CryValorUntil = 0; }

void UVenturesFeature::SaveCry(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> W = MakeShared<FJsonObject>();
	for (const FCryDef& C : Cries) W->SetNumberField(C.Id, CryWords.FindRef(C.Id));
	S->SetObjectField(TEXT("words"), W);
	S->SetStringField(TEXT("equipped"), CryEquipped);
	S->SetNumberField(TEXT("uses"), CryUses);
	S->SetNumberField(TEXT("last"), (double)CryLastMs);
	A.SetObjectField(TEXT("cries"), S);
}

void UVenturesFeature::LoadCry(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("cries"));
	if (!S) return;
	if (TSharedPtr<FJsonObject> W = JObj(*S, TEXT("words")))
		for (const FCryDef& C : Cries) CryWords.Add(C.Id, FMath::Clamp((int32)JInt(*W, C.Id), 0, 3));
	CryEquipped = JStr(*S, TEXT("equipped"));
	if (!FindCry(CryEquipped) || CryWords.FindRef(CryEquipped) <= 0)
	{
		CryEquipped.Reset();
		for (const FCryDef& C : Cries) if (CryWords.FindRef(C.Id) > 0) { CryEquipped = C.Id; break; }
	}
	CryUses = FMath::Max(0, (int32)JInt(*S, TEXT("uses")));
	CryLastMs = JInt(*S, TEXT("last"));
}

int32 UVenturesFeature::CryWordsOf(const FString& Id) const { return FMath::Clamp(CryWords.FindRef(Id), 0, 3); }

bool UVenturesFeature::CryNear(const FString& Id, float& Dist) const
{
	const FCryDef* D = FindCry(Id);
	FVector2D P; if (!D || !PlayerWebPos(P)) return false;
	Dist = FVector2D::Distance(P, FVector2D(D->X, D->Z));
	return Dist < 3.4f;
}

bool UVenturesFeature::CryLearn(const FString& Id)
{
	const FCryDef* D = FindCry(Id); if (!D) return false;
	float Dist = 0.f;
	if (!CryNear(Id, Dist)) { Say(FString::Printf(TEXT("Study the %s cry-stone first (at %.0f, %.0f)."), D->Name, D->X, D->Z)); return false; }
	const int32 N = CryWordsOf(Id);
	if (N >= 3) { Say(FString::Printf(TEXT("You already know every word of %s."), D->Name)); return false; }
	CryWords.Add(Id, N + 1);
	if (CryEquipped.IsEmpty()) CryEquipped = Id;
	Say(FString::Printf(TEXT("You learn \"%s\" of %s (%d/3)."), D->Words[N], D->Name, N + 1));
	return true;
}

bool UVenturesFeature::CryEquip(const FString& Id)
{ if (CryWordsOf(Id) < 1) return false; CryEquipped = Id; return true; }

int32 UVenturesFeature::CryIncoming(int32 Amount) const
{
	if (Amount <= 0 || !CryValorUntil || TickNow() >= CryValorUntil) return Amount;
	return FMath::Max(1, Amount - 1);
}

bool UVenturesFeature::CryUse(const FString& InId)
{
	FString Id = InId.IsEmpty() ? CryEquipped : InId;
	const FCryDef* D = FindCry(Id);
	if (!D) { Say(TEXT("Choose a Wordcry first (Ventures -> Wordcries).")); return false; }
	const int32 N = CryWordsOf(Id);
	if (N < 1) { Say(FString::Printf(TEXT("You have not yet learned a word of %s. Study its cry-stone."), D->Name)); return false; }
	const int64 CdMs = (int64)FMath::RoundToInt(D->Cd * 1000.f * (1.f - LeyFx().CryCd));
	const int64 Wait = (CryLastMs + FMath::Max<int64>(1000, CdMs)) - NowMs();
	if (Wait > 0) { Say(FString::Printf(TEXT("%s is still cooling (%llds)."), D->Name, (Wait + 999) / 1000)); return false; }
	AAerondelPlayerState* PS = GetPS();
	const int32 Cost = D->Prayer[N - 1];
	if (!PS || !PS->Stats || PS->Stats->PrayerPoints < Cost) { Say(FString::Printf(TEXT("You need %d Prayer points for %s."), Cost, D->Name)); return false; }
	PS->Stats->PrayerPoints -= Cost;
	CryLastMs = NowMs(); ++CryUses; CryEquipped = Id;
	FString Words = D->Words[0];
	for (int32 i = 1; i < N; ++i) { Words += TEXT("! "); Words += D->Words[i]; }
	Words += TEXT("!");
	Say(FString::Printf(TEXT("You cry %s: %s"), D->Name, *Words));
	FVector2D P; PlayerWebPos(P);
	if (LeyKey(TEXT("wordecho")))
	{
		for (AAerondelMob* M : MobsNear(P, 3.f + N * 1.5f, 8)) if (M && !M->IsDead()) M->Stun(2);
		Say(TEXT("The cry echoes and briefly slows nearby foes."));
	}
	const float R = 3.f + N * 1.5f; const int32 K = N;
	if (Id == TEXT("stormheave"))
	{
		APawn* Pawn = PS->GetPawn();
		for (AAerondelMob* M : MobsNear(P, R, 8))
		{
			if (!M || M->IsDead()) continue;
			FVector Dlt = M->GetActorLocation() - (Pawn ? Pawn->GetActorLocation() : M->GetActorLocation());
			Dlt.Z = 0.f; if (!Dlt.Normalize() && Pawn) Dlt = Pawn->GetActorForwardVector();
			M->SetActorLocation(M->GetActorLocation() + Dlt * (2.f + K) * 100.f, true);
			M->Stun(K);
		}
	}
	else if (Id == TEXT("rimehold"))
	{
		for (AAerondelMob* M : MobsNear(P, R, 8)) if (M && !M->IsDead()) M->Stun(1 + K);
	}
	else if (Id == TEXT("windstride"))
	{
		if (APawn* Pawn = PS->GetPawn()) Pawn->SetActorLocation(Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * ((3.f + K * 2.f) * 100.f), true);
	}
	else if (Id == TEXT("heartencry"))
	{
		CryValorUntil = TickNow() + 8 + K * 4;
		if (PS->Health && PS->Health->GetCurrent() > 0)
			PS->Health->Heal(FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * (0.04f * K))));
		Say(TEXT("Heartencry steels you and your companion."));
	}
	LeyOnCry();
	return true;
}

void UVenturesFeature::TabCry(FGPanelModel& M)
{
	M.Title = FString::Printf(TEXT("Wordcries · used %d"), CryUses);
	M.Lines.Add(TEXT("Three words per cry, learned from frontier cry-stones. Press ; or Cry to shout. Costs Prayer and a short cooldown — not Overflare, not an Attunement super."));
	for (const FCryDef& D : Cries)
	{
		const int32 N = CryWordsOf(D.Id);
		FString Known;
		for (int32 i = 0; i < 3; ++i) { if (i) Known += TEXT(" · "); Known += (i < N ? FString(D.Words[i]) : FString(TEXT("..."))); }
		float Dist = 0.f; const bool bNear = CryNear(D.Id, Dist);
		M.Lines.Add(FString::Printf(TEXT("%s — %d/3%s. %s Words: %s. Cost %d Prayer. Cry-stone at %.0f, %.0f%s."),
			D.Name, N, CryEquipped == D.Id ? TEXT(" · equipped") : TEXT(""), D.Desc, *Known,
			N ? D.Prayer[N - 1] : D.Prayer[0], D.X, D.Z, bNear ? TEXT(" · you are here") : TEXT("")));
		const FString Id = D.Id;
		if (bNear && N < 3) VAct11(M, FString::Printf(TEXT("Learn %s"), D.Words[N]), [this, Id] { CryLearn(Id); });
		if (N > 0)
		{
			VAct11(M, CryEquipped == D.Id ? FString::Printf(TEXT("%s (equipped)"), D.Name) : FString::Printf(TEXT("Equip %s"), D.Name), [this, Id] { CryEquip(Id); });
			VAct11(M, FString::Printf(TEXT("Cry %s (;)"), D.Name), [this, Id] { CryUse(Id); }, true, true);
		}
	}
}
