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

#ifndef _READ_BUFFER_INTERFACE_H_
#define _READ_BUFFER_INTERFACE_H_

#include <cstdint>


namespace EmbeddedProto 
{
  //! The pure virtual definition of a message buffer to read from.
  /*!
      The buffer deals with bytes (uint8_t) only.
  */
  class ReadBufferInterface
  {
    public:

      ReadBufferInterface() = default;
      virtual ~ReadBufferInterface() = default;

      //! Obtain the total number of bytes currently stored in the buffer.
      virtual uint32_t get_size() const = 0;

      //! Obtain the total number of bytes which can at most be stored in the buffer.
      virtual uint32_t get_max_size() const = 0;

      //! Obtain the value of the oldest byte in the buffer.
      /*!
          This function will not alter the buffer read index.
          
          The parameter byte will not be set if the buffer was empty.

          \param[out] byte When the buffer is not empty this variable will hold the oldest value.
          \return True when the buffer was not empty.
      */
      virtual bool peek(uint8_t& byte) const = 0;

      //! Advances the internal read index by one when the buffer is not empty.
      virtual void advance() = 0;

      //! Advances the internal read index by the given value.
      /*!
          The advance is limited to the number of bytes in the buffer.
          \param[in] N The number of bytes to advance the read index.
      */
      virtual void advance(const uint32_t N) = 0;

      //! Obtain the value of the oldest byte in the buffer and remove it from the buffer.
      /*!
          This function will alter the internal read index.
          
          The parameter byte will not be set if the buffer was empty.

          \param[out] byte When the buffer is not empty this variable will hold the oldest value.
          \return True when the buffer was not empty.
      */
      virtual bool pop(uint8_t& byte) = 0;

  };

} // End of namespace EmbeddedProto

#endif // End of _READ_BUFFER_INTERFACE_H_
