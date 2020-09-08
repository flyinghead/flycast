{% macro enum_macro(_enum) %}
enum {{ _enum.name }}
{
  {% for value in _enum.values() %}
  {{ value.name }} = {{ value.number }}{{ "," if not loop.last }}
  {% endfor %}
};

{% endmacro %}
{# #}
{# ------------------------------------------------------------------------------------------------------------------ #}
{# #}
{% macro oneof_init(_oneof) %}
void init_{{_oneof.name}}(const id field_id)
{
  if(id::NOT_SET != {{_oneof.which_oneof}})
  {
    // First delete the old object in the oneof.
    clear_{{_oneof.name}}();
  }

  // C++11 unions only support nontrivial members when you explicitly call the placement new statement.
  switch(field_id)
  {
    {% for field in _oneof.fields() %}
    case id::{{field.variable_id_name}}:
      {% if field.of_type_message or field.is_repeated_field or field.is_string or field.is_bytes %}
      new(&{{field.variable_full_name}}) {{field.type}};
      {{_oneof.which_oneof}} = id::{{field.variable_id_name}};
      {% endif %}
      break;
    {% endfor %}
    default:
      break;
   }
}
{% endmacro %}
{# #}
{# ------------------------------------------------------------------------------------------------------------------ #}
{% macro oneof_clear(_oneof) %}
void clear_{{_oneof.name}}()
{
  switch({{_oneof.which_oneof}})
  {
    {% for field in _oneof.fields() %}
    case id::{{field.variable_id_name}}:
      {% if field.of_type_message or field.is_repeated_field or field.is_string or field.is_bytes%}
      {{field.variable_full_name}}.~{{field.short_type}}();
      {% else %}
      {{field.variable_full_name}}.set(0);
      {% endif %}
      break;
    {% endfor %}
    default:
      break;
  }
  {{_oneof.which_oneof}} = id::NOT_SET;
}
{% endmacro %}
{# #}
{# ------------------------------------------------------------------------------------------------------------------ #}
{# #}
{% macro field_get_set_macro(_field) %}
{% if _field.is_string or _field.is_bytes %}
inline const {{_field.repeated_type}}& {{_field.name}}() const { return {{_field.variable_full_name}}; }
{% if _field.which_oneof is defined %}
inline void clear_{{_field.name}}()
{
  if(id::{{_field.variable_id_name}} == {{_field.which_oneof}})
  {
    {{_field.which_oneof}} = id::NOT_SET;
    {{_field.variable_full_name}}.~{{_field.short_type}}();
  }
}
inline {{_field.repeated_type}}& mutable_{{_field.name}}()
{
  if(id::{{_field.variable_id_name}} != {{_field.which_oneof}})
  {
    init_{{_field.oneof_name}}(id::{{_field.variable_id_name}});
  }
  return {{_field.variable_full_name}};
}
{% else %}
inline void clear_{{_field.name}}() { {{_field.variable_full_name}}.clear(); }
inline {{_field.repeated_type}}& mutable_{{_field.name}}() { return {{_field.variable_full_name}}; }
{% endif %}
{% if _field.is_string %}
inline const char* get_{{_field.name}}() const { return {{_field.variable_full_name}}.get_const(); }
{% else %}{# is bytes #}
inline const uint8_t* get_{{_field.name}}() const { return {{_field.variable_full_name}}.get_const(); }
{% endif %}
{% elif _field.is_repeated_field %}
inline const {{_field.type}}& {{_field.name}}(uint32_t index) const { return {{_field.variable_full_name}}[index]; }
{% if _field.which_oneof is defined %}
inline void clear_{{_field.name}}()
{
  if(id::{{_field.variable_id_name}} == {{_field.which_oneof}})
  {
    {{_field.which_oneof}} = id::NOT_SET;
    {{_field.variable_full_name}}.~{{_field.short_type}}();
  }
}
inline void set_{{_field.name}}(uint32_t index, const {{_field.type}}& value)
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  {{_field.variable_full_name}}.set(index, value);
}
inline void set_{{_field.name}}(uint32_t index, const {{_field.type}}&& value)
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  {{_field.variable_full_name}}.set(index, value);
}
inline void add_{{_field.name}}(const {{_field.type}}& value)
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  {{_field.variable_full_name}}.add(value);
}
inline {{_field.repeated_type}}& mutable_{{_field.name}}()
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  return {{_field.variable_full_name}};
}
{% else %}
inline void clear_{{_field.name}}() { {{_field.variable_full_name}}.clear(); }
inline void set_{{_field.name}}(uint32_t index, const {{_field.type}}& value) { {{_field.variable_full_name}}.set(index, value); }
inline void set_{{_field.name}}(uint32_t index, const {{_field.type}}&& value) { {{_field.variable_full_name}}.set(index, value); }
inline void add_{{_field.name}}(const {{_field.type}}& value) { {{_field.variable_full_name}}.add(value); }
inline {{_field.repeated_type}}& mutable_{{_field.name}}() { return {{_field.variable_full_name}}; }
{% endif %}
inline const {{_field.repeated_type}}& get_{{_field.name}}() const { return {{_field.variable_full_name}}; }
{% elif _field.of_type_message %}
inline const {{_field.type}}& {{_field.name}}() const { return {{_field.variable_full_name}}; }
{% if _field.which_oneof is defined %}
inline void clear_{{_field.name}}()
{
  if(id::{{_field.variable_id_name}} == {{_field.which_oneof}})
  {
    {{_field.which_oneof}} = id::NOT_SET;
    {{_field.variable_full_name}}.~{{_field.short_type}}();
  }
}
inline void set_{{_field.name}}(const {{_field.type}}& value)
{
  if(id::{{_field.variable_id_name}} != {{_field.which_oneof}})
  {
    init_{{_field.oneof_name}}(id::{{_field.variable_id_name}});
  }
  {{_field.variable_full_name}} = value;
}
inline void set_{{_field.name}}(const {{_field.type}}&& value)
{
  if(id::{{_field.variable_id_name}} != {{_field.which_oneof}})
  {
    init_{{_field.oneof_name}}(id::{{_field.variable_id_name}});
  }
  {{_field.variable_full_name}} = value;
}
inline {{_field.type}}& mutable_{{_field.name}}()
{
  if(id::{{_field.variable_id_name}} != {{_field.which_oneof}})
  {
    init_{{_field.oneof_name}}(id::{{_field.variable_id_name}});
  }
  return {{_field.variable_full_name}};
}
{% else %}
inline void clear_{{_field.name}}() { {{_field.variable_full_name}}.clear(); }
inline void set_{{_field.name}}(const {{_field.type}}& value) { {{_field.variable_full_name}} = value; }
inline void set_{{_field.name}}(const {{_field.type}}&& value) { {{_field.variable_full_name}} = value; }
inline {{_field.type}}& mutable_{{_field.name}}() { return {{_field.variable_full_name}}; }
{% endif %}
inline const {{_field.type}}& get_{{_field.name}}() const { return {{_field.variable_full_name}}; }
{% elif _field.of_type_enum %}
inline {{_field.type}} {{_field.name}}() const { return {{_field.variable_full_name}}; }
{% if _field.which_oneof is defined %}
inline void clear_{{_field.name}}()
{
  if(id::{{_field.variable_id_name}} == {{_field.which_oneof}})
  {
    {{_field.which_oneof}} = id::NOT_SET;
    {{_field.variable_full_name}} = static_cast<{{_field.type}}>({{_field.default_value}});
  }
}
inline void set_{{_field.name}}(const {{_field.type}}& value)
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  {{_field.variable_full_name}} = value;
}
inline void set_{{_field.name}}(const {{_field.type}}&& value)
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  {{_field.variable_full_name}} = value;
}
{% else %}
inline void clear_{{_field.name}}() { {{_field.variable_full_name}} = static_cast<{{_field.type}}>({{_field.default_value}}); }
inline void set_{{_field.name}}(const {{_field.type}}& value) { {{_field.variable_full_name}} = value; }
inline void set_{{_field.name}}(const {{_field.type}}&& value) { {{_field.variable_full_name}} = value; }
{% endif %}    inline {{_field.type}} get_{{_field.name}}() const { return {{_field.variable_full_name}}; }
{% else %}
inline {{_field.type}}::FIELD_TYPE {{_field.name}}() const { return {{_field.variable_full_name}}.get(); }
{% if _field.which_oneof is defined %}
inline void clear_{{_field.name}}()
{
  if(id::{{_field.variable_id_name}} == {{_field.which_oneof}})
  {
    {{_field.which_oneof}} = id::NOT_SET;
    {{_field.variable_full_name}}.set({{_field.default_value}});
  }
}
inline void set_{{_field.name}}(const {{_field.type}}::FIELD_TYPE& value)
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  {{_field.variable_full_name}}.set(value);
}
inline void set_{{_field.name}}(const {{_field.type}}::FIELD_TYPE&& value)
{
  {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  {{_field.variable_full_name}}.set(value);
}
{% else %}
inline void clear_{{_field.name}}() { {{_field.variable_full_name}}.set({{_field.default_value}}); }
inline void set_{{_field.name}}(const {{_field.type}}::FIELD_TYPE& value) { {{_field.variable_full_name}}.set(value); }
inline void set_{{_field.name}}(const {{_field.type}}::FIELD_TYPE&& value) { {{_field.variable_full_name}}.set(value); }
{% endif %}
inline {{_field.type}}::FIELD_TYPE get_{{_field.name}}() const { return {{_field.variable_full_name}}.get(); }
{% endif %}
{% endmacro %}
{# #}
{# ------------------------------------------------------------------------------------------------------------------ #}
{# #}
{% macro field_serialize_macro(_field) %}
{% if _field.is_repeated_field or _field.is_string or _field.is_bytes %}
if(::EmbeddedProto::Error::NO_ERRORS == return_value)
{
  return_value = {{_field.variable_full_name}}.serialize_with_id(static_cast<uint32_t>(id::{{_field.variable_id_name}}), buffer);
}
{% elif _field.of_type_message %}
if(::EmbeddedProto::Error::NO_ERRORS == return_value)
{
  return_value = {{_field.variable_full_name}}.serialize_with_id(static_cast<uint32_t>(id::{{_field.variable_id_name}}), buffer);
}
{% elif _field.of_type_enum %}
if(({{_field.default_value}} != {{_field.variable_full_name}}) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
{
  EmbeddedProto::uint32 value;
  value.set(static_cast<uint32_t>({{_field.variable_full_name}}));
  return_value = value.serialize_with_id(static_cast<uint32_t>(id::{{_field.variable_id_name}}), buffer);
}
{% else %}
if(({{_field.default_value}} != {{_field.variable_full_name}}.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
{
  return_value = {{_field.variable_full_name}}.serialize_with_id(static_cast<uint32_t>(id::{{_field.variable_id_name}}), buffer);
} {% endif %} {% endmacro %}
{# #}
{# ------------------------------------------------------------------------------------------------------------------ #}
{# #}
{% macro field_deserialize_macro(_field) %}
{% if _field.is_repeated_field %}
if(::EmbeddedProto::WireFormatter::WireType::LENGTH_DELIMITED == wire_type)
{
  return_value = {{_field.variable_full_name}}.deserialize(buffer);
}
{% else %}
if(::EmbeddedProto::WireFormatter::WireType::{{_field.wire_type}} == wire_type)
{
  {% if _field.of_type_message %}
  uint32_t size;
  return_value = ::EmbeddedProto::WireFormatter::DeserializeVarint(buffer, size);
  ::EmbeddedProto::ReadBufferSection bufferSection(buffer, size);
  if(::EmbeddedProto::Error::NO_ERRORS == return_value)
  {
    return_value = mutable_{{_field.name}}().deserialize(bufferSection);
  }
  {% elif _field.is_string or _field.is_bytes %}
  return_value = mutable_{{_field.name}}().deserialize(buffer);
  {% elif _field.of_type_enum %}
  uint32_t value;
  return_value = ::EmbeddedProto::WireFormatter::DeserializeVarint(buffer, value);
  if(::EmbeddedProto::Error::NO_ERRORS == return_value)
  {
    set_{{_field.name}}(static_cast<{{_field.type}}>(value));
  }
  {% else %}
  return_value = {{_field.variable_full_name}}.deserialize(buffer);
  {% endif %}
  {% if _field.which_oneof is defined %}
  if(::EmbeddedProto::Error::NO_ERRORS == return_value)
  {
    {{_field.which_oneof}} = id::{{_field.variable_id_name}};
  }
  {% endif %}
}
{% endif %}
else
{
  // Wire type does not match field.
  return_value = ::EmbeddedProto::Error::INVALID_WIRETYPE;
} {% endmacro %}
{# #}
{# ------------------------------------------------------------------------------------------------------------------ #}
{# #}
{% macro msg_macro(msg) %}
{% if msg.templates is defined %}
{% for template in msg.templates %}
{{"template<" if loop.first}}{{template['type']}} {{template['name']}}{{", " if not loop.last}}{{">" if loop.last}}
{% endfor %}
{% endif %}
class {{ msg.name }} final: public ::EmbeddedProto::MessageInterface
{
  public:
    {{ msg.name }}() :
    {% for field in msg.fields() %}
        {% if field.of_type_enum %}
        {{field.variable_full_name}}({{field.default_value}}){{"," if not loop.last}}
        {% else %}
        {{field.variable_full_name}}(){{"," if not loop.last}}{{"," if loop.last and msg.has_oneofs}}
        {% endif %}
    {% endfor %}
    {% for oneof in msg.oneofs() %}
        {{oneof.which_oneof}}(id::NOT_SET){{"," if not loop.last}}
    {% endfor %}
    {

    };
    ~{{ msg.name }}() override = default;

    {% for enum in msg.nested_enums() %}
    {{ enum_macro(enum) }}

    {% endfor %}
    enum class id
    {
      NOT_SET = 0,
      {% for id_set in msg.field_ids %}
      {{id_set[1]}} = {{id_set[0]}}{{ "," if not loop.last }}
      {% endfor %}
    };

    {% for field in msg.fields() %}
    {{ field_get_set_macro(field)|indent(4) }}
    {% endfor %}
    {% for oneof in msg.oneofs() %}
    id get_which_{{oneof.name}}() const { return {{oneof.which_oneof}}; }

    {% for field in oneof.fields() %}
    {{ field_get_set_macro(field)|indent(4) }}
    {% endfor %}
    {% endfor %}
    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const final
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      {% for field in msg.fields() %}
      {{ field_serialize_macro(field)|indent(6) }}

      {% endfor %}
      {% for oneof in msg.oneofs() %}
      switch({{oneof.which_oneof}})
      {
        {% for field in oneof.fields() %}
        case id::{{field.variable_id_name}}:
          {{ field_serialize_macro(field)|indent(12) }}
          break;

        {% endfor %}
        default:
          break;
      }

      {% endfor %}
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
          {% for field in msg.fields() %}
          case static_cast<uint32_t>(id::{{field.variable_id_name}}):
          {
            {{ field_deserialize_macro(field)|indent(12) }}
            break;
          }

          {% endfor %}
          {% for oneof in msg.oneofs() %}
          {% for field in oneof.fields() %}
          case static_cast<uint32_t>(id::{{field.variable_id_name}}):
          {
            {{ field_deserialize_macro(field)|indent(12) }}
            break;
          }

          {% endfor %}
          {% endfor %}
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
      {% for field in msg.fields() %}
      clear_{{field.name}}();
      {% endfor %}
      {% for oneof in msg.oneofs() %}
      clear_{{oneof.name}}();
      {% endfor %}
    }

  private:

    {% for field in msg.fields() %}
    {% if field.is_repeated_field or field.is_string or field.is_bytes %}
    {{field.repeated_type}} {{field.variable_name}};
    {% else %}
    {{field.type}} {{field.variable_name}};
    {% endif %}
    {% endfor %}

    {% for oneof in msg.oneofs() %}
    id {{oneof.which_oneof}};
    union {{oneof.name}}
    {
      {{oneof.name}}() {}
      ~{{oneof.name}}() {}
      {% for field in oneof.fields() %}
      {% if field.is_repeated_field or field.is_string or field.is_bytes %}
      {{field.repeated_type}} {{field.variable_name}};
      {% else %}
      {{field.type}} {{field.variable_name}};
      {% endif %}
      {% endfor %}
    };
    {{oneof.name}} {{oneof.name}}_;

    {{ oneof_init(oneof)|indent(4) }}
    {{ oneof_clear(oneof)|indent(4) }}
    {% endfor %}
};
{% endmacro %}
{# #}
{# ------------------------------------------------------------------------------------------------------------------ #}
{# #}
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
#ifndef _{{filename.upper()}}_H_
#define _{{filename.upper()}}_H_

#include <cstdint>
{% if messages %}
#include <MessageInterface.h>
#include <WireFormatter.h>
#include <Fields.h>
#include <MessageSizeCalculator.h>
#include <ReadBufferSection.h>
#include <RepeatedFieldFixedSize.h>
#include <FieldStringBytes.h>
#include <Errors.h>
{% endif %}
{% if dependencies %}

// Include external proto definitions
{% for dependency in dependencies %}
#include <{{dependency}}>
{% endfor %}
{% endif %}
{% if namespace %}

namespace {{ namespace }}
{
{% endif %}

{% for enum in enums %}
{{ enum_macro(enum) }}
{% endfor %}
{% for msg in messages %}
{{ msg_macro(msg) }}
{% endfor %}
{% if namespace %}
} // End of namespace {{ namespace }}
{% endif %}
#endif // _{{filename.upper()}}_H_

