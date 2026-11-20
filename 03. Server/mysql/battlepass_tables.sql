-- Battlepass ilerlemesi ayri tablolara tasindi (onceden quest tablosunda "bp<sezon>.*" satirlariydi).
-- Sunucu kapaliyken calistir. Sira: tablolar -> tasima -> eski quest satirlarinin silinmesi.

CREATE TABLE IF NOT EXISTS `player`.`battlepass_progress` (
  `pid` INT UNSIGNED NOT NULL,
  `kind` TINYINT UNSIGNED NOT NULL,
  `season` INT NOT NULL,
  `level` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `exp` INT NOT NULL DEFAULT 0,
  `premium` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `claimed_free` VARCHAR(255) NOT NULL DEFAULT '',
  `claimed_premium` VARCHAR(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`pid`, `kind`, `season`),
  KEY `season_idx` (`kind`, `season`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `player`.`battlepass_mission` (
  `pid` INT UNSIGNED NOT NULL,
  `kind` TINYINT UNSIGNED NOT NULL,
  `season` INT NOT NULL,
  `mission` INT NOT NULL,
  `value` INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`, `kind`, `season`, `mission`),
  KEY `season_idx` (`kind`, `season`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Eski quest flag'lerinden tasima. claimed_* : seviye basina bir karakter, '1' = alindi.
INSERT INTO `player`.`battlepass_progress` (`pid`, `kind`, `season`, `level`, `exp`, `premium`, `claimed_free`, `claimed_premium`)
SELECT `dwPID`,
       IF(`szName` LIKE 'bpe%', 1, 0),
       CAST(SUBSTRING(`szName`, IF(`szName` LIKE 'bpe%', 4, 3)) AS SIGNED),
       MAX(IF(`szState` = 'lv', `lValue`, 0)),
       MAX(IF(`szState` = 'exp', `lValue`, 0)),
       MAX(IF(`szState` = 'prem', `lValue`, 0)),
       REVERSE(BIN(MAX(IF(`szState` = 'cf', `lValue`, 0)))),
       REVERSE(BIN(MAX(IF(`szState` = 'cp', `lValue`, 0))))
FROM `player`.`quest`
WHERE `szName` REGEXP '^bpe?[0-9]+$' AND `szState` IN ('lv', 'exp', 'prem', 'cf', 'cp')
GROUP BY `dwPID`, `szName`
ON DUPLICATE KEY UPDATE `level` = VALUES(`level`), `exp` = VALUES(`exp`), `premium` = VALUES(`premium`),
  `claimed_free` = VALUES(`claimed_free`), `claimed_premium` = VALUES(`claimed_premium`);

INSERT INTO `player`.`battlepass_mission` (`pid`, `kind`, `season`, `mission`, `value`)
SELECT `dwPID`,
       IF(`szName` LIKE 'bpe%', 1, 0),
       CAST(SUBSTRING(`szName`, IF(`szName` LIKE 'bpe%', 4, 3)) AS SIGNED),
       CAST(SUBSTRING(`szState`, 2) AS SIGNED),
       `lValue`
FROM `player`.`quest`
WHERE `szName` REGEXP '^bpe?[0-9]+$' AND `szState` REGEXP '^m[0-9]+$'
ON DUPLICATE KEY UPDATE `value` = VALUES(`value`);

DELETE FROM `player`.`quest` WHERE `szName` REGEXP '^bpe?[0-9]+$';

-- Biten sezonlarin verisini silmek icin (ornek: normal pass sezon 11):
-- DELETE FROM `player`.`battlepass_progress` WHERE `kind` = 0 AND `season` = 11;
-- DELETE FROM `player`.`battlepass_mission`  WHERE `kind` = 0 AND `season` = 11;
