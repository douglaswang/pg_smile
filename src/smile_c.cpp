/* 
 * @file smile_c.cpp
 * @details A C-language wrapper on the SMILE library
 * Author: Eric Kemp-Benedict
 *
 * Created on December 14, 2011, 2:46 PM
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <math.h>
#include <time.h>
#include <smile/smile.h>
#include "smile_c.h"
#include "../include/bj_hash.h"

using namespace std;

struct net {
    DSL_network *ptr;
    int id;
};

/*
 * @brief Keeps track of a hash of networks, indexed by the filename
 * 
 * @param fname Filename of an .xdsl file
 * @return Pointer to the network
 * @note This has no prototype in the header file because of the C/C++ mix required
 * 
 */
struct net getNetwork(const char *fname) {
    static struct net nets[hashsize(10)];
    static int curr_id = 0;
    int h;
    
    // Use the filename as an index into a hash table so only load once
    h = hash((ub1*) fname, (ub4) strlen(fname), (ub4) 0);
    h = (h & hashmask(10));
    
    // Create the network, if not already created
    if (nets[h].ptr == NULL) {
        nets[h].ptr = new DSL_network();
        nets[h].ptr->SetDefaultBNAlgorithm(DSL_ALG_BN_LAURITZEN);
        nets[h].ptr->SetDefaultIDAlgorithm(DSL_ALG_ID_COOPERSOLVING);
        if (nets[h].ptr->ReadFile(fname, DSL_XDSL_FORMAT) < 0) {
            // Error reading in file
            delete nets[h].ptr;
            nets[h].ptr = NULL;
        }
        nets[h].id = curr_id++;
    }

    return nets[h];
}

/*
 * @brief Gets the set of all nodes for a network
 * 
 * @param fname Filename of an .xdsl file
 * @param nodes A node struct pointer: the function filles this in; calling function responsible to pfree it
 * @param numnodes The address of an int: will fill this in
 * @return status
 * 
 */

int checkFileName(const char *fname) {
    struct net net_info;
    
    net_info = getNetwork(fname);
    if (!net_info.ptr) {
        return SMILE_BAD_XDSL;
    } else {
        return SMILE_OK;
    }
}

int getNumNodes(const char *fname) {
    struct net net_info;
    
    net_info = getNetwork(fname);
    if (!net_info.ptr) {
        return -1;
    }
    
    return (net_info.ptr)->GetNumberOfNodes();
}

int getNodeNameLen(const char *fname, int id) {
    struct net net_info;
    
    net_info = getNetwork(fname);
    if (!net_info.ptr) {
        return 0;
    }
    
    return strlen((net_info.ptr)->GetNode(id)->GetId());
}

char* copyNodeName(const char *fname, int id, char name[]) {
    struct net net_info;
    
    net_info = getNetwork(fname);
    if (!net_info.ptr) {
        return 0;
    }
    
    return strcpy(name, (net_info.ptr)->GetNode(id)->GetId());
}

int getNumOutcomes(const char *fname, int id) {
    struct net net_info;
    
    net_info = getNetwork(fname);
    if (!net_info.ptr) {
        return -1;
    }
    
    return (net_info.ptr)->GetNode(id)->Definition()->GetNumberOfOutcomes();
    
}

// TODO: Catch invalid id's & states
int getStateId(const char *fname, int id, const char *state) {
    struct net net_info;
    DSL_idArray *outcomes;
    int i, numoutcomes;
    
    net_info = getNetwork(fname);
    if (!net_info.ptr) {
        return -1;
    }
    
    numoutcomes = (net_info.ptr)->GetNode(id)->Definition()->GetNumberOfOutcomes();
    outcomes = (net_info.ptr)->GetNode(id)->Definition()->GetOutcomesNames();
    
    for (i = 0; i < numoutcomes; i++) {
        if (!strcmp(state, ((string) (*outcomes)[i]).c_str())) {
            break;
        }
    }
    return i;
}

void free_node(struct node n) {
    if (n.name) pfree(n.name);
    if (n.state) pfree(n.state);
}

void free_nodes(struct node *n, int N) {
    int i;
    
    for (i = 0; i < N; i++) {
        free_node(n[i]);
    }
    
    pfree(n);
}

/*
 * @brief Carries out Bayesian inference for a row of values
 * 
 * @param fname Filename of an .xdsl file
 * @param target A node struct with the name and/or id of the target node
 *    This must have struct element target.count set to the number of outcomes
 * @param val Pointer to an array of size target.count to hold target probabilities (undef if error)
 * @param evidence An array of node structs with node names and/or ids set, and evidence names set or zero (null pointer); can be zero
 * @param nevidence Size of the evidence array
 * @return status
 * 
 */
int getProb(const char *fname, struct node *target, double val[], struct node evidence[], int nevidence) {
    DSL_network *net;
    struct net net_info;
    DSL_Dmatrix *matptr;
    int all_ok;
    int numnodes, numoutcomes;
    int i, j, m;
    int retval = SMILE_OK;
    DSL_idArray *outcomes;
    // Hash table for storing results
    int h;
    int tot_len;
    double prob_tot;
    static double prob_hash[hashsize(16)][MAX_UB1];
    // Assume that there are fewer than 255 possible values for each evidence node (use one negative value, -1 = 255)
    // Also add 1 entry for the id of the network
    ub1 evidence_key[MAX_NODES + EVIDENCE_OFFSET] = {0};
    
    net_info = getNetwork(fname);
    if (!net_info.ptr) {
        return SMILE_BAD_XDSL;
    }
    net = net_info.ptr;
    
    // First, put the network id at the start of the hash key
    evidence_key[0] = (ub1) net_info.id;
    tot_len = EVIDENCE_OFFSET + nevidence;
    
    numnodes = net->GetNumberOfNodes();

    // Get the id of the target node if not already defined
    if (target->id <= 0) {
        target->id = net->FindNode(target->name);
        // Even if a problem, loop over all and return to user
        if (target->id == DSL_OUT_OF_RANGE) {
            return SMILE_BAD_TARGET_NAME;
        }
    }

    // Clear evidence & set new evidence (if defined)
    net->ClearAllEvidence();

    // Set evidence, if set
    if (evidence) {
        // Get the ids of the evidence nodes if not already defined
        all_ok = 1;
        for (i = 0; i < nevidence; i++) {
            // A negative value for id signals that it's undefined
            // This modifies the return value, so on repeated calls these are defined
            if (evidence[i].id <= 0) {
                evidence[i].id = net->FindNode(evidence[i].name);
                // Even if a problem, loop over all and return to user
                if (all_ok && evidence[i].id == DSL_OUT_OF_RANGE) {
                    all_ok = 0;
                }
            }
        }
        if (!all_ok) {
            return SMILE_BAD_EVIDENCE_NAME;
        }

        for (i = 0; i < nevidence; i++) {
            numoutcomes = net->GetNode(evidence[i].id)->Definition()->GetNumberOfOutcomes();
            outcomes = net->GetNode(evidence[i].id)->Definition()->GetOutcomesNames();
            // Take the string state as definitive: if null, then no evidence set
            if (!evidence[i].state[0]) {
                evidence[i].stateid = -1;
            } else {
                for (j = 0; j < numoutcomes; j++) {
                    m = strcmp(evidence[i].state, ((string) (*outcomes)[j]).c_str());
                    if (!m) {
                        evidence[i].stateid = j;
                        break;
                    }
                }
            }
            // Calculate the key into the hash table -- values for evidence nodes -- add after filename
            evidence_key[i + EVIDENCE_OFFSET] = (ub1) evidence[i].stateid;
        }
        
        for (i = 0; i < nevidence; i++) {
            if (evidence[i].stateid >= 0) {
                net->GetNode(evidence[i].id)->Value()->SetEvidence(evidence[i].stateid);
            }
        }
    } else {
        // If evidence is null then no evidence set, and want these all to be -1 (or 255)
        for (i = 0; i < nevidence; i++) {
            evidence_key[i + EVIDENCE_OFFSET] = (ub1) -1;
        }
    }
    
            
    // Have we already stored this? If yes, return immediately.
    h = hash((ub1 *) evidence_key, (ub4) tot_len, (ub4) 0);
    h = (h & hashmask(16));
    prob_tot = 0.0;
    for (i = 0; i < target->count; i++) {
        prob_tot += prob_hash[h][i];
    }
    if (prob_tot > 0.5) { // Should be equal to 1.0, so this is well away from 0.0 (initialization) and 1.0
        for (i = 0; i < target->count; i++) {
            val[i] = prob_hash[h][i];
        }
        return SMILE_OK;
    }

    // Calculate network
    net->UpdateBeliefs();

    if (net->GetNode(target->id)->Value()->IsValueValid()) {
        m = net->GetNode(target->id)->Value()->GetSize();
        // If OK above, should be OK here, but check
        if (m != target->count) {
                return SMILE_TARGET_SIZE_DIFF_FROM_COUNT;
        }
        matptr = net->GetNode(target->id)->Value()->GetMatrix();
        for (i = 0; i < m; i++) {
            val[i] = matptr->Subscript(i);
        }
    } else {
        // Value was invalid for some reason
        return SMILE_INVALID_VALUE;
    }
    
    if (retval == SMILE_OK) {
        for (i = 0; i < target->count; i++) {
            prob_hash[h][i] = val[i];
        }
    }
    
    return retval;
}



