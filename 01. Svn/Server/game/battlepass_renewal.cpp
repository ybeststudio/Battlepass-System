/*
 * File Name : battlepass_renewal.cpp
 * Author    : BestStudio
 * Version   : 1.1
 * Date      : 2026-11-20
 *
 * Battlepass, see battlepass_renewal.h. Rules from the Anka uiBattlePass window data and battlepass_config.json:
 * reward i (free / premium) is unlocked at level i + 1, every weekly schedule whose date has come stays open until the
 * season ends, a finished mission gives its exp once, level exp 20 (event: from the config), after the last level
 * every further level gives the max level bonus item. State: tables battlepass_progress / battlepass_mission, read
 * asynchronously at login, progress written at once, mission counters batched (every 60 s and at logout).
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
#include "db.h"
#include "char_manager.h"
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <memory>
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
		BP_FLUSH_SECONDS = 60,
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

	struct SState
	{
		int iSeason;
		int iLevel, iExp, iPremium;
		std::string strFree, strPremium;
		std::map<int, int> mapMission;
		std::set<int> setDirty;

		SState() : iSeason(0), iLevel(0), iExp(0), iPremium(0) {}
	};

	struct SPending
	{
		int iType;
		DWORD dwVnum;
		int iCount;
	};

	struct SPlayer
	{
		DWORD dwGen;
		int iWaiting;
		bool bLoaded;
		bool bWantInfo;
		time_t tLastFlush;
		SState aState[BP_KIND_COUNT];
		std::vector<SPending> vecPending;

		SPlayer() : dwGen(0), iWaiting(0), bLoaded(false), bWantInfo(false), tLastFlush(0) {}
	};

	std::map<DWORD, SPlayer> s_mapPlayer;
	DWORD s_dwGen = 0;

	bool IsClaimed(const std::string& s, int i)
	{
		return i >= 0 && i < (int) s.size() && s[i] == '1';
	}

	void SetClaimed(std::string& s, int i)
	{
		if ((int) s.size() <= i)
			s.resize(i + 1, '0');
		s[i] = '1';
	}

	void SaveProgress(DWORD dwPID, int iKind, const SState& st)
	{
		if (!st.iSeason)
			return;
		DBManager::instance().Query(
			"INSERT INTO battlepass_progress%s (pid, kind, season, level, exp, premium, claimed_free, claimed_premium) "
			"VALUES (%u, %d, %d, %d, %d, %d, '%s', '%s') ON DUPLICATE KEY UPDATE level = VALUES(level), exp = VALUES(exp), "
			"premium = VALUES(premium), claimed_free = VALUES(claimed_free), claimed_premium = VALUES(claimed_premium)",
			get_table_postfix(), dwPID, iKind, st.iSeason, st.iLevel, st.iExp, st.iPremium, st.strFree.c_str(), st.strPremium.c_str());
	}

	void FlushMissions(DWORD dwPID, SPlayer& pl, bool bDirect)
	{
		for (int k = 0; k < BP_KIND_COUNT; ++k)
		{
			SState& st = pl.aState[k];
			if (!st.iSeason || st.setDirty.empty())
				continue;
			std::string rows;
			int iRows = 0;
			for (std::set<int>::const_iterator it = st.setDirty.begin(); ; )
			{
				const bool bEnd = it == st.setDirty.end();
				if (!bEnd)
				{
					char sz[64];
					snprintf(sz, sizeof(sz), "%s(%u,%d,%d,%d,%d)", rows.empty() ? "" : ",", dwPID, k, st.iSeason, *it, st.mapMission[*it]);
					rows += sz;
					++iRows;
					++it;
				}
				if ((bEnd || iRows >= 60) && iRows)
				{
					if (bDirect)
					{
						std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
							"INSERT INTO battlepass_mission%s (pid, kind, season, mission, value) VALUES %s ON DUPLICATE KEY UPDATE value = VALUES(value)",
							get_table_postfix(), rows.c_str()));
					}
					else
					{
						DBManager::instance().Query(
							"INSERT INTO battlepass_mission%s (pid, kind, season, mission, value) VALUES %s ON DUPLICATE KEY UPDATE value = VALUES(value)",
							get_table_postfix(), rows.c_str());
					}
					rows.clear();
					iRows = 0;
				}
				if (bEnd)
					break;
			}
			st.setDirty.clear();
		}
		pl.tLastFlush = time(0);
	}

	void SendSeason(LPCHARACTER ch, int iKind);
	void ApplyEvent(LPCHARACTER ch, SPlayer& pl, int iType, DWORD dwVnum, int iCount);

	void OnLoaded(DWORD dwPID, DWORD dwGen)
	{
		std::map<DWORD, SPlayer>::iterator it = s_mapPlayer.find(dwPID);
		if (it == s_mapPlayer.end() || it->second.dwGen != dwGen || --it->second.iWaiting > 0)
			return;
		SPlayer& pl = it->second;
		pl.bLoaded = true;
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);
		if (!ch || !ch->GetDesc())
			return;
		std::vector<SPending> vecPending;
		vecPending.swap(pl.vecPending);
		for (size_t i = 0; i < vecPending.size(); ++i)
			ApplyEvent(ch, pl, vecPending[i].iType, vecPending[i].dwVnum, vecPending[i].iCount);
		if (pl.bWantInfo)
		{
			pl.bWantInfo = false;
			for (int k = 0; k < BP_KIND_COUNT; ++k)
				SendSeason(ch, k);
		}
	}

	void LoadPlayer(DWORD dwPID)
	{
		SPlayer& pl = s_mapPlayer[dwPID];
		pl.dwGen = ++s_dwGen;
		pl.bLoaded = false;
		pl.iWaiting = 0;
		pl.vecPending.clear();
		pl.tLastFlush = time(0);

		std::string where;
		for (int k = 0; k < BP_KIND_COUNT; ++k)
		{
			pl.aState[k] = SState();
			const SSeason* s = Active(k);
			if (!s)
				continue;
			pl.aState[k].iSeason = s->iId;
			char sz[64];
			snprintf(sz, sizeof(sz), "%s(kind = %d AND season = %d)", where.empty() ? "" : " OR ", k, s->iId);
			where += sz;
		}
		if (where.empty())
		{
			pl.bLoaded = true;
			return;
		}

		const DWORD dwGen = pl.dwGen;
		pl.iWaiting = 2;
		DBManager::instance().FuncQuery([dwPID, dwGen](SQLMsg* pMsg)
		{
			std::map<DWORD, SPlayer>::iterator it = s_mapPlayer.find(dwPID);
			if (it != s_mapPlayer.end() && it->second.dwGen == dwGen && pMsg && pMsg->Get() && pMsg->Get()->pSQLResult)
			{
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
				{
					int k = 0, iSeason = 0;
					str_to_number(k, row[0]);
					str_to_number(iSeason, row[1]);
					if (k < 0 || k >= BP_KIND_COUNT || it->second.aState[k].iSeason != iSeason)
						continue;
					SState& st = it->second.aState[k];
					str_to_number(st.iLevel, row[2]);
					str_to_number(st.iExp, row[3]);
					str_to_number(st.iPremium, row[4]);
					st.strFree = row[5] ? row[5] : "";
					st.strPremium = row[6] ? row[6] : "";
				}
			}
			OnLoaded(dwPID, dwGen);
		}, "SELECT kind, season, level, exp, premium, claimed_free, claimed_premium FROM battlepass_progress%s WHERE pid = %u AND (%s)",
			get_table_postfix(), dwPID, where.c_str());

		DBManager::instance().FuncQuery([dwPID, dwGen](SQLMsg* pMsg)
		{
			std::map<DWORD, SPlayer>::iterator it = s_mapPlayer.find(dwPID);
			if (it != s_mapPlayer.end() && it->second.dwGen == dwGen && pMsg && pMsg->Get() && pMsg->Get()->pSQLResult)
			{
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
				{
					int k = 0, iSeason = 0, iMission = 0, iValue = 0;
					str_to_number(k, row[0]);
					str_to_number(iSeason, row[1]);
					str_to_number(iMission, row[2]);
					str_to_number(iValue, row[3]);
					if (k < 0 || k >= BP_KIND_COUNT || it->second.aState[k].iSeason != iSeason)
						continue;
					it->second.aState[k].mapMission[iMission] = iValue;
				}
			}
			OnLoaded(dwPID, dwGen);
		}, "SELECT kind, season, mission, value FROM battlepass_mission%s WHERE pid = %u AND (%s)",
			get_table_postfix(), dwPID, where.c_str());
	}

	SPlayer* GetPlayer(LPCHARACTER ch)
	{
		if (!ch || !ch->IsPC())
			return NULL;
		std::map<DWORD, SPlayer>::iterator it = s_mapPlayer.find(ch->GetPlayerID());
		if (it == s_mapPlayer.end())
		{
			LoadPlayer(ch->GetPlayerID());
			it = s_mapPlayer.find(ch->GetPlayerID());
		}
		SPlayer& pl = it->second;
		if (pl.bLoaded)
		{
			for (int k = 0; k < BP_KIND_COUNT; ++k)
			{
				const SSeason* s = Active(k);
				if ((s ? s->iId : 0) != pl.aState[k].iSeason)
				{
					FlushMissions(ch->GetPlayerID(), pl, false);
					LoadPlayer(ch->GetPlayerID());
					break;
				}
			}
		}
		return &pl;
	}

	void Result(LPCHARACTER ch, int iCode, int iValue = 0)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BPResult %d,%d", iCode, iValue);
	}

	void SendSeason(LPCHARACTER ch, int iKind)
	{
		const SSeason* s = Active(iKind);
		SPlayer* pl = GetPlayer(ch);
		if (!s || !pl)
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "BPInfo %d,0", iKind);
			return;
		}
		if (!pl->bLoaded)
		{
			pl->bWantInfo = true;
			return;
		}
		const SState& st = pl->aState[iKind];
		int iMaskFree = 0, iMaskPremium = 0;
		for (int i = 0; i < 31; ++i)
		{
			if (IsClaimed(st.strFree, i))
				iMaskFree |= (1 << i);
			if (IsClaimed(st.strPremium, i))
				iMaskPremium |= (1 << i);
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BPInfo %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", iKind, s->iId,
			st.iLevel, st.iExp, s->iLevelExp, st.iPremium, iMaskFree, iMaskPremium, (int) s->tEnd, (int) time(0), (int) s->vecReward.size());

		const time_t now = time(0);
		std::string buf;
		int iChunk = 0, iInChunk = 0;
		for (size_t i = 0; i < s->vecMission.size(); ++i)
		{
			const SMission& m = s->vecMission[i];
			if (m.tStart > now)
				continue;
			std::map<int, int>::const_iterator itv = st.mapMission.find(m.iId);
			char sz[32];
			snprintf(sz, sizeof(sz), "%s%d:%d", buf.empty() ? "" : ";", m.iId, itv == st.mapMission.end() ? 0 : itv->second);
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

	void GiveExp(LPCHARACTER ch, SPlayer& pl, const SSeason* s, int iExp)
	{
		if (iExp <= 0)
			return;
		SState& st = pl.aState[s->iKind];
		int iLevel = st.iLevel;
		int iCur = st.iExp + iExp;
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
		st.iLevel = iLevel;
		st.iExp = iCur;
		SaveProgress(ch->GetPlayerID(), s->iKind, st);
	}

	bool MatchTarget(const SMission& m, DWORD dwVnum)
	{
		return !m.dwVnum || m.dwVnum == dwVnum;
	}

	void ApplyEvent(LPCHARACTER ch, SPlayer& pl, int iType, DWORD dwVnum, int iCount)
	{
		const time_t now = time(0);
		for (int k = 0; k < BP_KIND_COUNT; ++k)
		{
			const SSeason* s = Active(k);
			if (!s || pl.aState[k].iSeason != s->iId)
				continue;
			SState& st = pl.aState[k];
			for (size_t i = 0; i < s->vecMission.size(); ++i)
			{
				const SMission& m = s->vecMission[i];
				if (m.iType != iType || m.tStart > now || !MatchTarget(m, dwVnum))
					continue;
				int& iValue = st.mapMission[m.iId];
				if (iValue >= m.iCount)
					continue;
				iValue = MIN(m.iCount, iValue + iCount);
				st.setDirty.insert(m.iId);
				ch->ChatPacket(CHAT_TYPE_COMMAND, "BPMission %d,%d,%d", k, m.iId, iValue);
				if (iValue >= m.iCount)
				{
					Result(ch, BR_MISSION_DONE, m.iId);
					FlushMissions(ch->GetPlayerID(), pl, false);
					GiveExp(ch, pl, s, m.iExp);
					SendSeason(ch, k);
				}
			}
		}
		if (time(0) - pl.tLastFlush >= BP_FLUSH_SECONDS)
			FlushMissions(ch->GetPlayerID(), pl, false);
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
	if (!ch || !ch->IsPC())
		return;
	LoadPlayer(ch->GetPlayerID());
}

void BattlepassRenewal_OnLogout(LPCHARACTER ch)
{
	if (!ch || !ch->IsPC())
		return;
	std::map<DWORD, SPlayer>::iterator it = s_mapPlayer.find(ch->GetPlayerID());
	if (it == s_mapPlayer.end())
		return;
	if (it->second.bLoaded)
		FlushMissions(ch->GetPlayerID(), it->second, true);
	s_mapPlayer.erase(it);
}

void BattlepassRenewal_OnEvent(LPCHARACTER ch, int iType, DWORD dwVnum, int iCount)
{
	if (!ch || !ch->IsPC() || !ch->GetDesc() || iCount <= 0)
		return;
	SPlayer* pl = GetPlayer(ch);
	if (!pl)
		return;
	if (!pl->bLoaded)
	{
		if (pl->vecPending.size() < 512)
		{
			SPending p = { iType, dwVnum, iCount };
			pl->vecPending.push_back(p);
		}
		return;
	}
	ApplyEvent(ch, *pl, iType, dwVnum, iCount);
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
		SPlayer* pl = GetPlayer(ch);
		if (!s || !pl)
		{
			Result(ch, BR_NO_SEASON, k);
			return true;
		}
		if (!pl->bLoaded)
			return true;
		SState& st = pl->aState[k];
		if (bPremium)
		{
			if (st.iPremium)
			{
				Result(ch, BR_PREMIUM_HAD, k);
				return true;
			}
			st.iPremium = 1;
			SaveProgress(ch->GetPlayerID(), k, st);
			Result(ch, BR_PREMIUM_ON, k);
		}
		else
		{
			const int iExp = MAX(1, item->GetValue(0)) * s->iLevelExp;
			GiveExp(ch, *pl, s, iExp);
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
	iKind = MINMAX(0, iKind, BP_KIND_COUNT - 1);
	const SSeason* s = Active(iKind);
	SPlayer* pl = GetPlayer(ch);
	if (!s || !pl)
	{
		Result(ch, BR_NO_SEASON, iKind);
		return;
	}
	if (!pl->bLoaded)
		return;
	SState& st = pl->aState[iKind];
	const int iLevel = st.iLevel;
	const bool bPremium = st.iPremium != 0;
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
		std::string& strMask = iTrack ? st.strPremium : st.strFree;
		for (int i = iFirst; i <= iLast && i < iLevel && i < 31; ++i)
		{
			if (IsClaimed(strMask, i))
				continue;
			const SReward& r = s->vecReward[i];
			const DWORD dwVnum = iTrack ? r.dwPremium : r.dwFree;
			const int iCount = iTrack ? r.iPremium : r.iFree;
			if (dwVnum && iCount > 0 && !ch->AutoGiveItem(dwVnum, iCount))
			{
				Result(ch, BR_INVENTORY);
				break;
			}
			SetClaimed(strMask, i);
			++iGiven;
		}
	}
	if (iGiven)
		SaveProgress(ch->GetPlayerID(), iKind, st);
	if (sub == "claim" && !iGiven)
		Result(ch, BR_CLAIMED, iFirst + 1);
	else if (iGiven)
		Result(ch, BR_REWARD, iGiven);
	SendSeason(ch, s->iKind);
}
#endif
