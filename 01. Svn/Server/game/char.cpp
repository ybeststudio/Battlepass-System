// Ara:
#include "../../common/service.h"

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
#include "battlepass_renewal.h"
#endif


// void CHARACTER::Disconnect(const char * c_pszReason) icinde ara:
	sys_log(0, "DISCONNECT: %s (%s)", GetName(), c_pszReason ? c_pszReason : "unset" );

// Ustune ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	BattlepassRenewal_OnLogout(this);
#endif
