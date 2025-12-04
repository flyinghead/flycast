/*
	Copyright 2025 flyinghead

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
#include "oslib/i18n.h"
#include "jni_util.h"
#include <string>
#include <vector>
#include <locale>
#include <time.h>
#include <unicode/utypes.h>
#include <unicode/ucol.h>
#include <unicode/ustring.h>
#include <dlfcn.h>

namespace i18n {

static jni::Class clazz;
static jmethodID formatShortDateTimeID;
static jmethodID getCurrentLocaleID;

void initJni(JNIEnv *env)
{
	jni::Class localClazz(env->FindClass("com/flycast/emulator/emu/LocaleUtils"));
	clazz = localClazz.globalRef<jni::Class>();
	formatShortDateTimeID = env->GetStaticMethodID(clazz, "formatShortDateTime", "(J)Ljava/lang/String;");
	getCurrentLocaleID = env->GetStaticMethodID(clazz, "getCurrentLocale", "()Ljava/lang/String;");
}

void termJni() {
	clazz = {};
}

std::string formatShortDateTime(time_t t) {
	jni::String str(jni::env()->CallStaticObjectMethod(clazz, formatShortDateTimeID, (jlong)t));
	return str.to_string();
}

std::string getCurrentLocale() {
	jni::String str(jni::env()->CallStaticObjectMethod(clazz, getCurrentLocaleID));
	return str.to_string();
}

//
// libicu4c API pointers
//
using ucol_open_t = UCollator *(*)(const char *, UErrorCode *err);
using ucol_close_t = void (*)(UCollator *);
using ucol_strcollUTF8_t = UCollationResult (*)(const UCollator *coll, const char *source, int32_t sourceLength, const char *target, int32_t targetLength, UErrorCode *status);
using u_strFromUTF8_t = UChar* (*)(UChar *dest, int32_t destCapacity, int32_t *pDestLength, const char *src, int32_t srcLength, UErrorCode *pErrorCode);
using ucol_getSortKey_t = int32_t (*)(const UCollator *coll, const UChar *source, int32_t sourceLength, uint8_t *result, int32_t resultLength);

static ucol_open_t dl_ucol_open;
static ucol_close_t dl_ucol_close;
static ucol_strcollUTF8_t dl_ucol_strcollUTF8;
static u_strFromUTF8_t dl_u_strFromUTF8;
static ucol_getSortKey_t dl_ucol_getSortKey;

#define ucol_open dl_ucol_open
#define ucol_close dl_ucol_close
#define ucol_strcollUTF8 dl_ucol_strcollUTF8
#define u_strFromUTF8 dl_u_strFromUTF8
#define ucol_getSortKey dl_ucol_getSortKey

static bool inited;
static bool available;
static UCollator *collator;

template<typename CharT>
class Collate : public std::collate<CharT> {
public:
	Collate(size_t refs = 0)
		: std::collate<CharT>(refs)
	{}
	typedef CharT char_type;
	typedef std::basic_string<CharT> string_type;

protected:
	int do_compare(const CharT* low1, const CharT* high1,
			const CharT* low2, const CharT* high2) const override;
    typename std::collate<CharT>::string_type do_transform(const CharT* low, const CharT* high) const override;
	long do_hash(const CharT* beg, const CharT* end) const override;
};

template<>
int Collate<char>::do_compare(const char* low1, const char* high1,
		const char* low2, const char* high2) const
{
	if (!available)
	{
		int rc = strcmp(low1, low2);
		if (rc < 0)
			return -1;
		else if (rc > 0)
			return 1;
		else
			return 0;
	}
	UErrorCode err = U_ZERO_ERROR;
	UCollationResult result = ucol_strcollUTF8(collator, low1, high1 - low1,
											   low2, high2 - low2, &err);
	if (U_FAILURE(err))
		return 0;
	return result;
}

locale::locale() : std::locale(customLocale)
{}
std::locale locale::customLocale { std::locale(), new Collate<char>() };

template<>
std::collate<char>::string_type Collate<char>::do_transform(const char* low, const char* high) const
{
	if (!available)
		return std::string(low, high);

    UErrorCode err = U_ZERO_ERROR;
    int32_t ucharSize;
    // Get the needed length for the wchar string
    u_strFromUTF8(nullptr, 0, &ucharSize, low, high - low, &err);
    if (U_FAILURE(err))
    	return std::string(low, high);
    std::vector<char16_t> wstr;
    wstr.resize(ucharSize);
    u_strFromUTF8(wstr.data(), wstr.size(), &ucharSize, low, high - low, &err);
    // Get the needed length of the sort key
    if (U_FAILURE(err))
    	return std::string(low, high);
    int32_t rc = ucol_getSortKey(collator, &wstr[0], wstr.size(), nullptr, 0);
    std::string str;
    str.resize(rc);
    rc = ucol_getSortKey(collator, &wstr[0], wstr.size(), (uint8_t *)str.data(), str.capacity());
    if (rc > 0)
    	// don't include the null terminator
    	str.resize(rc - 1);
    else
    	str.clear();
    return str;
}

template<>
long Collate<char>::do_hash(const char* beg, const char* end) const {
    return std::hash<std::string>()(do_transform(beg, end));
}

static void loadLibicu()
{
    if (!__builtin_available(android 33, *)) {
        INFO_LOG(COMMON, "ucol_open introduced in API 33 (Android 13)");
        return;
    }
	void *hLibicu = dlopen("libicu.so", RTLD_NOW | RTLD_GLOBAL);
	if (hLibicu == nullptr) {
		ERROR_LOG(COMMON, "dlopen libicu.so failed.");
		return;
	}
	dl_ucol_open = reinterpret_cast<ucol_open_t>(dlsym(hLibicu, "ucol_open"));
	if (dl_ucol_open == nullptr) {
		ERROR_LOG(COMMON, "dlsym ucol_open failed.");
		return;
	}
	dl_ucol_close = reinterpret_cast<ucol_close_t>(dlsym(hLibicu, "ucol_close"));
	dl_u_strFromUTF8 = reinterpret_cast<u_strFromUTF8_t>(dlsym(hLibicu, "u_strFromUTF8"));
	dl_ucol_strcollUTF8 = reinterpret_cast<ucol_strcollUTF8_t>(dlsym(hLibicu, "ucol_strcollUTF8"));
	dl_ucol_getSortKey = reinterpret_cast<ucol_getSortKey_t>(dlsym(hLibicu, "ucol_getSortKey"));
	NOTICE_LOG(COMMON, "libicu.so loaded. ucol_getSortKey is %p", dl_ucol_getSortKey);
}

void init(JNIEnv *env)
{
	if (inited)
		return;
	inited = true;
	initJni(env);
	loadLibicu();
	if (dl_ucol_open == nullptr)
		return;
	UErrorCode status = U_ZERO_ERROR;
	collator = ucol_open(getCurrentLocale().c_str(), &status);
	if (U_FAILURE(status)) {
		ERROR_LOG(COMMON, "Collator creation failed: %d", status);
		return;
	}
	available = true;
	// for some reason the global locale is overridden by someone later on, so we use i18n::locale instead
	//std::locale::global(std::locale(std::locale(), new Collate<char>()));
}

void term()
{
	if (available)
		ucol_close(collator);
	termJni();
}

}
