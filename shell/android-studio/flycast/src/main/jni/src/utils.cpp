#include "types.h"
#include <zip.h>

zip* APKArchive;
void setAPK (const char* apkPath) {
	INFO_LOG(COMMON, "Loading APK %s", apkPath);
	APKArchive = zip_open(apkPath, 0, NULL);
	if (APKArchive == NULL) {
		ERROR_LOG(COMMON, "Error loading APK");
		return;
	}

	//Just for debug, print APK contents
	int numFiles = zip_get_num_files(APKArchive);
	for (int i=0; i<numFiles; i++) {
		const char* name = zip_get_name(APKArchive, i, 0);
		if (name == NULL) {
			ERROR_LOG(COMMON, "Error reading zip file name at index %i : %s", i, zip_strerror(APKArchive));
			return;
		}
		INFO_LOG(COMMON, "File %i : %s", i, name);
	}
}
