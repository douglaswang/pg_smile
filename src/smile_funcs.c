/**
 * @file smile_funcs.c
 * @details Functions that expose SMILE functionality to PostgreSQL
 * Author: Eric Kemp-Benedict
 * 
 */

#include "smile_funcs.h"

/**
 * @brief A simple logging file for debugging
 * 
 * @param logfile Name of the logfile
 * @param msg The message string
 * @param srcfile The current source file
 * @param funcname Name of the function
 * @param lineno The current line number
 * @return void
 * 
 */
#define log(x) writetolog("logfile.txt", x, __FILE__, __FUNCTION__, __LINE__)
void writetolog(const char *logfile, const char *msg, const char *srcfile, const char *funcname, int lineno) {
    FILE *fhndl;
    
    fhndl = fopen(logfile, "a+");
    if (!fhndl) return;
    
    fprintf(fhndl, "%s (%s): %03d: %s\n", srcfile, funcname, lineno, msg);
    
    fclose(fhndl);
}

/**
 * @brief Convert a PostgreSQL text string to a null-terminated string
 * 
 * @param string The string as a text*
 * @return Null-terminated string
 * @details The calling function is responsible for pfreeing the string.
 * 
 */
char *text2cstring(text *string) {
    size_t len;
    char *retval;

    len = VARSIZE(string) - VARHDRSZ;
    retval = (char *) palloc(len + 1);

    if (retval) {
        memcpy(retval, VARDATA(string), len);
        retval[len] = '\0';
    }

    return retval;
}

/**
 * @brief Carries out Bayesian inference for a row of values
 * 
 * @param fcinfo
 *   A collection of arguments:
 *   bayes_file (text) = Filename of the .xdsl file;
 *   target_name (text) = Name of the node to calculate;
 *   target_state (text) = Label for the state to return. If INFO_STRING returns the info statistic
 *   row = A PostgreSQL row with node names and values (either text or int);
 * @return Datum Calculated (multiple) values of specified node as a row
 * @details Runs the SMILE Bayesian inference engine on multiple rows and returns result for one node
 * @todo Assumes a fixed number of states for the target
 */
Datum smile_infer(FunctionCallInfo fcinfo) {
    double value[NUM_TARG_NODES], nulvalue[NUM_TARG_NODES];
    int32 retval;
    double info, S0, S1;
    int i, j, retcode1, retcode2, numnodes, len, tstate;
    struct node target;
    struct node evidence[MAX_NODES];
    char *name_tmp, *target_name, *xdsl_file, *target_state;
    char log_msg[1024];
    HeapTupleHeader evidence_tuple;
    Datum tmp_datum;
    AttrNumber attrno;
    bool isnull;
    Oid tupType;
    int32 tupTypmod;
    TupleDesc tupDesc;

    xdsl_file = text2cstring(PG_GETARG_TEXT_P(0));
    target_name = text2cstring(PG_GETARG_TEXT_P(1));
    target_state = text2cstring(PG_GETARG_TEXT_P(2));
    evidence_tuple = PG_GETARG_HEAPTUPLEHEADER(3);
    
    if (target_name && strlen(target_name) < LEN_STRING) {
        strcpy(target.name, target_name);
    } else {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Target node name length exceeds maximum of %d bytes", LEN_STRING)));
    }
    if (target_name) pfree(target_name);
    target.count = 2; // TODO: Confirm that this is correct. It is assumed in some places that there are only two states
    target.id = -1;

    retcode1 = checkFileName(xdsl_file);
    if (retcode1 != SMILE_OK) {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Can't open XDSL file '%s'", xdsl_file)));
    }
    
    tupType = HeapTupleHeaderGetTypeId(evidence_tuple);
    tupTypmod = HeapTupleHeaderGetTypMod(evidence_tuple);
    tupDesc = lookup_rowtype_tupdesc_copy(tupType, tupTypmod);

    // Have to do this here because of problems passing memory locations from smile_c.cpp to here
    numnodes = getNumNodes(xdsl_file);
    for (i = 0; i < numnodes; i++) {
        evidence[i].id = i;
        evidence[i].count = getNumOutcomes(xdsl_file, i);
        
        if (!(len = getNodeNameLen(xdsl_file, i))) {
            ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Could not get node name length")));
        }
        if (len > LEN_STRING) {
            ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Node name length exceeds maximum of %d bytes", LEN_STRING)));
        }
        if (!copyNodeName(xdsl_file, i, evidence[i].name)) {
            ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Error getting node name")));
        }
        
        // Is this the target?
        if (!strcmp(target.name, evidence[i].name)) {
            if (evidence[i].count != NUM_TARG_NODES) {
                ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Target node can only have two possible values")));
            }
            target.id = i;
        }
        
        evidence[i].state[0] = '\0';
        attrno = InvalidAttrNumber;
        for (j = 0; j < tupDesc->natts; j++) {
            if (!namestrcmp(&(tupDesc->attrs[j]->attname), evidence[i].name)) {
                attrno = tupDesc->attrs[j]->attnum;
                break;
            }
        }
        if (attrno != InvalidAttrNumber) {
            tmp_datum = GetAttributeByNum(evidence_tuple, attrno, &isnull);
            // If the value is null then tmp_datum = 0
            if (tmp_datum && !isnull) {
                name_tmp = text2cstring(DatumGetTextP(tmp_datum));
                if (name_tmp && strlen(name_tmp) < LEN_STRING) {
                    strcpy(evidence[i].state, name_tmp);
                } else {
                    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: State of evidence node '%s' exceeds maximum length of %d bytes", evidence[i].name, LEN_STRING)));
                }
                if (name_tmp) pfree(name_tmp);
            }
        }
        evidence[i].stateid = -1;
    }

    // Calculate the result node
    retcode1 = getProb(xdsl_file, &target, value, evidence, numnodes);
    if (retcode1 != SMILE_OK) {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Error code %d", retcode1)));
    }
    tstate = getStateId(xdsl_file, target.id, target_state);
    
    // Calculate result node with no evidence, to calculate "info" value
    retcode2 = getProb(xdsl_file, &target, nulvalue, 0, numnodes);
    if (retcode2 != SMILE_OK) {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("SMILE: Error code %d", retcode2)));
    }
    // Calculate "information" measure
    // In principle this should be the log odds ratio, but that has bad behavior near p = 0,
    // and in any case is not bounded. This is a tunable function that fits a normalized version
    // of the log odds ratio over much of its extent:
    //    f(p) = 0.5 * (4 * p * (1-p))^INFO_EXPONENT , p =< 0.5
    //    f(p) = 1 - f(1-p), p > 0.5
    // Generalized to more than two states, but still test based on value[0]
    
    S0 = S1 = 1.0;
    for (i = 0; i < target.count; i++) {
        S0 *= target.count * nulvalue[i];
        S1 *= target.count * value[i];
    }
    S0 = 0.5 * pow(S0, INFO_EXPONENT);
    S1 = 0.5 * pow(S1, INFO_EXPONENT);
    if (nulvalue[0] > 0.5) {
        S0 = 1 - S0;
    }
    if (value[0] > 0.5) {
        S1 = 1 - S1;
    }
    info = abs(S0 - S1);
    
    retval = 0;
    if (info > THRESH_MODERATE) {
        if (info > THRESH_HIGH) {
            retval += 12;
        } else {
            retval += 8;
        }
    } else {
        retval += 4;
    }
    if (value[tstate] > THRESH_MODERATE) {
        if (value[tstate] > THRESH_HIGH) {
            retval += 3;
        } else {
            retval += 2;
        }
    } else {
        retval += 1;
    }

    // Clean up
    
    if (target_state) pfree(target_state);
    if (xdsl_file) pfree(xdsl_file);
    
    PG_RETURN_INT32(retval);
}

