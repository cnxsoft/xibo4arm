<?php
/*
 * Xibo - Digitial Signage - http://www.xibo.org.uk
 * Copyright (C) 2009 Daniel Garner
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
defined('XIBO') or die("Sorry, you are not allowed to directly access this page.<br /> Please press the back button in your browser.");

class Data
{
	protected $db;
	private $errorNo;
	private $errorMessage;
	
	public function __construct(database $db)
	{
		$this->db =& $db;
		
		$this->error		= false;
		$this->errorNo 		= 0;
		$this->errorMessage	= '';
	}
	
	/**
	 * Gets the error state
	 * @return 
	 */
	public function IsError()
	{
		return $this->error;
	}
	
	/**
	 * Gets the Error Number
	 * @return 
	 */
	public function GetErrorNumber()
	{
		return $this->errorNo;
	}
	
	/**
	 * Gets the Error Message
	 * @return 
	 */
	public function GetErrorMessage()
	{
		return $this->errorMessage;
	}
	
	/**
	 * Sets the Error for this Data object
	 * @return 
	 * @param $errNo Object
	 * @param $errMessage Object
	 */
	protected function SetError($errNo, $errMessage = '')
	{
		$this->error		= true;
		$this->errorNo 		= $errNo;
		$this->errorMessage	= $errMessage;
		
		Debug::LogEntry($this->db, 'audit', sprintf('Data Class: Error Number [%d] Error Message [%s]', $errNo, $errMessage), 'Data Module', 'SetError');

                // Return false so that we can use this method as the return call for parent methods
		return false;
	}
} 
?>