// void CInputLogin::Entergame(LPDESC d, const char * data) icinde:
// Ara:
	MountRenewal_RefreshAffect(ch);
	MountRenewal_SendInfo(ch);
	MountRenewal_RefreshFollower(ch);

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	BattlepassRenewal_OnLogin(ch);
#endif

