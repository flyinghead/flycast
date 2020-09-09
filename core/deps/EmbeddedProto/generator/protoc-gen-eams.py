#
# Copyright (C) 2020 Embedded AMS B.V. - All Rights Reserved
#
# This file is part of Embedded Proto.
#
# Embedded Proto is open source software: you can redistribute it and/or 
# modify it under the terms of the GNU General Public License as published 
# by the Free Software Foundation, version 3 of the license.
#
# Embedded Proto  is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Embedded Proto. If not, see <https://www.gnu.org/licenses/>.
#
# For commercial and closed source application please visit:
# <https://EmbeddedProto.com/license/>.
#
# Embedded AMS B.V.
# Info:
#   info at EmbeddedProto dot com
#
# Postal address:
#   Johan Huizingalaan 763a
#   1066 VH, Amsterdam
#   the Netherlands
#

import io
import sys
import os
import jinja2
from copy import deepcopy

from google.protobuf.compiler import plugin_pb2 as plugin
from google.protobuf.descriptor_pb2 import DescriptorProto, FieldDescriptorProto, EnumDescriptorProto

# -----------------------------------------------------------------------------


class EnumTemplateParameters:
    def __init__(self, enum_proto):
        self.name = enum_proto.name
        self.enum_proto = enum_proto

    def values(self):
        for value in self.enum_proto.value:
            yield value


def generate_enums(enums):
    for enum in enums:
        yield EnumTemplateParameters(enum)

# -----------------------------------------------------------------------------


class FieldTemplateParameters:
    type_to_default_value = {FieldDescriptorProto.TYPE_DOUBLE:   "0.0",
                             FieldDescriptorProto.TYPE_FLOAT:    "0.0",
                             FieldDescriptorProto.TYPE_INT64:    "0",
                             FieldDescriptorProto.TYPE_UINT64:   "0U",
                             FieldDescriptorProto.TYPE_INT32:    "0",
                             FieldDescriptorProto.TYPE_FIXED64:  "0U",
                             FieldDescriptorProto.TYPE_FIXED32:  "0U",
                             FieldDescriptorProto.TYPE_BOOL:     "false",
                             FieldDescriptorProto.TYPE_STRING:   "''",
                             FieldDescriptorProto.TYPE_MESSAGE:  "",
                             FieldDescriptorProto.TYPE_BYTES:    "0U",
                             FieldDescriptorProto.TYPE_UINT32:   "0U",
                             FieldDescriptorProto.TYPE_ENUM:     "0",
                             FieldDescriptorProto.TYPE_SFIXED32: "0",
                             FieldDescriptorProto.TYPE_SFIXED64: "0",
                             FieldDescriptorProto.TYPE_SINT32:   "0",
                             FieldDescriptorProto.TYPE_SINT64:   "0"}

    type_to_cpp_type = {FieldDescriptorProto.TYPE_DOUBLE:   "EmbeddedProto::doublefixed",
                        FieldDescriptorProto.TYPE_FLOAT:    "EmbeddedProto::floatfixed",
                        FieldDescriptorProto.TYPE_INT64:    "EmbeddedProto::int64",
                        FieldDescriptorProto.TYPE_UINT64:   "EmbeddedProto::uint64",
                        FieldDescriptorProto.TYPE_INT32:    "EmbeddedProto::int32",
                        FieldDescriptorProto.TYPE_FIXED64:  "EmbeddedProto::fixed64",
                        FieldDescriptorProto.TYPE_FIXED32:  "EmbeddedProto::fixed32",
                        FieldDescriptorProto.TYPE_BOOL:     "EmbeddedProto::boolean",
                        FieldDescriptorProto.TYPE_STRING:   "char",
                        FieldDescriptorProto.TYPE_BYTES:    "uint8_t",
                        FieldDescriptorProto.TYPE_UINT32:   "EmbeddedProto::uint32",
                        FieldDescriptorProto.TYPE_SFIXED32: "EmbeddedProto::sfixed32",
                        FieldDescriptorProto.TYPE_SFIXED64: "EmbeddedProto::sfixed64",
                        FieldDescriptorProto.TYPE_SINT32:   "EmbeddedProto::sint32",
                        FieldDescriptorProto.TYPE_SINT64:   "EmbeddedProto::sint64"}

    type_to_wire_type = {FieldDescriptorProto.TYPE_INT32:    "VARINT",
                         FieldDescriptorProto.TYPE_INT64:    "VARINT",
                         FieldDescriptorProto.TYPE_UINT32:   "VARINT",
                         FieldDescriptorProto.TYPE_UINT64:   "VARINT",
                         FieldDescriptorProto.TYPE_SINT32:   "VARINT",
                         FieldDescriptorProto.TYPE_SINT64:   "VARINT",
                         FieldDescriptorProto.TYPE_BOOL:     "VARINT",
                         FieldDescriptorProto.TYPE_ENUM:     "VARINT",
                         FieldDescriptorProto.TYPE_FIXED64:  "FIXED64",
                         FieldDescriptorProto.TYPE_SFIXED64: "FIXED64",
                         FieldDescriptorProto.TYPE_DOUBLE:   "FIXED64",
                         FieldDescriptorProto.TYPE_STRING:   "LENGTH_DELIMITED",
                         FieldDescriptorProto.TYPE_BYTES:    "LENGTH_DELIMITED",
                         FieldDescriptorProto.TYPE_MESSAGE:  "LENGTH_DELIMITED",
                         FieldDescriptorProto.TYPE_FIXED32:  "FIXED32",
                         FieldDescriptorProto.TYPE_FLOAT:    "FIXED32",
                         FieldDescriptorProto.TYPE_SFIXED32: "FIXED32"}

    def __init__(self, field_proto, oneof=None):
        self.name = field_proto.name
        self.variable_name = self.name + "_"
        self.variable_id_name = self.name.upper()
        self.variable_id = field_proto.number

        if oneof:
            # When set this field is part of a oneof.
            self.oneof_name = oneof
            self.which_oneof = "which_" + oneof + "_"
            self.variable_full_name = oneof + "_." + self.variable_name
        else:
            self.variable_full_name = self.variable_name

        self.of_type_message = FieldDescriptorProto.TYPE_MESSAGE == field_proto.type
        self.wire_type = self.type_to_wire_type[field_proto.type]

        self.of_type_enum = FieldDescriptorProto.TYPE_ENUM == field_proto.type
        self.is_repeated_field = FieldDescriptorProto.LABEL_REPEATED == field_proto.label
        self.is_string = FieldDescriptorProto.TYPE_STRING == field_proto.type
        self.is_bytes = FieldDescriptorProto.TYPE_BYTES == field_proto.type

        if FieldDescriptorProto.TYPE_MESSAGE == field_proto.type or self.of_type_enum:
            self.type = field_proto.type_name if "." != field_proto.type_name[0] else field_proto.type_name[1:]
            self.type = self.type.replace(".", "::")
            # Store only the type without namespace or class scopes
            self.short_type = self.type.split("::")[-1]
        elif self.is_string:
            self.type = "::EmbeddedProto::FieldString"
            self.short_type = "FieldString"
        elif self.is_bytes:
            self.type = "::EmbeddedProto::FieldBytes"
            self.short_type = "FieldBytes"
        else:
            self.type = self.type_to_cpp_type[field_proto.type]

        self.default_value = None
        self.repeated_type = None
        self.templates = []

        self.field_proto = field_proto

    def update_templates(self, messages):
        if self.of_type_message:
            for msg in messages:
                if msg.name == self.type:
                    msg_templates = deepcopy(msg.templates)
                    for tmpl in msg_templates:
                        tmpl["name"] = self.variable_name + tmpl["name"]
                    self.templates.extend(msg_templates)

                    if self.templates:
                        self.type += "<"
                        for tmpl in self.templates[:-1]:
                            self.type += tmpl["name"] + ", "
                        self.type += self.templates[-1]["name"] + ">"

                    break

        if self.of_type_enum:
            self.default_value = "static_cast<" + self.type + ">(0)"
        else:
            self.default_value = self.type_to_default_value[self.field_proto.type]

        if self.is_repeated_field:
            self.repeated_type = "::EmbeddedProto::RepeatedFieldFixedSize<" + self.type + ", " + self.variable_name \
                                 + "LENGTH>"

        if self. is_string or self.is_bytes:
            self.repeated_type = self.type + "<" + self.variable_name + "LENGTH>"
            self.type = self.repeated_type

        if self.is_repeated_field or self.is_string or self.is_bytes:
            self.templates.append({"type": "uint32_t", "name": self.variable_name + "LENGTH"})


# -----------------------------------------------------------------------------


class OneofTemplateParameters:
    def __init__(self, name, index, msg_proto):
        self.name = name
        self.which_oneof = "which_" + name + "_"
        self.index = index
        self.msg_proto = msg_proto

        self.fields_array = []
        # Loop over all the fields in this oneof
        for f in self.msg_proto.field:
            if f.HasField('oneof_index') and self.index == f.oneof_index:
                self.fields_array.append(FieldTemplateParameters(f, self.name))

    def fields(self):
        for f in self.fields_array:
            yield f

    def update_templates(self, messages):
        for f in self.fields_array:
            f.update_templates(messages)

# -----------------------------------------------------------------------------


class MessageTemplateParameters:
    def __init__(self, msg_proto):
        self.name = msg_proto.name
        self.msg_proto = msg_proto
        self.has_fields = len(self.msg_proto.field) > 0
        self.has_oneofs = len(self.msg_proto.oneof_decl) > 0

        self.fields_array = []
        # Loop over only the normal fields in this message.
        for f in self.msg_proto.field:
            if not f.HasField('oneof_index'):
                self.fields_array.append(FieldTemplateParameters(f))

        self.oneof_fields = []
        # Loop over all the oneofs in this message.
        for index, oneof in enumerate(self.msg_proto.oneof_decl):
            self.oneof_fields.append(OneofTemplateParameters(oneof.name, index, self.msg_proto))

        self.templates = []
        self.field_ids = []

        for field in self.fields_array:
            self.field_ids.append((field.variable_id, field.variable_id_name))

        for oneof in self.oneof_fields:
            for field in oneof.fields():
                self.field_ids.append((field.variable_id, field.variable_id_name))

        # Sort the field id's such they will appear in order in the id enum.
        self.field_ids.sort()

    def fields(self):
        for f in self.fields_array:
            yield f

    def oneofs(self):
        for o in self.oneof_fields:
            yield o

    def nested_enums(self):
        # Yield all the enumerations defined in the scope of this message.
        for enum in self.msg_proto.enum_type:
            yield EnumTemplateParameters(enum)

    def update_templates(self, messages):
        for field in self.fields_array:
            field.update_templates(messages)

            self.templates.extend(field.templates)

        for oneof in self.oneof_fields:
            for field in oneof.fields():
                field.update_templates(messages)

                self.templates.extend(field.templates)

# -----------------------------------------------------------------------------


def generate_code(request, respones):
    # Based upon the request from protoc generate

    filepath = os.path.dirname(os.path.abspath(__file__))
    template_loader = jinja2.FileSystemLoader(searchpath=filepath)
    template_env = jinja2.Environment(loader=template_loader, trim_blocks=True, lstrip_blocks=True)
    template_file = "Header_Template.h"
    template = template_env.get_template(template_file)

    messages_array = []
    number_of_processed_msg = 0

    # Loop over all proto files in the request
    for proto_file in request.proto_file:

        if "proto2" == proto_file.syntax:
            raise Exception(proto_file.name + ": Sorry, proto2 is not supported, please use proto3.")

        for msg_type in proto_file.message_type:
            msg = MessageTemplateParameters(msg_type)
            msg.update_templates(messages_array)
            messages_array.append(msg)

        enums_generator = generate_enums(proto_file.enum_type)

        filename_str = os.path.splitext(proto_file.name)[0]

        imported_dependencies = []
        if proto_file.dependency:
            imported_dependencies = [os.path.splitext(dependency)[0] + ".h" for dependency in proto_file.dependency]

        namespace_name = ""
        if proto_file.package:
            namespace_name = proto_file.package.replace(".", "::")

        try:
            file_str = template.render(filename=filename_str, namespace=namespace_name,
                                       messages=messages_array[number_of_processed_msg:],
                                       enums=enums_generator, dependencies=imported_dependencies)

        except jinja2.TemplateError as e:
            print("TemplateError exception: " + str(e))
        except jinja2.UndefinedError as e:
            print("UndefinedError exception: " + str(e))
        except jinja2.TemplateSyntaxError as e:
            print("TemplateSyntaxError exception: " + str(e))
        except jinja2.TemplateRuntimeError as e:
            print("TemplateRuntimeError exception: " + str(e))
        except jinja2.TemplateAssertionError as e:
            print("TemplateAssertionError exception: " + str(e))
        except Exception as e:
            print("Template renderer exception: " + str(e))
        else:
            number_of_processed_msg = len(messages_array)
            f = respones.file.add()
            f.name = filename_str + ".h"
            f.content = file_str


def main_plugin():
    # The main function when running the scrip as a protoc plugin. It will read in the protoc data from the stdin and
    # write back the output to stdout.

    # Read request message from stdin
    data = io.open(sys.stdin.fileno(), "rb").read()
    request = plugin.CodeGeneratorRequest.FromString(data)

    if '--debug' in sys.argv:
        # Write the requests to a file for easy debugging.
        with open("./debug_embbeded_proto.bin", 'wb') as file:
            file.write(request.SerializeToString())

    # Create response
    response = plugin.CodeGeneratorResponse()

    # Generate code
    generate_code(request, response)

    # Serialize response message
    output = response.SerializeToString()

    # Write to stdout
    io.open(sys.stdout.fileno(), "wb").write(output)


def main_cli():
    # The main function when running from the command line and debugging.  Instead of receiving data from protoc this
    # will read in a binary file stored the previous time main_plugin() is ran.

    with open("debug_embbeded_proto.bin", 'rb') as file:
        data = file.read()
        request = plugin.CodeGeneratorRequest.FromString(data)

        # Create response
        response = plugin.CodeGeneratorResponse()

        # Generate code
        generate_code(request, response)

        for response_file in response.file:
            print(response_file.name)
            print(response_file.content)

# -----------------------------------------------------------------------------


if __name__ == '__main__':
    # Check if we are running as a plugin under protoc
    if '--protoc-plugin' in sys.argv:
        main_plugin()
    else:
        main_cli()

# protoc --plugin=protoc-gen-eams=protoc-gen-eams --eams_out=./build ./test/proto/state.proto
