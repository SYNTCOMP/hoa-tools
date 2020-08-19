/**************************************************************************
 * Copyright (c) 2019- Guillermo A. Perez
 * 
 * This file is part of HOATOOLS.
 * 
 * HOATOOLS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * HOATOOLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with HOATOOLS. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Guillermo A. Perez
 * University of Antwerp
 * guillermoalberto.perez@uantwerpen.be
 *************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simplehoa.h"

// A simple data structure for a linked list of PGSolver vertex
// entries

typedef struct IntList {
    int i;
    struct IntList* next;
} IntList;

static IntList* prependIntNode(IntList* node, int val) {
    IntList* newHead = (IntList*) malloc(sizeof(IntList));
    newHead->i = val;
    newHead->next = node;
    return newHead;
}

typedef struct PGSVertices {
    int id;
    int owner;
    int priority;
    char* name;
    IntList* successors;
    struct PGSVertices* next;
} PGSVertices;

static PGSVertices* initPGSVertices(size_t amount) {
    assert(amount > 0);
    PGSVertices* head = malloc(sizeof(PGSVertices));
    head->name = NULL;
    head->successors = NULL;
    head->next = NULL;
    PGSVertices* cur = head;
    for (int i = 0; i < amount - 1; i++) {
        cur->next = malloc(sizeof(PGSVertices));
        cur = cur->next;
        cur->name = NULL;
        cur->successors = NULL;
        cur->next = NULL;
    }
    return head;
}

static void deletePGSVertices(PGSVertices* u) {
    if (u == NULL)
        return;
    PGSVertices* v = u->next;
    if (u->name != NULL)
        free(u->name);
    IntList* succ = u->successors;
    while (succ != NULL) {
        IntList* temp = succ->next;
        free(succ);
        succ = temp;
    }
    free(u);
    deletePGSVertices(v);
}

static void printPGSVertices(PGSVertices* vlist) {
    for (PGSVertices* v = vlist; v != NULL; v = v->next) {
        assert(v->successors != NULL);
        printf("%d %d %d %d", v->id, v->priority, v->owner,
               v->successors->i);
        for (IntList* succ = v->successors->next; succ != NULL; succ = succ->next) {
            printf(",%d", succ->i);
        }
        if (v->name != NULL)
            printf(" \"%s\"\n", v->name);
        else
            printf(" \"%d\"\n", v->id);
    }
}

/* Given a label and a valuation of some of the atomic propositions,
 * we determine whether the label is true (1), false (-1), or its
 * value is unknown (0). The valuation is expected as an unsigned
 * integer whose i-th bit is 1 iff the i-th AP in apIds is set to 1
 */
static int evalLabel(BTree* label, Alias* aliases, int noAliases,
                      int numAPs, int* apIds, unsigned value) {
    assert(label != NULL);
    int left;
    int right;
    unsigned mask;
    switch (label->type) {
        case NT_BOOL:
            return label->id ? 1 : -1;  // 0 becomes -1 like this
        case NT_AND:
            left = evalLabel(label->left, aliases, noAliases,
                             numAPs, apIds, value);
            right = evalLabel(label->right, aliases, noAliases,
                              numAPs, apIds, value);
            if (left == -1 || right == -1)
                return -1;
            if (left == 0 || right == 0)
                return 0;
            // otherwise
            return 1;
        case NT_OR:
            left = evalLabel(label->left, aliases, noAliases,
                             numAPs, apIds, value);
            right = evalLabel(label->right, aliases, noAliases,
                              numAPs, apIds, value);
            if (left == 1 || right == 1)
                return 1;
            if (left == 0 || right == 0)
                return 0;
            // otherwise
            return -1;
        case NT_NOT:
            return -1 * evalLabel(label->left, aliases, noAliases,
                                  numAPs, apIds, value);
        case NT_AP:
            mask = 1;
            for (int i = 0; i < numAPs; i++) {
                if (label->id == apIds[i]) {
                    return ((mask & value) == mask) ? 1 : -1;
                }
                mask = mask << 1;
            }
            return 0;
        case NT_ALIAS:
            for (int i = 0; i < noAliases; i++) {
                if (strcmp(aliases[i].alias, label->alias) == 0)
                    return evalLabel(aliases[i].labelExpr, aliases, noAliases,
                                     numAPs, apIds, value);
            }
            break;
        default:
            assert(false);  // all cases should be covered above
    }
    return -2;
}

/* Adjust the priorities since we have to make sure we output a max, even
 * parity game and that the priorities of player-0 vertices are useless
 * (that is, irrelevant). This assumes isMaxParity is true iff the original
 * objective is max and resGoodPriority = 0 if even, otherwise it is 1 if odd.
 */
static inline int adjustPriority(int p, bool isMaxParity, short resGoodPriority,
                                 int noPriorities) {
    // To deal with max vs min, we subtract from noPriorities if
    // originally it was min (for this we need it to be even!)
    int evenMax = noPriorities;
    if (evenMax % 2 != 0)
        evenMax += 1;
    int pForMax = isMaxParity ? p : evenMax - p;
    // The plan is to use 0 as the priority for player-0 vertices,
    // this means shifting everything up; we take the opportunity
    // to make odd priorities even if the original objective asked
    // for odd ones
    int shifted = pForMax + (2 - resGoodPriority);
#ifndef NDEBUG
    fprintf(stderr, "Changed %d into %d. Original objective: %s %s with "
                    "maximal priority %d\n", p, shifted,
                    (isMaxParity ? "max" : "min"),
                    (resGoodPriority == 0 ? "even" : "odd"),
                    noPriorities);
#endif
    return shifted;
}


/* Read the EHOA file, construct a graph-based game and
 * dump it as a PGSolver game
 */
int main(int argc, char* argv[]) {
    HoaData data;
    defaultsHoa(&data);
    int ret = parseHoa(stdin, &data);
    // 0 means everything was parsed correctly
    if (ret != 0)
        return ret;
    // A few semantic checks! essentially, the automaton should be
    // good to be used as a game specification
    bool isMaxParity;
    short resGoodPriority;
    ret = isParityGFG(&data, &isMaxParity, &resGoodPriority);
    if (ret != 0)
        return ret;

    // Step 1: prepare a list of all uncontrollable inputs
    int numUcntAPs = data.noAPs - data.noCntAPs;
    int ucntAPs[numUcntAPs];
    int apIdx = 0;
    for (int i = 0; i < data.noAPs; i++) {
        bool found = false;
        for (int j = 0; j < data.noCntAPs; j++) {
            if (i == data.cntAPs[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            ucntAPs[apIdx] = i;
#ifndef NDEBUG
            fprintf(stderr, "Found an uncontrollable AP: %d\n", i);
#endif
            apIdx++;
        }
    }
    assert(apIdx == numUcntAPs);

    // Step 2: for all states in the automaton and all valuations, create
    // vertices for both players and edges to go with them
    // NOTE: states retain their index while "intermediate" state-valuation
    // vertices receive new indices
    const unsigned numValuations = (1 << numUcntAPs);
    int nextIndex = data.noStates;
    // we need to store vertices and edges somehow:
    PGSVertices* states = initPGSVertices(data.noStates);
    PGSVertices* curState = states;
    PGSVertices* partVals = initPGSVertices(data.noStates * numValuations);
    PGSVertices* curPartVal = partVals;
    PGSVertices* fullVals = NULL;
    PGSVertices* curFullVal = NULL;
    for (int i = 0; i < data.noStates; i++) {
        State* state = &(data.states[i]);
        int firstSucc = nextIndex;
        nextIndex += numValuations;
        for (unsigned value = 0; value < numValuations; value++) {
            IntList* validVals = NULL;
            for (int j = 0; j < state->noTrans; j++) {
                Transition* trans = &(state->transitions[j]);
                // there should be a single successor per transition
                assert(trans->noSucc == 1);
                // there should be a label at state or transition level
                BTree* label;
                if (state->label != NULL)
                    label = state->label;
                else
                    label = trans->label;
                assert(label != NULL);
                // there should be a priority at state or transition level
                int* acc = state->accSig;
                int noAcc = state->noAccSig;
                if (state->accSig == NULL) {
                    acc = trans->accSig;
                    noAcc = trans->noAccSig;
                }
                // one of the two should be non-NULL
                // and there should be exactly one acceptance set!
                assert(acc != NULL && noAcc == 1);
                int priority = adjustPriority(acc[0], isMaxParity, resGoodPriority,
                                              data.noAccSets);
                // we add a vertex + edges if the transition is compatible with the
                // valuation we are currently considering; because of PGSolver
                // format we add only the leaving edge to a state and defer
                // edges to it (from partial valuations) to later
                int evald = evalLabel(label, data.aliases, data.noAliases,
                                      numUcntAPs, ucntAPs, value);
#ifndef NDEBUG
                fprintf(stderr, "Called evalLabel for value %d with "
                                "%d uncontrollable APs; got %d\n",
                        value, numUcntAPs, evald);
#endif
                if (evald != -1) {
                    int fval = nextIndex;
                    nextIndex++;
                    // as unique successor we add the successor via the
                    // transition (so the choice of player is unimportant)
                    if (curFullVal == NULL) {
                        assert(fullVals == NULL);
                        fullVals = malloc(sizeof(PGSVertices));
                        curFullVal = fullVals;
                    } else {
                        assert(fullVals != NULL);
                        curFullVal->next = malloc(sizeof(PGSVertices));
                        curFullVal = curFullVal->next;
                    }
                    assert(curFullVal != NULL);
                    curFullVal->id = fval;
                    curFullVal->priority = priority;
                    curFullVal->owner = 0;
                    curFullVal->name = NULL;
                    curFullVal->next = NULL;
                    curFullVal->successors = prependIntNode(NULL, trans->successors[0]);
                    // we also keep it as a list for the successors of the
                    // partial value
                    validVals = prependIntNode(validVals, fval);
                }
            }
            assert(validVals != NULL);
            assert(curPartVal != NULL);
            // Now we can add priority-0 edges from the player-0
            // partial-valuation vertices to full-valuation vertices
            curPartVal->id = firstSucc + value;
            curPartVal->priority = 0;
            curPartVal->owner = 0;
            curPartVal->successors = validVals;
            curPartVal = curPartVal->next;
        }
        // Now we can add priority-0 edges from the player-1 vertex to
        // all partial-valuation vertices owned by player 0
        assert(curState != NULL);
        curState->id = state->id;
        curState->priority = 0;
        curState->owner = 1;
        for (int succ = firstSucc; succ < firstSucc + numValuations; succ++) 
            curState->successors = prependIntNode(curState->successors, succ);
        curState->name = state->name;
        curState = curState->next;
    }

    // Step 3: print the PGSolver file
    printf("parity %d;\n", nextIndex - 1);
    printPGSVertices(states);
#ifndef NDEBUG
    fprintf(stderr, "Partial valuation vertices:\n");
#endif
    printPGSVertices(partVals);
#ifndef NDEBUG
    fprintf(stderr, "Full valuation vertices:\n");
#endif
    printPGSVertices(fullVals);

    // Free dynamic memory
    resetHoa(&data);
    deletePGSVertices(states);
    deletePGSVertices(partVals);
    deletePGSVertices(fullVals);
    return EXIT_SUCCESS;
}
