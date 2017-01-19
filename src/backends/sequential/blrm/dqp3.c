#include <stdio.h>
#include <stdlib.h>
#include <mkl.h>
#include "stars.h"
#include "misc.h"

int starsh_blrm__dqp3(STARS_BLRM **M, STARS_BLRF *F, double tol, int onfly)
// Double precision Tile Low-Rank geSDD approximation
{
    STARS_Problem *P = F->problem;
    block_kernel kernel = P->kernel;
    size_t nblocks_far = F->nblocks_far, nblocks_near = F->nblocks_near;
    // Following values default to given block low-rank format F, but they are
    // changed when there are false far-field blocks.
    size_t new_nblocks_far = nblocks_far, new_nblocks_near = nblocks_near;
    int *block_far = F->block_far, *block_near = F->block_near;
    // Places to store low-rank factors, dense blocks and ranks
    Array **far_U = NULL, **far_V = NULL, **near_D = NULL;
    int *far_rank = NULL;
    // Init buffers to store low-rank factors of far-field blocks if needed
    if(nblocks_far > 0)
    {
        STARS_MALLOC(far_U, nblocks_far);
        STARS_MALLOC(far_V, nblocks_far);
        STARS_MALLOC(far_rank, nblocks_far);
    }
    // Shortcuts to information about clusters
    STARS_Cluster *R = F->row_cluster, *C = F->col_cluster;
    void *RD = R->data, *CD = C->data;
    // Work variables
    int info;
    size_t bi, bj = 0;
    // Simple cycle over all far-field admissible blocks
    for(bi = 0; bi < nblocks_far; bi++)
    {
        // Get indexes of corresponding block row and block column
        int i = block_far[2*bi];
        int j = block_far[2*bi+1];
        // Get corresponding sizes and minimum of them
        int nrows = R->size[i];
        int ncols = C->size[j];
        int mn = nrows < ncols ? nrows : ncols;
        //int mn2 = mn < maxrank ? mn : maxrank;
        int mn2 = mn/2+10;
        if(mn2 > mn)
            mn2 = mn;
        // Get size of temporary arrays
        size_t lwork = 3*ncols+1, lwork_sdd = (4*(size_t)mn+7)*mn;
        if(lwork_sdd > lwork)
            lwork = lwork_sdd;
        size_t liwork = ncols, liwork_sdd = 8*mn;
        if(liwork_sdd > liwork)
            liwork = liwork_sdd;
        double *U, *V, *work, *U2, *V2, *tau, *svd_U, *svd_S, *svd_V;
        int *iwork, *ipiv;
        size_t D_size = (size_t)nrows*(size_t)ncols;
        // Allocate temporary arrays
        STARS_MALLOC(U, D_size);
        STARS_MALLOC(V, (size_t)mn2*(size_t)ncols);
        STARS_MALLOC(iwork, liwork);
        ipiv = iwork;
        STARS_MALLOC(work, lwork);
        STARS_MALLOC(tau, mn);
        STARS_MALLOC(svd_U, (size_t)nrows*(size_t)mn2);
        STARS_MALLOC(svd_S, mn2);
        STARS_MALLOC(svd_V, (size_t)mn2*(size_t)ncols);
        // Compute elements of a block
        kernel(nrows, ncols, R->pivot+R->start[i], C->pivot+C->start[j],
                RD, CD, U);
        // Set pivots for GEQP3 to zeros
        for(int k = 0; k < ncols; k++)
            ipiv[k] = 0;
        // Call GEQP3
        LAPACKE_dgeqp3_work(LAPACK_COL_MAJOR, nrows, ncols, U, nrows, ipiv,
                tau, work, lwork);
        // Copy R factor to V
        for(size_t k = 0; k < ncols; k++)
        {
            size_t kk = ipiv[k]-1;
            size_t l = k < mn2 ? k+1 : mn2;
            cblas_dcopy(l, U+k*nrows, 1, V+kk*mn2, 1);
            //for(size_t ll = 0; ll < l; ll++)
            //    V[kk*mn2+ll] = U[k*nrows+ll];
            for(size_t ll = l; ll < mn2; ll++)
                V[kk*mn2+ll] = 0.;
        }
        LAPACKE_dorgqr_work(LAPACK_COL_MAJOR, nrows, mn2, mn2, U, nrows, tau,
                work, lwork);
        free(tau);
        LAPACKE_dgesdd_work(LAPACK_COL_MAJOR, 'S', mn2, ncols, V, mn2, svd_S,
                svd_U, mn2, svd_V, mn2, work, lwork, iwork);
        free(work);
        free(iwork);
        // Get rank, corresponding to given error tolerance
        int rank = starsh__dsvfr(mn2, svd_S, tol);
        //int rank = mn;
        //if(i == j)
        //    rank = mn;
        if(rank < mn/2)
        // If far-field block is low-rank
        {
            far_rank[bi] = rank;
            int shapeU[2] = {nrows, rank}, shapeV[2] = {ncols, rank};
            Array_new(far_U+bi, 2, shapeU, 'd', 'F');
            Array_new(far_V+bi, 2, shapeV, 'd', 'F');
            U2 = far_U[bi]->data;
            V2 = far_V[bi]->data;
            cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, nrows, rank,
                    mn2, 1.0, U, nrows, svd_U, mn2, 0.0, U2, nrows);
            for(size_t k = 0; k < rank; k++)
            {
                //cblas_dcopy(nrows, U+k*nrows, 1, U2+k*nrows, 1);
                cblas_dcopy(ncols, svd_V+k, mn2, V2+k*ncols, 1);
                cblas_dscal(ncols, svd_S[k], V2+k*ncols, 1);
            }
        }
        else
        // If far-field block is dense, although it was initially assumed
        // to be low-rank. Let denote such a block as false far-field block
        {
            far_rank[bi] = -1;
            far_U[bi] = NULL;
            far_V[bi] = NULL;
        }
        // Free temporary arrays
        free(U);
        free(V);
        free(svd_U);
        free(svd_S);
        free(svd_V);
    }
    // Get number of false far-field blocks
    size_t nblocks_false_far = 0;
    size_t *false_far = NULL;
    for(bi = 0; bi < nblocks_far; bi++)
        if(far_rank[bi] == -1)
            nblocks_false_far++;
    if(nblocks_false_far > 0)
    {
        // IMPORTANT: `false_far` must to be in ascending order for later code
        // to work normally
        STARS_MALLOC(false_far, nblocks_false_far);
        bj = 0;
        for(bi = 0; bi < nblocks_far; bi++)
            if(far_rank[bi] == -1)
                false_far[bj++] = bi;
    }
    // Update lists of far-field and near-field blocks using previously
    // generated list of false far-field blocks
    if(nblocks_false_far > 0)
    {
        // Update list of near-field blocks
        new_nblocks_near = nblocks_near+nblocks_false_far;
        STARS_MALLOC(block_near, 2*new_nblocks_near);
        // At first get all near-field blocks, assumed to be dense
        for(bi = 0; bi < 2*nblocks_near; bi++)
            block_near[bi] = F->block_near[bi];
        // Add false far-field blocks
        for(bi = 0; bi < nblocks_false_far; bi++)
        {
            size_t bj = false_far[bi];
            block_near[2*(bi+nblocks_near)] = F->block_far[2*bj];
            block_near[2*(bi+nblocks_near)+1] = F->block_far[2*bj+1];
        }
        // Update list of far-field blocks
        new_nblocks_far = nblocks_far-nblocks_false_far;
        if(new_nblocks_far > 0)
        {
            STARS_MALLOC(block_far, 2*new_nblocks_far);
            bj = 0;
            for(bi = 0; bi < nblocks_far; bi++)
            {
                // `false_far` must be in ascending order for this to work
                if(false_far[bj] == bi)
                {
                    bj++;
                }
                else
                {
                    block_far[2*(bi-bj)] = F->block_far[2*bi];
                    block_far[2*(bi-bj)+1] = F->block_far[2*bi+1];
                }
            }
        }
        // Update format by creating new format
        STARS_BLRF *F2;
        info = STARS_BLRF_new(&F2, P, F->symm, R, C, new_nblocks_far,
                block_far, new_nblocks_near, block_near, F->type);
        // Swap internal data of formats and free unnecessary data
        STARS_BLRF tmp_blrf = *F;
        *F = *F2;
        *F2 = tmp_blrf;
        STARS_WARNING("`F` was modified due to false far-field blocks");
        info = STARS_BLRF_free(F2);
        if(info != 0)
            return info;
    }
    // Compute near-field blocks if needed
    if(onfly == 0 && new_nblocks_near > 0)
    {
        STARS_MALLOC(near_D, new_nblocks_near);
        // For each near-field block compute its elements
        for(bi = 0; bi < new_nblocks_near; bi++)
        {
            // Get indexes of corresponding block row and block column
            int i = block_near[2*bi];
            int j = block_near[2*bi+1];
            // Get corresponding sizes and minimum of them
            int nrows = R->size[i];
            int ncols = C->size[j];
            int shape[2] = {nrows, ncols};
            Array_new(near_D+bi, 2, shape, 'd', 'F');
            kernel(nrows, ncols, R->pivot+R->start[i], C->pivot+C->start[j],
                    RD, CD, near_D[bi]->data);
        }
    }
    // Change sizes of far_rank, far_U and far_V if there were false
    // far-field blocks
    if(nblocks_false_far > 0 && new_nblocks_far > 0)
    {
        bj = 0;
        for(bi = 0; bi < nblocks_far; bi++)
        {
            if(false_far[bj] == bi)
                bj++;
            else
            {
                far_U[bi-bj] = far_U[bi];
                far_V[bi-bj] = far_V[bi];
                far_rank[bi-bj] = far_rank[bi];
            }
        }
        STARS_REALLOC(far_rank, new_nblocks_far);
        STARS_REALLOC(far_U, new_nblocks_far);
        STARS_REALLOC(far_V, new_nblocks_far);
    }
    // If all far-field blocks are false, then dealloc buffers
    if(new_nblocks_far == 0 && nblocks_far > 0)
    {
        block_far = NULL;
        free(far_rank);
        far_rank = NULL;
        free(far_U);
        far_U = NULL;
        free(far_V);
        far_V = NULL;
    }
    // Dealloc list of false far-field blocks if it is not empty
    if(nblocks_false_far > 0)
        free(false_far);
    // Finish with creating instance of Block Low-Rank Matrix with given
    // buffers
    return STARS_BLRM_new(M, F, far_rank, far_U, far_V, onfly, near_D,
            NULL, NULL, NULL, '2');
}

