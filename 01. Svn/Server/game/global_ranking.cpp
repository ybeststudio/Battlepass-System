// void GlobalRanking_Add(LPCHARACTER ch, int category, long long value) icinde:
// Ara:
}
void GlobalRanking_Add(LPCHARACTER ch, int category, long long value)
{

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	BattlepassRenewal_OnGlobalRanking(ch, category, value);
#endif


// void GlobalRanking_OnDead(LPCHARACTER victim, LPCHARACTER killer) icinde:
// Ara:
}
void GlobalRanking_OnDead(LPCHARACTER victim, LPCHARACTER killer)
{

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	BattlepassRenewal_OnKill(victim, killer);
#endif

