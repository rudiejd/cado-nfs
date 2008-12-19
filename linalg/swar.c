/* 
 * Program: history
 * Author : F. Morain
 * Purpose: data structure for elimination
 * 
 * Algorithm:
 *
 */

#include "utils/utils.h"
#include "sparse.h"
#include "dclist.h"
#include "sparse_mat.h"
#include "swar.h"
#include "merge_mono.h"

/* Initializes the SWAR data structure.
   Inputs:
      mat->ncols - number of columns of matrix mat
      mat->wt[j] - weight of column j, for 0 <= j < mat->ncols
      mat->cwmax - weight bound
      mat->S[w]  - lists containing only one element (-1), 0 <= w <= mat->cwmax
   Outputs:
      mat->S[w]  - doubly-chained list with columns j of weight w
 */
void
fillSWAR(sparse_mat_t *mat)
{
    INT j, *Rj, jmin = mat->jmin, jmax = mat->jmax;

    for(j = jmin; j < jmax; j++){
#  if DEBUG >= 1
	fprintf(stderr, "Treating column %d\n", j);
#  endif
	if(mat->wt[GETJ(mat, j)] <= mat->cwmax){
	    mat->A[GETJ(mat, j)] = dclistInsert(mat->S[mat->wt[GETJ(mat, j)]], j);
#  if DEBUG >= 1
	    fprintf(stderr, "Inserting %d in S[%d]:", j, mat->wt[GETJ(mat, j)]);
	    dclistPrint(stderr, mat->S[mat->wt[GETJ(mat, j)]]->next);
	    fprintf(stderr, "\n");
#  endif
#ifndef USE_COMPACT_R
	    Rj = (INT *)malloc((mat->wt[GETJ(mat, j)]+1) * sizeof(INT));
	    Rj[0] = 0; // last index used
	    mat->R[GETJ(mat, j)] = Rj;
#else
	    fprintf(stderr, "R: NYI in fillSWAR\n");
	    exit(1);
#endif
	}
	else{
#if USE_MERGE_FAST <= 1
	    mat->wt[GETJ(mat, j)] = -1;
#else
	    mat->wt[GETJ(mat, j)] = -mat->wt[GETJ(mat, j)]; // trick!!!
#endif
	    mat->A[GETJ(mat, j)] = NULL; // TODO: renumber j's?????
#ifndef USE_COMPACT_R
	    mat->R[GETJ(mat, j)] = NULL;
#else
            fprintf(stderr, "R: NYI2 in fillSWAR\n");
            exit(1);
#endif
	}
    }
}

// TODO
void
closeSWAR(/*sparse_mat_t *mat*/)
{
}

// dump for debugging reasons
void
printSWAR(sparse_mat_t *mat, int ncols)
{
    int j, w;
    INT k;

    fprintf(stderr, "===== S is\n");
    for(w = 0; w <= mat->cwmax; w++){
	fprintf(stderr, "  S[%d] ", w);
	dclistPrint(stderr, mat->S[w]->next);
	fprintf(stderr, "\n");
    }
    fprintf(stderr, "===== Wj is\n");
    for(j = 0; j < ncols; j++)
	fprintf(stderr, "  Wj[%d]=%d\n", j, mat->wt[GETJ(mat, j)]);
    fprintf(stderr, "===== R is\n");
#ifndef USE_COMPACT_R
    for(j = 0; j < ncols; j++){
	if(mat->R[GETJ(mat, j)] == NULL)
	    continue;
	fprintf(stderr, "  R[%d]=", j);
	for(k = 1; k <= mat->R[GETJ(mat, j)][0]; k++)
          fprintf(stderr, " %ld", (long int) mat->R[GETJ(mat, j)][k]);
	fprintf(stderr, "\n");
    }
#else
	fprintf(stderr, "R: NYI in printSWAR\n");
	exit(1);
#endif
}

// w(j) has decreased in such a way that it can be incorporated in the
// SWAR structure. We need find all the info we need to do that. Note this
// can be costly. We must not store row index i0, since j was discovered
// when row i0 was treated.
void
incorporateColumn(sparse_mat_t *mat, INT j, int i0)
{
    int i, ni, wj;
    INT *Rj;

    wj = -mat->wt[GETJ(mat, j)];
#ifndef USE_COMPACT_R
    Rj = (INT *)malloc((wj+1) * sizeof(INT));
    // find all rows in which j appears
    for(i = 0, ni = 1; i < mat->nrows; i++)
	if(!isRowNull(mat, i))
	    if((i != i0) && hasCol(mat->rows, i, j))
#if 1
		Rj[ni++] = i;
#else
                ni++;
    fprintf(stderr, "iC: %d %d\n", j, ni);
#endif
    Rj[0] = ni-1;
    mat->R[GETJ(mat, j)] = Rj;
#else
    fprintf(stderr, "R: NYI in incorporateColumn\n");
    exit(1);
#endif
    mat->wt[GETJ(mat, j)] = wj;
    ASSERT(wj == Rj[0]);
    mat->A[GETJ(mat, j)] = dclistInsert(mat->S[wj], j);
}

int
getNextj(dclist dcl)
{
    INT j;
    dclist foo;

    foo = dcl->next;
    j = foo->j;
    dcl->next = foo->next;
    if(foo->next != NULL)
	foo->next->prev = dcl;
    free(foo);
    return j;
}

void
remove_j_from_S(sparse_mat_t *mat, int j)
{
    dclist dcl = mat->A[GETJ(mat, j)], foo;

    if(dcl == NULL){
	fprintf(stderr, "Column %d already removed?\n", j);
	return;
    }
#if DEBUG >= 2
    int ind = mat->wt[GETJ(mat, j)];
    if(ind > mat->cwmax)
        ind = mat->cwmax+1;
    fprintf(stderr, "S[%d]_b=", ind);
    dclistPrint(stderr, mat->S[ind]->next); fprintf(stderr, "\n");
    fprintf(stderr, "dcl="); dclistPrint(stderr, dcl); fprintf(stderr, "\n");
#endif
    foo = dcl->prev;
    foo->next = dcl->next;
    if(dcl->next != NULL)
	dcl->next->prev = foo;
#if DEBUG >= 2
    fprintf(stderr, "S[%d]_a=", ind);
    dclistPrint(stderr, mat->S[ind]->next); fprintf(stderr, "\n");
#endif
#if USE_CONNECT == 0
    free(dcl);
#endif
}

void
remove_j_from_SWAR(sparse_mat_t *mat, int j)
{
    remove_j_from_S(mat, j);
#if USE_CONNECT
    free(mat->A[GETJ(mat, j)]);
#endif
    mat->A[GETJ(mat, j)] = NULL;
    mat->wt[GETJ(mat, j)] = 0;
    destroyRj(mat, j);
}

int
decrS(int w)
{
#if USE_MERGE_FAST <= 1
    return w-1;
#else
    return (w >= 0 ? w-1 : w+1);
#endif
}

int
incrS(int w)
{
#if USE_MERGE_FAST <= 1
    return w+1;
#else
    return (w >= 0 ? w+1 : w-1);
#endif
}

/* remove the cell (i,j), and updates matrix correspondingly.
   Note: A[j] contains the address of the cell in S[w] where j is stored.
   
   Updates:
   - mat->wt[j] (weight of column j)
   - mat->S[w] : cell j is removed, with w = weight(j)
   - mat->S[w-1] : cell j is added
   - A[j] : points to S[w-1] instead of S[w]
*/
void
removeCellSWAR(sparse_mat_t *mat, int i, INT j)
{
    int ind;

#if TRACE_ROW >= 0
    if(i == TRACE_ROW){
	fprintf(stderr, "TRACE_ROW: removeCellSWAR i=%d j=%d\n", i, j);
    }
#endif
    // update weight
#if DEBUG >= 1
    fprintf(stderr, "removeCellSWAR: moving j=%d from S[%d] to S[%d]\n",
	    j, mat->wt[GETJ(mat, j)], decrS(mat->wt[GETJ(mat, j)]));
#endif
#if USE_MERGE_FAST > 1
    if(mat->wt[GETJ(mat, j)] < 0){
	// if mat->wt[j] is already < 0, we don't care about
	// decreasing, updating, etc. except when > 2
# if USE_MERGE_FAST > 2
	ind = mat->wt[GETJ(mat, j)] = decrS(mat->wt[GETJ(mat, j)]);
	// we incorporate the column and update the data structure
	if(abs(ind) <= mat->mergelevelmax){
#  if DEBUG >= 1
	    fprintf(stderr, "WARNING: column %d becomes light at %d...!\n",
		    j, abs(ind));
#  endif
	    incorporateColumn(mat, j, i);
	}
# endif
	return;
    }
#endif
    // at this point, we should have mat->wt[j] > 0
    ind = mat->wt[GETJ(mat, j)] = decrS(mat->wt[GETJ(mat, j)]);
    remove_j_from_S(mat, j);
    if(mat->wt[GETJ(mat, j)] > mat->cwmax)
	ind = mat->cwmax+1;
    // update A[j]
#if DEBUG >= 2
    fprintf(stderr, "S[%d]_b=", ind);
    dclistPrint(stderr, mat->S[ind]->next); fprintf(stderr, "\n");
#endif
    // TODO: replace this with a move of pointers...!
#if USE_CONNECT == 0
    mat->A[GETJ(mat, j)] = dclistInsert(mat->S[ind], j);
#else
    dclistConnect(mat->S[ind], mat->A[GETJ(mat, j)]);
#endif
#if DEBUG >= 2
    fprintf(stderr, "S[%d]_a=", ind);
    dclistPrint(stderr, mat->S[ind]->next); fprintf(stderr, "\n");
#endif
    // update R[j] by removing i
    remove_i_from_Rj(mat, i, j);
}

// M[i, j] is set to 1.
void
addCellSWAR(sparse_mat_t *mat, int i, INT j)
{
    int ind;

    // update weight
#if DEBUG >= 1
    fprintf(stderr, "addCellSWAR: moving j=%d from S[%d] to S[%d]\n",
	    j, mat->wt[GETJ(mat, j)], incrS(mat->wt[GETJ(mat, j)]));
#endif
    ind = mat->wt[GETJ(mat, j)] = incrS(mat->wt[GETJ(mat, j)]);
#if USE_MERGE_FAST > 1
    if(ind < 0)
	return;
#endif
    remove_j_from_S(mat, j);
    if(mat->wt[GETJ(mat, j)] > mat->cwmax){
#if DEBUG >= 1
	fprintf(stderr, "WARNING: column %d is too heavy (%d)\n", j,
		mat->wt[GETJ(mat, j)]);
#endif
	ind = mat->cwmax+1; // trick
    }
    // update A[j]
#if USE_CONNECT == 0
    mat->A[GETJ(mat, j)] = dclistInsert(mat->S[ind], j);
#else
    dclistConnect(mat->S[ind], mat->A[GETJ(mat, j)]);
#endif
    // update R[j] by adding i
    // TODO: this is more or less independant of SWAR...!
    // but take care to the return stuff above!
    add_i_to_Rj(mat, i, j);
}

int
deleteEmptyColumns(sparse_mat_t *mat)
{
#if 0
    return deleteAllColsFromStack(mat, 0);
#else
    dclist dcl = mat->S[0], foo;
    int njrem = 0;
    INT j;

    while(dcl->next != NULL){
	foo = dcl->next;
	j = foo->j;
	dcl->next = foo->next;
	free(foo);
	njrem++;
	mat->A[GETJ(mat, j)] = NULL;
	mat->wt[GETJ(mat, j)] = 0;
	destroyRj(mat, j);
    }
    mat->rem_ncols -= njrem;
    return njrem;
#endif
}

