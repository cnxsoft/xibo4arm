<?php
/*
 * Xibo - Digitial Signage - http://www.xibo.org.uk
 * Copyright (C) 2011 Daniel Garner
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

class Template extends Data
{
    /**
     * Deletes a layout
     * @param <type> $layoutId
     * @return <type>
     */
    public function Delete($templateId)
    {
        $db =& $this->db;

        // Remove the Layout
        if (!$db->query(sprintf('DELETE FROM template WHERE TemplateId = %d', $templateId)))
            return $this->SetError(25105, __('Unable to delete template'));

        return true;
    }
}
?>