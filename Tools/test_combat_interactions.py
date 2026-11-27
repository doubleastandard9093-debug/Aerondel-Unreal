#!/usr/bin/env python3
"""Offline checks that the combat / interaction audit fixes stay wired up."""
import sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
S = lambda p: (root / "Source/Aerondel" / p).read_text(encoding="utf-8", errors="replace")
errs = []

def body(src, sig):
    i = src.find(sig)
    if i < 0: return None
    j = src.find("{", i)
    if j < 0: return None
    d = 0
    for k in range(j, len(src)):
        d += src[k] == "{"
        d -= src[k] == "}"
        if d == 0: return src[j:k+1]
    return None

# 1. Player death goes through HandleDeath (controller must not revive first).
ctl = S("Player/AerondelPlayerController.cpp")
od = body(ctl, "AAerondelPlayerController::OnPlayerDied(")
if not od:
    errs.append("OnPlayerDied not found")
else:
    if "SetMax(" in od or "TeleportTo(" in od:
        errs.append("OnPlayerDied must not refill HP or teleport; that skipped HandleDeath")
    if "HandleDeath()" not in od:
        errs.append("OnPlayerDied should call HandleDeath when the player is still dead")

hd = body(S("Core/PlayerStatsComponent.cpp"), "UPlayerStatsComponent::HandleDeath")
if not hd:
    errs.append("HandleDeath not found")
else:
    if "NotifyPlayerDied" not in hd:
        errs.append("HandleDeath must NotifyPlayerDied so feature hooks still fire")
    if "ClearTarget" not in hd:
        errs.append("HandleDeath should ClearTarget")
    if "IsDead()" not in hd:
        errs.append("HandleDeath must no-op if already revived")

# 2. Shared inventory use, bound from canvas HUD, widget, and pad panel.
ps = S("Core/PlayerStatsComponent.cpp")
if "UPlayerStatsComponent::UseInventorySlot" not in ps:
    errs.append("UseInventorySlot missing")
else:
    u = body(ps, "UPlayerStatsComponent::UseInventorySlot")
    for s in ("Consume(", "BuryBones(", "CleanHerb(", "AddTool(", "EquipFromInventory", "LightFrom(", "DelayNextAttack"):
        if s not in u:
            errs.append(f"UseInventorySlot should call {s}")

hud = S("UI/AerondelHUD.cpp")
if "UseInventorySlot" not in body(hud, "AAerondelHUD::HandleHudClick") or "InvClickSlots" not in hud:
    errs.append("Canvas HUD must click inventory slots via UseInventorySlot")
if "UAerondelInventoryWidget::UseSlot" in S("UI/AerondelWidgetBase.cpp"):
    if "UseInventorySlot" not in body(S("UI/AerondelWidgetBase.cpp"), "UAerondelInventoryWidget::UseSlot"):
        errs.append("Inventory widget UseSlot must go through UseInventorySlot")
pan = S("UI/AerondelPanels.cpp")
if "UseInventorySlot" not in pan:
    errs.append("pad_inventory should offer Use/Deposit actions via UseInventorySlot")
ch = S("Player/AerondelCharacter.cpp")
if "OnToggleInventory" not in ch or "ToggleInventory" not in ch:
    errs.append("IA_ToggleInventory (Tab) must be bound")

# 3. Nav fallback for player movement and mob chase.
if "MoveToWorld" not in S("Player/AerondelCharacter.h") or "HasNavMesh" not in ch:
    errs.append("AAerondelCharacter needs MoveToWorld / HasNavMesh")
if "MoveToWorld(Hit.ImpactPoint)" not in S("Player/AerondelPlayerController.cpp"):
    errs.append("Click-to-move must use MoveToWorld (nav or straight-line)")
if "MoveToWorld(Mob->GetActorLocation())" not in S("Combat/PlayerCombatComponent.cpp"):
    errs.append("Combat approach must use MoveToWorld")
mob = S("Combat/AerondelMob.cpp")
if "MobSteerOrPath" not in mob or "MobHasNav" not in mob:
    errs.append("Mobs need a no-navmesh steer fallback")

# 4. Mob respawn clears combat status (taunt would skip leashing).
rs = body(mob, "AAerondelMob::Respawn")
if not rs:
    errs.append("Respawn not found")
else:
    for s in ("WitherTicks = 0", "TauntTicks = 0", "VenomDamage = 0"):
        if s not in rs:
            errs.append(f"Respawn should reset {s}")

# 5. Bankers open the bank; chest label; loot message.
npc = S("Interaction/NpcTalk.cpp")
inter = body(npc, "ANpcTalk::Interact")
if not inter or "OpenFrom" not in inter or "banker" not in inter:
    errs.append("Talking to a banker NPC must OpenFrom the bank")
if "Use Bank chest" not in S("Interaction/BankChest.h"):
    errs.append("Bank chest label should be 'Use Bank chest'")
if "OpenFrom" not in S("Bank/BankComponent.h"):
    errs.append("BankComponent needs OpenFrom so the bank closes when you walk away")
gi = body(S("Interaction/GroundItem.cpp"), "AGroundItem::Interact")
if not gi or "You take" not in gi:
    errs.append("Taking a ground item should log 'You take'")

# 6. Hover prompt mentions both gamepad A and keyboard E.
if "A / E to use" not in hud:
    errs.append("Gamepad hover prompt should mention A / E")

# 7. Station recipe picker (mouse / pad panel / VR wrist).
st = S("Interaction/Station.cpp")
inter = body(st, "AStation::Interact(")
if not inter or 'TEXT("station")' not in inter:
    errs.append("AStation::Interact should open the station recipe panel")
if "CraftLeft" not in st or "CountAffordable" not in S("Interaction/Station.h"):
    errs.append("Stations need make-1 / make-all (CraftLeft / CountAffordable)")
if 'P == TEXT("station")' not in S("UI/AerondelPanels.cpp"):
    errs.append("FAerondelPanels must build a station recipe picker")
if "Make 1" not in S("UI/AerondelPanels.cpp") or "Make all" not in S("UI/AerondelPanels.cpp"):
    errs.append("Station panel should offer Make 1 and Make all")
op = body(S("UI/AerondelHUD.cpp"), "AAerondelHUD::OpenPanel(")
if not op or "WristPanel" not in op:
    errs.append("OpenPanel should show the C++ panel on the VR wrist widget")

# 8. Target HP bar + floating damage numbers (capped on Low).
if "AAerondelFloatText" not in S("Combat/AerondelFloatText.h"):
    errs.append("AAerondelFloatText missing")
mob = S("Combat/AerondelMob.cpp")  # re-read after earlier mob = 
if "AAerondelFloatText::ShowOn" not in mob:
    errs.append("Mob hits should spawn floating damage numbers")
if "{2}/{3}" not in S("Combat/AerondelMob.cpp") and "GetHp()" not in S("Combat/AerondelMob.cpp"):
    errs.append("Mob nameplate should show current HP")
if "GetTarget()" not in S("UI/AerondelHUD.cpp") or "BarW" not in S("UI/AerondelHUD.cpp"):
    errs.append("HUD should draw a target HP bar for the current combat target")
if "ShowOn(S->GetPawn(), Amount, false)" not in S("Core/PlayerStatsComponent.cpp"):
    errs.append("Incoming player damage should spawn a floating number")
if "bHeal" not in S("Combat/AerondelFloatText.h"):
    errs.append("Floating heals need a green path")

if errs:
    print("FAIL " + "\nFAIL ".join(errs))
    sys.exit(1)
print("OK: combat and interaction audit wiring is in place.")
