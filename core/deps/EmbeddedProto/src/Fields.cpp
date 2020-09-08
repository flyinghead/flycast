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

#include "Fields.h"
#include "MessageSizeCalculator.h"

namespace EmbeddedProto 
{
  uint32_t Field::serialized_size() const
  {
    ::EmbeddedProto::MessageSizeCalculator calcBuffer;
    this->serialize(calcBuffer);
    return calcBuffer.get_size();
  }

  Error int32::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::VARINT), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error int64::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::VARINT), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error uint32::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::VARINT), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error uint64::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::VARINT), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error sint32::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::VARINT), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error sint64::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::VARINT), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error boolean::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::VARINT), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error fixed32::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::FIXED32), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error fixed64::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::FIXED64), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error sfixed32::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::FIXED32), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error sfixed64::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::FIXED64), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error floatfixed::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::FIXED32), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }

  Error doublefixed::serialize_with_id(uint32_t field_number, WriteBufferInterface& buffer) const
  { 
    Error return_value = WireFormatter::SerializeVarint(WireFormatter::MakeTag(field_number, WireFormatter::WireType::FIXED64), buffer);
    if(Error::NO_ERRORS == return_value)
    {
      return_value = serialize(buffer);
    }
    return return_value;
  }



  Error int32::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerializeVarint(static_cast<uint32_t>(get()), buffer);
  }

  Error int64::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerializeVarint(static_cast<uint64_t>(get()), buffer);
  }

  Error uint32::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerializeVarint(get(), buffer);
  }

  Error uint64::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerializeVarint(get(), buffer);
  }

  Error sint32::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerializeVarint(WireFormatter::ZigZagEncode(get()), buffer);
  }

  Error sint64::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerializeVarint(WireFormatter::ZigZagEncode(get()), buffer);
  }

  Error boolean::serialize(WriteBufferInterface& buffer) const
  {
    const uint8_t byte = get() ? 0x01 : 0x00;
    return buffer.push(byte) ? Error::NO_ERRORS : Error::BUFFER_FULL;
  }

  Error fixed32::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerialzieFixedNoTag(get(), buffer);
  }

  Error fixed64::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerialzieFixedNoTag(get(), buffer);
  }

  Error sfixed32::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerialzieSFixedNoTag(get(), buffer);
  }

  Error sfixed64::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerialzieSFixedNoTag(get(), buffer);
  }

  Error floatfixed::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerialzieFloatNoTag(get(), buffer);
  }

  Error doublefixed::serialize(WriteBufferInterface& buffer) const
  {
    return WireFormatter::SerialzieDoubleNoTag(get(), buffer);
  }



  Error int32::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeInt(buffer, get());
  }

  Error int64::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeInt(buffer, get());
  }

  Error uint32::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeUInt(buffer, get());
  }

  Error uint64::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeUInt(buffer, get());
  }

  Error sint32::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeSInt(buffer, get());
  }

  Error sint64::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeSInt(buffer, get());
  }

  Error boolean::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeBool(buffer, get());
  }

  Error fixed32::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeFixed(buffer, get());
  }

  Error fixed64::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeFixed(buffer, get());
  }

  Error sfixed32::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeSFixed(buffer, get());
  }

  Error sfixed64::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeSFixed(buffer, get());
  }

  Error floatfixed::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeFloat(buffer, get());
  }

  Error doublefixed::deserialize(ReadBufferInterface& buffer) 
  { 
    return WireFormatter::DeserializeDouble(buffer, get());
  }
  
} // End of namespace EmbeddedProto
