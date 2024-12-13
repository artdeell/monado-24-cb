// Copyright 2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Implementation of android_environment.h
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup aux_android
 */
#include "android_environment.h"
#include "util/u_logging.h"

#include "wrap/android.os.h"
#include "wrap/android.content.h"
#include "wrap/java.io.h"

#include <string>
#include <stdexcept>


bool android_environment_get_files_dir(void* context, char *str, size_t size)
{
	try {
		if (size == 0 || str == nullptr) {
			throw std::invalid_argument("Dst string is null or zero buffer size");
		}
		wrap::java::io::File file = wrap::android::content::Context{(jobject)context}.getFilesDir();
		if (file.isNull()) {
			throw std::runtime_error("Failed to get File object");
		}
		const std::string dirPath = file.getAbsolutePath();
		if (size < (dirPath.length() + 1)) {
			throw std::length_error("Dst string length to small");
		}
		dirPath.copy(str, dirPath.length());
		str[dirPath.length()] = '\0';
		return true;
	} catch (std::exception const &e) {
		U_LOG_E("Could not get external storage directory path: %s", e.what());
		return false;
	}
}
