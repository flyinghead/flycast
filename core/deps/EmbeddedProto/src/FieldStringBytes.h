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

#ifndef _FIELD_STRING_BYTES_H_
#define _FIELD_STRING_BYTES_H_

#include "Fields.h"
#include "Errors.h"

#include <cstdint>
#include <string.h>
#include <type_traits>


namespace EmbeddedProto
{

  namespace internal
  {

    template<uint32_t MAX_LENGTH, class DATA_TYPE>
    class FieldStringBytes : public Field
    {
      static_assert(std::is_same<uint8_t, DATA_TYPE>::value || std::is_same<char, DATA_TYPE>::value, 
                    "This class only supports unit8_t or chars.");

      public:

        FieldStringBytes()
          : current_length_(0),
            data_{0}
        {

        }

        virtual ~FieldStringBytes()
        {
          clear();
        }

        //! Obtain the number of characters in the string right now.
        uint32_t get_length() const { return current_length_; }

        //! Obtain the maximum number characters in the string.
        uint32_t get_max_length() const { return MAX_LENGTH; }

        //! Get a constant pointer to the first element in the array.
        const DATA_TYPE* get_const() const { return data_; }

        //! Get a reference to the value at the given index. 
        /*!
          This function will update the number of elements used in the array/string.

          \param[in] index The desired index to return.
          \return The reference to the value at the given index. Will return the last element if the 
                  index is out of bounds
        */
        DATA_TYPE& get(uint32_t index) 
        { 
          uint32_t limited_index = std::min(index, MAX_LENGTH-1);
          // Check if we need to update the number of elements in the array.
          if(limited_index >= current_length_) {
            current_length_ = limited_index + 1;
          }
          return data_[limited_index]; 
        }

        //! Get a constant reference to the value at the given index. 
        /*!
          \param[in] index The desired index to return.
          \return The reference to the value at the given index. Will return the last element if the 
                  index is out of bounds
        */
        const DATA_TYPE& get_const(uint32_t index) const 
        { 
          uint32_t limited_index = std::min(index, MAX_LENGTH-1);
          return data_[limited_index]; 
        }

        //! Get a reference to the value at the given index. 
        /*!
          This function will update the number of elements used in the array/string.

          \param[in] index The desired index to return.
          \return The reference to the value at the given index. Will return the last element if the 
                  index is out of bounds
        */
        DATA_TYPE& operator[](uint32_t index) { return this->get(index); }

        //! Get a constant reference to the value at the given index. 
        /*!
          \param[in] index The desired index to return.
          \return The reference to the value at the given index. Will return the last element if the 
                  index is out of bounds
        */
        const DATA_TYPE& operator[](uint32_t index) const { return this->get_const(index); }


        Error set(const DATA_TYPE* data, const uint32_t length)
        {
          Error return_value = Error::NO_ERRORS;
          if(MAX_LENGTH >= length)
          {
            current_length_ = length;
            memcpy(data_, data, length);
          }
          else
          {
            return_value = Error::ARRAY_FULL;
          }
          return return_value;
        }


        Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const override 
        {
          Error return_value = Error::NO_ERRORS;

          if(0 < current_length_) 
          {
            if(current_length_ <= buffer.get_available_size())
            {
              uint32_t tag = WireFormatter::MakeTag(field_number, 
                                                    WireFormatter::WireType::LENGTH_DELIMITED);
              return_value = WireFormatter::SerializeVarint(tag, buffer);
              if(Error::NO_ERRORS == return_value) 
              {
                return_value = serialize(buffer);
              }
            }
            else 
            {
              return_value = Error::BUFFER_FULL;
            }
          }

          return return_value;
        }

        Error serialize(WriteBufferInterface& buffer) const override 
        { 
          Error return_value = WireFormatter::SerializeVarint(current_length_, buffer);
          if(Error::NO_ERRORS == return_value) 
          {
            const void* void_pointer = static_cast<const void*>(&(data_[0]));
            const uint8_t* byte_pointer = static_cast<const uint8_t*>(void_pointer);
            if(!buffer.push(byte_pointer, current_length_))
            {
              return_value = Error::BUFFER_FULL;
            }
          }
          return return_value;
        }

        Error deserialize(ReadBufferInterface& buffer) override 
        {
          uint32_t availiable;
          Error return_value = WireFormatter::DeserializeVarint(buffer, availiable);
          if(Error::NO_ERRORS == return_value)
          {
            if(MAX_LENGTH >= availiable) 
            {
              clear();

              uint8_t byte;
              while((current_length_ < availiable) && buffer.pop(byte)) 
              {
                data_[current_length_] = static_cast<DATA_TYPE>(byte);
                ++current_length_;
              }

              if(current_length_ != availiable)
              {
                // If at the end we did not read the same number of characters something went wrong.
                return_value = Error::END_OF_BUFFER;
              }
            }
            else 
            {
              return_value = Error::ARRAY_FULL;
            }
          }

          return return_value;
        }

        //! Reset the field to it's initial value.
        void clear() override 
        { 
          memset(data_, 0, current_length_);
          current_length_ = 0; 
        }
    
      protected:

        //! Number of item in the data array.
        uint32_t current_length_;

        //! The text.
        DATA_TYPE data_[MAX_LENGTH];

    }; // End of class FieldStringBytes

  } // End of namespace internal

  template<uint32_t MAX_LENGTH>
  class FieldString : public internal::FieldStringBytes<MAX_LENGTH, char>
  {
    public:
      FieldString() = default;
      virtual ~FieldString() = default;

      void operator=(const char* const &&rhs)
      {
        const uint32_t rhs_MAX_LENGTH = strlen(rhs);
        this->current_length_ = std::min(rhs_MAX_LENGTH, MAX_LENGTH);
        strncpy(this->data_, rhs, this->current_length_);

        // Make sure the string is null terminated.
        if(MAX_LENGTH > this->current_length_)
        {
          this->data_[this->current_length_] = 0;
        }
      }
  };

  template<uint32_t MAX_LENGTH>
  class FieldBytes : public internal::FieldStringBytes<MAX_LENGTH, uint8_t>
  {
    public:
      FieldBytes() = default;
      virtual ~FieldBytes() = default; 
  };


} // End of namespace EmbeddedProto

#endif // End of _FIELD_STRING_BYTES_H_