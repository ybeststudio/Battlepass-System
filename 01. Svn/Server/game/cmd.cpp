// Ara:
ACMD(do_biolog_renewal);

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
ACMD(do_battlepass_renewal);
#endif


// Ara:
	{ "biolog",							do_biolog_renewal,				0,						POS_DEAD,			GM_PLAYER		},

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	{ "bp",								do_battlepass_renewal,			0,						POS_DEAD,			GM_PLAYER		},
#endif

