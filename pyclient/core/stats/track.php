<?php

/*
 * Xibo - Digitial Signage - http://www.xibo.org.uk
 * Copyright (C) 2009 Alex Harrington
 *
 * This file is part of Xibo.
 *
 * Xibo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version. 
 *
 * Xibo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Xibo.  If not, see <http://www.gnu.org/licenses/>.
 */ 

DEFINE('XIBO',1);
include('kit.class.php');

$id = Kit::GetParam('id',_REQUEST,_STRING);
$version = Kit::GetParam('version',_REQUEST,_STRING);
$numClients = Kit::GetParam('numClients',_REQUEST,_INT);

if (!($id && $version && $numClients)) {
	die("This page is called from Xibo. You should not access it directly.");
}

$db_host = 'localhost';
$db_admin_user = 'user';
$db_admin_pass = 'password';
$db_name = 'stats';

$db = @mysql_connect($db_host,$db_admin_user,$db_admin_pass);

if (! $db) {
  die("Could not connect to database. Error: " . mysql_error());
}

@mysql_select_db($db_name,$db);

$sql = sprintf("SELECT id FROM stats WHERE id='%s'",
		mysql_real_escape_string($id));
$result = @mysql_query($sql,$db);

if (! $result) {
    die("Error running query. Error: " . mysql_error());
}

if (mysql_num_rows($result) == 0) {
	// New install
	$sql = sprintf("INSERT INTO `stats` (`id`,`version`,`numClients`,`installDate`,`pingDate`) VALUES ('%s','%s','%s',NOW(),NOW())",
		mysql_real_escape_string($id),
		mysql_real_escape_string($version),
		mysql_real_escape_string($numClients));
	if (! @mysql_query($sql,$db)) {
		die("Error running query. Error: " . mysql_error());
	}
}
else {
	// Update existing
	$sql = sprintf("UPDATE `stats` SET `version` = '%s', `numClients` = '%s',`pingDate` = NOW() WHERE `id` = '%s' LIMIT 1",
		mysql_real_escape_string($version),
		mysql_real_escape_string($numClients),
		mysql_real_escape_string($id));
	if (! @mysql_query($sql,$db)) {
		die("Error running query. Error: " . mysql_error());
	}
}

print "OK";
mysql_free_result($result);
mysql_close($db);
?>
