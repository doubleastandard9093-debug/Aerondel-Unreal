#include "Features/StableCodexFeature.h"
#include "Features/SteedsFeature.h"
#include "Features/SkywingFeature.h"

static const TCHAR* GLookSlots[] = { TEXT("head"), TEXT("body"), TEXT("legs"), TEXT("shield"), TEXT("cape") };

const TArray<FString>& UStableCodexFeature::Roosts()
{
	static const TArray<FString> R = { TEXT("Brightwater"), TEXT("Ashford"), TEXT("Port Sarim"), TEXT("Varrock"), TEXT("Falador") };
	return R;
}

TArray<FCodexMount> UStableCodexFeature::GetMounts() const
{
	const USteedsFeature* St = Feature<USteedsFeature>();
	const USkywingFeature* Sk = Feature<USkywingFeature>();
	TArray<FCodexMount> Out;
	auto Add = [&Out](const TCHAR* Id, const TCHAR* Name, const TCHAR* Kind, const TCHAR* How, bool bOwned) { FCodexMount& M = Out.AddDefaulted_GetRef(); M.Id = Id; M.Name = Name; M.Kind = Kind; M.How = How; M.bOwned = bOwned; };
	Add(TEXT("stag"), TEXT("Fernback Stag"), TEXT("steed"), TEXT("Agility 10"), St && St->Owns(TEXT("stag")));
	Add(TEXT("boar"), TEXT("Ironbristle Boar"), TEXT("steed"), TEXT("Agility 30"), St && St->Owns(TEXT("boar")));
	Add(TEXT("skywing"), TEXT("Skywing"), TEXT("flyer"), TEXT("Agility 70 and Exalted with a faction"), Sk && Sk->IsUnlocked());
	Add(TEXT("skyrook"), TEXT("Skyrook"), TEXT("flyer"), TEXT("Visit Roostmaster Hale at Brightwater"), bSkyrook);
	return Out;
}

bool UStableCodexFeature::StartFlight(const FString& Roost)
{
	if (!bSkyrook) { Say(TEXT("Visit Roostmaster Hale at Brightwater to ride the Skyrooks.")); return false; }
	if (!Roosts().Contains(Roost)) return false;
	++Flights; return true;
}

bool UStableCodexFeature::SetLook(const FString& Slot, const FString& Item)
{
	bool bSlot = false; for (const TCHAR* S : GLookSlots) if (Slot == S) bSlot = true;
	if (!bSlot) return false;
	if (Item.IsEmpty()) { Looks.Remove(Slot); return true; }
	if (!Seen.Contains(Item)) return false;
	Looks.Add(Slot, Item); return true;
}

void UStableCodexFeature::SaveState(FJsonObject& Out) const
{
	Out.SetBoolField(TEXT("skyrook"), bSkyrook); Out.SetNumberField(TEXT("flights"), Flights);
	TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>(); for (const TPair<FString, FString>& P : Looks) L->SetStringField(P.Key, P.Value);
	Out.SetObjectField(TEXT("looks"), L);
	PutStrArr(Out, TEXT("seen"), Seen);
}

void UStableCodexFeature::LoadState(const FJsonObject& In)
{
	bSkyrook = JBool(In, TEXT("skyrook")); Flights = FMath::Max(0, (int32)JInt(In, TEXT("flights")));
	if (TSharedPtr<FJsonObject> L = JObj(In, TEXT("looks"))) for (const TCHAR* S : GLookSlots) { const FString V = JStr(*L, S); if (!V.IsEmpty()) Looks.Add(S, V); }
	Seen = JStrArr(In, TEXT("seen")); if (Seen.Num() > 400) Seen.SetNum(400);
}
