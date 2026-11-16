/*
 * File Name : battlepass_renewal.cpp
 * Author    : BestStudio
 * Version   : 1.0
 * Date      : 2026-11-14
 *
 * Battlepass, see battlepass_renewal.h. Rules from the Anka uiBattlePass window data and battlepass_config.json:
 * reward i (free / premium) is unlocked at level i + 1, every weekly schedule whose date has come stays open until the
 * season ends, a finished mission gives its exp once, level exp 20 (event: from the config), after the last level
 * every further level gives the max level bonus item. State: quest flags "bp<season>.*" (event "bpe<season>.*").
 *
 * Protocol (server -> client, one comma-joined token each):
 *   BPInfo kind,season,level,exp,levelExp,premium,claimedFree,claimedPremium,endTime,serverTime,maxLevel  (season 0 = none)
 *   BPMissions kind,chunk,id:progress;id:progress...     (chunk 0 clears the list)
 *   BPMission kind,id,progress
 *   BPResult code,value
 */
#include "stdafx.h"
#include "../../common/service.h"
#include "battlepass_renewal.h"

#ifdef ENABLE_BATTLEPASS_RENEWAL
#include "utils.h"
#include "config.h"
#include "char.h"
#include "item.h"
#include "desc.h"
#include "locale_service.h"
#include "global_ranking.h"
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <ctime>

namespace
{
	enum
	{
		BP_KIND_PASS = 0,
		BP_KIND_EVENT = 1,
		BP_KIND_COUNT = 2,
		BP_MISSION_CHUNK = 12,
		BP_LEVEL_GAP_NORMAL = 15,		// QUEST_DESCRIPTION: "En fazla 15 seviye alt"
		BP_LEVEL_GAP_BOSS = 20,			// "En fazla 20 seviye alt"
	};

	const DWORD s_adwPremiumItem[BP_KIND_COUNT] = { 61050, 61053 };	// BATTLEPASS_PREMIUM_ITEM_VNUM / EVENT_..._PREMIUM
	const DWORD s_adwExpItem[BP_KIND_COUNT] = { 61051, 61054 };		// BATTLEPASS_EXP_ITEM_VNUM / EVENT_..._EXP

	enum EResult
	{
		BR_NO_SEASON,
		BR_LOCKED,			// level not reached
		BR_NO_PREMIUM,
		BR_CLAIMED,
		BR_REWARD,			// value = level
		BR_PREMIUM_ON,
		BR_PREMIUM_HAD,
		BR_EXP,				// value = exp
		BR_LEVEL_UP,		// value = level
		BR_MISSION_DONE,	// value = mission id
		BR_INVENTORY,
	};

	struct SReward
	{
		DWORD dwFree, dwPremium;
		int iFree, iPremium;
	};

	struct SMission
	{
		time_t tStart;
		int iId, iType, iCount, iExp;
		DWORD dwVnum;
	};

	struct SSeason
	{
		int iKind, iId, iLevelExp, iBonusCount, iBonusExp;
		time_t tStart, tEnd;
		DWORD dwBonusVnum;
		std::vector<SReward> vecReward;
		std::vector<SMission> vecMission;
	};

	std::vector<SSeason> s_vecSeason;
	bool s_bLoaded = false;

	const std::map<std::string, int>& TypeNames()
	{
		static std::map<std::string, int> s_map;
		if (s_map.empty())
		{
			const struct { const char* n; int t; } a[] = {
				{ "KILL_MONSTER", BP_KILL_MONSTER }, { "DESTROY_METIN", BP_DESTROY_METIN }, { "KILL_BOSS", BP_KILL_BOSS },
				{ "FINISH_DUNGEON", BP_FINISH_DUNGEON }, { "KILL_PLAYERS", BP_KILL_PLAYERS }, { "CATCH_FISH", BP_CATCH_FISH },
				{ "MINING", BP_MINING }, { "RECHARGE_DEW_DRAGON", BP_RECHARGE_DEW_DRAGON }, { "GET_SPIRIT_SHARD", BP_GET_SPIRIT_SHARD },
				{ "SELL_ITEMS_AT_PNJ", BP_SELL_ITEMS_AT_PNJ }, { "BUY_ITEM_TO_PNJ", BP_BUY_ITEM_TO_PNJ }, { "CRAFT_ITEMS", BP_CRAFT_ITEMS },
				{ "FINISH_QUEST_BOOK", BP_FINISH_QUEST_BOOK }, { "USE_SWITCH", BP_USE_SWITCH }, { "ADD_67_BONUS", BP_ADD_67_BONUS },
				{ "ADD_ORE", BP_ADD_ORE }, { "REROLL_LUCKYBOX", BP_REROLL_LUCKYBOX }, { "UPGRADE_STUFF", BP_UPGRADE_STUFF },
				{ "OPEN_CHEST", BP_OPEN_CHEST }, { "OPEN_EGG", BP_OPEN_EGG }, { "BUY_ITEM_GAYA", BP_BUY_ITEM_GAYA },
				{ "SEND_MSG_GENERAL_CHAT", BP_SEND_MSG_GENERAL_CHAT }, { "GET_SILVER_COIN", BP_GET_SILVER_COIN }, { "FEED_PET", BP_FEED_PET },
				{ "BUILD_SANDCASTLE", BP_BUILD_SANDCASTLE }, { "OPEN_MYSTICAL_CHEST", BP_OPEN_MYSTICAL_CHEST },
				{ "DESTROY_TARGET_METIN", BP_DESTROY_TARGET_METIN }, { "DESTROY_SUMMER_SHRINES", BP_DESTROY_SUMMER_SHRINES },
				{ "FINISH_SUMMER_STONE_HUNT", BP_FINISH_SUMMER_STONE_HUNT }, { "UPGRADE_CIRCUS_SKILLS", BP_UPGRADE_CIRCUS_SKILLS },
				{ "HARVEST_HALLOWEEN_FLOWERS", BP_HARVEST_HALLOWEEN_FLOWERS }, { "CATCH_HAUNTED_PEARLS", BP_CATCH_HAUNTED_PEARLS },
				{ "FINISH_HALLOWEEN_DAILY", BP_FINISH_HALLOWEEN_DAILY },
			};
			for (size_t i = 0; i < sizeof(a) / sizeof(a[0]); ++i)
				s_map[a[i].n] = a[i].t;
		}
		return s_map;
	}

	time_t ParseStamp(const std::string& s)
	{
		if (s.size() < 14)
			return 0;
		struct tm t = {};
		t.tm_year = atoi(s.substr(0, 4).c_str()) - 1900;
		t.tm_mon = atoi(s.substr(4, 2).c_str()) - 1;
		t.tm_mday = atoi(s.substr(6, 2).c_str());
		t.tm_hour = atoi(s.substr(8, 2).c_str());
		t.tm_min = atoi(s.substr(10, 2).c_str());
		t.tm_sec = atoi(s.substr(12, 2).c_str());
		t.tm_isdst = -1;
		return mktime(&t);
	}

	SSeason* FindSeason(int iKind, int iId)
	{
		for (size_t i = 0; i < s_vecSeason.size(); ++i)
			if (s_vecSeason[i].iKind == iKind && s_vecSeason[i].iId == iId)
				return &s_vecSeason[i];
		return NULL;
	}

	void Load()
	{
		s_bLoaded = true;
		s_vecSeason.clear();
		char szPath[256];
		snprintf(szPath, sizeof(szPath), "%s/battlepass.txt", LocaleService_GetBasePath().c_str());
		std::ifstream in(szPath);
		if (!in.is_open())
		{
			sys_err("battlepass: cannot open %s", szPath);
			return;
		}
		std::string line;
		while (std::getline(in, line))
		{
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream ss(line);
			std::string tag;
			ss >> tag;
			int iKind = 0, iId = 0;
			ss >> iKind >> iId;
			if (tag == "SEASON")
			{
				SSeason s;
				std::string a, b, theme;
				s.iKind = iKind;
				s.iId = iId;
				ss >> a >> b >> s.iLevelExp >> s.dwBonusVnum >> s.iBonusCount >> s.iBonusExp >> theme;
				s.tStart = ParseStamp(a);
				s.tEnd = ParseStamp(b);
				if (s.iLevelExp <= 0)
					s.iLevelExp = 20;
				s_vecSeason.push_back(s);
			}
			else if (tag == "REWARD")
			{
				SSeason* s = FindSeason(iKind, iId);
				int iLevel = 0;
				SReward r = {};
				ss >> iLevel >> r.dwFree >> r.iFree >> r.dwPremium >> r.iPremium;
				if (!s || iLevel < 0 || iLevel > 200)
					continue;
				if ((int) s->vecReward.size() <= iLevel)
					s->vecReward.resize(iLevel + 1);
				s->vecReward[iLevel] = r;
			}
			else if (tag == "MISSION")
			{
				SSeason* s = FindSeason(iKind, iId);
				std::string start, type;
				SMission m = {};
				ss >> start >> m.iId >> type >> m.dwVnum >> m.iCount >> m.iExp;
				if (!s)
					continue;
				std::map<std::string, int>::const_iterator it = TypeNames().find(type);
				if (it == TypeNames().end())
				{
					sys_err("battlepass: unknown mission type %s", type.c_str());
					continue;
				}
				m.iType = it->second;
				m.tStart = ParseStamp(start);
				s->vecMission.push_back(m);
			}
		}
		sys_log(0, "battlepass: %u seasons", (unsigned) s_vecSeason.size());
	}

	const SSeason* Active(int iKind)
	{
		if (!s_bLoaded)
			Load();
		const time_t now = time(0);
		for (size_t i = 0; i < s_vecSeason.size(); ++i)
			if (s_vecSeason[i].iKind == iKind && s_vecSeason[i].tStart <= now && now <= s_vecSeason[i].tEnd)
				return &s_vecSeason[i];
		return NULL;
	}

	std::string Flag(const SSeason* s, const char* c_szField)
	{
		char sz[64];
		snprintf(sz, sizeof(sz), "%s%d.%s", s->iKind == BP_KIND_EVENT ? "bpe" : "bp", s->iId, c_szField);
		return sz;
	}

	std::string MissionFlag(const SSeason* s, int iId)
	{
		char sz[16];
		snprintf(sz, sizeof(sz), "m%d", iId);
		return Flag(s, sz);
	}

	void Result(LPCHARACTER ch, int iCode, int iValue = 0)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BPResult %d,%d", iCode, iValue);
	}

	void SendSeason(LPCHARACTER ch, int iKind)
	{
		const SSeason* s = Active(iKind);
		if (!s)
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "BPInfo %d,0", iKind);
			return;
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BPInfo %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", iKind, s->iId,
			ch->GetQuestFlag(Flag(s, "lv")), ch->GetQuestFlag(Flag(s, "exp")), s->iLevelExp, ch->GetQuestFlag(Flag(s, "prem")),
			ch->GetQuestFlag(Flag(s, "cf")), ch->GetQuestFlag(Flag(s, "cp")), (int) s->tEnd, (int) time(0), (int) s->vecReward.size());

		const time_t now = time(0);
		std::string buf;
		int iChunk = 0, iInChunk = 0;
		for (size_t i = 0; i < s->vecMission.size(); ++i)
		{
			const SMission& m = s->vecMission[i];
			if (m.tStart > now)
				continue;
			char sz[32];
			snprintf(sz, sizeof(sz), "%s%d:%d", buf.empty() ? "" : ";", m.iId, ch->GetQuestFlag(MissionFlag(s, m.iId)));
			buf += sz;
			if (++iInChunk >= BP_MISSION_CHUNK)
			{
				ch->ChatPacket(CHAT_TYPE_COMMAND, "BPMissions %d,%d,%s", iKind, iChunk++, buf.c_str());
				buf.clear();
				iInChunk = 0;
			}
		}
		if (!buf.empty() || iChunk == 0)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "BPMissions %d,%d,%s", iKind, iChunk, buf.empty() ? "-" : buf.c_str());
	}

	void GiveExp(LPCHARACTER ch, const SSeason* s, int iExp)
	{
		if (iExp <= 0)
			return;
		int iLevel = ch->GetQuestFlag(Flag(s, "lv"));
		int iCur = ch->GetQuestFlag(Flag(s, "exp")) + iExp;
		const int iMax = (int) s->vecReward.size();
		while (iCur >= s->iLevelExp)
		{
			iCur -= s->iLevelExp;
			if (iLevel < iMax)
			{
				++iLevel;
				Result(ch, BR_LEVEL_UP, iLevel);
			}
			else if (s->dwBonusVnum && s->iBonusCount > 0)
				ch->AutoGiveItem(s->dwBonusVnum, s->iBonusCount);	// max_level_bonus_reward
		}
		ch->SetQuestFlag(Flag(s, "lv"), iLevel);
		ch->SetQuestFlag(Flag(s, "exp"), iCur);
	}

	bool MatchTarget(const SMission& m, DWORD dwVnum)
	{
		return !m.dwVnum || m.dwVnum == dwVnum;
	}
}

void BattlepassRenewal_SendInfo(LPCHARACTER ch)
{
	if (!ch || !ch->GetDesc())
		return;
	for (int k = 0; k < BP_KIND_COUNT; ++k)
		SendSeason(ch, k);
}

void BattlepassRenewal_OnLogin(LPCHARACTER ch)
{
	BattlepassRenewal_SendInfo(ch);
}

void BattlepassRenewal_OnEvent(LPCHARACTER ch, int iType, DWORD dwVnum, int iCount)
{
	if (!ch || !ch->IsPC() || !ch->GetDesc() || iCount <= 0)
		return;
	const time_t now = time(0);
	for (int k = 0; k < BP_KIND_COUNT; ++k)
	{
		const SSeason* s = Active(k);
		if (!s)
			continue;
		for (size_t i = 0; i < s->vecMission.size(); ++i)
		{
			const SMission& m = s->vecMission[i];
			if (m.iType != iType || m.tStart > now || !MatchTarget(m, dwVnum))
				continue;
			const std::string flag = MissionFlag(s, m.iId);
			const int iOld = ch->GetQuestFlag(flag);
			if (iOld >= m.iCount)
				continue;
			const int iNew = MIN(m.iCount, iOld + iCount);
			ch->SetQuestFlag(flag, iNew);
			ch->ChatPacket(CHAT_TYPE_COMMAND, "BPMission %d,%d,%d", k, m.iId, iNew);
			if (iNew >= m.iCount)
			{
				Result(ch, BR_MISSION_DONE, m.iId);
				GiveExp(ch, s, m.iExp);
				SendSeason(ch, k);
			}
		}
	}
}

void BattlepassRenewal_OnKill(LPCHARACTER victim, LPCHARACTER killer)
{
	if (!victim || !killer || !killer->IsPC() || victim->IsPC())
		return;
	const int iGap = killer->GetLevel() - victim->GetLevel();
	if (victim->IsStone())
	{
		if (iGap <= BP_LEVEL_GAP_NORMAL)
			BattlepassRenewal_OnEvent(killer, BP_DESTROY_METIN, victim->GetRaceNum());
		BattlepassRenewal_OnEvent(killer, BP_DESTROY_TARGET_METIN, victim->GetRaceNum());
	}
	else if (victim->GetMobRank() >= MOB_RANK_BOSS)
	{
		if (iGap <= BP_LEVEL_GAP_BOSS)
			BattlepassRenewal_OnEvent(killer, BP_KILL_BOSS, victim->GetRaceNum());
	}
	else if (victim->IsMonster())
	{
		if (iGap <= BP_LEVEL_GAP_NORMAL)
			BattlepassRenewal_OnEvent(killer, BP_KILL_MONSTER, victim->GetRaceNum());
	}
}

void BattlepassRenewal_OnGlobalRanking(LPCHARACTER ch, int iCategory, long long llValue)
{
	int iType = 0;
	switch (iCategory)
	{
		case GLOBAL_RANK_REFINE_SUCCESS:
		case GLOBAL_RANK_REFINE_FAIL:		iType = BP_UPGRADE_STUFF; break;
		case GLOBAL_RANK_SWITCH_ITEM:
		case GLOBAL_RANK_SWITCH_COSTUME:
		case GLOBAL_RANK_SWITCH_SASH:
		case GLOBAL_RANK_SWITCH_MOUNT:		iType = BP_USE_SWITCH; break;
		case GLOBAL_RANK_FISH:				iType = BP_CATCH_FISH; break;
		case GLOBAL_RANK_MINING:			iType = BP_MINING; break;
		case GLOBAL_RANK_DUNGEON:			iType = BP_FINISH_DUNGEON; break;
		case GLOBAL_RANK_QUEST_BOOK:		iType = BP_FINISH_QUEST_BOOK; break;
		case GLOBAL_RANK_PVP_KILLS:			iType = BP_KILL_PLAYERS; break;
		default:
			return;
	}
	BattlepassRenewal_OnEvent(ch, iType, 0, (int) MIN(llValue, 1000000LL));
}

bool BattlepassRenewal_UseItem(LPCHARACTER ch, LPITEM item)
{
	if (!ch || !item)
		return false;
	for (int k = 0; k < BP_KIND_COUNT; ++k)
	{
		const bool bPremium = item->GetVnum() == s_adwPremiumItem[k];
		const bool bExp = item->GetVnum() == s_adwExpItem[k];
		if (!bPremium && !bExp)
			continue;
		const SSeason* s = Active(k);
		if (!s)
		{
			Result(ch, BR_NO_SEASON, k);
			return true;
		}
		if (bPremium)
		{
			if (ch->GetQuestFlag(Flag(s, "prem")))
			{
				Result(ch, BR_PREMIUM_HAD, k);
				return true;
			}
			ch->SetQuestFlag(Flag(s, "prem"), 1);
			Result(ch, BR_PREMIUM_ON, k);
		}
		else
		{
			const int iExp = MAX(1, item->GetValue(0)) * s->iLevelExp;
			GiveExp(ch, s, iExp);
			Result(ch, BR_EXP, iExp);
		}
		item->SetCount(item->GetCount() - 1);
		SendSeason(ch, k);
		return true;
	}
	return false;
}

// /bp info | claim <kind> <track 0 free / 1 premium> <level index> | claim_all <kind>
ACMD(do_battlepass_renewal)
{
	char arg1[256], arg2[256], arg3[256], arg4[256];
	const char* rest = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	two_arguments(rest, arg3, sizeof(arg3), arg4, sizeof(arg4));
	const std::string sub = arg1;

	if (sub.empty() || sub == "info")
	{
		BattlepassRenewal_SendInfo(ch);
		return;
	}
	if (sub == "reload" && ch->GetGMLevel() >= GM_IMPLEMENTOR)
	{
		Load();
		BattlepassRenewal_SendInfo(ch);
		return;
	}
	if (sub != "claim" && sub != "claim_all")
		return;
	if (ch->IsDead() || ch->GetExchange() || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen())
		return;

	int iKind = 0;
	str_to_number(iKind, arg2);
	const SSeason* s = Active(MINMAX(0, iKind, BP_KIND_COUNT - 1));
	if (!s)
	{
		Result(ch, BR_NO_SEASON, iKind);
		return;
	}
	const int iLevel = ch->GetQuestFlag(Flag(s, "lv"));
	const bool bPremium = ch->GetQuestFlag(Flag(s, "prem")) != 0;
	int iFirst = 0, iLast = (int) s->vecReward.size() - 1, iTrackFirst = 0, iTrackLast = 1;
	if (sub == "claim")
	{
		int iTrack = 0, iIndex = 0;
		str_to_number(iTrack, arg3);
		str_to_number(iIndex, arg4);
		if (iIndex < 0 || iIndex >= (int) s->vecReward.size())
			return;
		if (iIndex >= iLevel)
		{
			Result(ch, BR_LOCKED, iIndex + 1);
			return;
		}
		if (iTrack && !bPremium)
		{
			Result(ch, BR_NO_PREMIUM);
			return;
		}
		iFirst = iLast = iIndex;
		iTrackFirst = iTrackLast = iTrack ? 1 : 0;
	}

	int iGiven = 0;
	for (int iTrack = iTrackFirst; iTrack <= iTrackLast; ++iTrack)
	{
		if (iTrack && !bPremium)
			continue;
		const char* c_szMask = iTrack ? "cp" : "cf";
		int iMask = ch->GetQuestFlag(Flag(s, c_szMask));
		for (int i = iFirst; i <= iLast && i < iLevel && i < 31; ++i)
		{
			if (iMask & (1 << i))
				continue;
			const SReward& r = s->vecReward[i];
			const DWORD dwVnum = iTrack ? r.dwPremium : r.dwFree;
			const int iCount = iTrack ? r.iPremium : r.iFree;
			if (dwVnum && iCount > 0 && !ch->AutoGiveItem(dwVnum, iCount))
			{
				Result(ch, BR_INVENTORY);
				break;
			}
			iMask |= (1 << i);
			++iGiven;
		}
		ch->SetQuestFlag(Flag(s, c_szMask), iMask);
	}
	if (sub == "claim" && !iGiven)
		Result(ch, BR_CLAIMED, iFirst + 1);
	else if (iGiven)
		Result(ch, BR_REWARD, iGiven);
	SendSeason(ch, s->iKind);
}
#endif
