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

#ifndef _REPEATED_FIELD_SIZE_H_
#define _REPEATED_FIELD_SIZE_H_

#include "RepeatedField.h"
#include "Errors.h"

#include <cstdint>
#include <cstring>
#include <algorithm>


namespace EmbeddedProto
{

  //! A template class that actually holds some data.
  /*!
    This is a separate class to make it possible to not have the size defined in every function or 
    class using this type of object.
  */
  template<class DATA_TYPE, uint32_t MAX_LENGTH>
  class RepeatedFieldFixedSize : public RepeatedField<DATA_TYPE>
  { 
      static constexpr uint32_t BYTES_PER_ELEMENT = sizeof(DATA_TYPE);

    public:

      RepeatedFieldFixedSize()
        : current_length_(0),
          data_{}
      {

      }  

      ~RepeatedFieldFixedSize() override = default;

      //! Obtain the total number of DATA_TYPE items in the array.
      uint32_t get_length() const override { return current_length_; }

      //! Obtain the maximum number of DATA_TYPE items which can at most be stored in the array.
      uint32_t get_max_length() const override { return MAX_LENGTH; }

      //! Obtain the total number of bytes currently stored in the array.
      uint32_t get_size() const override { return BYTES_PER_ELEMENT * current_length_; }

      //! Obtain the maximum number of bytes which can at most be stored in the array.
      uint32_t get_max_size() const override { return BYTES_PER_ELEMENT * MAX_LENGTH; }

      DATA_TYPE& get(uint32_t index) override 
      { 
        uint32_t limited_index = std::min(index, MAX_LENGTH-1);
        // Check if we need to update the number of elements in the array.
        if(limited_index >= current_length_) {
          current_length_ = limited_index + 1;
        }
        return data_[limited_index]; 
      }

      const DATA_TYPE& get_const(uint32_t index) const override 
      { 
        uint32_t limited_index = std::min(index, MAX_LENGTH-1);
        return data_[limited_index]; 
      }

      void set(uint32_t index, const DATA_TYPE& value) override 
      { 
        uint32_t limited_index = std::min(index, MAX_LENGTH-1);
        // Check if we need to update the number of elements in the array.
        if(limited_index >= current_length_) {
          current_length_ = limited_index + 1;
        }
        data_[limited_index] = value;  
      }

      Error set_data(const DATA_TYPE* data, const uint32_t length) override 
      {
        Error return_value = Error::NO_ERRORS;
        if(MAX_LENGTH >= length) 
        {
          current_length_ = length;
          memcpy(data_, data, length * BYTES_PER_ELEMENT);
        }
        else 
        {
          return_value = Error::ARRAY_FULL;
        }
        return return_value;
      }

      Error add(const DATA_TYPE& value) override 
      {
        Error return_value = Error::NO_ERRORS;
        if(MAX_LENGTH > current_length_) 
        {
          data_[current_length_] = value;
          ++current_length_;
        }
        else 
        {
          return_value = Error::ARRAY_FULL;
        }
        return return_value;
      }

      void clear() override 
      {
        for(uint32_t i = 0; i < current_length_; ++i)
        {
          data_[i].clear();
        }
        current_length_ = 0;
      }

    private:

      //! Number of item in the data array.
      uint32_t current_length_;

      //! The actual data 
      DATA_TYPE data_[MAX_LENGTH];
  };

} // End of namespace EmbeddedProto

#endif // End of _REPEATED_FIELD_SIZE_H_