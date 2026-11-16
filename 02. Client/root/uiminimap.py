# MiniMap.__init__ icinde ara:
		self.tooltipWarpLink = MiniMapButtonToolTip()
		self.tooltipWarpLink.SetText(localeInfo.MINIMAP_TOOLTIP_SHARE_POSITION)

# Altina ekle:
		self.tooltipBattlePass = MiniMapButtonToolTip()
		self.tooltipBattlePass.SetText(localeInfo.MINIMAP_TOOLTIP_BATTLEPASS)


# MiniMap.__Initialize icinde ara:
		self.tooltipTeleport = None
		self.tooltipWarpLink = None

# Altina ekle:
		self.tooltipBattlePass = None


# MiniMap.__Initialize icinde ara:
			self.shopSearchButton = 0
			self.tooltipShopSearch = 0

# Altina ekle:
		self.battlepassButton = 0
		self.wndBattlePassMenu = None


# MiniMap.__LoadWindow icinde ara:
			exception.Abort("MiniMap.LoadWindow.Bind")
		try:

# Altina ekle:
			self.battlepassButton = self.GetChild("battlepass")


# MiniMap.__LoadWindow icinde ara:
		except (KeyError, TypeError):
			try:

# Altina ekle:
				self.battlepassButton = self.OpenWindow.GetChild("battlepass")


# MiniMap.__LoadWindow icinde ara:
			except (KeyError, TypeError):

# Altina ekle:
				self.battlepassButton = None


# MiniMap.__LoadWindow icinde ara:
		self.btnRank.SetEvent(ui.__mem_func__(self.OpenRanking))
		self.teleportButton.SetEvent(ui.__mem_func__(self.OpenTeleportWindow))

# Altina ekle:
		if self.battlepassButton:
			self.battlepassButton.SetEvent(ui.__mem_func__(self.OpenBattlePassMenu))


# MiniMap.__LoadWindow icinde ara:
		(ButtonPosX, ButtonPosY) = self.warpLinkButton.GetGlobalPosition()
		self.tooltipWarpLink.SetTooltipPosition(ButtonPosX, ButtonPosY)

# Altina ekle:
		if self.battlepassButton:
			(ButtonPosX, ButtonPosY) = self.battlepassButton.GetGlobalPosition()
			self.tooltipBattlePass.SetTooltipPosition(ButtonPosX, ButtonPosY)


# MiniMap icinde ara:
			if self.interface:
				self.interface.RankingRefresh()

# Altina ekle:
	def OpenBattlePassMenu(self):
		if app.ENABLE_BATTLEPASS_RENEWAL and self.interface:
			self.interface.ToggleBattlePassWindow()


# MiniMap.Destroy icinde ara:
	def Destroy(self):
		self.HideMiniMap()

# Altina ekle:
		if self.wndBattlePassMenu and self.wndBattlePassMenu.IsShow():
			self.wndBattlePassMenu.Hide()


# MiniMap.OnUpdate icinde ara:
		else:
			self.tooltipWarpLink.Hide()

# Altina ekle:
		if self.battlepassButton and TRUE == self.battlepassButton.IsIn():
			self.tooltipBattlePass.Show()
		else:
			self.tooltipBattlePass.Hide()

