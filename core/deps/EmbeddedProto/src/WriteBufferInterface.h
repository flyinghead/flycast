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

#ifndef _WRITE_BUFFER_INTERFACE_H_
#define _WRITE_BUFFER_INTERFACE_H_

#include <cstdint>


namespace EmbeddedProto 
{
  //! The pure virtual definition of a message buffer used for writing .
  /*!
      This interface is to be used by classes wishing to use the buffer. An actual implementation
      is made specific to how you would like to store data.

      The buffer deals with bytes (uint8_t) only.
  */
  class WriteBufferInterface
  {
    public:

      WriteBufferInterface() = default;
      virtual ~WriteBufferInterface() = default;

      //! Delete all data in the buffer.
      virtual void clear() = 0;

      //! Obtain the total number of bytes currently stored in the buffer.
      virtual uint32_t get_size() const = 0;

      //! Obtain the total number of bytes which can at most be stored in the buffer.
      virtual uint32_t get_max_size() const = 0;

      //! Obtain the total number of bytes still available in the buffer.
      virtual uint32_t get_available_size() const = 0;

      //! Push a single byte into the buffer.
      /*!
          \param[in] byte The data to append after previously added data in the buffer.
          \return True when there was space to add the byte.
      */
      virtual bool push(const uint8_t byte) = 0;

      //! Push an array of bytes into the buffer.
      /*!
          The given array will be appended after already addded data in the buffer.
          \param[in] bytes Pointer to the array of bytes.
          \param[in] length The number of bytes in the array.
          \return True when there was space to add the bytes.
      */
      virtual bool push(const uint8_t* bytes, const uint32_t length) = 0;
      
  };

} // End of namespace EmbeddedProto

#endif // End of _WRITE_BUFFER_INTERFACE_H_
