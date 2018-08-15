/*
 * Copyright(c)		2017-2018	UT-Battelle, LLC
 * 					All rights reserved.
 */

#ifndef MOC_COMMON_H
#define MOC_COMMON_H

/* Whether C compiler supports -fvisibility */
#define MOC_HAVE_VISIBILITY 1

#if MOC_HAVE_VISIBILITY == 1
#define MOC_EXPORT __attribute__((__visibility__("default")))
#else
#define MOC_EXPORT
#endif

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

typedef pmix_info_t     moc_info_t;

#define MOC_INFO_CREATE PMIX_INFO_CREATE
#define MOC_INFO_LOAD   PMIX_INFO_LOAD
#define MOC_INFO_FREE   PMIX_INFO_FREE

#define MOC_STRING      PMIX_STRING

typedef int moc_status_t;

#define MOC_SUCCESS                  0
#define MOC_ERROR                   -1
#define MOC_ERR_OUT_OF_RESOURCE	    -2

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* MOC_COMMON_H */
