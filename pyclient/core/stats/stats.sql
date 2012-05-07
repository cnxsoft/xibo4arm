-- Host: localhost
-- Generation Time: Mar 02, 2009 at 05:39 PM
-- Server version: 5.0.67
-- PHP Version: 5.2.6-2ubuntu4.1

SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";

--
-- Database: `xibostats`
--

-- --------------------------------------------------------

--
-- Table structure for table `stats`
--

DROP TABLE IF EXISTS `stats`;
CREATE TABLE IF NOT EXISTS `stats` (
  `id` varchar(32) character set utf8 NOT NULL,
  `version` varchar(20) character set utf8 NOT NULL,
  `numClients` int(11) default '0',
  `installDate` datetime NOT NULL,
  `pingDate` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

