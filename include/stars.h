#ifndef _STARS_H_
#define _STARS_H_

#include <sys/types.h>

//! typedef for structure array
typedef struct array Array;
typedef struct starsh_problem STARSH_problem;
typedef struct starsh_cluster STARSH_cluster;
typedef struct starsh_blrf STARSH_blrf;
typedef struct starsh_blrm STARSH_blrm;
typedef void (*STARSH_kernel)(int nrows, int ncols, int *irow, int *icol,
        void *row_data, void *col_data, void *result);

//! Enum type to show actual block low-rank format
typedef enum {STARSH_TILED, STARSH_H, STARSH_HODLR}
    STARSH_blrf_type;

//! Enum type to show type of clusterization
typedef enum {STARSH_PLAIN, STARSH_HIERARCHICAL}
    STARSH_cluster_type;


struct array
//! `N`-dimensional array.
/*! Simplifies debugging. */
{
    int ndim;
    //!< Number of dimensions of array.
    int *shape;
    //!< Shape of array.
    ssize_t *stride;
    //!< Strides of array
    char order;
    //!< Ordering of array.
    /*!< `'C'` for C order (row-major), `'F'` for Fortran order
     * (column-major). */
    size_t size;
    //!< Number of elements of array.
    char dtype;
    //!< Precision of array.
    /*!< Possile value is `'s'`, `'d'`, `'c'` or `'z'`, much like
     * in names of **LAPACK** routines. */
    size_t dtype_size;
    //!< Size of one element of array in bytes.
    size_t nbytes;
    //!< Size of data buffer and array structure together in bytes.
    size_t data_nbytes;
    //!< Size of data buffer in bytes.
    void *data;
    //!< Pointer to data buffer.
};

int array_from_buffer(Array **A, int ndim, int *shape, char dtype,
        char order, void *buffer);
int array_new(Array **A, int ndim, int *shape, char dtype, char order);
int array_new_like(Array **A, Array *B);
int array_new_copy(Array **A, Array *B, char order);
int array_free(Array *A);
int array_info(Array *A);
int array_print(Array *A);
int array_init(Array *A, char *kind);
int array_init_randn(Array *A);
int array_init_rand(Array *A);
int array_init_zeros(Array *A);
int array_init_ones(Array *A);
int array_to_matrix(Array *A, char kind);
int array_trans_inplace(Array *A);
int array_dot(Array* A, Array *B, Array **C);
int array_SVD(Array *A, Array **U, Array **S, Array **V);
int SVD_get_rank(Array *S, double tol, char type, int *rank);
int array_scale(Array *A, char kind, Array *S);
int array_diff(Array *A, Array *B, double *result);
int array_norm(Array *A, double *result);
int array_convert(Array **A, Array *B, char dtype);
int array_cholesky(Array *A, char uplo);


struct starsh_problem
//! Container to store everything about given problem.
/*! Structure, storing all the necessary data for reconstruction of
 * array, generated by given kernel. This array may be not
 * `2`-dimensional (e.g. for astrophysics problem, where each matrix
 * entry is a vector of `3` elements). Array elements are not stored in
 * memory, but computed on demand. Rows correspond to first dimension
 * of the array and columns correspond to last dimension of the array. */
{
    int ndim;
    //!< Number of dimensions of corresponding problem.
    /*!< Real dimensionality of corresponding array. `ndim=2` for
     * problems with scalar kernel. `ndim=3` for astrophysics problem.
     * Can not be less, than `2`. */
    int *shape;
    //!< Shape of corresponding array.
    /*!< In case of non-scalar kernel `shape[0]` stands for number of
     * rows of array, `shape[ndim-1]` stands for number of columns of
     * array and `(ndim-2)`-dimensional tuple `shape[1:ndim-2]` stands
     * for kernel shape. */
    char symm;
    //!< `'S'` if problem is symmetric, and `'N'` otherwise.
    char dtype;
    //!< Precision of problem and corresponding array.
    /*!< Possile value is `'s'`, `'d'`, `'c'` or `'z'`, much like in
     * names of **LAPACK** routines. */
    size_t dtype_size;
    //!< Size of element of array in bytes.
    size_t entry_size;
    //!< Size of subarray, corresponding to kernel shape, in bytes.
    /*!< Corresponds to size of subarray on a single row on a single
     * column. Equal to size of array element, multiplied by total
     * number of elements and divided by number of rows and number of
     * columns. */
    void *row_data;
    //!< Pointer to data, corresponding to rows.
    void *col_data;
    //!< Pointer to data, corresponding to columns.
    STARSH_kernel kernel;
    //!< Pointer to a kernel.
    /*!< Kernel computes elements of a submatrix on intersection of
     * given rows and columns. Rows stand for first dimension and
     * columns stand for last dimension. */
    char *name;
    //!< Name of corresponding problem.
};

int starsh_problem_new(STARSH_problem **P, int ndim, int *shape, char symm,
        char dtype, void *row_data, void *col_data, STARSH_kernel kernel,
        char *name);
int starsh_problem_free(STARSH_problem *P);
int starsh_problem_info(STARSH_problem *P);
int starsh_problem_get_block(STARSH_problem *P, int nrows, int ncols,
        int *irow, int *icol, Array **A);
int starsh_problem_from_array(STARSH_problem **P, Array *A, char symm);
int starsh_problem_to_array(STARSH_problem *P, Array **A);


struct starsh_cluster
//! Info about clusterization of physical data.
{
    void *data;
    //!< Pointer to structure, holding physical data.
    int ndata;
    //!< Number of discrete elements, corresponding to physical data.
    /*!< Discrete element can be anything, i.e. particles, grid nodes,
     * edges or mesh elements. */
    int *pivot;
    //!< Pivoting of discrete elements for clusterization.
    /*!< After pivoting discrete elements of a single cluster are one
     * after another. Used by field `start`. */
    int nblocks;
    //!< Total number of subclusters (blocks) of discrete elements.
    int nlevels;
    //!< Number of levels of hierarchy.
    /*!< `0` in case of tiled clusterization. */
    int *level;
    //!< Index of first cluster for each level of hierarchy.
    /*!< All subclusters, corresponding to given level of hierarchy are
     * stored in compressed format: subclusters with indexes from
     * `level[i]` to `level[i+1]-1` inclusively correspond to `i`-th
     * level of hierarchy. Field `level` is an array with `nlevels+1`
     * elements in hierarchical case and is `NULL` in tiled case. */
    int *start;
    //!< Index of first pivoted discrete element of a cluster.
    /*!< Indexes of discrete elements, corresponding to a single
     * cluster, are located one after another in array `pivot`. */
    int *size;
    //!< Number of discrete elements in each cluster.
    int *parent;
    //!< Parent cluster for each subcluster. `-1` for root cluster.
    int *child_start;
    //!< Start index of `child` for each cluster.
    /*!< Clusters with indexes from `child[child_start[i]]` to
     * `child[child_start[i+1]]-1`inclusively are children for a
     * cluster `i`. In case of tiled clusterization `child_start` is
     * `NULL`.*/
    int *child;
    //!< Children clusters of each cluster.
    /*!< Clusters with indexes from `child[child_start[i]]` to
     * `child[child_start[i+1]]-1`inclusively are children for a
     * cluster `i`. In case of tiled clusterization `child` is
     * `NULL`.*/
    STARSH_cluster_type type;
    //!< Type of cluster (tiled or hierarchical).
};

int starsh_cluster_new(STARSH_cluster **C, void *data, int ndata, int *pivot,
        int nblocks, int nlevels, int *level, int *start, int *size,
        int *parent, int *child_start, int *child, STARSH_cluster_type type);
int starsh_cluster_free(STARSH_cluster *cluster);
int starsh_cluster_info(STARSH_cluster *cluster);
int starsh_cluster_new_tiled(STARSH_cluster **C, void *data, int ndata,
        int block_size);


struct starsh_blrf
//! Non-nested block low-rank format.
/*! Stores non-nested division of problem/array into admissible blocks.
 * Some of admissible blocks are low-rank, some are dense. */
{
    STARSH_problem *problem;
    //!< Pointer to a problem.
    char symm;
    //!< `'S'` if format is symmetric, and `'N'` otherwise.
    STARSH_cluster *row_cluster;
    //!< Clusterization of rows into subclusters(blocks).
    STARSH_cluster *col_cluster;
    //!< Clusterization of columns into subclusters(blocks).
    int nbrows;
    //!< Number of block rows (clusters of rows).
    int nbcols;
    //!< Number of block columns (clusters of columns).
    size_t nblocks_far;
    //!< Number of admissible far-field blocks.
    /*!< Far-field blocks are approximated. */
    size_t nblocks_near;
    //!< Number of admissible near-field blocks.
    /*!< Near-field blocks are dense. */
    int *block_far;
    //!< Coordinates of far-field admissible blocks.
    /*!< `block_far[2*i]` is an index of block row (row cluster) and
     * `block_far[2*i+1]` is an index of block column (column cluster).
     * */
    int **block_near;
    //!< Coordinates of near-field admissible blocks.
    /*!< `block_near[2*i]` is an index of block row (row cluster) and
     * `block_near[2*i+1]` is an index of block column (column cluster).
     * */
    size_t *brow_far_start;
    size_t *brow_far;
    // Compressed sparse format to store indexes of admissible far-field blocks
    // for each block row.
    size_t *bcol_far_start;
    size_t *bcol_far;
    // Compressed sparse format to store indexes of admissible far-field blocks
    // for each block column.
    size_t *brow_near_start;
    size_t *brow_near;
    // Compressed sparse format to store indexes of admissible near-field
    // blocks for each block row.
    size_t *bcol_near_start;
    size_t *bcol_near;
    // Compressed sparse format to store indexes of admissible near-field
    // blocks for each block column.
//    MPI_comm comm;
    // MPI communicator. Equal to MPI_COMM_NULL if not using MPI.
//    size_t *block_start, *block;
    // Blocks, corresponding to i-th MPI node, located in array block from
    // index block_start[i] to index block_start[i+1]-1 inclusively.
    STARSH_blrf_type type;
    // Type of format. Possible value is STARS_Tiled, STARS_H or STARS_HODLR.
};

int starsh_blrf_new(STARSH_blrf **F, STARSH_problem *P, char symm,
        STARSH_cluster *R, STARSH_cluster *C, size_t nblocks_far,
        int *block_far, size_t nblocks_near, int *block_near,
        STARSH_blrf_type type);
int starsh_blrf_free(STARSH_blrf *F);
int starsh_blrf_info(STARSH_blrf *F);
int starsh_blrf_print(STARSH_blrf *F);
int starsh_blrf_new_tiled(STARSH_blrf **F, STARSH_problem *P,
        STARSH_cluster *R, STARSH_cluster *C, char symm);
int starsh_blrf_get_block(STARSH_blrf *F, int i, int j, int *shape, void **D);

struct starsh_blrm
// STARS Block Low-Rank Matrix, which is used as an approximation in non-nested
// block low-rank format.
{
    STARSH_blrf *format;
    // Pointer to block low-rank format.
    int *far_rank;
    // Rank of each far-field block.
    Array **far_U, **far_V;
    // Arrays of pointers to factors U and V of each low-rank far-field block
    // and dense A of each dense far-field block.
    int onfly;
    // 1 to store dense blocks, 0 not to store them and compute on demand.
    Array **near_D;
    // Array of pointers to dense A of each near-field block.
    void *alloc_U, *alloc_V, *alloc_D;
    // Pointer to memory buffer, holding buffers of low-rank factors of
    // low-rank blocks and dense buffers of dense blocks
    char alloc_type;
    // Type of memory allocation: '1' for allocating 3 big buffers U_alloc,
    // V_alloc and D_alloc, '2' for allocating many small buffers U, V and D.
    size_t nbytes, data_nbytes;
    // Total number of bytes, consumed by Block Low-Rank Matrix, and total size
    // of only data buffers of corresponding arrays.
};


int starsh_blrm_new(STARSH_blrm **M, STARSH_blrf *F, int *far_rank,
        Array **far_U, Array **far_V, int onfly,
        Array **near_D, void *alloc_U, void *alloc_V,
        void *alloc_D, char alloc_type);
int starsh_blrm_free(STARSH_blrm *M);
int starsh_blrm_info(STARSH_blrm *M);
int starsh_blrm_get_block(STARSH_blrm *M, int i, int j, int *shape, int *rank,
        void **U, void **V, void **D);
int STARS_BLRM_heatmap(STARSH_blrm *M, char *filename);

int starsh_blrm__dsdd(STARSH_blrm **M, STARSH_blrf *F, double tol, int onfly);
int starsh_blrm__dqp3(STARSH_blrm **M, STARSH_blrf *F, int maxrank,
        int oversample, double tol, int onfly);
int starsh_blrm__drsdd(STARSH_blrm **M, STARSH_blrf *F, int maxrank,
        int oversample, double tol, int onfly);
int starsh_blrm__drsdd2(STARSH_blrm **M, STARSH_blrf *F, int maxrank,
        int oversample, double tol, int onfly);
int starsh__dsvfr(int size, double *S, double tol);
int starsh_blrm__dmml(STARSH_blrm *M, int nrhs, double *A, int lda,
        double *B, int ldb);
double starsh_blrm__dfe(STARSH_blrm *M);
int starsh_blrm__dca(STARSH_blrm *M, Array *A);


#endif // _STARSH_H_
