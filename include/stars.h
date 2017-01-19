#ifndef _STARS_H_
#define _STARS_H_

#include <sys/types.h>
//#include <mpi.h>

struct timespec stars_gettime();
double stars_timer_delay(struct timespec t1, struct timespec t2);

typedef struct Array Array;
typedef struct List List;
typedef struct STARS_Problem STARS_Problem;
typedef struct STARS_Cluster STARS_Cluster;
typedef struct STARS_BLRF STARS_BLRF;
typedef struct STARS_BLRM STARS_BLRM;
typedef int (*block_kernel)(int nrows, int ncols, int *irow, int *icol,
        void *row_data, void *col_data, void *result);

typedef enum {STARS_Dense, STARS_LowRank, STARS_Unknown} STARS_BlockStatus;
// Enum type to show lowrankness of admissible blocks

typedef enum {STARS_BLRF_Tiled, STARS_BLRF_H, STARS_BLRF_HODLR}
    STARS_BLRF_Type;
// Enum type to show format type (tiled, hierarchical HOLDR or hierarchical H).

typedef enum {STARS_ClusterTiled, STARS_ClusterHierarchical}
    STARS_ClusterType;
// Enum type to show type of clusterization.

struct Array
// N-dimensional A
{
    int ndim;
    // Number of dimensions of A.
    int *shape;
    // Shape of A.
    ssize_t *stride;
    // Size of step to increase value of corresponding axis by 1
    char order;
    // C or Fortran order. C-order means stride is descending, Fortran-order
    // means stride is ascending.
    size_t size;
    // Number of elements of an A.
    char dtype;
    // Data type of each element of A. Possile value is 's', 'd', 'c' or
    // 'z', much like in names of LAPACK routines.
    size_t dtype_size;
    // Size in bytes of each element of a matrix
    size_t nbytes, data_nbytes;
    // Size of entire Array and only data buffer in bytes.
    void *data;
    // Buffer, containing A. Stored in Fortran style.
};

// Routines to work with N-dimensional As
int Array_from_buffer(Array **A, int ndim, int *shape, char dtype,
        char order, void *buffer);
// Init A from given buffer. Check if all parameters are good and
// proceed.
int Array_new(Array **A, int ndim, int *shape, char dtype, char order);
// Allocation of memory for A
int Array_new_like(Array **A, Array *B);
// Initialize new A with exactly the same shape, dtype and so on, but
// with a different memory buffer
int Array_new_copy(Array **A, Array *B, char order);
// Create copy of A with given data layout or keeping layout if order ==
// 'N'
int Array_free(Array *A);
// Free memory, consumed by A structure and buffer
int Array_info(Array *A);
// Print all the data from Array structure
int Array_print(Array *A);
// Print elements of A, different rows of A are printed on different
// rows of output
int Array_init(Array *A, char *kind);
// Init buffer in a special manner: randn, rand, ones or zeros
int Array_init_randn(Array *A);
// Init buffer of A with random numbers of normal (0,1) distribution
int Array_init_rand(Array *A);
// Init buffer with random numbers of uniform [0,1] distribution
int Array_init_zeros(Array *A);
// Set all elements to 0.0
int Array_init_ones(Array *A);
// Set all elements to 1.0
int Array_tomatrix(Array *A, char kind);
// Convert N-dimensional A to 2-dimensional A (matrix) by
// collapsing dimensions. This collapse can be assumed as attempt to look
// at A as at a matrix with long rows (kind == 'R') or long columns
// (kind == 'C'). If kind is 'R', dimensions from 1 to the last are
// collapsed into columns. If kind is 'C', dimensions from 0 to the last
// minus one are collapsed into rows. Example: A of shape (2,3,4,5)
// will be collapsed to A of shape (2,60) if kind is 'R' or to A of
// shape (24,5) if kind is 'C'.
int Array_trans_inplace(Array *A);
// Transposition of A. No real transposition is performed, only changes
// shape, stride and order.
int Array_dot(Array* A, Array *B, Array **C);
// GEMM for two As. Multiplication is performed by last dimension of
// A A and first dimension of A B. These dimensions, data types and
// ordering of both As should be equal.
int Array_SVD(Array *A, Array **U, Array **S, Array **V);
// Compute short SVD of 2-dimensional A
int SVD_get_rank(Array *S, double tol, char type, int *rank);
// Returns rank by given A of singular values, tolerance and type of norm
// ('2' for spectral norm, 'F' for Frobenius norm)
int Array_scale(Array *A, char kind, Array *S);
// Apply row or column scaling to A
int Array_diff(Array *A, Array *B, double *result);
// Measure Frobenius error of approximation of A by B
int Array_norm(Array *A, double *result);
// Measure Frobenius norm of A
int Array_convert(Array **A, Array *B, char dtype);
// Copy A and convert data type
int Array_Cholesky(Array *A, char uplo);
// Cholesky factoriation for an A


struct STARS_Problem
// Structure, storing all the necessary data for reconstruction of matrix,
// generated by given kernel. This matrix may be not 2-dimensional (e.g. for
// Astrophysics problem, where each matrix entry is a vector of 3 elements.
// Matrix elements are not stored in memory, but computed on demand.
// Rows correspond to first dimension of the A and columns correspond to
// last dimension of the A.
{
    int ndim;
    // Real dimensionality of corresponding A. ndim=2 for problems with
    // scalar kernel. ndim=3 for Astrophysics problem. May be greater than 2.
    int *shape;
    // Real shape of corresponding A.
    char symm;
    // 'S' if problem is symmetric, and 'N' otherwise.
    char dtype;
    // Possible values are 's', 'd', 'c' or 'z', just as in LAPACK routines
    // names.
    size_t dtype_size;
    // Size of data type in bytes (size of scalar element of A).
    size_t entry_size;
    // Size of matrix entry in bytes (size of subA, corresponding to
    // matrix entry on a given row on a given column). Equal to dtype_size,
    // multiplied by total number of elements and divided by number of rows
    // and by number of columns.
    void *row_data, *col_data;
    // Pointers to physical data, corresponding to rows and columns.
    block_kernel kernel;
    // Pointer to a function, returning submatrix on intersection of
    // given rows and columns. Rows stand for first dimension, columns stand
    // for last dimension.
    char *name;
    // Name of problem, useful for printing additional info. It is up to user
    // to set it as desired.
};

int STARS_Problem_new(STARS_Problem **P, int ndim, int *shape, char symm,
        char dtype, void *row_data, void *col_data, block_kernel kernel,
        char *name);
// Init for STARS_Problem instance
// Parameters:
//   ndim: dimensionality of corresponding A. Equal 2+dimensionality of
//     kernel
//   shape: shape of corresponding A. shape[1:ndim-2] is equal to shape of
//     kernel
//   symm: 'S' for summetric problem, 'N' for nonsymmetric problem. Symmetric
//     problem require symmetric kernel and equality of row_data and col_data
//   dtype: data type of the problem. Equal to 's', 'd', 'c' or 'z' as in
//     LAPACK routines. Stands for data type of an element of a kernel.
//   row_data: pointer to some structure of physical data for rows
//   col_data: pointer to some srructure of physical data for columns
//   kernel: pointer to a function of interaction. More on this is written
//     somewhere else.
//   name: string, containgin name of the problem. Used only to print
//     information about structure problem.
// Returns:
//    STARS_Problem *: pointer to structure problem with proper filling of all
//    the fields of structure.
int STARS_Problem_free(STARS_Problem *P);
// Free memory, consumed by data buffers of data
int STARS_Problem_info(STARS_Problem *P);
// Print some info about Problem
int STARS_Problem_get_block(STARS_Problem *P, int nrows, int ncols, int *irow,
        int *icol, Array **A);
// Get submatrix on given rows and columns (rows=first dimension, columns=last
// dimension)
int STARS_Problem_from_array(STARS_Problem **P, Array *A, char symm);
// Generate STARS_Problem with a given A and flag if it is symmetric
int STARS_Problem_to_array(STARS_Problem *P, Array **A);
// Compute matrix/A, corresponding to the problem


struct STARS_Cluster
// Information on clusterization (hierarchical or plain) of physical data.
{
    void *data;
    // Pointer to structure, holding physical data.
    int ndata;
    // Number of discrete elements, corresponding to physical data (particles,
    // grid nodes or mesh elements).
    int *pivot;
    // Pivoting for clusterization. After applying this pivoting, discrete
    // elements, corresponding to a given cluster, have indexes in a row. pivot
    // has ndata elements.
    int nblocks;
    // Total number of subclusters/blocks of discrete elements.
    int nlevels;
    // Number of levels of hierarchy. 0 in case of tiled cluster.
    int *level;
    // Compressed format to store start points of each level of hierarchy.
    // indexes of subclusters from level[i] to level[i+1]-1 correspond to i-th
    // level of hierarchy. level has nlevels+1 elements in hierarchical case
    // and is NULL in tiled case.
    int *start, *size;
    // Start points in A pivot and corresponding sizes of each subcluster
    // of discrete elements. Since subclusters overlap in hierarchical
    // case, value start[i+1]-start[i] does not represent actual size of
    // subcluster. start and size have nblocks elements.
    int *parent;
    // Array of parents, each node has only one parent. parent[0] = -1 since 0
    // is assumed to be root node. In case of tiled cluster, parent is NULL.
    int *child_start;
    int *child;
    // Arrays of children and start points in A of children of each
    // subcluster. child_start has nblocks+1 elements, child has nblocks
    // elements in case of hierarchical cluster. In case of tiled cluster
    // child and child_start are NULL.
    STARS_ClusterType type;
    // Type of cluster (tiled or hierarchical).
};

int STARS_Cluster_new(STARS_Cluster **C, void *data, int ndata, int *pivot,
        int nblocks, int nlevels, int *level, int *start, int *size,
        int *parent, int *child_start, int *child, STARS_ClusterType type);
// Init for STARS_Cluster instance
// Parameters:
//   data: pointer structure, holding to physical data.
//   ndata: number of discrete elements (particles or mesh elements),
//     corresponding to physical data.
//   pivot: pivoting of clusterization. After applying this pivoting, rows (or
//     columns), corresponding to one block are placed in a row.
//   nblocks: number of blocks/block rows/block columns/subclusters.
//   nlevels: number of levels of hierarchy.
//   level: A of size nlevels+1, indexes of blocks from level[i] to
//     level[i+1]-1 inclusively belong to i-th level of hierarchy.
//   start: start point of of indexes of discrete elements of each
//     block/subcluster in A pivot.
//   size: size of each block/subcluster/block row/block column.
//   parent: A of parents, size is nblocks.
//   child_start: A of start points in A child of each subcluster.
//   child: A of children of each subcluster.
//   type: type of cluster. Tiled with STARS_ClusterTiled or hierarchical with
//     STARS_ClusterHierarchical.
int STARS_Cluster_free(STARS_Cluster *cluster);
// Free data buffers, consumed by clusterization information.
int STARS_Cluster_info(STARS_Cluster *cluster);
// Print some info about clusterization
int STARS_Cluster_new_tiled(STARS_Cluster **C, void *data, int ndata,
        int block_size);
// Plain (non-hierarchical) division of data into blocks of discrete elements.


struct STARS_BLRF
// STARS Block Low-Rank Format, means non-nested division of a matrix/A
// into admissible blocks. Some of admissible blocks are low-rank, some are
// dense.
{
    STARS_Problem *problem;
    // Pointer to a problem.
    char symm;
    // 'S' if format and problem are symmetric, and 'N' otherwise.
    STARS_Cluster *row_cluster, *col_cluster;
    // Clusterization of rows and columns into blocks/subclusters of discrete
    // elements.
    int nbrows, nbcols;
    // Number of block rows/row subclusters and block columns/column
    // subclusters.
    size_t nblocks_far, nblocks_near;
    // Number of admissible far-field blocks and admissible near-field blocks.
    // Far-field blocks can be approximated with low rank, whereas near-field
    // blocks can not.
    int *block_far, *block_near;
    // Indexes of far-field and near-field admissible blocks. block[2*i] and
    // block[2*i+1] are indexes of block row and block column correspondingly.
    size_t *brow_far_start, *brow_far;
    // Compressed sparse format to store indexes of admissible far-field blocks
    // for each block row.
    size_t *bcol_far_start, *bcol_far;
    // Compressed sparse format to store indexes of admissible far-field blocks
    // for each block column.
    size_t *brow_near_start, *brow_near;
    // Compressed sparse format to store indexes of admissible near-field
    // blocks for each block row.
    size_t *bcol_near_start, *bcol_near;
    // Compressed sparse format to store indexes of admissible near-field
    // blocks for each block column.
//    MPI_comm comm;
    // MPI communicator. Equal to MPI_COMM_NULL if not using MPI.
//    size_t *block_start, *block;
    // Blocks, corresponding to i-th MPI node, located in array block from
    // index block_start[i] to index block_start[i+1]-1 inclusively.
    STARS_BLRF_Type type;
    // Type of format. Possible value is STARS_Tiled, STARS_H or STARS_HODLR.
};

int STARS_BLRF_new(STARS_BLRF **F, STARS_Problem *P, char symm,
        STARS_Cluster *R, STARS_Cluster *C, size_t nblocks_far, int *block_far,
        size_t nblocks_near, int *block_near, STARS_BLRF_Type type);
// Initialization of structure STARS_BLRF
// Parameters:
//   problem: pointer to a structure, holding all the information about problem
//   symm: 'S' if problem and division into blocks are both symmetric, 'N'
//     otherwise.
//   row_cluster: clusterization of rows into block rows.
//   col_cluster: clusterization of columns into block columns.
//   nblocks_far: number of admissible far-field blocks.
//   block_far: A of pairs of admissible far-filed block rows and block
//     columns. block_far[2*i] is an index of block row and block_far[2*i+1]
//     is an index of block column.
//   nblocks_near: number of admissible far-field blocks.
//   block_near: A of pairs of admissible near-filed block rows and block
//     columns. block_near[2*i] is an index of block row and block_near[2*i+1]
//     is an index of block column.
//   type: type of block low-rank format. Tiled with STARS_BLRF_Tiled or
//     hierarchical with STARS_BLRF_H or STARS_BLRF_HOLDR.
int STARS_BLRF_free(STARS_BLRF *F);
// Free memory, used by block low rank format (partitioning of A into
// blocks)
void STARS_BLRF_swap(STARS_BLRF *F, STARS_BLRF *F2);
// Swaps content of two BLR formats. Useful when inplace modification of one of
// them is required due to new information (more accurate lists of far-field
// and near-filed blocks)
int STARS_BLRF_info(STARS_BLRF *F);
// Print short info on block partitioning
int STARS_BLRF_print(STARS_BLRF *F);
// Print full info on block partitioning
int STARS_BLRF_new_tiled(STARS_BLRF **F, STARS_Problem *P, STARS_Cluster *R,
        STARS_Cluster *C, char symm);
// Create plain division into tiles/blocks using plain cluster trees for rows
// and columns without actual pivoting
int STARS_BLRF_get_block(STARS_BLRF *F, int i, int j, int *shape, void **D);
// PLEASE CLEAN MEMORY POINTER *D AFTER USE

struct STARS_BLRM
// STARS Block Low-Rank Matrix, which is used as an approximation in non-nested
// block low-rank format.
{
    STARS_BLRF *blrf;
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


int STARS_BLRM_new(STARS_BLRM **M, STARS_BLRF *F, int *far_rank,
        Array **far_U, Array **far_V, int onfly,
        Array **near_D, void *alloc_U, void *alloc_V,
        void *alloc_D, char alloc_type);
// Init procedure for a non-nested block low-rank matrix
int STARS_BLRM_free(STARS_BLRM *M);
// Free memory of a non-nested block low-rank matrix
int STARS_BLRM_info(STARS_BLRM *M);
// Print short info on non-nested block low-rank matrix
int STARS_BLRM_error(STARS_BLRM *M);
int STARS_BLRM_error_ompfor(STARS_BLRM *M);
// Measure error of approximation by non-nested block low-rank matrix
int STARS_BLRM_get_block(STARS_BLRM *M, int i, int j, int *shape, int *rank,
        void **U, void **V, void **D);
// Returns shape of block, its rank and low-rank factors or dense
// representation of a block
int STARS_BLRM_to_matrix(STARS_BLRM *M, Array **A);
int STARS_BLRM_to_matrix_ompfor(STARS_BLRM *M, Array **A);
// Creates copy of Block Low-rank Matrix in dense format
int STARS_BLRM_tiled_compress_algebraic_svd(STARS_BLRM **M, STARS_BLRF *F,
        int fixrank, double tol, int onfly);
int STARS_BLRM_tiled_compress_algebraic_svd_starpu(STARS_BLRM **M, STARS_BLRF *F,
        int fixrank, double tol, int onfly, int maxrank);
// Private function of STARS-H
// Uses SVD to acquire rank of each block, compresses given matrix (given
// by block kernel, which returns submatrices) with relative accuracy tol
// or with given maximum rank (if maxrank <= 0, then tolerance is used)
//int STARS_BRLM_tiled_compress_algebraic_svd_ompfor(STARS_BLRM **M,
int STARS_BLRM_tiled_compress_algebraic_svd_ompfor(STARS_BLRM **M,
        STARS_BLRF *F, int fixrank, double tol, int onfly);
int STARS_BLRM_tiled_compress_algebraic_svd_ompfor_nested(STARS_BLRM **M,
        STARS_BLRF *F, int fixrank, double tol, int onfly, int nthreads_outer,
        int nthreads_inner);

int STARS_BLRM_tiled_compress_algebraic_svd_batched(STARS_BLRM **M,
        STARS_BLRF *F, int fixrank, double tol, int onfly, int maxrank,
        size_t max_buffer_size);
int STARS_BLRM_tiled_compress_algebraic_svd_batched_nested(STARS_BLRM **M,
        STARS_BLRF *F, int fixrank, double tol, int onfly, int maxrank,
        size_t max_buffer_size, int nthreads_outer, int nthreads_inner);

int STARS_BLRM_heatmap(STARS_BLRM *M, char *filename);
// Put all the ranks to a specified file

int starsh_blrm__dsdd(STARS_BLRM **M, STARS_BLRF *F, double tol, int onfly);
int starsh_blrm__dqp3(STARS_BLRM **M, STARS_BLRF *F, double tol, int onfly);
int starsh__dsvfr(int size, double *S, double tol);
int starsh_blrm__dmml(STARS_BLRM *M, int nrhs, double *A, int lda,
        double *B, int ldb);
double starsh_blrm__dfe(STARS_BLRM *M);


#endif // _STARS_H_
