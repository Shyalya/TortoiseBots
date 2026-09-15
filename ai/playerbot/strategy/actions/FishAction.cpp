
#include "playerbot/playerbot.h"
#include "Timer.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/GuidPosition.h"
#include <ctime>
#include "FishAction.h"
#include "playerbot/TravelMgr.h"
#include "TellLosAction.h"
#include "EquipAction.h"

using namespace ai;

bool MoveToFishAction::isUseful()
{
    if (qualifier == "travel")
    {
        if (!AI_VALUE(bool, "travel target working"))
            return false;

        TravelTarget* target = AI_VALUE(TravelTarget*, "leader travel target");

        if (target->GetDestination()->GetPurpose() != TravelDestinationPurpose::GatherFishing)
            return false;
    }

    return true;
}

bool ai::IsFishingSpotGuarded(Player* bot, WorldPosition const& spot, float radius)
{
    if (!bot)
        return false;
    uint32 const botLevel = bot->GetLevel();
    for (CreatureDataPair const* pair : spot.getCreaturesNear(radius))
    {
        GuidPosition guard(pair);
        CreatureInfo const* info = guard.GetCreatureTemplate();
        if (!info || info->level_max + 2 < botLevel)
            continue; // grey to the bot: no danger
        if (!guard.IsHostileTo(bot))
            continue;
        return true;
    }
    return false;
}

WorldPosition* ai::GetSafeFishSpot(Player* bot, bool onlyNearestGrid)
{
    WorldPosition* spot = nullptr;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        spot = sTravelMgr.GetFishSpot(WorldPosition(bot), onlyNearestGrid);
        if (!spot || !IsFishingSpotGuarded(bot, *spot))
            break;
    }
    return spot;
}

bool MoveToFishAction::Execute(Event& event)
{
    WorldPosition fishSpot;

    fishSpot = AI_VALUE2(WorldPosition, "custom position", "fish spot");

    if (!fishSpot && qualifier == "travel") //Get travel fish spot if available.
    {
        TravelTarget* target = AI_VALUE(TravelTarget*, "leader travel target");
        fishSpot = *target->getPosition();

        if (AI_VALUE(TravelTarget*, "travel target") != target) //Do not fish ontop of master.
            fishSpot = *GetSafeFishSpot(bot, true);
    }

    if (!fishSpot) //Get any fish spot.
    {
        fishSpot = *GetSafeFishSpot(bot);

        TravelPath movePath = sTravelNodeMap.GetFullPath(bot, fishSpot, bot);


        if (movePath.empty())
            return false;

        AI_VALUE(LastMovement&, "last movement").setPath(movePath);
    }

    SET_AI_VALUE2(WorldPosition, "custom position", "fish spot", fishSpot);

    if (fishSpot.distance(bot) < 1.0f)
        return false;

    return MoveTo(fishSpot);
}

bool FishAction::isUseful()
{
    if (qualifier == "travel")
    {
        if (!AI_VALUE(bool, "travel target working"))
            return false;

        TravelTarget* target = AI_VALUE(TravelTarget*, "leader travel target");

        if (target->GetDestination()->GetPurpose() != TravelDestinationPurpose::GatherFishing)
            return false;

        if (!bot->GetGroup() || ai->IsGroupLeader() || target->GetTimeLeft() < 0)
            target->CheckStatus();
    }

    WorldPosition fishSpot = AI_VALUE2(WorldPosition, "custom position", "fish spot");

    if (!fishSpot)
        return false;

    if (!AI_VALUE(bool, "can fish"))
        return false;

    if (fishSpot.distance(bot) > 1.0f)
        return false;

    return true;
}

bool FishAction::Execute(Event& event)
{
    if (qualifier == "travel")
    {
        if (!AI_VALUE(bool, "travel target working"))
            return false;
    }

    if (bot->IsMoving())
    {
        ai->StopMoving();
        SetDuration(100);
        return true;
    }

    WorldPosition fishSpot = AI_VALUE2(WorldPosition, "custom position", "fish spot");

    if (abs(fishSpot.getO() - bot->getOrientation()) > 0.5)
    {
        bot->SetFacingTo(fishSpot.getO());
        SetDuration(100);
        return true;
    }

    ai->StopMoving();

    // A hostile creature near the bot's level within 30 yd right now - a patrol, a respawn -
    // ends the fishing here: the spot is dropped and a fishing travel target expires, so the
    // next pick is somewhere else instead of the bot standing still until it is dead.
    for (ObjectGuid const& guid : AI_VALUE(std::list<ObjectGuid>, "possible targets no los"))
    {
        Unit* unit = ai->GetUnit(guid);
        if (!unit || !unit->IsCreature() || unit->GetLevel() + 2 < bot->GetLevel() || !sServerFacade.IsHostileTo(bot, unit))
            continue;
        if (bot->GetDistance(unit) > 30.0f)
            continue;
        RESET_AI_VALUE2(WorldPosition, "custom position", "fish spot");
        if (qualifier == "travel")
            if (TravelTarget* travelTarget = AI_VALUE(TravelTarget*, "travel target"))
                travelTarget->SetStatus(TravelStatus::TRAVEL_STATUS_EXPIRED);
        ai->TellDebug(GetMaster(), "No fishing here - " + std::string(unit->GetName()) + " is too close", "debug move");
        if (sPlayerbotAIConfig.hasLog("unreachable_targets.csv"))
        {
            time_t const nowFish = time(nullptr);
            char stampFish[32];
            strftime(stampFish, sizeof(stampFish), "%Y-%m-%d %H:%M:%S", localtime(&nowFish));
            std::ostringstream outFish;
            outFish << stampFish << "," << bot->GetName() << "," << bot->GetLevel() << ",FISH " << unit->GetName() << "," << unit->GetLevel() << ",guarded";
            sPlayerbotAIConfig.log("unreachable_targets.csv", outFish.str().c_str());
        }
        return false;
    }

    std::list<Item*> poles = AI_VALUE2(std::list<Item*>, "inventory items", "fishing pole");

    if (poles.empty())
        return false;

    // A pole already in hand stays in hand. The list holds the equipped pole and every spare
    // one, and its first entry was a spare more often than not: equipping it swapped the two
    // poles, the next tick swapped them back - 140,000 swaps in half an hour for four bots
    // that carried a second pole. Without a pole in hand the best one is taken.
    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    bool const poleInHand = mainHand && mainHand->GetProto()->Class == ITEM_CLASS_WEAPON &&
        mainHand->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE;
    if (!poleInHand)
    {
        Item* pole = poles.front();
        for (Item* candidate : poles)
            if (candidate->GetProto()->ItemLevel > pole->GetProto()->ItemLevel)
                pole = candidate;
        EquipAction::EquipItem(ai, GetMaster(), pole);
    }

    Event fishCastEvent = Event("fish", "7731 " + chat->formatWorldobject(bot));
    bool didCast = CastCustomSpellAction::Execute(fishCastEvent);
    if (didCast)
        SET_AI_VALUE2(int, "manual int", "last fish cast", int(WorldTimer::getMSTime()));

    SetDuration(sPlayerbotAIConfig.globalCoolDown);

    return didCast;
}

bool UseFishingBobberAction::Execute(Event& event)
{
    std::list<GameObject*> objects = TellLosAction::GoGuidListToObjList(ai, AI_VALUE(std::list<ObjectGuid>, "nearest game objects no los"));

    for (auto& obj : objects)
    {
        if (obj->GetEntry() != 35591)
            continue;

        if (obj->GetOwnerGuid() != bot->getObjectGuid())
            continue;

        if (obj->getLootState() != GO_READY)
        {
            time_t bobberActiveTime = obj->GetRespawnTime() - FISHING_BOBBER_READY_TIME;
            if (bobberActiveTime > time(0))
                SetDuration((bobberActiveTime - time(0)) * IN_MILLISECONDS + 500);
            else
                SetDuration(1000);
            return true;
        }

        std::unique_ptr<WorldPacket> packet(new WorldPacket(CMSG_GAMEOBJ_USE));
        *packet << obj->getObjectGuid();
        bot->GetSession()->QueuePacket(packet.release());

        std::ostringstream out; out << "Opening " << chat->formatGameobject(obj);
        ai->TellPlayerNoFacing(ai->GetMaster(), out.str(), PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);

        SetDuration(3000);

        if (!urand(0,10))
        {
            RESET_AI_VALUE2(WorldPosition, "custom position", "fish spot");
        }

        return true;
    }

    return false;
}