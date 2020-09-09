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

// This file is generated. Please do not edit!
#ifndef _PACKET_H_
#define _PACKET_H_

#include <cstdint>
#include <MessageInterface.h>
#include <WireFormatter.h>
#include <Fields.h>
#include <MessageSizeCalculator.h>
#include <ReadBufferSection.h>
#include <RepeatedFieldFixedSize.h>
#include <FieldStringBytes.h>
#include <Errors.h>

namespace proto
{

enum MessageType
{
  None = 0,
  HelloServer = 1,
  Ping = 2,
  Pong = 3,
  Battle = 4
};


template<uint32_t user_id_LENGTH, 
uint32_t body_LENGTH>
class BattleMessage final: public ::EmbeddedProto::MessageInterface
{
  public:
    BattleMessage() :
        user_id_(),
        seq_(),
        body_()
    {

    };
    ~BattleMessage() override = default;

    enum class id
    {
      NOT_SET = 0,
      USER_ID = 1,
      SEQ = 2,
      BODY = 3
    };

    inline const ::EmbeddedProto::FieldString<user_id_LENGTH>& user_id() const { return user_id_; }
    inline void clear_user_id() { user_id_.clear(); }
    inline ::EmbeddedProto::FieldString<user_id_LENGTH>& mutable_user_id() { return user_id_; }
    inline const char* get_user_id() const { return user_id_.get_const(); }

    inline EmbeddedProto::uint32::FIELD_TYPE seq() const { return seq_.get(); }
    inline void clear_seq() { seq_.set(0U); }
    inline void set_seq(const EmbeddedProto::uint32::FIELD_TYPE& value) { seq_.set(value); }
    inline void set_seq(const EmbeddedProto::uint32::FIELD_TYPE&& value) { seq_.set(value); }
    inline EmbeddedProto::uint32::FIELD_TYPE get_seq() const { return seq_.get(); }

    inline const ::EmbeddedProto::FieldBytes<body_LENGTH>& body() const { return body_; }
    inline void clear_body() { body_.clear(); }
    inline ::EmbeddedProto::FieldBytes<body_LENGTH>& mutable_body() { return body_; }
    inline const uint8_t* get_body() const { return body_.get_const(); }

    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = user_id_.serialize_with_id(static_cast<uint32_t>(id::USER_ID), buffer);
      }
       

      if((0U != seq_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = seq_.serialize_with_id(static_cast<uint32_t>(id::SEQ), buffer);
      }  

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = body_.serialize_with_id(static_cast<uint32_t>(id::BODY), buffer);
      }
       

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type;
      uint32_t id_number = 0;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        switch(id_number)
        {
          case static_cast<uint32_t>(id::USER_ID):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              return_value = mutable_user_id().deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::SEQ):
          {
            if(::EmbeddedProto::WireFormatter::WireType::VARINT == wire_type)
            {
              return_value = seq_.deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::BODY):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              return_value = mutable_body().deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          default:
            break;
        }
        
        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
            // Read the next tag.
            tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() final
    {
      clear_user_id();
      clear_seq();
      clear_body();
    }

  private:

    ::EmbeddedProto::FieldString<user_id_LENGTH> user_id_;
    EmbeddedProto::uint32 seq_;
    ::EmbeddedProto::FieldBytes<body_LENGTH> body_;

};

template<uint32_t user_id_LENGTH>
class PingMessage final: public ::EmbeddedProto::MessageInterface
{
  public:
    PingMessage() :
        timestamp_(),
        user_id_()
    {

    };
    ~PingMessage() override = default;

    enum class id
    {
      NOT_SET = 0,
      TIMESTAMP = 1,
      USER_ID = 2
    };

    inline EmbeddedProto::int64::FIELD_TYPE timestamp() const { return timestamp_.get(); }
    inline void clear_timestamp() { timestamp_.set(0); }
    inline void set_timestamp(const EmbeddedProto::int64::FIELD_TYPE& value) { timestamp_.set(value); }
    inline void set_timestamp(const EmbeddedProto::int64::FIELD_TYPE&& value) { timestamp_.set(value); }
    inline EmbeddedProto::int64::FIELD_TYPE get_timestamp() const { return timestamp_.get(); }

    inline const ::EmbeddedProto::FieldString<user_id_LENGTH>& user_id() const { return user_id_; }
    inline void clear_user_id() { user_id_.clear(); }
    inline ::EmbeddedProto::FieldString<user_id_LENGTH>& mutable_user_id() { return user_id_; }
    inline const char* get_user_id() const { return user_id_.get_const(); }

    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0 != timestamp_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = timestamp_.serialize_with_id(static_cast<uint32_t>(id::TIMESTAMP), buffer);
      }  

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = user_id_.serialize_with_id(static_cast<uint32_t>(id::USER_ID), buffer);
      }
       

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type;
      uint32_t id_number = 0;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        switch(id_number)
        {
          case static_cast<uint32_t>(id::TIMESTAMP):
          {
            if(::EmbeddedProto::WireFormatter::WireType::VARINT == wire_type)
            {
              return_value = timestamp_.deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::USER_ID):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              return_value = mutable_user_id().deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          default:
            break;
        }
        
        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
            // Read the next tag.
            tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() final
    {
      clear_timestamp();
      clear_user_id();
    }

  private:

    EmbeddedProto::int64 timestamp_;
    ::EmbeddedProto::FieldString<user_id_LENGTH> user_id_;

};

template<uint32_t user_id_LENGTH, 
uint32_t public_addr_LENGTH>
class PongMessage final: public ::EmbeddedProto::MessageInterface
{
  public:
    PongMessage() :
        timestamp_(),
        user_id_(),
        public_addr_()
    {

    };
    ~PongMessage() override = default;

    enum class id
    {
      NOT_SET = 0,
      TIMESTAMP = 1,
      USER_ID = 2,
      PUBLIC_ADDR = 3
    };

    inline EmbeddedProto::int64::FIELD_TYPE timestamp() const { return timestamp_.get(); }
    inline void clear_timestamp() { timestamp_.set(0); }
    inline void set_timestamp(const EmbeddedProto::int64::FIELD_TYPE& value) { timestamp_.set(value); }
    inline void set_timestamp(const EmbeddedProto::int64::FIELD_TYPE&& value) { timestamp_.set(value); }
    inline EmbeddedProto::int64::FIELD_TYPE get_timestamp() const { return timestamp_.get(); }

    inline const ::EmbeddedProto::FieldString<user_id_LENGTH>& user_id() const { return user_id_; }
    inline void clear_user_id() { user_id_.clear(); }
    inline ::EmbeddedProto::FieldString<user_id_LENGTH>& mutable_user_id() { return user_id_; }
    inline const char* get_user_id() const { return user_id_.get_const(); }

    inline const ::EmbeddedProto::FieldString<public_addr_LENGTH>& public_addr() const { return public_addr_; }
    inline void clear_public_addr() { public_addr_.clear(); }
    inline ::EmbeddedProto::FieldString<public_addr_LENGTH>& mutable_public_addr() { return public_addr_; }
    inline const char* get_public_addr() const { return public_addr_.get_const(); }

    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0 != timestamp_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = timestamp_.serialize_with_id(static_cast<uint32_t>(id::TIMESTAMP), buffer);
      }  

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = user_id_.serialize_with_id(static_cast<uint32_t>(id::USER_ID), buffer);
      }
       

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = public_addr_.serialize_with_id(static_cast<uint32_t>(id::PUBLIC_ADDR), buffer);
      }
       

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type;
      uint32_t id_number = 0;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        switch(id_number)
        {
          case static_cast<uint32_t>(id::TIMESTAMP):
          {
            if(::EmbeddedProto::WireFormatter::WireType::VARINT == wire_type)
            {
              return_value = timestamp_.deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::USER_ID):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              return_value = mutable_user_id().deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::PUBLIC_ADDR):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              return_value = mutable_public_addr().deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          default:
            break;
        }
        
        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
            // Read the next tag.
            tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() final
    {
      clear_timestamp();
      clear_user_id();
      clear_public_addr();
    }

  private:

    EmbeddedProto::int64 timestamp_;
    ::EmbeddedProto::FieldString<user_id_LENGTH> user_id_;
    ::EmbeddedProto::FieldString<public_addr_LENGTH> public_addr_;

};

template<uint32_t session_id_LENGTH>
class HelloServerMessage final: public ::EmbeddedProto::MessageInterface
{
  public:
    HelloServerMessage() :
        session_id_(),
        ok_()
    {

    };
    ~HelloServerMessage() override = default;

    enum class id
    {
      NOT_SET = 0,
      SESSION_ID = 1,
      OK = 2
    };

    inline const ::EmbeddedProto::FieldString<session_id_LENGTH>& session_id() const { return session_id_; }
    inline void clear_session_id() { session_id_.clear(); }
    inline ::EmbeddedProto::FieldString<session_id_LENGTH>& mutable_session_id() { return session_id_; }
    inline const char* get_session_id() const { return session_id_.get_const(); }

    inline EmbeddedProto::boolean::FIELD_TYPE ok() const { return ok_.get(); }
    inline void clear_ok() { ok_.set(false); }
    inline void set_ok(const EmbeddedProto::boolean::FIELD_TYPE& value) { ok_.set(value); }
    inline void set_ok(const EmbeddedProto::boolean::FIELD_TYPE&& value) { ok_.set(value); }
    inline EmbeddedProto::boolean::FIELD_TYPE get_ok() const { return ok_.get(); }

    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = session_id_.serialize_with_id(static_cast<uint32_t>(id::SESSION_ID), buffer);
      }
       

      if((false != ok_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = ok_.serialize_with_id(static_cast<uint32_t>(id::OK), buffer);
      }  

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type;
      uint32_t id_number = 0;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        switch(id_number)
        {
          case static_cast<uint32_t>(id::SESSION_ID):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              return_value = mutable_session_id().deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::OK):
          {
            if(::EmbeddedProto::WireFormatter::WireType::VARINT == wire_type)
            {
              return_value = ok_.deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          default:
            break;
        }
        
        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
            // Read the next tag.
            tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() final
    {
      clear_session_id();
      clear_ok();
    }

  private:

    ::EmbeddedProto::FieldString<session_id_LENGTH> session_id_;
    EmbeddedProto::boolean ok_;

};

template<uint32_t battle_data_LENGTH,
        uint32_t user_id_LENGTH,
        uint32_t body_LENGTH,
        uint32_t session_id_LENGTH,
        uint32_t public_addr_LENGTH>
class Packet final: public ::EmbeddedProto::MessageInterface
{
  public:
    Packet() :
        type_(static_cast<proto::MessageType>(0)),
        seq_(),
        ack_(),
        hello_server_data_(),
        ping_data_(),
        pong_data_(),
        battle_data_()
    {

    };
    ~Packet() override = default;

    enum class id
    {
      NOT_SET = 0,
      TYPE = 1,
      SEQ = 2,
      ACK = 3,
      HELLO_SERVER_DATA = 10,
      PING_DATA = 11,
      PONG_DATA = 12,
      BATTLE_DATA = 13
    };

    inline proto::MessageType type() const { return type_; }
    inline void clear_type() { type_ = static_cast<proto::MessageType>(static_cast<proto::MessageType>(0)); }
    inline void set_type(const proto::MessageType& value) { type_ = value; }
    inline void set_type(const proto::MessageType&& value) { type_ = value; }
        inline proto::MessageType get_type() const { return type_; }

    inline EmbeddedProto::uint32::FIELD_TYPE seq() const { return seq_.get(); }
    inline void clear_seq() { seq_.set(0U); }
    inline void set_seq(const EmbeddedProto::uint32::FIELD_TYPE& value) { seq_.set(value); }
    inline void set_seq(const EmbeddedProto::uint32::FIELD_TYPE&& value) { seq_.set(value); }
    inline EmbeddedProto::uint32::FIELD_TYPE get_seq() const { return seq_.get(); }

    inline EmbeddedProto::uint32::FIELD_TYPE ack() const { return ack_.get(); }
    inline void clear_ack() { ack_.set(0U); }
    inline void set_ack(const EmbeddedProto::uint32::FIELD_TYPE& value) { ack_.set(value); }
    inline void set_ack(const EmbeddedProto::uint32::FIELD_TYPE&& value) { ack_.set(value); }
    inline EmbeddedProto::uint32::FIELD_TYPE get_ack() const { return ack_.get(); }

    inline const proto::HelloServerMessage<session_id_LENGTH>& hello_server_data() const { return hello_server_data_; }
    inline void clear_hello_server_data() { hello_server_data_.clear(); }
    inline void set_hello_server_data(const proto::HelloServerMessage<session_id_LENGTH>& value) { hello_server_data_ = value; }
    inline void set_hello_server_data(const proto::HelloServerMessage<session_id_LENGTH>&& value) { hello_server_data_ = value; }
    inline proto::HelloServerMessage<session_id_LENGTH>& mutable_hello_server_data() { return hello_server_data_; }
    inline const proto::HelloServerMessage<session_id_LENGTH>& get_hello_server_data() const { return hello_server_data_; }

    inline const proto::PingMessage<user_id_LENGTH>& ping_data() const { return ping_data_; }
    inline void clear_ping_data() { ping_data_.clear(); }
    inline void set_ping_data(const proto::PingMessage<user_id_LENGTH>& value) { ping_data_ = value; }
    inline void set_ping_data(const proto::PingMessage<user_id_LENGTH>&& value) { ping_data_ = value; }
    inline proto::PingMessage<user_id_LENGTH>& mutable_ping_data() { return ping_data_; }
    inline const proto::PingMessage<user_id_LENGTH>& get_ping_data() const { return ping_data_; }

    inline const proto::PongMessage<user_id_LENGTH, public_addr_LENGTH>& pong_data() const { return pong_data_; }
    inline void clear_pong_data() { pong_data_.clear(); }
    inline void set_pong_data(const proto::PongMessage<user_id_LENGTH, public_addr_LENGTH>& value) { pong_data_ = value; }
    inline void set_pong_data(const proto::PongMessage<user_id_LENGTH, public_addr_LENGTH>&& value) { pong_data_ = value; }
    inline proto::PongMessage<user_id_LENGTH, public_addr_LENGTH>& mutable_pong_data() { return pong_data_; }
    inline const proto::PongMessage<user_id_LENGTH, public_addr_LENGTH>& get_pong_data() const { return pong_data_; }

    inline const proto::BattleMessage<user_id_LENGTH, body_LENGTH>& battle_data(uint32_t index) const { return battle_data_[index]; }
    inline void clear_battle_data() { battle_data_.clear(); }
    inline void set_battle_data(uint32_t index, const proto::BattleMessage<user_id_LENGTH, body_LENGTH>& value) { battle_data_.set(index, value); }
    inline void set_battle_data(uint32_t index, const proto::BattleMessage<user_id_LENGTH, body_LENGTH>&& value) { battle_data_.set(index, value); }
    inline void add_battle_data(const proto::BattleMessage<user_id_LENGTH, body_LENGTH>& value) { battle_data_.add(value); }
    inline ::EmbeddedProto::RepeatedFieldFixedSize<proto::BattleMessage<user_id_LENGTH, body_LENGTH>, battle_data_LENGTH>& mutable_battle_data() { return battle_data_; }
    inline const ::EmbeddedProto::RepeatedFieldFixedSize<proto::BattleMessage<user_id_LENGTH, body_LENGTH>, battle_data_LENGTH>& get_battle_data() const { return battle_data_; }

    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((static_cast<proto::MessageType>(0) != type_) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        EmbeddedProto::uint32 value;
        value.set(static_cast<uint32_t>(type_));
        return_value = value.serialize_with_id(static_cast<uint32_t>(id::TYPE), buffer);
      }
       

      if((0U != seq_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = seq_.serialize_with_id(static_cast<uint32_t>(id::SEQ), buffer);
      }  

      if((0U != ack_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = ack_.serialize_with_id(static_cast<uint32_t>(id::ACK), buffer);
      }  

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = hello_server_data_.serialize_with_id(static_cast<uint32_t>(id::HELLO_SERVER_DATA), buffer);
      }
       

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = ping_data_.serialize_with_id(static_cast<uint32_t>(id::PING_DATA), buffer);
      }
       

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = pong_data_.serialize_with_id(static_cast<uint32_t>(id::PONG_DATA), buffer);
      }
       

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = battle_data_.serialize_with_id(static_cast<uint32_t>(id::BATTLE_DATA), buffer);
      }
       

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type;
      uint32_t id_number = 0;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        switch(id_number)
        {
          case static_cast<uint32_t>(id::TYPE):
          {
            if(::EmbeddedProto::WireFormatter::WireType::VARINT == wire_type)
            {
              uint32_t value;
              return_value = ::EmbeddedProto::WireFormatter::DeserializeVarint(buffer, value);
              if(::EmbeddedProto::Error::NO_ERRORS == return_value)
              {
                set_type(static_cast<proto::MessageType>(value));
              }
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::SEQ):
          {
            if(::EmbeddedProto::WireFormatter::WireType::VARINT == wire_type)
            {
              return_value = seq_.deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::ACK):
          {
            if(::EmbeddedProto::WireFormatter::WireType::VARINT == wire_type)
            {
              return_value = ack_.deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::HELLO_SERVER_DATA):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              uint32_t size;
              return_value = ::EmbeddedProto::WireFormatter::DeserializeVarint(buffer, size);
              ::EmbeddedProto::ReadBufferSection bufferSection(buffer, size);
              if(::EmbeddedProto::Error::NO_ERRORS == return_value)
              {
                return_value = mutable_hello_server_data().deserialize(bufferSection);
              }
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::PING_DATA):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              uint32_t size;
              return_value = ::EmbeddedProto::WireFormatter::DeserializeVarint(buffer, size);
              ::EmbeddedProto::ReadBufferSection bufferSection(buffer, size);
              if(::EmbeddedProto::Error::NO_ERRORS == return_value)
              {
                return_value = mutable_ping_data().deserialize(bufferSection);
              }
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::PONG_DATA):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              uint32_t size;
              return_value = ::EmbeddedProto::WireFormatter::DeserializeVarint(buffer, size);
              ::EmbeddedProto::ReadBufferSection bufferSection(buffer, size);
              if(::EmbeddedProto::Error::NO_ERRORS == return_value)
              {
                return_value = mutable_pong_data().deserialize(bufferSection);
              }
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          case static_cast<uint32_t>(id::BATTLE_DATA):
          {
            if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
            {
              return_value = battle_data_.deserialize(buffer);
            }
            else
            {
              // Wire type does not match field.
              return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
            } 
            break;
          }

          default:
            break;
        }
        
        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
            // Read the next tag.
            tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() final
    {
      clear_type();
      clear_seq();
      clear_ack();
      clear_hello_server_data();
      clear_ping_data();
      clear_pong_data();
      clear_battle_data();
    }

  private:

    proto::MessageType type_;
    EmbeddedProto::uint32 seq_;
    EmbeddedProto::uint32 ack_;
    proto::HelloServerMessage<session_id_LENGTH> hello_server_data_;
    proto::PingMessage<user_id_LENGTH> ping_data_;
    proto::PongMessage<user_id_LENGTH, public_addr_LENGTH> pong_data_;
    ::EmbeddedProto::RepeatedFieldFixedSize<proto::BattleMessage<user_id_LENGTH, body_LENGTH>, battle_data_LENGTH> battle_data_;
};

} // End of namespace proto
#endif // _PACKET_H_
