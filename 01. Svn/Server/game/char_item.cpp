// bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell) icinde:
// Ara:
	if (MountRenewal_UseItem(this, item))
		return true;

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	if (BattlepassRenewal_UseItem(this, item))	// battlepass premium / exp items
		return true;
#endif


// bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell) icinde:
// Ara:
					SetRankPoints(15, GetRankPoints(15) + 1);

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
					BattlepassRenewal_OnEvent(this, BP_OPEN_CHEST, dwBoxVnum);
					BattlepassRenewal_OnEvent(this, BP_OPEN_MYSTICAL_CHEST, dwBoxVnum);
#endif

