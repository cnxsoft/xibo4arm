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

$db_host = 'localhost';
$db_admin_user = 'user';
$db_admin_pass = 'password';
$db_name = 'stats';

$db = @mysql_connect($db_host,$db_admin_user,$db_admin_pass);

if (! $db) {
  die('<div class="xibo_error">Could not connect to database. Error: ' . mysql_error() . '</div>');
}

@mysql_select_db($db_name,$db);

// Setup the stats we want to pull
$stats[0]['desc'] = "Active Xibo Installs";
$stats[0]['sql'] = "SELECT COUNT(*) FROM `stats` WHERE `pingDate` > SUBDATE(CURDATE(),INTERVAL 28 DAY)";
$stats[0]['calculate'] = 1;
$stats[0]['show'] = 1;
$stats[0]['div'] = "xibo_stats_active_installs";

$stats[1]['desc'] = "Active Xibo Displays";
$stats[1]['sql'] = "SELECT SUM(numClients) FROM `stats` WHERE `pingDate` > SUBDATE(CURDATE(),INTERVAL 28 DAY)";
$stats[1]['calculate'] = 1;
$stats[1]['show'] = 1;
$stats[1]['div'] = "xibo_stats_active_displays";

$stats[2]['desc'] = "Xibo Installs Ever";
$stats[2]['sql'] = "SELECT COUNT(*) FROM `stats`";
$stats[2]['calculate'] = 1;
$stats[2]['show'] = 1;
$stats[2]['div'] = "xibo_stats_installs";

print '<div class="xibo_stats">';
print "Xibo Statistics";

foreach ($stats as $stat) {
	if ($stat['calculate'] == 1) {
		$result = @mysql_query($stat['sql'],$db);
		
		if ($result) {
			$row = mysql_fetch_row($result);
			mysql_free_result($result);
			
			if ($stat['show'] == 1) {
				print '<div class="' . $stat['div'] . '">';
				print $stat['desc'];
				print '<div class="xibo_stats_result">';
				print $row[0];
				print '</div></div>';
			}
			else {
				print '<div class="xibo_stats_hidden">';
				print $stat['desc'];
				print $row[0];
				print '</div>';			
			}
		}
		else {
			print "Error" . mysql_error();
		}
		
	}
}
print '</div>';

mysql_close($db);
?>
