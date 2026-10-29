#include "Features/SailingFeature.h"
#include "Features/CompanionsFeature.h"
#include "Features/StandingFeature.h"
#include "Core/GielinorPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Data/ItemDef.h"
#include "World/GielinorScalability.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	const FVector2D SeaCentre(0.f, 1000.f);
	constexpr float SeaR = 170.f;
	const FVector2D SarimDock(-80.f, 76.f);
	const FVector2D SeaEntry(-40.f, 870.f);
	constexpr int32 EnemyMax = 3, CrewCost = 150, CrewMax = 6;
	constexpr float Range = 26.f, ReloadS = 3.2f, BallSpeed = 38.f;
	struct FEnemyKind { const TCHAR* Name; int32 Hp, Guns; float Speed; int32 DmgLo, DmgHi, LootLo, LootHi; };
	const FEnemyKind Kinds[2] = { { TEXT("Drowned sloop"), 90, 2, 6.5f, 6, 12, 120, 320 }, { TEXT("Drowned brig"), 160, 3, 5.8f, 8, 16, 300, 700 } };
	struct FOfficer { const TCHAR* Id; const TCHAR* Trait; };
	const FOfficer OfficerDefs[4] = { { TEXT("tamsin"), TEXT("surgeon") }, { TEXT("ysolde"), TEXT("lookout") }, { TEXT("brannoc"), TEXT("gunnery") }, { TEXT("mirelle"), TEXT("helm") } };
	const TCHAR* TraitOf(FName Id) { for (const FOfficer& O : OfficerDefs) if (Id == FName(O.Id)) return O.Trait; return nullptr; }
	// Chart dig spots, as fractions of the island radius.
	const float ChartSpots[3][3][2] = { { { 0.55f, 0.2f }, { -0.5f, 0.35f }, { 0.2f, 0.6f } }, { { -0.55f, -0.45f }, { 0.55f, -0.2f }, { 0.35f, 0.6f } }, { { 0.6f, -0.4f }, { -0.6f, -0.1f }, { 0.3f, 0.55f } } };
}

const TArray<FSeaIsland>& USailingFeature::Islands()
{
	static TArray<FSeaIsland> L;
	if (L.Num() == 0)
	{
		auto Add = [](const TCHAR* Id, const TCHAR* Name, float X, float Z, float R, bool bPort, float Dx, float Dz)
		{ FSeaIsland I; I.Id = Id; I.Name = Name; I.X = X; I.Z = Z; I.R = R; I.bPort = bPort; I.Dock = FVector2D(Dx, Dz); L.Add(I); };
		Add(TEXT("gullrest"), TEXT("Gullrest"), -70.f, 960.f, 26.f, true, -70.f, 932.f);
		Add(TEXT("brineholt"), TEXT("Brineholt"), 80.f, 1030.f, 24.f, true, 54.f, 1030.f);
		Add(TEXT("reef"), TEXT("Wreckers' Reef"), 10.f, 1120.f, 22.f, false, 10.f, 1096.f);
	}
	return L;
}

const USailingFeature::FHullDef& USailingFeature::HullDef(int32 N)
{
	static const FHullDef H[3] = {
		{ TEXT("Sloop"), 120, 9.f, 2, 2000, 0, 0, nullptr, 0, nullptr, 0 },
		{ TEXT("Brigantine"), 200, 8.2f, 3, 6000, 30, 25, TEXT("Steel bar"), 10, TEXT("Oak logs"), 20 },
		{ TEXT("Galleon"), 300, 7.6f, 4, 15000, 50, 40, TEXT("Steel bar"), 25, TEXT("Willow logs"), 40 } };
	return H[FMath::Clamp(N, 1, 3) - 1];
}

void USailingFeature::SaveState(FJsonObject& Out) const
{
	Out.SetNumberField(TEXT("hull"), Hull); Out.SetNumberField(TEXT("hp"), HullHp); Out.SetNumberField(TEXT("crew"), Crew);
	Out.SetNumberField(TEXT("sunk"), Sunk); Out.SetNumberField(TEXT("sank"), Sank); Out.SetNumberField(TEXT("drowned"), Drowned); Out.SetNumberField(TEXT("dug"), Dug);
	TArray<FString> O; for (FName N : Officers) O.Add(N.ToString()); PutStrArr(Out, TEXT("officers"), O);
	TArray<TSharedPtr<FJsonValue>> C;
	for (const FTreasureChart& Ch : Charts)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("x"), Ch.X); J->SetNumberField(TEXT("z"), Ch.Z); J->SetStringField(TEXT("isl"), Ch.Island.ToString()); J->SetNumberField(TEXT("tier"), Ch.Tier);
		C.Add(MakeShared<FJsonValueObject>(J));
	}
	Out.SetArrayField(TEXT("charts"), C);
	TSharedPtr<FJsonObject> Cards = MakeShared<FJsonObject>(); Cards->SetNumberField(TEXT("won"), CardsWon); Cards->SetNumberField(TEXT("lost"), CardsLost);
	Out.SetObjectField(TEXT("cards"), Cards);
}

void USailingFeature::LoadState(const FJsonObject& In)
{
	ResetState();
	Hull = FMath::Clamp((int32)JInt(In, TEXT("hull")), 0, 3);
	HullHp = Hull ? FMath::Clamp((int32)JInt(In, TEXT("hp"), HullDef(Hull).Hp), 1, HullDef(Hull).Hp) : 0;
	Crew = FMath::Clamp((int32)JInt(In, TEXT("crew")), 0, CrewMax);
	Sunk = FMath::Max(0, (int32)JInt(In, TEXT("sunk"))); Sank = FMath::Max(0, (int32)JInt(In, TEXT("sank")));
	Drowned = FMath::Max(0, (int32)JInt(In, TEXT("drowned"))); Dug = FMath::Max(0, (int32)JInt(In, TEXT("dug")));
	for (const FString& S : JStrArr(In, TEXT("officers"))) if (TraitOf(FName(*S)) && Officers.Num() < 4) Officers.AddUnique(FName(*S));
	const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
	if (In.TryGetArrayField(TEXT("charts"), C))
		for (const TSharedPtr<FJsonValue>& V : *C)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O || Charts.Num() >= 5) continue;
			FTreasureChart Ch; Ch.X = JNum(*O, TEXT("x")); Ch.Z = JNum(*O, TEXT("z")); Ch.Island = FName(*JStr(*O, TEXT("isl"))); Ch.Tier = FMath::Clamp((int32)JInt(*O, TEXT("tier"), 1), 1, 3);
			Charts.Add(Ch);
		}
	if (TSharedPtr<FJsonObject> Cards = JObj(In, TEXT("cards"))) { CardsWon = FMath::Max(0, (int32)JInt(*Cards, TEXT("won"))); CardsLost = FMath::Max(0, (int32)JInt(*Cards, TEXT("lost"))); }
}

void USailingFeature::ResetState()
{
	Hull = HullHp = Crew = Sunk = Sank = Drowned = Dug = CardsWon = CardsLost = 0;
	Officers.Reset(); Charts.Reset(); Enemies.Reset(); Balls.Reset();
	bAtSea = false; Moored = NAME_None; bHasTarget = false; Throttle = TurnInput = 0.f;
}

bool USailingFeature::Officer(const TCHAR* Trait) const
{
	const UCompanionsFeature* C = Feature<UCompanionsFeature>();
	if (!C) return false;
	for (FName Id : Officers) if (const TCHAR* Tr = TraitOf(Id)) if (FCString::Strcmp(Tr, Trait) == 0 && C->GetActive() == Id) return true;
	return false;
}

const FSeaIsland* USailingFeature::IslandAt(float X, float Z, float Pad) const
{
	for (const FSeaIsland& I : Islands()) if (FMath::Sqrt(FMath::Square(X - I.X) + FMath::Square(Z - I.Z)) < I.R + Pad) return &I;
	return nullptr;
}

// ---------------- Shipwright ----------------

FString USailingFeature::ShipwrightLine() const
{
	if (!Hull) return TEXT("Halvard, shipwright. Want to see the Sound? You'll need a hull under you. A Sloop's 2,000 coins: two guns a side, quick on the wind.");
	const FHullDef& H = HullDef(Hull);
	FString Off; for (FName O : Officers) Off += (Off.IsEmpty() ? TEXT("") : TEXT(", ")) + O.ToString();
	return FString::Printf(TEXT("Your %s: hull %d/%d, %d/%d deckhands%s."), H.Name, HullHp, H.Hp, Crew, CrewMax, Off.IsEmpty() ? TEXT("") : *(TEXT(", officers: ") + Off));
}

FString USailingFeature::BuildOrUpgrade()
{
	if (!Hull)
	{
		if (!TakeCoins(2000)) return TEXT("Come back with 2,000 coins.");
		Hull = 1; HullHp = HullDef(1).Hp; return TEXT("She's yours. Talk to me when you want to cast off.");
	}
	if (Hull >= 3) return TEXT("She's the finest hull I can build.");
	const FHullDef& N = HullDef(Hull + 1);
	if (SkillLevel(TEXT("Smithing")) < N.SmithReq || SkillLevel(TEXT("Crafting")) < N.CraftReq)
		return FString::Printf(TEXT("You'll need Smithing %d, Crafting %d to help me build a %s."), N.SmithReq, N.CraftReq, N.Name);
	if (CountItem(TEXT("Coins")) < N.Cost) return FString::Printf(TEXT("That's %d coins."), N.Cost);
	if (CountItem(N.Bar) < N.Bars || CountItem(N.Logs) < N.LogN) return FString::Printf(TEXT("Bring me %d %s, %d %s."), N.Bars, N.Bar, N.LogN, N.Logs);
	TakeCoins(N.Cost); TakeItem(N.Bar, N.Bars); TakeItem(N.Logs, N.LogN);
	++Hull; HullHp = N.Hp;
	GiveXp(TEXT("Smithing"), Hull * 300.f); GiveXp(TEXT("Crafting"), Hull * 300.f);
	return FString::Printf(TEXT("There she is: a %s. %d guns a side and a tougher hull."), N.Name, N.Guns);
}

FString USailingFeature::PatchHull()
{
	if (!Hull) return TEXT("You've no ship to patch.");
	const int32 Missing = HullDef(Hull).Hp - HullHp;
	if (Missing <= 0) return TEXT("She's sound already.");
	const int32 Cost = Missing * 4;
	if (!TakeCoins(Cost)) return TEXT("Not enough coin for the timber.");
	HullHp = HullDef(Hull).Hp; return TEXT("Good as new.");
}

FString USailingFeature::HireDeckhand()
{
	if (Crew >= CrewMax) return TEXT("Your decks are full.");
	if (!TakeCoins(CrewCost)) return TEXT("Deckhands want paying up front.");
	++Crew; return FString::Printf(TEXT("A strong back for the guns. %d aboard; each one reloads the broadsides faster."), Crew);
}

FString USailingFeature::ToggleOfficer(FName Id)
{
	const UCompanionsFeature* C = Feature<UCompanionsFeature>();
	if (!TraitOf(Id) || !C || !C->GetState(Id).bRecruited) return TEXT("Only companions sworn to you can serve as officers.");
	if (Officers.Contains(Id)) Officers.Remove(Id); else Officers.Add(Id);
	return TEXT("Done.");
}

// ---------------- Sailing ----------------

void USailingFeature::PlacePawn(float X, float Z, float Y, float Yaw) const
{
	APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!P) return;
	if (ACharacter* Ch = Cast<ACharacter>(P))
		if (UCharacterMovementComponent* M = Ch->GetCharacterMovement())
		{
			const EMovementMode Want = bAtSea ? MOVE_Flying : MOVE_Walking; // no sea floor collision under the deck
			if (M->MovementMode != Want) M->SetMovementMode(Want);
			if (bAtSea) M->Velocity = FVector::ZeroVector;
		}
	P->SetActorLocation(WebToWorld(X, Z, Y), false, nullptr, ETeleportType::TeleportPhysics);
	// Web yaw 0 faces +z (UE +Y); UE yaw 0 faces +X.
	if (bAtSea) P->SetActorRotation(FRotator(0.f, 90.f - FMath::RadiansToDegrees(Yaw), 0.f));
}

bool USailingFeature::SetSail()
{
	if (!Hull) { Say(TEXT("You have no ship. The Shipwright on the Port Sarim dock can build you one.")); return false; }
	if (HullHp <= 0) HullHp = FMath::CeilToInt(HullDef(Hull).Hp * 0.5f);
	Ship = FSeaBoat(); Ship.Name = HullDef(Hull).Name; Ship.X = SeaEntry.X; Ship.Z = SeaEntry.Y; Ship.Kind = Hull; Ship.Guns = HullDef(Hull).Guns;
	Ship.Hp = HullHp; Ship.MaxHp = HullDef(Hull).Hp;
	bAtSea = true; Moored = NAME_None; Throttle = 0.f; bHasTarget = false; NextSpawn = T + 6.f; Enemies.Reset(); Balls.Reset();
	PlacePawn(Ship.X, Ship.Z, 1.f, Ship.Yaw);
	Say(FString::Printf(TEXT("You cast off in your %s into the Sarim Sound. Steer with the stick or tap the sea; fire broadsides port and starboard."), HullDef(Hull).Name));
	return true;
}

void USailingFeature::GoAshore(const FSeaIsland& I)
{
	bAtSea = false; Moored = I.Id; Throttle = 0.f; bHasTarget = false;
	const FVector2D A(I.Dock.X - I.X, I.Dock.Y - I.Z); const FVector2D D = A.GetSafeNormal();
	Ship.X = I.X + D.X * (I.R + 6.f); Ship.Z = I.Z + D.Y * (I.R + 6.f); Ship.Yaw = FMath::Atan2(D.X, D.Y) + PI / 2.f; Ship.Speed = 0.f;
	PlacePawn(I.X + D.X * I.R * 0.6f, I.Z + D.Y * I.R * 0.6f, 1.3f, 0.f);
	Say(FString::Printf(TEXT("You tie up at %s. Use your ship to set sail again."), *I.Name));
}

bool USailingFeature::Reboard()
{
	if (Moored.IsNone()) return false;
	bAtSea = true; Moored = NAME_None; Throttle = 0.f;
	Say(TEXT("You climb aboard and take the helm."));
	return true;
}

void USailingFeature::HomeToSarim(const FString& Msg)
{
	bAtSea = false; Moored = NAME_None; Throttle = 0.f; bHasTarget = false; Enemies.Reset(); Balls.Reset();
	PlacePawn(SarimDock.X, SarimDock.Y - 3.f, 1.f, 0.f);
	if (!Msg.IsEmpty()) Say(Msg);
}

bool USailingFeature::SteerTo(FVector2D P) { if (!bAtSea) return false; bHasTarget = true; Target = P; Throttle = 1.f; return true; }

void USailingFeature::Helm(float Turn, float InThrottle)
{
	TurnInput = FMath::Clamp(Turn, -1.f, 1.f);
	if (FMath::Abs(TurnInput) > 0.1f) bHasTarget = false;
	if (InThrottle >= 0.f) { Throttle = FMath::Clamp(InThrottle, 0.f, 1.f); if (Throttle == 0.f) bHasTarget = false; }
}

FCannonball* USailingFeature::NewBall()
{
	const int32 Cap = UGielinorScalability::GetSavedTier() <= EGielinorTier::Low ? 10 : 20; // Low keeps a small pool
	if (Balls.Num() >= Cap) return nullptr;
	return &Balls.AddDefaulted_GetRef();
}

void USailingFeature::Fire(bool bPort)
{
	if (!bAtSea) return;
	float& R = bPort ? ReloadPort : ReloadStar;
	if (R > 0.f) { Say(FString::Printf(TEXT("The %s guns are still reloading."), bPort ? TEXT("port") : TEXT("starboard"))); return; }
	R = ReloadS * FMath::Max(0.45f, 1.f - 0.08f * Crew);
	const float Rng = Range * (Officer(TEXT("lookout")) ? 1.25f : 1.f), K = Officer(TEXT("gunnery")) ? 1.3f : 1.f;
	const float Sg = bPort ? 1.f : -1.f, Sx = FMath::Cos(Ship.Yaw) * Sg, Sz = -FMath::Sin(Ship.Yaw) * Sg;
	const float Fx = FMath::Sin(Ship.Yaw), Fz = FMath::Cos(Ship.Yaw);
	for (int32 i = 0; i < HullDef(Hull).Guns; ++i)
	{
		FCannonball* B = NewBall(); if (!B) break;
		const float Off = 1.6f - i * 1.4f;
		B->Pos = FVector(Ship.X + Fx * Off + Sx * 2.f, 1.2f, Ship.Z + Fz * Off + Sz * 2.f);
		B->Vel = FVector(Sx * BallSpeed + FMath::FRandRange(-1.f, 1.f), 3.f, Sz * BallSpeed + FMath::FRandRange(-1.f, 1.f));
		B->Left = Rng / BallSpeed; B->bMine = true; B->Dmg = FMath::RoundToInt((10.f + FMath::FRand() * 10.f) * K);
	}
	Say(bPort ? TEXT("Fire port!") : TEXT("Fire starboard!"));
}

void USailingFeature::SpawnEnemy()
{
	const int32 K = FMath::FRand() < 0.65f ? 0 : 1;
	FSeaBoat E; E.Name = Kinds[K].Name; E.Kind = K; E.Hp = E.MaxHp = Kinds[K].Hp; E.Guns = Kinds[K].Guns; E.Yaw = FMath::FRand() * 2.f * PI; E.Reload = 2.f + FMath::FRand() * 2.f;
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = FMath::FRand() * 2.f * PI, Rr = 40.f + FMath::FRand() * 30.f;
		E.X = Ship.X + FMath::Sin(A) * Rr; E.Z = Ship.Z + FMath::Cos(A) * Rr;
		if (FVector2D::Distance(FVector2D(E.X, E.Z), SeaCentre) < SeaR - 10.f && !IslandAt(E.X, E.Z, 6.f)) break;
	}
	Enemies.Add(E);
	Say(FString::Printf(TEXT("A %s of the Drowned Crew rises from the deep!"), *E.Name));
}

void USailingFeature::SinkEnemy(int32 Index)
{
	const FEnemyKind& K = Kinds[Enemies[Index].Kind];
	Enemies.RemoveAt(Index);
	++Sank;
	const int32 G = FMath::RandRange(K.LootLo, K.LootHi - 1);
	GiveCoins(G); GiveXp(TEXT("Ranged"), 60.f + K.Hp * 0.6f); GiveXp(TEXT("Smithing"), 15.f);
	if (UStandingFeature* St = Feature<UStandingFeature>()) St->AddRep(TEXT("drowned"), 40);
	Say(FString::Printf(TEXT("The %s sinks! You salvage %d coins."), K.Name, G));
	if (FMath::FRand() < 0.25f) { GiveItem(TEXT("Treasure chart"), 1); Say(TEXT("A sodden Treasure chart floats up from the wreck.")); }
}

void USailingFeature::HitShip(int32 Dmg)
{
	HullHp = FMath::Max(0, HullHp - Dmg); Ship.Hp = HullHp;
	if (HullHp <= 0) { ++Sunk; HomeToSarim(TEXT("Your ship goes down! You wash ashore on the Port Sarim docks. The Shipwright can patch her up.")); }
}

bool USailingFeature::MoveBoat(FSeaBoat& B, float Dt, float Speed, float TurnTo) const
{
	const float Dy = FMath::Atan2(FMath::Sin(TurnTo - B.Yaw), FMath::Cos(TurnTo - B.Yaw));
	B.Yaw += FMath::Clamp(Dy, -1.f, 1.f) * Dt * 0.9f;
	const float Nx = B.X + FMath::Sin(B.Yaw) * Speed * Dt, Nz = B.Z + FMath::Cos(B.Yaw) * Speed * Dt;
	if (!IslandAt(Nx, Nz, 4.f) && FVector2D::Distance(FVector2D(Nx, Nz), SeaCentre) <= SeaR) { B.X = Nx; B.Z = Nz; return true; }
	return false;
}

void USailingFeature::Tick(float Dt)
{
	Dt = FMath::Min(Dt, 0.1f);
	T += Dt;
	if (!bAtSea) return;
	const FHullDef& H = HullDef(Hull);
	float TurnTo = Ship.Yaw;
	if (FMath::Abs(TurnInput) > 0.1f) { TurnTo = Ship.Yaw - TurnInput; Throttle = FMath::Max(Throttle, 0.5f); } // web: A turns +1, D turns -1; stick right (+1) = D
	if (bHasTarget)
	{
		const float Dx = Target.X - Ship.X, Dz = Target.Y - Ship.Z;
		if (FMath::Sqrt(Dx * Dx + Dz * Dz) < 5.f) { bHasTarget = false; Throttle = 0.f; } else TurnTo = FMath::Atan2(Dx, Dz);
	}
	const float Top = H.Speed * (Officer(TEXT("helm")) ? 1.12f : 1.f) * Throttle;
	Ship.Speed += (Top - Ship.Speed) * FMath::Min(1.f, Dt * 0.8f);
	if (!MoveBoat(Ship, Dt, Ship.Speed, TurnTo))
	{
		Ship.Speed *= 0.3f; bHasTarget = false; Throttle = 0.f;
		if (const FSeaIsland* I = IslandAt(Ship.X + FMath::Sin(Ship.Yaw) * 8.f, Ship.Z + FMath::Cos(Ship.Yaw) * 8.f, 6.f))
			if (I->bPort || I->Id == FName(TEXT("reef"))) { GoAshore(*I); return; }
	}
	PlacePawn(Ship.X - FMath::Sin(Ship.Yaw) * 1.6f, Ship.Z - FMath::Cos(Ship.Yaw) * 1.6f, 1.85f, Ship.Yaw);
	if (Ship.Z < SeaEntry.Y - 25.f) { HomeToSarim(TEXT("You sail back into Port Sarim harbour.")); return; }
	ReloadPort = FMath::Max(0.f, ReloadPort - Dt); ReloadStar = FMath::Max(0.f, ReloadStar - Dt);
	if (Officer(TEXT("surgeon"))) { RepairT += Dt; if (RepairT >= 2.f) { RepairT = 0.f; if (HullHp < H.Hp) Ship.Hp = ++HullHp; } }
	// Drowned Crew: close in, turn broadside, fire.
	if (T > NextSpawn && Enemies.Num() < EnemyMax && Ship.Z > SeaEntry.Y + 30.f) { SpawnEnemy(); NextSpawn = T + 25.f + FMath::FRand() * 20.f; }
	for (int32 i = Enemies.Num() - 1; i >= 0; --i)
	{
		FSeaBoat& E = Enemies[i];
		const float Dx = Ship.X - E.X, Dz = Ship.Z - E.Z, D = FMath::Max(0.01f, FMath::Sqrt(Dx * Dx + Dz * Dz)), ToYou = FMath::Atan2(Dx, Dz);
		const FEnemyKind& K = Kinds[E.Kind];
		MoveBoat(E, Dt, D > 18.f ? K.Speed : K.Speed * 0.5f, D > 18.f ? ToYou : ToYou + PI / 2.f);
		E.Reload -= Dt;
		if (D < Range && E.Reload <= 0.f)
		{
			E.Reload = 4.f + FMath::FRand() * 2.f;
			for (int32 g = 0; g < K.Guns; ++g)
			{
				FCannonball* B = NewBall(); if (!B) break;
				const float Sp = BallSpeed * 0.85f, Sc = FMath::FRandRange(-0.09f, 0.09f);
				B->Pos = FVector(E.X, 1.2f, E.Z); B->Vel = FVector((Dx / D + Sc) * Sp, 3.f, (Dz / D - Sc) * Sp);
				B->Left = Range / Sp; B->bMine = false; B->Dmg = FMath::RandRange(K.DmgLo, K.DmgHi);
			}
		}
		if (D > 140.f) Enemies.RemoveAt(i);
	}
	for (int32 b = Balls.Num() - 1; b >= 0; --b)
	{
		FCannonball& B = Balls[b];
		B.Pos += B.Vel * Dt; B.Vel.Y -= 6.f * Dt; B.Left -= Dt;
		bool bHit = false;
		if (B.bMine)
		{
			for (int32 e = 0; e < Enemies.Num(); ++e)
				if (FVector2D::Distance(FVector2D(B.Pos.X, B.Pos.Z), FVector2D(Enemies[e].X, Enemies[e].Z)) < 3.2f)
				{ bHit = true; Enemies[e].Hp -= B.Dmg; if (Enemies[e].Hp <= 0) SinkEnemy(e); break; }
		}
		else if (FVector2D::Distance(FVector2D(B.Pos.X, B.Pos.Z), FVector2D(Ship.X, Ship.Z)) < 3.2f * (1.f + 0.15f * (Hull - 1)))
		{
			const int32 Dmg = B.Dmg; Balls.RemoveAt(b); HitShip(Dmg);
			if (!bAtSea) return;
			continue;
		}
		if (bHit || B.Left <= 0.f || B.Pos.Y < 0.f) Balls.RemoveAt(b);
	}
}

// ---------------- Treasure charts ----------------

bool USailingFeature::ReadChart()
{
	if (Charts.Num() >= 5) { Say(TEXT("You already carry five marked spots. Dig some up first.")); return false; }
	if (!TakeItem(TEXT("Treasure chart"), 1)) return false;
	const int32 Ii = FMath::RandRange(0, 2); const FSeaIsland& I = Islands()[Ii]; const int32 Sp = FMath::RandRange(0, 2);
	FTreasureChart C; C.Island = I.Id; C.X = I.X + ChartSpots[Ii][Sp][0] * I.R; C.Z = I.Z + ChartSpots[Ii][Sp][1] * I.R;
	C.Tier = I.Id == FName(TEXT("reef")) ? 3 : FMath::FRand() < 0.3f ? 2 : 1;
	Charts.Add(C);
	Say(FString::Printf(TEXT("The chart shows an X on %s. It's marked on your map."), *I.Name));
	return true;
}

bool USailingFeature::DigHere()
{
	bool bSpade = CountItem(TEXT("Spade")) > 0; // pack, or the toolbelt (web hasTool)
	if (const AGielinorPlayerState* PS = GetPS())
		if (PS->Stats) for (const UItemDef* T : PS->Stats->Toolbelt) bSpade |= T && T->ToolType == EToolType::Spade;
	if (!bSpade) { Say(TEXT("You need a Spade. The Gullrest Chandlery sells them.")); return false; }
	FVector2D P; if (!PlayerWebPos(P)) return false;
	const int32 Idx = Charts.IndexOfByPredicate([&P](const FTreasureChart& C) { return FVector2D::Distance(P, FVector2D(C.X, C.Z)) < 3.f; });
	if (Idx == INDEX_NONE) { Say(Charts.Num() ? TEXT("Nothing here. The X must be somewhere else.") : TEXT("You dig a hole, but find nothing. Treasure charts show where to dig.")); return false; }
	const int32 Tier = Charts[Idx].Tier; Charts.RemoveAt(Idx); ++Dug;
	static const int32 Base[4] = { 0, 400, 1100, 2600 };
	const int32 G = Base[Tier] + FMath::RandRange(0, 400 * Tier - 1);
	GiveCoins(G);
	static const TCHAR* Gems[4] = { TEXT("Uncut sapphire"), TEXT("Uncut emerald"), TEXT("Uncut ruby"), TEXT("Uncut diamond") };
	const TCHAR* Gem = Gems[FMath::Min(3, FMath::RandRange(0, Tier))];
	GiveItem(Gem, 1);
	if (Tier >= 2 && FMath::FRand() < 0.5f) GiveItem(TEXT("Silver shot"), 15);
	if (Tier == 3 && FMath::FRand() < 0.08f) GiveItem(TEXT("Saltgrave's tricorn"), 1);
	GiveXp(TEXT("Crafting"), 40.f * Tier);
	Say(FString::Printf(TEXT("You dig up a buried chest: %d coins and an %s!"), G, *FString(Gem).ToLower()));
	return true;
}
