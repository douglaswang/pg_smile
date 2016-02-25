/**
 * @file pg_smile.h
 * Author: Eric Kemp-Benedict
 *
 * Created on August 2, 2011, 6:28 PM
 */
#ifdef __cplusplus
extern "C" {
#endif
#include "postgresql/postgres.h"
#include "postgresql/9.1/server/fmgr.h"
#include "postgresql/9.1/server/funcapi.h"
#include "postgresql/utils/errcodes.h"
#include "postgresql/9.1/server/utils/typcache.h"
#include "postgresql/9.1/server/access/attnum.h"
#include "postgresql/9.1/server/utils/builtins.h"
#include "smile_c.h"

#ifndef PG_SMILE_H
#define	PG_SMILE_H

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define INFO_STRING "__information"

/*
 * Standard declaration required for all PG functions, using "V1" syntax:
 * PG_FUNCTION_INFO_V1(funcname);
 * Datum funcname(PG_FUNCTION_ARGS);
 * 
 * In this code, use literal:
 *   PG_FUNCTION_ARGS = FunctionCallInfo fcinfo
 * Because it makes the code more clear and works better with Doxygen.
*/

PG_FUNCTION_INFO_V1(smile_infer);
Datum smile_infer(FunctionCallInfo fcinfo);

char *text2cstring(text *string);
#ifdef __cplusplus
}
#endif
#endif	/* PG_SMILE_H */

