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
#include <jni.h>

extern JavaVM* g_jvm;

namespace jni
{

// Convenience class to get the java environment for the current thread.
// Also attach the threads, and detach it on destruction, if needed.
class JVMAttacher
{
public:
    JNIEnv *getEnv()
    {
        if (_env == nullptr)
        {
            if (g_jvm == nullptr) {
                die("g_jvm == NULL");
                return nullptr;
            }
            int rc = g_jvm->GetEnv((void **)&_env, JNI_VERSION_1_6);
            if (rc  == JNI_EDETACHED) {
                if (g_jvm->AttachCurrentThread(&_env, nullptr) != 0) {
                    die("AttachCurrentThread failed");
                    return nullptr;
                }
                _detach_thread = true;
            }
            else if (rc == JNI_EVERSION) {
                die("JNI version error");
                return nullptr;
            }
        }
        return _env;
    }

    ~JVMAttacher()
    {
        if (_detach_thread)
            g_jvm->DetachCurrentThread();
    }

private:
    JNIEnv *_env = nullptr;
    bool _detach_thread = false;
};

extern thread_local JVMAttacher jvm_attacher;

inline static JNIEnv *env() {
	return jvm_attacher.getEnv();
}

class Class;

class Object
{
public:
	using jtype = jobject;

	Object(jobject o = nullptr, bool ownRef = true) : object(o), ownRef(ownRef) { }
	Object(Object &&other) {
		std::swap(object, other.object);
		std::swap(ownRef, other.ownRef);
	}
	~Object() {
		if (ownRef && object != nullptr)
			env()->DeleteLocalRef(object);
	}

	Object& operator=(const Object& other) {
		if (this != &other)
		{
			if (ownRef && object != nullptr)
				env()->DeleteLocalRef(object);
			object = env()->NewLocalRef(other.object);
			ownRef = true;
		}
		return *this;
	}

	bool isNull() const { return object == nullptr; }
	operator jobject() const { return object; }

	Class getClass() const;

protected:
	jobject object = nullptr;
	bool ownRef = true;
};

class Class : public Object
{
public:
	Class(jclass clazz = nullptr, bool ownRef = true) : Object(clazz, ownRef) {}
	Class(Class &&other) : Object(std::move(other)) {}

	operator jclass() const { return (jclass)object; }
};

Class Object::getClass() const
{
	if (object == nullptr)
		return Class();
	else
		return Class(env()->GetObjectClass(object));
}

class String : public Object
{
public:
	using jtype = jstring;

	String(jobject s = nullptr, bool ownRef = true) : Object(s, ownRef) { }
	String(const char *s) {
		object = env()->NewStringUTF(s);
	}
	String(const std::string& s) : String(s.c_str()) { }
	String(String &&other) : Object(std::move(other)) {}

	String& operator=(const String& other) {
		return (String&)Object::operator=(other);
	}

	operator jstring() const { return (jstring)object; }
	operator std::string() const { return to_string(); }

	std::string to_string() const
	{
		if (object == nullptr)
			return "";
		const char *s = env()->GetStringUTFChars((jstring)object, 0);
		std::string str(s);
		env()->ReleaseStringUTFChars((jstring)object, s);
		return str;
	}

	size_t size() const {
		return object == nullptr ? 0 : env()->GetStringLength((jstring)object);
	}
	size_t length() const {
		return size();
	}
	bool empty() const {
		return size() == 0;
	}
};

class Array : public Object
{
public:
	using jtype = jarray;

	Array(jobject array = nullptr, bool ownRef = true) : Object(array, ownRef) { }
	Array(Array &&other) : Object(std::move(other)) {}

	operator jarray() const { return (jarray)object; }

	size_t size() const {
		return env()->GetArrayLength((jarray)object);
	}
};

template<typename T = Object>
class ObjectArray : public Array
{
public:
	using jtype = jobjectArray;

	ObjectArray(jobject array = nullptr, bool ownRef = true) : Array(array, ownRef) { }
	ObjectArray(ObjectArray &&other) : Array(std::move(other)) {}

	operator jobjectArray() const { return (jobjectArray)object; }

	T operator[](int i) {
		return T(env()->GetObjectArrayElement((jobjectArray)object, i));
	}
};

class Throwable : public Object
{
public:
	Throwable(jthrowable throwable = nullptr, bool ownRef = true) : Object(throwable, ownRef) { }
	Throwable(Throwable &&other) : Object(std::move(other)) {}

	operator jthrowable() const { return (jthrowable)object; }

	std::string getMessage()
	{
		if (object == nullptr)
			return "";
		jmethodID jgetMessage = env()->GetMethodID(getClass(), "getMessage", "()Ljava/lang/String;");
		return String(env()->CallObjectMethod(object, jgetMessage)).to_string();
	}
};

}
