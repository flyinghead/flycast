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
#include "rend/boxart/http_client.h"

namespace http {

    static jobject HttpClient;
    static jmethodID openUrlMid;

    void init() {
    }

    int get(const std::string &url, std::vector<u8> &content, std::string &contentType) {
        JNIEnv *env = jvm_attacher.getEnv();
        jstring jurl = env->NewStringUTF(url.c_str());
        jclass byteArrayClass = env->FindClass("[B");
        jobjectArray contentOut = env->NewObjectArray(1, byteArrayClass, NULL);
        jclass stringClass = env->FindClass("java/lang/String");
        jobjectArray contentTypeOut = env->NewObjectArray(1, stringClass, NULL);

        int httpStatus = env->CallIntMethod(HttpClient, openUrlMid, jurl, contentOut,
                                           contentTypeOut);

        jbyteArray jcontent = (jbyteArray)env->GetObjectArrayElement(contentOut, 0);
        if (jcontent != nullptr) {
            int len = env->GetArrayLength(jcontent);
            content.resize(len);
            env->GetByteArrayRegion(jcontent, 0, len, (jbyte *)content.data());
            env->DeleteLocalRef(jcontent);
        }
        jstring jcontentType = (jstring)env->GetObjectArrayElement(contentTypeOut, 0);
        if (jcontentType != nullptr) {
            const char *data = env->GetStringUTFChars(jcontentType, 0);
            contentType = data;
            env->ReleaseStringUTFChars(jcontentType, data);
            env->DeleteLocalRef(jcontentType);
        }
        env->DeleteLocalRef(contentTypeOut);
        env->DeleteLocalRef(contentOut);
        env->DeleteLocalRef(jurl);

        return httpStatus;
    }

    void term() {
    }

}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_HttpClient_nativeInit(JNIEnv *env, jobject obj)
{
    http::HttpClient = env->NewGlobalRef(obj);
    http::openUrlMid = env->GetMethodID(env->GetObjectClass(obj), "openUrl", "(Ljava/lang/String;[[B[Ljava/lang/String;)I");
}
