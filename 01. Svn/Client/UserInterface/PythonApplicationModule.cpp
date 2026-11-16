// void initapp() icinde:
// Ara:
	PyModule_AddIntConstant(poModule, "ENABLE_BIOLOG_RENEWAL",	0);

// Altina ekle:
#ifdef ENABLE_BATTLEPASS_RENEWAL
	PyModule_AddIntConstant(poModule, "ENABLE_BATTLEPASS_RENEWAL", 1);
#else
	PyModule_AddIntConstant(poModule, "ENABLE_BATTLEPASS_RENEWAL", 0);
#endif

