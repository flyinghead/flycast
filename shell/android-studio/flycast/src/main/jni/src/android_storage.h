/*
	Copyright 2023 flyinghead

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
#include "types.h"
#include "oslib/storage.h"
#include "jni_util.h"
#include <jni.h>

namespace hostfs
{

class AndroidStorage : public CustomStorage
{
public:
	void init(JNIEnv *env, jobject storage)
	{
		jstorage = env->NewGlobalRef(storage);
		jni::Class clazz(env->GetObjectClass(storage));
		jopenFile = env->GetMethodID(clazz, "openFile", "(Ljava/lang/String;Ljava/lang/String;)I");
		jlistContent = env->GetMethodID(clazz, "listContent", "(Ljava/lang/String;)[Lcom/flycast/emulator/FileInfo;");
		jgetParentUri = env->GetMethodID(clazz, "getParentUri", "(Ljava/lang/String;)Ljava/lang/String;");
		jgetSubPath = env->GetMethodID(clazz, "getSubPath", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
		jgetFileInfo = env->GetMethodID(clazz, "getFileInfo", "(Ljava/lang/String;)Lcom/flycast/emulator/FileInfo;");
		jaddStorage = env->GetMethodID(clazz, "addStorage", "(ZZ)V");
	}

	bool isKnownPath(const std::string& path) override {
		return path.substr(0, 10) == "content://";
	}

	FILE *openFile(const std::string& uri, const std::string& mode) override
	{
		jni::String juri(uri);
		const char *amode;
		if (mode.substr(0, 2) == "r+")
			amode = "rw";
		else if (mode[0] == 'r')
			amode = "r";
		else if (mode.substr(0, 2) == "w+")
			amode = "rwt";
		else if (mode[0] == 'w')
			amode = "wt";
		else if (mode.substr(0, 2) == "a+")
			// Incorrect but this is a weird mode anyway
			amode = "rw";
		else // a
			amode = "wa";
	    jni::String jmode(amode);
		jint fd = jni::env()->CallIntMethod(jstorage, jopenFile, (jstring)juri, (jstring)jmode);
		try {
			checkException();
		} catch (const hostfs::StorageException& e) {
			WARN_LOG(COMMON, "openFile failed: %s", e.what());
			return nullptr;
		}
		return fdopen(fd, mode.c_str());
	}

	std::vector<FileInfo> listContent(const std::string& uri) override
	{
		std::vector<FileInfo> ret;
		if (uri.empty())
			// Nothing to see here
			return ret;

		JNIEnv *env = jni::env();
		jni::String juri(uri);
		jni::ObjectArray<> fileInfos(env->CallObjectMethod(jstorage, jlistContent, (jstring)juri));
		checkException();
		int len = fileInfos.size();
		for (int i = 0; i < len; i++)
		{
			jni::Object fileInfo = fileInfos[i];
			ret.emplace_back(fromJavaFileInfo(fileInfo));
		}

		return ret;
	}

	std::string getParentPath(const std::string& uri) override
	{
		jni::String juri(uri);
		jni::String jparentUri(jni::env()->CallObjectMethod(jstorage, jgetParentUri, (jstring)juri));
		checkException();
		return jparentUri;
	}

	std::string getSubPath(const std::string& reference, const std::string& relative) override
	{
		jni::String jref(reference);
		jni::String jrel(relative);
		jni::String jretUri(jni::env()->CallObjectMethod(jstorage, jgetSubPath, (jstring)jref, (jstring)jrel));
		checkException();
		return jretUri;
	}

	FileInfo getFileInfo(const std::string& uri) override
	{
		jni::String juri(uri);
		jni::Object jinfo(jni::env()->CallObjectMethod(jstorage, jgetFileInfo, (jstring)juri));
		checkException();
		return fromJavaFileInfo(jinfo);
	}

	void addStorage(bool isDirectory, bool writeAccess, void (*callback)(bool cancelled, std::string selectedPath)) override
	{
		jni::env()->CallVoidMethod(jstorage, jaddStorage, isDirectory, writeAccess);
		checkException();
		addStorageCallback = callback;
	}

	void doStorageCallback(jstring path)
	{
		if (addStorageCallback != nullptr)
		{
			try {
				addStorageCallback(path == nullptr, jni::String(path, false));
			} catch (...) {
			}
			addStorageCallback = nullptr;
		}
	}

private:
	void checkException()
	{
		jni::Throwable throwable(jni::env()->ExceptionOccurred());
		if (throwable.isNull())
			return;

		jni::env()->ExceptionClear();
		throw StorageException("Storage access failed: " + throwable.getMessage());
	}

	FileInfo fromJavaFileInfo(const jni::Object& jinfo)
	{
		loadFileInfoMethods(jinfo);

		FileInfo info;
		JNIEnv *env = jni::env();
		info.name = jni::String(env->CallObjectMethod(jinfo, jgetName));
		info.path = jni::String(env->CallObjectMethod(jinfo, jgetPath));
		info.isDirectory = env->CallBooleanMethod(jinfo, jisDirectory);
		info.isWritable = env->CallBooleanMethod(jinfo, jisWritable);
		info.size = env->CallLongMethod(jinfo, jgetSize);

		return info;
	}

	void loadFileInfoMethods(const jni::Object& jinfo)
	{
		if (jgetName != nullptr)
			return;
		JNIEnv *env = jni::env();
		jni::Class infoClass = jinfo.getClass();
		checkException();
		jgetName = env->GetMethodID(infoClass, "getName", "()Ljava/lang/String;");
		jgetPath = env->GetMethodID(infoClass, "getPath", "()Ljava/lang/String;");
		jisDirectory = env->GetMethodID(infoClass, "isDirectory", "()Z");
		jisWritable = env->GetMethodID(infoClass, "isWritable", "()Z");
		jgetSize = env->GetMethodID(infoClass, "getSize", "()J");
	}

	jobject jstorage;
	jmethodID jopenFile;
	jmethodID jlistContent;
	jmethodID jgetParentUri;
	jmethodID jaddStorage;
	jmethodID jgetSubPath;
	jmethodID jgetFileInfo;
	// FileInfo accessors lazily initialized to avoid having to load the class
	jmethodID jgetName = nullptr;
	jmethodID jgetPath = nullptr;
	jmethodID jisDirectory = nullptr;
	jmethodID jisWritable = nullptr;
	jmethodID jgetSize = nullptr;
	void (*addStorageCallback)(bool cancelled, std::string selectedPath);
};

Storage& customStorage()
{
	static std::unique_ptr<AndroidStorage> androidStorage;
	if (!androidStorage)
		androidStorage = std::make_unique<AndroidStorage>();
	return *androidStorage;
}

}	// namespace hostfs

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_AndroidStorage_addStorageCallback(JNIEnv *env, jobject obj, jstring path)
{
	static_cast<hostfs::AndroidStorage&>(hostfs::customStorage()).doStorageCallback(path);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_AndroidStorage_init(JNIEnv *env, jobject jstorage)
{
	static_cast<hostfs::AndroidStorage&>(hostfs::customStorage()).init(env, jstorage);
}
