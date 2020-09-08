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

#ifndef _FIELDS_H_
#define _FIELDS_H_

#include "Errors.h"
#include "WireFormatter.h"
#include "WriteBufferInterface.h"
#include "ReadBufferInterface.h"

#include <cstdint>


namespace EmbeddedProto 
{

  class Field 
  {
    public:
      Field() = default;
      virtual ~Field() = default;

      virtual Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const = 0;

      virtual Error serialize(WriteBufferInterface& buffer) const = 0;

      virtual Error deserialize(ReadBufferInterface& buffer) = 0;

      //! Calculate the size of this message when serialized.
      /*!
          \return The number of bytes this message will require once serialized.
      */
      uint32_t serialized_size() const;

      //! Reset the field to it's initial value.
      virtual void clear() = 0;
  };

  template<class TYPE>
  class FieldTemplate : public Field
  {
    public:
      typedef TYPE FIELD_TYPE;

      FieldTemplate() = default;
      FieldTemplate(const TYPE& v) : value_(v) { };
      FieldTemplate(const TYPE&& v) : value_(v) { };
      ~FieldTemplate() override = default;

      void set(const TYPE& v) { value_ = v; }      
      void set(const TYPE&& v) { value_ = v; }
      void operator=(const TYPE& v) { value_ = v; }
      void operator=(const TYPE&& v) { value_ = v; }

      const TYPE& get() const { return value_; }
      TYPE& get() { return value_; }

      operator TYPE() const { return value_; }

      bool operator==(const TYPE& rhs) { return value_ == rhs; }
      bool operator!=(const TYPE& rhs) { return value_ != rhs; }
      bool operator>(const TYPE& rhs) { return value_ > rhs; }
      bool operator<(const TYPE& rhs) { return value_ < rhs; }
      bool operator>=(const TYPE& rhs) { return value_ >= rhs; }
      bool operator<=(const TYPE& rhs) { return value_ <= rhs; }

      template<class TYPE_RHS>
      bool operator==(const FieldTemplate<TYPE_RHS>& rhs) { return value_ == rhs.get(); }
      template<class TYPE_RHS>
      bool operator!=(const FieldTemplate<TYPE_RHS>& rhs) { return value_ != rhs.get(); }
      template<class TYPE_RHS>
      bool operator>(const FieldTemplate<TYPE_RHS>& rhs) { return value_ > rhs.get(); }
      template<class TYPE_RHS>
      bool operator<(const FieldTemplate<TYPE_RHS>& rhs) { return value_ < rhs.get(); }
      template<class TYPE_RHS>
      bool operator>=(const FieldTemplate<TYPE_RHS>& rhs) { return value_ >= rhs.get(); }
      template<class TYPE_RHS>
      bool operator<=(const FieldTemplate<TYPE_RHS>& rhs) { return value_ <= rhs.get(); }

      void clear() override { value_ = static_cast<TYPE>(0); }

    private:

      TYPE value_;
  };


  class int32 : public FieldTemplate<int32_t> 
  { 
    public: 
      int32() : FieldTemplate<int32_t>(0) {};
      int32(const int32_t& v) : FieldTemplate<int32_t>(v) {};
      int32(const int32_t&& v) : FieldTemplate<int32_t>(v) {};

      ~int32() override = default;

      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class int64 : public FieldTemplate<int64_t> 
  { 
    public: 
      int64() : FieldTemplate<int64_t>(0) {};
      int64(const int64_t& v) : FieldTemplate<int64_t>(v) {};
      int64(const int64_t&& v) : FieldTemplate<int64_t>(v) {};

      ~int64() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class uint32 : public FieldTemplate<uint32_t> 
  { 
    public: 
      uint32() : FieldTemplate<uint32_t>(0) {};
      uint32(const uint32_t& v) : FieldTemplate<uint32_t>(v) {};
      uint32(const uint32_t&& v) : FieldTemplate<uint32_t>(v) {};

      ~uint32() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class uint64 : public FieldTemplate<uint64_t> 
  { 
    public: 
      uint64() : FieldTemplate<uint64_t>(0) {};
      uint64(const uint64_t& v) : FieldTemplate<uint64_t>(v) {};
      uint64(const uint64_t&& v) : FieldTemplate<uint64_t>(v) {};

      ~uint64() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffe) final; 
  };

  class sint32 : public FieldTemplate<int32_t> 
  { 
    public: 
      sint32() : FieldTemplate<int32_t>(0) {};
      sint32(const int32_t& v) : FieldTemplate<int32_t>(v) {};
      sint32(const int32_t&& v) : FieldTemplate<int32_t>(v) {};

      ~sint32() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class sint64 : public FieldTemplate<int64_t> 
  { 
    public: 
      sint64() : FieldTemplate<int64_t>(0) {};
      sint64(const int64_t& v) : FieldTemplate<int64_t>(v) {};
      sint64(const int64_t&& v) : FieldTemplate<int64_t>(v) {};

      ~sint64() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class boolean : public FieldTemplate<bool> 
  { 
    public: 
      boolean() : FieldTemplate<bool>(false) {};
      boolean(const bool& v) : FieldTemplate<bool>(v) {};
      boolean(const bool&& v) : FieldTemplate<bool>(v) {};

      ~boolean() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class fixed32 : public FieldTemplate<uint32_t> 
  { 
    public: 
      fixed32() : FieldTemplate<uint32_t>(0) {};
      fixed32(const uint32_t& v) : FieldTemplate<uint32_t>(v) {};
      fixed32(const uint32_t&& v) : FieldTemplate<uint32_t>(v) {};

      ~fixed32() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class fixed64 : public FieldTemplate<uint64_t> 
  { 
    public: 
      fixed64() : FieldTemplate<uint64_t>(0) {};
      fixed64(const uint64_t& v) : FieldTemplate<uint64_t>(v) {};
      fixed64(const uint64_t&& v) : FieldTemplate<uint64_t>(v) {};

      ~fixed64() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class sfixed32 : public FieldTemplate<int32_t> 
  { 
    public: 
      sfixed32() : FieldTemplate<int32_t>(0) {};
      sfixed32(const int32_t& v) : FieldTemplate<int32_t>(v) {};
      sfixed32(const int32_t&& v) : FieldTemplate<int32_t>(v) {};

      ~sfixed32() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class sfixed64 : public FieldTemplate<int64_t> 
  { 
    public: 
      sfixed64() : FieldTemplate<int64_t>(0) {};
      sfixed64(const int64_t& v) : FieldTemplate<int64_t>(v) {};
      sfixed64(const int64_t&& v) : FieldTemplate<int64_t>(v) {};

      ~sfixed64() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class floatfixed : public FieldTemplate<float> 
  { 
    public: 
      floatfixed() : FieldTemplate<float>(0.0F) {};
      floatfixed(const float& v) : FieldTemplate<float>(v) {};
      floatfixed(const float&& v) : FieldTemplate<float>(v) {};

      ~floatfixed() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

  class doublefixed : public FieldTemplate<double> 
  { 
    public: 
      doublefixed() : FieldTemplate<double>(0.0) {};
      doublefixed(const double& v) : FieldTemplate<double>(v) {};
      doublefixed(const double&& v) : FieldTemplate<double>(v) {};

      ~doublefixed() override = default;
      
      Error serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const final;
      Error serialize(WriteBufferInterface& buffer) const final;
      Error deserialize(ReadBufferInterface& buffer) final; 
  };

} // End of namespace EmbeddedProto.
#endif
