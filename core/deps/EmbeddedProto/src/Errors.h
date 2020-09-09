/*
 *  Copyright (C) 2020 Embedded AMS B.V. - All Rights Reserved
 *
 *  This file is part of Embedded Proto.
 *
 *  Embedded Proto is open source software: you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as published 
 *  by the Free Software Foundation, version 3 of the license.
 *
 *  Embedded Proto  is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Embedded Proto. If not, see <https://www.gnu.org/licenses/>.
 *
 *  For commercial and closed source application please visit:
 *  <https://EmbeddedProto.com/license/>.
 *
 *  Embedded AMS B.V.
 *  Info:
 *    info at EmbeddedProto dot com
 *
 *  Postal address:
 *    Johan Huizingalaan 763a
 *    1066 VH, Amsterdam
 *    the Netherlands
 */

#ifndef _ERRORS_H_
#define _ERRORS_H_

namespace EmbeddedProto 
{

  //! This enumeration defines errors which can occur during serialization and deserialization. 
  enum class Error
  {
    NO_ERRORS        = 0, //!< No errors have occurred.
    END_OF_BUFFER    = 1, //!< While trying to read from the buffer we ran out of bytes tor read.
    BUFFER_FULL      = 2, //!< The write buffer is full, unable to push more bytes in to it.
    INVALID_WIRETYPE = 3, //!< When reading a Wiretype from the tag we got an invalid value.
    ARRAY_FULL       = 4, //!< The array is full, it is not possible to push more items in it.
  };

}; // End of namespace EmbeddedProto

#endif // End of _ERRORS_H_
