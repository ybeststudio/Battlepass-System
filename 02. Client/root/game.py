# GameWindow.__ServerCommand_Build icinde ara:
		if app.__AUTO_HUNT__:
			serverCommandList.update({"AutoHuntStatus" : self.interface.AutoHuntStatus})

# Altina ekle:
		if app.ENABLE_BATTLEPASS_RENEWAL:
			serverCommandList["BPInfo"] = self.BPInfo
			serverCommandList["BPMissions"] = self.BPMissions
			serverCommandList["BPMission"] = self.BPMission
			serverCommandList["BPResult"] = self.BPResult


# GameWindow icinde ara:
			if self.interface:
				self.interface.BINARY_CUBE_RENEWAL_OPEN()

# Altina ekle:
	def BINARY_BATTLEPASS_OPEN(self):
		if self.interface:
			self.interface.BINARY_BATTLEPASS_OPEN()


# GameWindow icinde ara:
			if constInfo.HUNTING_MAIN_UI_SHOW == 0:
				constInfo.HUNTING_BUTTON_FLASH = 1

# Altina ekle:
	if app.ENABLE_BATTLEPASS_RENEWAL:
		def BPInfo(self, data):
			if self.interface:
				self.interface.BattlePassInfo(data)

		def BPMissions(self, data):
			if self.interface:
				self.interface.BattlePassMissions(data)

		def BPMission(self, data):
			if self.interface:
				self.interface.BattlePassMission(data)

		def BPResult(self, data):
			if self.interface:
				self.interface.BattlePassResult(data)

