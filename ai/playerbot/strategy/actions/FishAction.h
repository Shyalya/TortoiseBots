#pragma once
#include "playerbot/PlayerbotAI.h"
#include "CastCustomSpellAction.h"
#include "UseItemAction.h"


namespace ai
{
    // A fishing spot with a hostile creature spawn near the bot's level within `radius` (a
    // murloc camp on the shore, scouts by the river) is no place to stand still and fish:
    // 18 % of all deaths happened while fishing. Static spawn data, no grid needed.
    bool IsFishingSpotGuarded(Player* bot, WorldPosition const& spot, float radius = 35.0f);
    // TravelMgr::GetFishSpot, but up to eight candidates are tried until one is not guarded;
    // the last candidate is returned when every one of them is.
    WorldPosition* GetSafeFishSpot(Player* bot, bool onlyNearestGrid = false);

    class MoveToFishAction : public MovementAction, public Qualified
    {
    public:
        MoveToFishAction(PlayerbotAI* ai) : MovementAction(ai, "move to fish"), Qualified() {}
        virtual bool isUseful() override;
        virtual bool Execute(Event& event) override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "move to fish"; }
        virtual std::string GetHelpDescription()
        {
            return "This action makes the bot move to a fishing location.\n"
                   "It identifies suitable spots for fishing and navigates there.";
        }
        virtual std::vector<std::string> GetUsedActions() { return {}; }
        virtual std::vector<std::string> GetUsedValues() { return {}; }
#endif
    };

    class FishAction : public CastCustomSpellAction
    {
    public:
        FishAction(PlayerbotAI* ai) : CastCustomSpellAction(ai, "fish") {}
        virtual bool isUseful() override;
        virtual bool Execute(Event& event) override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "fish"; }
        virtual std::string GetHelpDescription()
        {
            return "This action makes the bot perform fishing.\n"
                   "It casts the fishing spell to catch fish at the current location.";
        }
        virtual std::vector<std::string> GetUsedActions() { return {}; }
        virtual std::vector<std::string> GetUsedValues() { return {}; }
#endif
    };

    class UseFishingBobberAction : public UseAction
    {
    public:
        UseFishingBobberAction(PlayerbotAI* ai) : UseAction(ai, "use fishing bobber") {}

        bool Execute(Event& event) override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "use fishing bobber"; }
        virtual std::string GetHelpDescription()
        {
            return "This action makes the bot use a fishing bobber.\n"
                   "It interacts with the bobber to complete the fishing process.";
        }
        virtual std::vector<std::string> GetUsedActions() { return {}; }
        virtual std::vector<std::string> GetUsedValues() { return {}; }
#endif
    };
}
