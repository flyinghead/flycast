/*
	Copyright 2022 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include "oslib/http_client.h"
#include "jni_util.h"

namespace http {

    static jobject HttpClient;
    static jmethodID openUrlMid;
    static jmethodID postMid;
    static jmethodID postRawMid;

    void init() {
    }

    int get(const std::string &url, std::vector<u8> &content, std::string &contentType)
    {
        jni::String jurl(url);
        jni::ObjectArray<jni::ByteArray> contentOut(1);
        jni::ObjectArray<jni::String> contentTypeOut(1);

        int httpStatus = jni::env()->CallIntMethod(HttpClient, openUrlMid,
        		static_cast<jstring>(jurl),
				static_cast<jobjectArray>(contentOut),
        		static_cast<jobjectArray>(contentTypeOut));

        content = contentOut[0];
        contentType = contentTypeOut[0];

        return httpStatus;
    }

    int post(const std::string &url, const char *payload, const char *contentType, std::vector<u8>& reply)
    {
        jni::String jurl(url);
        jni::String jpayload(payload);
        jni::String jcontentType(contentType);
        jni::ObjectArray<jni::ByteArray> replyOut(1);

        int httpStatus = jni::env()->CallIntMethod(HttpClient, postRawMid,
        		static_cast<jstring>(jurl),
				static_cast<jstring>(jpayload),
				static_cast<jstring>(jcontentType),
				static_cast<jobjectArray>(replyOut));
        reply = replyOut[0];

        return httpStatus;
    }

    int post(const std::string &url, const std::vector<PostField>& fields)
    {
        jni::String jurl(url);
        jni::ObjectArray<jni::String> names(fields.size());
        jni::ObjectArray<jni::String> values(fields.size());
        jni::ObjectArray<jni::String> contentTypes(fields.size());

        for (size_t i = 0; i < fields.size(); i++)
        {
        	names.setAt(i, fields[i].name);
        	values.setAt(i, fields[i].value);
        	contentTypes.setAt(i, fields[i].contentType);
        }

        int httpStatus = jni::env()->CallIntMethod(HttpClient, postMid,
        		static_cast<jstring>(jurl),
				static_cast<jobjectArray>(names),
        		static_cast<jobjectArray>(values),
				static_cast<jobjectArray>(contentTypes));

        return httpStatus;
    }

    void term() {
    }

}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_HttpClient_nativeInit(JNIEnv *env, jobject obj)
{
    http::HttpClient = env->NewGlobalRef(obj);
    http::openUrlMid = env->GetMethodID(env->GetObjectClass(obj), "openUrl", "(Ljava/lang/String;[[B[Ljava/lang/String;)I");
    http::postMid = env->GetMethodID(env->GetObjectClass(obj), "post", "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)I");
    http::postRawMid = env->GetMethodID(env->GetObjectClass(obj), "post", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[[B)I");
}
