// int CInputMain::Chat(LPCHARACTER ch, const char * data, size_t uiBytes) icinde:
// Ara:
		if (thecore_heart->pulse - (int) ch->GetLastShoutPulse() < passes_per_sec * 15)
			return (iExtraLen);
		ch->SetLastShoutPulse(thecore_heart->pulse);

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
		BattlepassRenewal_OnEvent(ch, BP_SEND_MSG_GENERAL_CHAT);
#endif

