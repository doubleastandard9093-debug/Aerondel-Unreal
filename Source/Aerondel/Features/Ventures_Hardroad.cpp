#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Data/ItemDef.h"
#include "Dom/JsonObject.h"

// Ventures group 36 (web adapt/hardroad.js): optional hunger, thirst and rest meters.
// Off by default on new and old saves. Never lethal. Existing food still heals; wells and
// waterskins fill thirst; beds at a Townstead or Waycamp fill rest. Low meters only slow
// health, prayer and run-energy recovery. Turn it off at any time; meters pause.
// They never strip XP and never block a skill. An Oathframe core still drains either way.

namespace VenturesRoad
{
	static constexpr int32 MeterMax = 100, Low = 30;
	static constexpr int64 DrainMs = 45000;
	static void VAct36(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesRoad;

void UVenturesFeature::ResetRoad()
{
	bRoadOn = false;
	bRoadHideHud = false;
	RoadHunger = RoadThirst = RoadRestAmt = MeterMax;
	RoadLast = 0;
}

void UVenturesFeature::SaveRoad(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetBoolField(TEXT("on"), bRoadOn);
	S->SetNumberField(TEXT("hunger"), RoadHunger);
	S->SetNumberField(TEXT("thirst"), RoadThirst);
	S->SetNumberField(TEXT("rest"), RoadRestAmt);
	S->SetBoolField(TEXT("hideHud"), bRoadHideHud);
	A.SetObjectField(TEXT("road"), S);
}

void UVenturesFeature::LoadRoad(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("road"));
	if (!S) { bRoadOn = false; RoadHunger = RoadThirst = RoadRestAmt = MeterMax; return; } // old saves stay off
	bRoadOn = JBool(*S, TEXT("on"));
	auto Meter = [&](const TCHAR* K) { const int32 V = S->HasField(K) ? (int32)JInt(*S, K, MeterMax) : MeterMax; return FMath::Clamp(V, 0, MeterMax); };
	RoadHunger = Meter(TEXT("hunger"));
	RoadThirst = Meter(TEXT("thirst"));
	RoadRestAmt = Meter(TEXT("rest"));
	bRoadHideHud = JBool(*S, TEXT("hideHud"));
	RoadLast = NowMs();
}

float UVenturesFeature::RoadRegenMul() const
{
	if (!bRoadOn) return 1.f;
	const int32 L = FMath::Min3(RoadHunger, RoadThirst, RoadRestAmt);
	if (L >= Low) return 1.f;
	return 0.5f + 0.5f * ((float)L / (float)Low); // at 0, half speed — never a full stop, never lethal
}

float UVenturesFeature::RoadRegenMul(const UObject* WorldContext)
{
	const UVenturesFeature* V = Get(WorldContext);
	return V ? V->RoadRegenMul() : 1.f;
}

bool UVenturesFeature::RoadToggle(bool bOn)
{
	bRoadOn = bOn;
	RoadLast = NowMs();
	Say(bRoadOn
		? TEXT("Hardroad is on. Hunger, thirst and rest will tick. They never kill you, never strip XP, and never block a skill. Turn it off at any time.")
		: TEXT("Hardroad is off. The meters pause and are not used."));
	return bRoadOn;
}

int32 UVenturesFeature::RoadFill(const FString& Kind, int32 N)
{
	int32* M = nullptr;
	if (Kind == TEXT("hunger")) M = &RoadHunger;
	else if (Kind == TEXT("thirst")) M = &RoadThirst;
	else if (Kind == TEXT("rest")) M = &RoadRestAmt;
	if (!M) return 0;
	const int32 Before = *M;
	*M = FMath::Min(MeterMax, *M + FMath::Max(0, N));
	return *M - Before;
}

int32 UVenturesFeature::RoadEat(int32 N)
{
	const int32 Got = RoadFill(TEXT("hunger"), N);
	if (Got && bRoadOn) Say(TEXT("That helps your hunger. The food still heals as it always did."));
	return Got;
}

int32 UVenturesFeature::RoadDrink(int32 N)
{
	const int32 Got = RoadFill(TEXT("thirst"), N);
	if (Got && bRoadOn) Say(TEXT("That helps your thirst."));
	return Got;
}

int32 UVenturesFeature::RoadRest(int32 N)
{
	const int32 Got = RoadFill(TEXT("rest"), N);
	if (Got && bRoadOn) Say(TEXT("You rest. That helps your rest meter."));
	return Got;
}

void UVenturesFeature::RoadOnConsume(const UItemDef* Item)
{
	if (!Item) return;
	const FString N = Item->DisplayName.ToString();
	if (Item->IsFood()) RoadEat(25);
	if (N.Contains(TEXT("Waterskin")) || N.Contains(TEXT("waterskin")) || N.Contains(TEXT("Season flask"))) RoadDrink(25);
}

void UVenturesFeature::TickRoad()
{
	if (!bRoadOn) { RoadLast = NowMs(); return; }
	const int64 Now = NowMs();
	if (!RoadLast) { RoadLast = Now; return; }
	const int64 Span = Now - RoadLast;
	if (Span < DrainMs) return;
	const int32 Steps = (int32)(Span / DrainMs);
	RoadLast += (int64)Steps * DrainMs;
	RoadHunger = FMath::Max(0, RoadHunger - Steps);
	RoadThirst = FMath::Max(0, RoadThirst - Steps);
	RoadRestAmt = FMath::Max(0, RoadRestAmt - Steps);
	// Meters do not damage you, do not strip XP, and do not block skills.
}

void UVenturesFeature::TabRoad(FGPanelModel& M)
{
	M.Title = TEXT("Hardroad");
	M.Lines.Add(TEXT("An optional travel mode. When it is on, hunger, thirst and rest tick. Food still heals as it always did. Wells and waterskins help thirst. A Townstead or Waycamp bed helps rest. Low meters only slow health, prayer and run-energy recovery. They never kill you, never strip XP, and never block a skill. An Oathframe core still drains in or out of Hardroad. Off by default."));
	M.Lines.Add(FString::Printf(TEXT("Hunger %d · Thirst %d · Rest %d"), RoadHunger, RoadThirst, RoadRestAmt));
	VAct36(M, bRoadOn ? TEXT("Turn Hardroad off") : TEXT("Turn Hardroad on"), [this] { RoadToggle(!bRoadOn); });
	VAct36(M, bRoadHideHud ? TEXT("Show the meters") : TEXT("Hide the meters"), [this] { bRoadHideHud = !bRoadHideHud; });
}
