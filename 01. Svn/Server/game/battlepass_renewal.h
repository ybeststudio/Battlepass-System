/*
 * File Name : battlepass_renewal.h
 * Author    : BestStudio
 * Version   : 1.0
 * Date      : 2026-11-14
 *
 * Battlepass (Anka uiBattlePass): monthly pass + event pass, free / premium reward track, weekly cumulative
 * missions, level exp (pass 20, event from the config). Table: locale/europe/battlepass.txt (Tools/battlepass/
 * convert_battlepass.py from the client battlepass_config.json). Client: root/uibattlepass.py, command "/bp".
 */
#pragma once

#ifdef ENABLE_BATTLEPASS_RENEWAL
class CHARACTER;
class CItem;

enum EBattlepassMission
{
	BP_KILL_MONSTER = 1,
	BP_DESTROY_METIN = 2,
	BP_KILL_BOSS = 3,
	BP_FINISH_DUNGEON = 4,
	BP_KILL_PLAYERS = 5,
	BP_CATCH_FISH = 6,
	BP_MINING = 7,
	BP_RECHARGE_DEW_DRAGON = 8,
	BP_GET_SPIRIT_SHARD = 9,
	BP_SELL_ITEMS_AT_PNJ = 10,
	BP_BUY_ITEM_TO_PNJ = 11,
	BP_CRAFT_ITEMS = 12,
	BP_FINISH_QUEST_BOOK = 13,
	BP_USE_SWITCH = 14,
	BP_ADD_67_BONUS = 15,
	BP_ADD_ORE = 16,
	BP_REROLL_LUCKYBOX = 17,
	BP_UPGRADE_STUFF = 18,
	BP_OPEN_CHEST = 19,
	BP_OPEN_EGG = 20,
	BP_BUY_ITEM_GAYA = 21,
	BP_SEND_MSG_GENERAL_CHAT = 22,
	BP_GET_SILVER_COIN = 23,
	BP_FEED_PET = 24,
	BP_BUILD_SANDCASTLE = 25,
	BP_OPEN_MYSTICAL_CHEST = 27,
	BP_DESTROY_TARGET_METIN = 28,
	BP_DESTROY_SUMMER_SHRINES = 29,
	BP_FINISH_SUMMER_STONE_HUNT = 30,
	BP_UPGRADE_CIRCUS_SKILLS = 31,
	BP_HARVEST_HALLOWEEN_FLOWERS = 32,
	BP_CATCH_HAUNTED_PEARLS = 33,
	BP_FINISH_HALLOWEEN_DAILY = 34,
};

void BattlepassRenewal_OnLogin(CHARACTER* ch);
void BattlepassRenewal_OnLogout(CHARACTER* ch);	// mission counters written, cache dropped
void BattlepassRenewal_SendInfo(CHARACTER* ch);
void BattlepassRenewal_OnEvent(CHARACTER* ch, int iType, DWORD dwVnum = 0, int iCount = 1);
void BattlepassRenewal_OnKill(CHARACTER* victim, CHARACTER* killer);
void BattlepassRenewal_OnGlobalRanking(CHARACTER* ch, int iCategory, long long llValue);
bool BattlepassRenewal_UseItem(CHARACTER* ch, CItem* item);	// premium / exp items; true = handled
#endif
