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

class XiboServiceResponse
{
    private $serviceLocation;

    public function __construct()
    {
        $this->serviceLocation = Kit::GetXiboRoot();
    }

    /**
     * Outputs the WSDL
     */
    public function WSDL()
    {
        // We need to buffer the output so that we can send a Content-Length header with the WSDL
        ob_start();
        $wsdl = file_get_contents('lib/service/service.wsdl');
        $wsdl = str_replace('{{XMDS_LOCATION}}', $this->serviceLocation, $wsdl);
        echo $wsdl;

        // Get the contents of the buffer and work out its length
        $buffer = ob_get_contents();
        $length = strlen($buffer);

        // Output the headers
        header('Content-Type: text/xml; charset=ISO-8859-1\r\n');
        header('Content-Length: ' . $length);

        // Flush the buffer
        ob_end_flush();
        exit;
    }

    /**
     * Outputs the XRDS
     */
    public function XRDS()
    {
        header('Content-Type: application/xrds+xml');

        // TODO: Need to strip out the services.php part of serviceLocation - or work out a better way to do it.

        $xrds = file_get_contents('lib/service/services.xrds.xml');
        $xrds = str_replace('{{XRDS_LOCATION}}', $this->serviceLocation, $xrds);
        echo $xrds;

        die();
    }

    /**
     * Outputs an error
     */
    public function ErrorUnauthorized($message = 'Unauthorized')
    {
        header('HTTP/1.1 401 Unauthorized');
        header('Content-Type: text/plain');
        die($message);
    }

    /**
     * Outputs an internal server error
     */
    public function ErrorServerError($message = 'Unknown Error')
    {
        header('HTTP/1.1 500 Internal Server Error');
        header('Content-Type: text/plain');
        die($message);
    }

    /**
     * Success
     * @param <string> $response
     */
    public function Success($response)
    {
        echo $response;
        die();
    }
}
?>
