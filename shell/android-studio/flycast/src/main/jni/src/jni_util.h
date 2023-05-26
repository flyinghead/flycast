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

	Object(jobject o = nullptr, bool ownRef = true, bool globalRef = false)
		: object(o), ownRef(ownRef), globRef(globalRef) {}

	Object(Object &&other)
	{
		std::swap(object, other.object);
		std::swap(ownRef, other.ownRef);
		std::swap(globRef, other.globRef);
	}

	~Object() {
		deleteRef();
	}

	Object& operator=(const Object& other)
	{
		if (this != &other)
		{
			deleteRef();
			globRef = other.globRef;
			object = globRef ? env()->NewGlobalRef(other.object) : env()->NewLocalRef(other.object);
			ownRef = true;
		}
		return *this;
	}

	bool isNull() const { return object == nullptr; }
	operator jobject() const { return object; }

	Class getClass() const;

	template<typename T>
	T globalRef() {
		return T(env()->NewGlobalRef(object), true, true);
	}

protected:
	jobject object = nullptr;

	void deleteRef()
	{
		if (ownRef && object != nullptr)
		{
			if (globRef)
				env()->DeleteGlobalRef(object);
			else
				env()->DeleteLocalRef(object);
		}
	}

private:
	bool ownRef = true;
	bool globRef = false;
};

class Class : public Object
{
public:
	Class(jclass clazz = nullptr, bool ownRef = true, bool globalRef = false)
		: Object(clazz, ownRef, globalRef) {}
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

	String(jobject s = nullptr, bool ownRef = true, bool globalRef = false) : Object(s, ownRef, globalRef) { }
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

	static Class getClass() {
		return Class(env()->FindClass("java/lang/String"));
	}
};

class Array : public Object
{
public:
	using jtype = jarray;

	Array(jobject array = nullptr, bool ownRef = true, bool globalRef = false) : Object(array, ownRef, globalRef) { }
	Array(Array &&other) : Object(std::move(other)) {}

	Array& operator=(const Array& other) {
		return (Array&)Object::operator=(other);
	}

	operator jarray() const { return (jarray)object; }

	size_t size() const
	{
		if (object == nullptr)
			return 0;
		else
			return env()->GetArrayLength((jarray)object);
	}
};

template<typename T = Object>
class ObjectArray : public Array
{
public:
	using jtype = jobjectArray;

	ObjectArray(jobject array = nullptr, bool ownRef = true, bool globalRef = false) : Array(array, ownRef, globalRef) { }
	ObjectArray(ObjectArray &&other) : Array(std::move(other)) {}

	explicit ObjectArray(size_t size) : ObjectArray() {
		object = env()->NewObjectArray(size, T::getClass(), nullptr);
	}
	ObjectArray(size_t size, const Class& elemClass) : ObjectArray() {
		object = env()->NewObjectArray(size, elemClass, nullptr);
	}

	ObjectArray& operator=(const ObjectArray& other) {
		return (ObjectArray&)Object::operator=(other);
	}

	operator jobjectArray() const { return (jobjectArray)object; }

	T operator[](int i) {
		return T(env()->GetObjectArrayElement((jobjectArray)object, i));
	}

	void setAt(size_t index, const T& o) {
		env()->SetObjectArrayElement((jobjectArray)object, index, (jobject)o);
	}
};

class ByteArray : public Array
{
public:
	using jtype = jbyteArray;

	ByteArray(jobject array = nullptr, bool ownRef = true, bool globalRef = false) : Array(array, ownRef, globalRef) { }
	ByteArray(ByteArray &&other) : Array(std::move(other)) {}
	explicit ByteArray(size_t size) : ByteArray() {
		object = env()->NewByteArray(size);
	}

	ByteArray& operator=(const ByteArray& other) {
		return (ByteArray&)Object::operator=(other);
	}

	operator jbyteArray() const { return (jbyteArray)object; }

	void getData(u8 *dst, size_t first = 0, size_t len = 0) const {
		if (len == 0)
			len = size();
		if (len != 0)
			env()->GetByteArrayRegion((jbyteArray)object, first, len, (jbyte *)dst);
	}

	void setData(const u8 *src, size_t first = 0, size_t len = 0) {
		if (len == 0)
			len = size();
		env()->SetByteArrayRegion((jbyteArray)object, first, len, (const jbyte *)src);
	}

	operator std::vector<u8>() const
	{
		std::vector<u8> v;
		v.resize(size());
		getData(v.data());
		return v;
	}

	static Class getClass() {
		return Class(env()->FindClass("[B"));
	}
};

class IntArray : public Array
{
public:
	using jtype = jintArray;

	IntArray(jobject array = nullptr, bool ownRef = true, bool globalRef = false) : Array(array, ownRef, globalRef) { }
	IntArray(IntArray &&other) : Array(std::move(other)) {}
	explicit IntArray(size_t size) : IntArray() {
		object = env()->NewIntArray(size);
	}

	IntArray& operator=(const IntArray& other) {
		return (IntArray&)Object::operator=(other);
	}

	operator jintArray() const { return (jintArray)object; }

	void getData(int *dst, size_t first = 0, size_t len = 0) const {
		if (len == 0)
			len = size();
		if (len != 0)
			env()->GetIntArrayRegion((jintArray)object, first, len, (jint *)dst);
	}

	void setData(const int *src, size_t first = 0, size_t len = 0) {
		if (len == 0)
			len = size();
		env()->SetIntArrayRegion((jintArray)object, first, len, (const jint *)src);
	}

	operator std::vector<int>() const
	{
		std::vector<int> v;
		v.resize(size());
		getData(v.data());
		return v;
	}

	static Class getClass() {
		return Class(env()->FindClass("[I"));
	}
};

class ShortArray : public Array
{
public:
	using jtype = jshortArray;

	ShortArray(jobject array = nullptr, bool ownRef = true, bool globalRef = false) : Array(array, ownRef, globalRef) { }
	ShortArray(ShortArray &&other) : Array(std::move(other)) {}
	explicit ShortArray(size_t size) : ShortArray() {
		object = env()->NewShortArray(size);
	}

	ShortArray& operator=(const ShortArray& other) {
		return (ShortArray&)Object::operator=(other);
	}

	operator jshortArray() const { return (jshortArray)object; }

	void getData(short *dst, size_t first = 0, size_t len = 0) {
		if (len == 0)
			len = size();
		if (len != 0)
			env()->GetShortArrayRegion((jshortArray)object, first, len, (jshort *)dst);
	}

	void setData(const short *src, size_t first = 0, size_t len = 0) {
		if (len == 0)
			len = size();
		env()->SetShortArrayRegion((jshortArray)object, first, len, (const jshort *)src);
	}

	static Class getClass() {
		return Class(env()->FindClass("[S"));
	}
};

class Throwable : public Object
{
public:
	Throwable(jthrowable throwable = nullptr, bool ownRef = true, bool globalRef = false) : Object(throwable, ownRef, globalRef) { }
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
