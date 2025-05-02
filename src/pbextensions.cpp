#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Playerbots.h"
#include <map>
#include <string>
#include "PlayerbotMgr.h"
#include "Language.h"

static bool IsPlayerBot(Player* player)
{
    if (!player)
    {
        return false;
    }
    PlayerbotAI* botAI = sPlayerbotsMgr->GetPlayerbotAI(player);
    return botAI && botAI->IsBotAI();
}

// Capture player SAY message and make bot equip item in provided slot ie: /say [slot] [itemlink]-> /say mainhand [itemLink]
// To make it work player have to target bot 1st and bot must be in party/raid
// [slot] -> head, neck, shoulder, chest, waist, legs, feet, wrist, hands, finger1, finger2, trinket1, trinket2, mainhand, offhand, ranged
// Usefull for rings, trinkets and weapons (rest PB handles well)
class PbExtensionsScripts : public PlayerScript
{
public:
    PbExtensionsScripts() : PlayerScript("PbExtensions", { PLAYERHOOK_ON_CHAT }) {}

    void Execute(const std::string& message, Player* player);
    static uint32 parseSlotFromText(const std::string& text);
    static ItemIds parseItems(const std::string& text);
    void OnPlayerChat(Player* player, uint32 type, uint32/* lang*/, std::string& msg) override
    {
        if (type == ChatMsg::CHAT_MSG_SAY)
        {
            if (player->GetGUID() == player->GetSession()->GetPlayer()->GetGUID())             // Capture only our Say
            {
                Execute(msg, player);
            }
        }
    }

private:
    static const std::map<std::string, uint32> slots;
};

// Define slot mappings for equipment
const std::map<std::string, uint32> PbExtensionsScripts::slots = {
    {"head", EQUIPMENT_SLOT_HEAD},
    {"neck", EQUIPMENT_SLOT_NECK},
    {"shoulder", EQUIPMENT_SLOT_SHOULDERS},
    {"chest", EQUIPMENT_SLOT_CHEST},
    {"waist", EQUIPMENT_SLOT_WAIST},
    {"legs", EQUIPMENT_SLOT_LEGS},
    {"feet", EQUIPMENT_SLOT_FEET},
    {"wrist", EQUIPMENT_SLOT_WRISTS},
    {"hands", EQUIPMENT_SLOT_HANDS},
    {"finger1", EQUIPMENT_SLOT_FINGER1},
    {"finger2", EQUIPMENT_SLOT_FINGER2},
    {"trinket1", EQUIPMENT_SLOT_TRINKET1},
    {"trinket2", EQUIPMENT_SLOT_TRINKET2},
    {"mainhand", EQUIPMENT_SLOT_MAINHAND},
    {"offhand", EQUIPMENT_SLOT_OFFHAND},
    {"ranged", EQUIPMENT_SLOT_RANGED}
};

// Corrected Execute() Definition
void PbExtensionsScripts::Execute(const std::string& msg, Player* player)
{
    Player* target = ObjectAccessor::FindPlayer(player->GetTarget());                       //Get player target
    PlayerbotAI* botAI = sPlayerbotsMgr->GetPlayerbotAI(target);                            //Check if taget is a bot

    if (botAI)                                                                              //And if is
    {
        Player* targetPlayer = target->ToPlayer();
        if (!player->GetGroup())
        {
            //player->Say("Bot target is not in the group/raid.", LANG_UNIVERSAL);
            return;
        }
    }
    else                                                                                    //It's not a bot
    {
        //player->Say("Target is not a bot.", LANG_UNIVERSAL);
        return;
    }

    uint32 slotId = parseSlotFromText(msg);                                                 // Parse slot from message
    ItemIds ids = parseItems(msg);                                                          // Parse item from message

    if (slotId != EQUIPMENT_SLOT_END && !ids.empty())
    {
        EquipmentSlots slot = static_cast<EquipmentSlots>(slotId);
        auto ItemId = *ids.begin();
        Item* item = botAI->GetBot()->GetItemByEntry(ItemId);

        if (!item)
        {
            //player->Say("Invalid item.", LANG_UNIVERSAL);
            return;
        }

        // Send equip item to slot packet
        WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
        ObjectGuid itemguid = item->GetGUID();
        packet << itemguid << slot;
        botAI->GetBot()->GetSession()->HandleAutoEquipItemSlotOpcode(packet);
        // Let bot notify you
        const ItemTemplate* itemProto = item->GetTemplate();
        std::ostringstream out;
        out << "Equipping " << itemProto->Name1;
        botAI->TellMaster(out.str());

        return;
    }

    return;  // Return false if equipping failed
}

// Function to parse item IDs from chat message
ItemIds PbExtensionsScripts::parseItems(const std::string& text)
{
    ItemIds itemIds;
    size_t pos = 0;

    while (true)
    {
        auto i = text.find("Hitem:", pos);
        if (i == std::string::npos)
            break;

        pos = i + 6;
        auto endPos = text.find(':', pos);
        if (endPos == std::string::npos)
            break;

        std::string idC = text.substr(pos, endPos - pos);
        auto id = atol(idC.c_str());
        pos = endPos;
        if (id)
            itemIds.insert(id);
    }

    return itemIds;
}

// Function to parse slot from chat command
uint32 PbExtensionsScripts::parseSlotFromText(const std::string& text)
{
    for (auto const& pair : PbExtensionsScripts::slots)
    {
        if (text.find(pair.first) != std::string::npos)                                 // Check if slot name exists in text
        {
            return pair.second;                                                         // Return matched slot value
        }
    }
    return EQUIPMENT_SLOT_END;                                                          // Default if no match is found
}

void SendInspectRequest(Player* player, Player* target)
{
    if (!player || !target)
        return;

    WorldPacket packet(CMSG_INSPECT, 8);
    packet << target->GetGUID();
    player->GetSession()->SendPacket(&packet);
}

// Module registration
void AddPbExtensionsScripts()
{
    new PbExtensionsScripts();
}
