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

#ifndef _REPEATED_FIELD_H_
#define _REPEATED_FIELD_H_

#include "Fields.h"
#include "MessageInterface.h"
#include "MessageSizeCalculator.h"
#include "ReadBufferSection.h" 
#include "Errors.h"

#include <cstdint>
#include <type_traits>


namespace EmbeddedProto
{

  //! Class template that specifies the interface of an arry with the data type.
  template<class DATA_TYPE>
  class RepeatedField : public Field
  {
    static_assert(std::is_base_of<::EmbeddedProto::Field, DATA_TYPE>::value, "A Field can only be used as template paramter.");

    //! Check how this field shoeld be serialized, packed or not.
    static constexpr bool PACKED = !std::is_base_of<MessageInterface, DATA_TYPE>::value;

    public:

      RepeatedField() = default;
      virtual ~RepeatedField() = default;

      //! Obtain the total number of DATA_TYPE items in the array.
      virtual uint32_t get_length() const = 0;

      //! Obtain the maximum number of DATA_TYPE items which can at most be stored in the array.
      virtual uint32_t get_max_length() const = 0;
      
      //! Obtain the total number of bytes currently stored in the array.
      virtual uint32_t get_size() const = 0;

      //! Obtain the maximum number of bytes which can at most be stored in the array.
      virtual uint32_t get_max_size() const = 0;

      //! Get a reference to the value at the given index. 
      /*!
        \param[in] index The desired index to return.
        \return The reference to the value at the given index.
      */
      virtual DATA_TYPE& get(uint32_t index) = 0;

      //! Get a constant reference to the value at the given index. 
      /*!
        \param[in] index The desired index to return.
        \return The constant reference to the value at the given index.
      */
      virtual const DATA_TYPE& get_const(uint32_t index) const = 0;

      //! Get a reference to the value at the given index. 
      /*!
        \param[in] index The desired index to return.
        \return The reference to the value at the given index.
      */
      DATA_TYPE& operator[](uint32_t index) { return this->get(index); }

      //! Get a reference to the value at the given index. But constant. 
      /*!
        \param[in] index The desired index to return.
        \return The constant reference to the value at the given index.
      */
      const DATA_TYPE& operator[](uint32_t index) const { return this->get_const(index); }

      //! Set the value at the given index.
      /*!
        \param[in] index The desired index to change.
        \param[in] value The value we would like to set.
      */
      virtual void set(uint32_t index, const DATA_TYPE& value) = 0;

      //! Given a different array of known length copy that data into this object.
      /*!
        \param[in] data A pointer the array to copy from.
        \param[in] length The number of value of DATA_TYPE in the array.
        \return Error::NO_ERRORS when every was successful. Error::ARRAY_FULL when there is no space left.
      */
      virtual Error set_data(const DATA_TYPE* data, const uint32_t length) = 0;

      //! Append a value to the end of the array.
      /*!
        \param[in] value The data to add.
        \return Error::NO_ERRORS when every was successful. Error::ARRAY_FULL when there is no space left.
      */
      virtual Error add(const DATA_TYPE& value) = 0;

      //! Remove all data in the array and set it to the default value.
      virtual void clear() = 0;

      Error serialize(WriteBufferInterface& buffer) const
      {
        // This function should not be called on a repeated field.
        return Error::BUFFER_FULL;
      };

      //! \see Field::serialize_with_id()
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final
      {
        Error return_value = Error::NO_ERRORS;

        if(PACKED)
        {
          const uint32_t size_x = this->serialized_size_packed(field_number);

          // Use the packed way of serialization for base fields.
          if(size_x <= buffer.get_available_size())
          {
            if(0 < size_x)
            {          
              uint32_t tag = WireFormatter::MakeTag(field_number, 
                                          WireFormatter::WireType::LENGTH_DELIMITED);
              return_value = WireFormatter::SerializeVarint(tag, buffer);
              if(Error::NO_ERRORS == return_value) 
              {
                return_value = WireFormatter::SerializeVarint(size_x, buffer);
                if(Error::NO_ERRORS == return_value) 
                {
                  return_value = serialize_packed(buffer);
                }
              }
            }
          }
          else
          {
            return_value = Error::BUFFER_FULL;
          }
        }
        else 
        {
          const uint32_t size_x = this->serialized_size_unpacked(field_number);
          return_value = (size_x <= buffer.get_available_size()) ? serialize_unpacked(field_number, buffer) 
                                                                 : Error::BUFFER_FULL;
        }

        return return_value;
      }

      //! Function to deserialize this array.
      /*!
          From a buffer of data fill this array with data.
          \param buffer [in]  The memory from which the message is obtained.
          \return Error::NO_ERRORS when every was successful. 
      */
      Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) final
      {
        Error return_value = Error::NO_ERRORS;
        if(PACKED)
        {              
          uint32_t size;
          return_value = WireFormatter::DeserializeVarint(buffer, size);
          ReadBufferSection bufferSection(buffer, size);
          DATA_TYPE x;
          
          return_value = x.deserialize(bufferSection);
          while(Error::NO_ERRORS == return_value)
          {
            return_value = this->add(x);
            if(Error::NO_ERRORS == return_value)
            {
              return_value = x.deserialize(bufferSection);
            }
          }

          // We expect the buffersection to be empty, in that case everything is fine..
          if(Error::END_OF_BUFFER == return_value)
          {
            return_value = Error::NO_ERRORS;
          }
        }
        else 
        {
          uint32_t size;
          return_value = WireFormatter::DeserializeVarint(buffer, size);
          if(Error::NO_ERRORS == return_value) 
          {
            ReadBufferSection bufferSection(buffer, size);
            DATA_TYPE x;
            return_value = x.deserialize(bufferSection);
            if(Error::NO_ERRORS == return_value)
            {
              return_value = this->add(x);
            }
          }
        }
        return return_value;
      }

      //! Calculate the size of this field when serialized.
      /*!
          The calculation only includes the data, not the size required by the tag and 
          \return The number of bytes this field will require once serialized.
      */
      uint32_t serialized_size_packed(int32_t field_number) const 
      {
        ::EmbeddedProto::MessageSizeCalculator calcBuffer;
        serialize_packed(calcBuffer);
        return calcBuffer.get_size();
      }

      //! Calculate the size of this field when serialized.
      /*!
          \return The number of bytes this field will require once serialized.
      */
      uint32_t serialized_size_unpacked(int32_t field_number) const 
      {
        ::EmbeddedProto::MessageSizeCalculator calcBuffer;
        serialize_unpacked(field_number, calcBuffer);
        return calcBuffer.get_size();
      }

    private:

      Error serialize_packed(WriteBufferInterface& buffer) const
      {
        Error return_value = Error::NO_ERRORS;
        for(uint32_t i = 0; (i < this->get_length()) && (Error::NO_ERRORS == return_value); ++i)
        {
          return_value = this->get_const(i).serialize(buffer);
        }
        return return_value;
      }

      Error serialize_unpacked(uint32_t field_number, WriteBufferInterface& buffer) const
      {
        Error return_value = Error::NO_ERRORS;
        for(uint32_t i = 0; (i < this->get_length()) && (Error::NO_ERRORS == return_value); ++i)
        {
          const uint32_t size_x = this->get_const(i).serialized_size();
          uint32_t tag = WireFormatter::MakeTag(field_number, 
                                    WireFormatter::WireType::LENGTH_DELIMITED);
          return_value = WireFormatter::SerializeVarint(tag, buffer);
          if(Error::NO_ERRORS == return_value)
          {
            return_value = WireFormatter::SerializeVarint(size_x, buffer);
            if((Error::NO_ERRORS == return_value) && (0 < size_x)) 
            {
              return_value = this->get_const(i).serialize(buffer);
            }
          }
        }
        return return_value;
      }

  };


} // End of namespace EmbeddedProto

#endif // End of _REPEATED_FIELD_H_
