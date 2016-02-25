/**
 * @file smile_c.h
 * @details A C-language wrapper on the SMILE library
 * Author: Eric Kemp-Benedict
 *
 * Created on August 2, 2011, 6:28 PM
 */

#ifndef SMILE_C_H
#define	SMILE_C_H

/*###################################
#
# Constants
#
###################################*/

#define LEN_STRING 256
#define MAX_UB1 256
#define MAX_NODES 1024
#define NUM_TARG_NODES 2
#define EVIDENCE_OFFSET 1

#define INFO_EXPONENT 0.5

#define THRESH_MODERATE 0.3
#define THRESH_HIGH 0.35

#define SMILE_OK 0
#define SMILE_BAD_XDSL 1
#define SMILE_BAD_TARGET_NAME 2
#define SMILE_BAD_EVIDENCE_NAME 3
#define SMILE_TARGET_SIZE_DIFF_FROM_COUNT 4
#define SMILE_INVALID_VALUE 5

/*###################################
#
# Exported functions
#
###################################*/

#ifdef __cplusplus
extern "C" {
#endif
// Use this for palloc definition
#include "postgresql/postgres.h"

struct node {
    char name[LEN_STRING];
    int id; // If nonnegative, assume this has been set from a previous call
    char state[LEN_STRING]; // Set to null string to make sure no evidence set
    int stateid; // Always set based on state
    int count;
};

int checkFileName(const char *fname);
int getNumNodes(const char *fname);
int getNodeNameLen(const char *fname, int id);
char* copyNodeName(const char *fname, int id, char *name);
int getNumOutcomes(const char *fname, int id);
int getStateId(const char *fname, int id, const char *state);
int getProb(const char *fname, struct node *target, double val[], struct node evidence[], int nevidence);

void free_node(struct node n);
void free_nodes(struct node *n, int N);

#ifdef __cplusplus
}
#endif


#endif	/* SMILE_C_H */

