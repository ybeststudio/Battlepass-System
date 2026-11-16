// void CShopManager::Sell(LPCHARACTER ch, BYTE bCell, BYTE bCount) icinde:
// Ara:
	else
		item->SetCount(item->GetCount() - bCount);
	ch->PointChange(POINT_GOLD, dwPrice, false);

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	BattlepassRenewal_OnEvent(ch, BP_SELL_ITEMS_AT_PNJ);
#endif

