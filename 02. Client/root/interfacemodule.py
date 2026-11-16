# Ara:
if app.ENABLE_BIOLOG_RENEWAL:
	import uibiologist

# Altina ekle:
if app.ENABLE_BATTLEPASS_RENEWAL:
	import uibattlepass


# Interface.__init__ icinde ara:
		if app.ENABLE_BIOLOG_RENEWAL:
			self.wndBiologist = None

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			self.wndBattlePass = None


# Interface.__MakeWindows icinde ara:
			self.wndBiologist = uibiologist.BiologistWindow()
			self.wndBiologist.BindInterface(self)

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			self.wndBattlePass = uibattlepass.BattlePassWindow()
			self.wndBattlePass.BindInterface(self)


# Interface.MakeInterface icinde ara:
		if app.ENABLE_BIOLOG_RENEWAL:
			self.wndBiologist.SetItemToolTip(self.tooltipItem)

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			self.wndBattlePass.SetItemToolTip(self.tooltipItem)


# Interface.Close icinde ara:
			if self.wndBiologist:
				self.wndBiologist.Destroy()

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			if self.wndBattlePass:
				self.wndBattlePass.Destroy()


# Interface.Close icinde ara:
			if self.wndBiologist:
				del self.wndBiologist

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			if self.wndBattlePass:
				del self.wndBattlePass


# Interface.HideAllWindows icinde ara:
			if self.wndBiologist:
				self.wndBiologist.Hide()

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			if self.wndBattlePass:
				self.wndBattlePass.Hide()


# Interface.__HideWindows icinde ara:
			if self.wndBiologist:
				hideWindows += self.wndBiologist,

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			if self.wndBattlePass:
				hideWindows += self.wndBattlePass,


# Interface icinde ara:
			else:
				self.wndAutoHunt.Open()

# Altina ekle:
	if app.ENABLE_BATTLEPASS_RENEWAL:
		def ToggleBattlePassWindow(self):
			if self.wndBattlePass:
				self.wndBattlePass.Toggle()

		def BattlePassInfo(self, data):
			if self.wndBattlePass:
				self.wndBattlePass.SetInfo(data)

		def BattlePassMissions(self, data):
			if self.wndBattlePass:
				self.wndBattlePass.SetMissions(data)

		def BattlePassMission(self, data):
			if self.wndBattlePass:
				self.wndBattlePass.SetMission(data)

		def BattlePassResult(self, data):
			if self.wndBattlePass:
				self.wndBattlePass.OnResult(data)

