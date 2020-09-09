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

#ifndef _READ_BUFFER_SECTION_H_
#define _READ_BUFFER_SECTION_H_

#include "ReadBufferInterface.h"

#include <cstdint>


namespace EmbeddedProto 
{
  //! This is a wrapper around a ReadBufferInterface only exposing a given number of bytes.
  /*!
      This class is used when decoding a length delimited fields. It is constructed given a message
      buffer and a size. This class will return bytes from the buffer for the given number of bytes
      stated in the size parameter.

      \see ReadBufferInterface
  */
  class ReadBufferSection : public ReadBufferInterface
  {
    public:

      //! Explicitly delete the default constructor in favor of the one with parameters.
      ReadBufferSection() = delete;

      //! The constructor of the class with the required parameters
      /*!
        \param buffer The actual data buffer from which the bytes are obtained.
        \param size The maximum number of bytes to return from buffer.
      */
      ReadBufferSection(ReadBufferInterface& buffer, const uint32_t size);

      virtual ~ReadBufferSection() = default;


      //! Return the number of bytes remaining.
      uint32_t get_size() const override;

      //! Obtain the total number of bytes which can at most be stored in the buffer.
      /*!
        In the case of this buffer section this will return the size of the section.
      */
      uint32_t get_max_size() const override;

      //! Expose the function of the parent buffer.
      /*!
        This will not do anything if size zero is reached.
      */
      bool peek(uint8_t& byte) const override;

      //! Decrement the size and call advance on the parent buffer.
      /*!
        This will not do anything if size zero is reached.
      */
      void advance() override;

      //! Decrement the size by N bytes and call advance on the parent buffer.
      /*!
        This will not do anything if size zero is reached.
      */
      void advance(const uint32_t N) override;

      //! Decrement the size and pop the next byte from the parent buffer.
      /*!
        This will not do anything if size zero is reached.
      */
      bool pop(uint8_t& byte) override;

    private:

      //! A reference to the buffer containing the actual data.
      ReadBufferInterface& buffer_;

      //! The number of bytes left in this section.
      uint32_t size_;

      //! The total number of bytes masked of by this section.
      const uint32_t max_size_;
  };

} // End of namespace EmbeddedProto

#endif // _READ_BUFFER_SECTION_H_
