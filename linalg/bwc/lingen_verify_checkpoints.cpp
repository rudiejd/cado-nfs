/* This standalone program checks matrix products produced by lingen
   (as stored in the cp/ subdirectory).

  Example: check -prime ... -dim 13 -k 3 pi.3.2.127 pi.3.127.251 pi.2.2.251

  Optional arguments: [-seed xxx] [-v]
*/

#include "cado.h"
#include "cxx_mpz.hpp"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "gmp-hacks.h"
#include "lingen_submatrix.hpp"
#include "macros.h"
#include "params.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <gmp.h>
#include <istream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#ifdef HAVE_OPENMP
#include <omp.h>
#endif

/* define WARNING to get warning for non-zero padding coefficients */
// #define WARNING

struct
{
    unsigned int m, n;
} bw_parameters;

cxx_mpz prime;          /* prime modulus */
unsigned long lingen_p; /* number of limbs per coefficient */
int k = 1;              /* matrix is cut in k x k submatrices */
gmp_randstate_t state;
int verbose = 0;
unsigned long seed;

struct matrix
{
    unsigned long nrows; /* matrix dimension */
    unsigned long ncols; /* matrix dimension */
    unsigned long k;     /* matrix is cut into k x k submatrices */
    unsigned long deg;   /* degree of coefficients */
    std::vector<cxx_mpz> coeff;

    matrix(unsigned long nrows,
           unsigned int ncols,
           unsigned long k,
           unsigned long deg)
      : nrows(nrows)
      , ncols(ncols)
      , k(k)
      , deg(deg)
      , coeff(nrows * ncols)

    {}
    void zero()
    {
        for (auto& x : coeff)
            x = 0;
    }
};

struct matrix_reader
{
    unsigned long nrows; /* matrix dimension */
    unsigned long ncols; /* matrix dimension */
    unsigned long k;     /* matrix is cut into k x k submatrices */
    unsigned long deg;
    bool reverse = false;
    std::vector<std::ifstream> files;
    std::vector<bool> warned_padding;
    matrix_reader(unsigned long nrows,
                  unsigned int ncols,
                  unsigned long k,
                  unsigned long deg,
                  std::string const& stem,
                  bool reverse)
      : nrows(nrows)
      , ncols(ncols)
      , k(k)
      , deg(deg)
      , reverse(reverse)
      , warned_padding(nrows * ncols, false)
    {
        for (unsigned long i = 0; i < k; i++) {
            for (unsigned long j = 0; j < k; j++) {
                int nij = k * i + j;
                std::string filename;
                if (k > 1) {
                    filename = stem + fmt::format(".{}.data", nij);
                } else {
                    filename = stem + ".single.data";
                }
                files.emplace_back(filename, std::ios_base::in);
                ASSERT_ALWAYS(files.back().good());
            }
        }
    }

    private:
    /* read 1 coefficient (degree idx, or deg-idx in reverse case),
     * from block (block_i, block_j), multiply it by x, and accumulate to
     * M at the right place.
     */
    void read1_accumulate(matrix& M,
                          unsigned long block_i,
                          unsigned long block_j,
                          cxx_mpz const& x,
                          unsigned int idx)
    {
        int nij = k * block_i + block_j;
        ASSERT_ALWAYS(k == M.k);
        ASSERT_ALWAYS(nrows == M.nrows);
        ASSERT_ALWAYS(ncols == M.ncols);
        unsigned int i0, i1;
        unsigned int j0, j1;
        subdivision Srows(M.nrows, k);
        subdivision Scols(M.ncols, k);
        std::tie(i0, i1) = Srows.nth_block(block_i);
        std::tie(j0, j1) = Scols.nth_block(block_j);
        unsigned int nix = Srows.block_size_upper_bound();
        unsigned int njx = Scols.block_size_upper_bound();
        cxx_mpz tmp;
        mpz_realloc2(tmp, lingen_p * mp_bits_per_limb);
        std::ifstream& F(files[nij]);
        if (reverse) {
            F.seekg((deg - idx) * nix * njx * lingen_p * sizeof(mp_limb_t));
            ASSERT_ALWAYS(F.good());
        }
        for (unsigned int di = 0; di < nix; di++) {
            for (unsigned int dj = 0; dj < njx; dj++) {
                ASSERT_ALWAYS((unsigned long)ALLOC(tmp) >= lingen_p);
                SIZ(tmp) = lingen_p;
                size_t sz = lingen_p * sizeof(mp_limb_t);
                ASSERT_ALWAYS(F.read((char*)PTR(tmp), sz));
                MPN_NORMALIZE(PTR(tmp), SIZ(tmp));
                if (i0 + di < i1 && j0 + dj < j1) {
                    cxx_mpz& dst(M.coeff[(i0 + di) * ncols + j0 + dj]);
                    mpz_mul(tmp, tmp, x);
                    mpz_add(tmp, tmp, dst);
                    mpz_mod(dst, tmp, prime);
                } else {
#ifdef WARNING
                    if (SIZ(tmp) != 0 &&
                        !warned_padding[(i0 + di) * ncols + j0 + dj]) {
                        fprintf(
                          stderr,
                          "Warning, padding coefficient %lu,%lu is not zero\n",
                          i0 + di,
                          j0 + dj);
                        warned_padding[(i0 + di) * ncols + j0 + dj] = true;
                    }
#endif
                }
            }
        }
    }

    public:
    /* read 1 coefficient (degree idx, or deg-idx in reverse case),
     * multiply it by x, and accumulate to M at the right place.
     */
    void read1_accumulate(matrix& M, cxx_mpz const& x, unsigned int idx)
    {
        for (unsigned long i = 0; i < k; i++)
            for (unsigned long j = 0; j < k; j++)
                read1_accumulate(M, i, j, x, idx);
    }
};

void mpz_urandomm_nz(mpz_ptr a, gmp_randstate_t state, mpz_srcptr prime)
{
    ASSERT_ALWAYS(mpz_cmp_ui(prime, 1) > 0);
    do {
        mpz_urandomm(a, state, prime);
    } while (mpz_cmp_ui(a, 0) == 0);
}

/* return a vector of n random numbers mod p */
void
fill_random(std::vector<cxx_mpz>& u)
{
    for (auto& a : u)
        mpz_urandomm_nz(a, state, prime);
}

struct cp_useful_info
{
    int level;
    unsigned int t0;
    unsigned int t1;
    unsigned int t;
    unsigned long deg;
};

cp_useful_info
read_cp_aux(std::string const& s)
{
    std::string filename = s + ".aux";
    FILE* fp;
    fp = fopen(filename.c_str(), "r");
    if (fp == NULL) {
        fprintf(stderr, "Error, unable to read file %s\n", filename.c_str());
        exit(1);
    }
    unsigned int xm, xn;
    int level;
    unsigned int t0;
    unsigned int t1;
    unsigned int t;
    int format;
    int ret = fscanf(fp, "format %d\n", &format);
    ASSERT_ALWAYS(ret == 1);
    unsigned long ncoeff;
    ret = fscanf(fp, "%u", &xm);
    ASSERT_ALWAYS(ret == 1);
    ret = fscanf(fp, "%u", &xn);
    ASSERT_ALWAYS(ret == 1);
    ret = fscanf(fp, "%d", &level);
    ASSERT_ALWAYS(ret == 1);
    ret = fscanf(fp, "%u", &t0);
    ASSERT_ALWAYS(ret == 1);
    ret = fscanf(fp, "%u", &t1);
    ASSERT_ALWAYS(ret == 1);
    ret = fscanf(fp, "%u", &t);
    ASSERT_ALWAYS(ret == 1);
    ASSERT_ALWAYS(t0 <= t && t <= t1);
    ret = fscanf(fp, "%lu\n", &ncoeff);
    ASSERT_ALWAYS(ret == 1);
    fclose(fp);
    return cp_useful_info{ level, t0, t1, t, ncoeff - 1 };
}

/* read a matrix of dimension n, divided into kxk submatrices.
 * return the evaluation of the matrix polynomial at x.
 * */
matrix
read_matrix(const char* s,
            unsigned long nrows,
            unsigned long ncols,
            unsigned long k,
            cxx_mpz const& x)
{
    unsigned long deg = read_cp_aux(s).deg;
    matrix M(nrows, ncols, k, deg);
    matrix_reader R(nrows, ncols, k, deg, s, false);
    cxx_mpz x_power_k = 1;
    for (k = 0; k <= deg; k++, mpz_mul(x_power_k, x_power_k, x)) {
        /* invariant: x_power_k = x^k mod prime */
        R.read1_accumulate(M, x_power_k, k);
    }
    return M;
}

/* w <- v*M evaluated at x and modulo p */
void
mul_left(std::vector<cxx_mpz>& w,
         std::vector<cxx_mpz> const& v,
         matrix const& M)
{
    ASSERT_ALWAYS(v.size() == M.nrows);
    ASSERT_ALWAYS(w.size() == M.ncols);
    cxx_mpz tmp;
    for (unsigned long j = 0; j < M.ncols; j++) {
        mpz_set_ui(w[j], 0);
        for (unsigned long i = 0; i < M.nrows; i++) {
            /* w[j] += v[i]*M[i,j] */
            mpz_mul(tmp, v[i], M.coeff[i * M.ncols + j]);
            mpz_add(w[j], w[j], tmp);
        }
        mpz_mod(w[j], w[j], prime);
    }
}

std::vector<cxx_mpz> operator*(std::vector<cxx_mpz> const& v, matrix const& M)
{
    std::vector<cxx_mpz> res(M.ncols);
    mul_left(res, v, M);
    return res;
}

/* w <- M*v evaluated */
void
mul_right(std::vector<cxx_mpz>& w,
          matrix const& M,
          std::vector<cxx_mpz> const& v)
{
    ASSERT_ALWAYS(v.size() == M.ncols);
    ASSERT_ALWAYS(w.size() == M.nrows);
    unsigned long n = M.ncols;
    cxx_mpz tmp;
    for (unsigned long i = 0; i < M.nrows; i++) {
        mpz_set_ui(w[i], 0);
        for (unsigned long j = 0; j < n; j++) {
            /* w[i] += M[i,j]*v[j] */
            mpz_mul(tmp, M.coeff[i * M.ncols + j], v[j]);
            mpz_add(w[i], w[i], tmp);
        }
        mpz_mod(w[i], w[i], prime);
    }
}

std::vector<cxx_mpz> operator*(matrix const& M, std::vector<cxx_mpz> const& v)
{
    std::vector<cxx_mpz> res(M.nrows);
    mul_right(res, M, v);
    return res;
}

void
add_scalar_product(cxx_mpz& res,
                   std::vector<cxx_mpz> const& u,
                   std::vector<cxx_mpz> const& v)
{
    ASSERT_ALWAYS(u.size() == v.size());
    for (unsigned long i = 0; i < u.size(); i++)
        mpz_addmul(res, u[i], v[i]);
    mpz_mod(res, res, prime);
}

cxx_mpz
scalar_product(std::vector<cxx_mpz> const& u, std::vector<cxx_mpz> const& v)
{
    ASSERT_ALWAYS(u.size() == v.size());
    cxx_mpz res = 0;
    add_scalar_product(res, u, v);
    return res;
}

void
print_vector(std::vector<cxx_mpz> const& u)
{
    for (auto const& x : u)
        gmp_printf("%Zd ", (mpz_srcptr)x);
    printf("\n");
}

/* print a 0 for zero coefficients, otherwise 1 */
void
print_matrix(matrix const& M)
{
    for (unsigned long i = 0; i < M.nrows; i++) {
        for (unsigned long j = 0; j < M.ncols; j++)
            if (mpz_cmp_ui(M.coeff[i * M.ncols + j], 0) == 0)
                printf("0 ");
            else
                printf("1 ");
        printf("\n");
    }
}

void
declare_usage(cxx_param_list& pl)
{
    param_list_usage_header(
      pl,
      "Usage: lingen_verify_checkpoints [options] -- [list of file names]\n"
      "\n"
      "The list of file names can have one of the following formats:\n"
      " - pi0 pi1 pi2 : check that pi0*pi1==pi2\n"
      " - E pi : check that E*pi=O(x^length(E))\n"
      "Options are as follows.\n");
    param_list_decl_usage(pl, "prime", "characteristic of the base field");
    param_list_decl_usage(pl, "mpi", "mpi geometry");
    param_list_decl_usage(pl, "seed", "random seed");
    param_list_decl_usage(pl, "m", "block Wiedemann parameter m");
    param_list_decl_usage(pl, "n", "block Wiedemann parameter n");
    param_list_decl_usage(pl, "v", "More verbose output");
}

void
lookup_parameters(cxx_param_list& pl)
{
    param_list_lookup_string(pl, "prime");
    param_list_lookup_string(pl, "mpi");
    param_list_lookup_string(pl, "seed");
}

int
do_check_pi(const char* pi_left_filename,
            const char* pi_right_filename,
            const char* pi_filename)
{
    int ret;
    unsigned long nrows = bw_parameters.m + bw_parameters.n;
    unsigned long ncols = bw_parameters.m + bw_parameters.n;

    std::vector<cxx_mpz> u(nrows);
    std::vector<cxx_mpz> v(nrows);

    fill_random(u);
    fill_random(v);

    cxx_mpz x;
    mpz_urandomm_nz(x, state, prime); /* random variable */
    // printf("Using seed %lu\n", seed);
    if (verbose)
        gmp_printf("x=%Zd\n", (mpz_srcptr)x);

    cp_useful_info cp = read_cp_aux(pi_filename);

    std::string check_name =
      fmt::sprintf("check (seed=%lu, depth %d, t=%u, pi_left*pi_right=pi)",
                   seed,
                   cp.level,
                   cp.t);

    std::vector<cxx_mpz> u_times_piab(ncols);
    std::vector<cxx_mpz> pibc_times_v(nrows);
    std::vector<cxx_mpz> piac_times_v(nrows);
#ifdef HAVE_OPENMP
#pragma omp parallel sections
#endif
    {
#ifdef HAVE_OPENMP
#pragma omp section
#endif
        {
            matrix Mab = read_matrix(pi_left_filename, nrows, ncols, k, x);
            mul_left(u_times_piab, u, Mab);
        }

#ifdef HAVE_OPENMP
#pragma omp section
#endif
        {
            matrix Mbc = read_matrix(pi_right_filename, nrows, ncols, k, x);
            mul_right(pibc_times_v, Mbc, v);
        }

#ifdef HAVE_OPENMP
#pragma omp section
#endif
        {
            matrix Mac = read_matrix(pi_filename, nrows, ncols, k, x);
            mul_right(piac_times_v, Mac, v);
        }
    }

    cxx_mpz res_left = scalar_product(u_times_piab, pibc_times_v);
    cxx_mpz res_right = scalar_product(u, piac_times_v);

    if (mpz_cmp(res_left, res_right) != 0) {
        fprintf(stderr, "FAILED %s\n", check_name.c_str());
        gmp_printf("res_left  = %Zd\n", (mpz_srcptr)res_left);
        gmp_printf("res_right = %Zd\n", (mpz_srcptr)res_right);
        ret = 0;
    } else {
        printf("ok %s\n", check_name.c_str());
        ret = 1;
    }

    return ret;
}

std::tuple<unsigned int, unsigned int>
parse_t0_t1(std::string const& E_filename, std::string const& pi_filename)
{
    unsigned int t0;
    unsigned int t1;
    /* parse E_filename and pi_filename, find level (unused), t0 (unused),
     * and t1
     */
    size_t E_pos = E_filename.rfind('/');
    if (E_pos == std::string::npos)
        E_pos = 0;
    else
        E_pos++;

    size_t pi_pos = pi_filename.rfind('/');
    if (pi_pos == std::string::npos)
        pi_pos = 0;
    else
        pi_pos++;

    /* skip over the basename (E, pi) */
    E_pos = E_filename.find('.', E_pos);
    ASSERT_ALWAYS(E_pos != std::string::npos);
    pi_pos = pi_filename.find('.', pi_pos);
    ASSERT_ALWAYS(pi_pos != std::string::npos);
    E_pos++;
    pi_pos++;

    /* comparison will start from here */
    size_t E_pos0 = E_pos;
    size_t pi_pos0 = pi_pos;

    /* skip over the level */
    E_pos = E_filename.find('.', E_pos);
    ASSERT_ALWAYS(E_pos != std::string::npos);
    pi_pos = pi_filename.find('.', pi_pos);
    ASSERT_ALWAYS(pi_pos != std::string::npos);
    E_pos++;
    pi_pos++;

    /* skip over t0 */
    ASSERT_ALWAYS(std::istringstream(E_filename.substr(E_pos)) >> t0);
    // E_pos = E_filename.find('.', E_pos);
    // ASSERT_ALWAYS(E_pos != std::string::npos);
    pi_pos = pi_filename.find('.', pi_pos);
    ASSERT_ALWAYS(pi_pos != std::string::npos);
    // E_pos++;

    ASSERT_ALWAYS(E_filename.substr(E_pos0) ==
                  pi_filename.substr(pi_pos0, pi_pos - pi_pos0));

    pi_pos++;

    /* at this point, E should have ben consumed, and all that is left in
     * pi is t1 */
    ASSERT_ALWAYS(std::istringstream(pi_filename.substr(pi_pos)) >> t1);
    return { t0, t1 };
}

/* check that E*pi = O(x^length(E)) at a given level. */
int
do_check_E_short(std::string const& E_filename, std::string const& pi_filename)
{
    int ret;
    unsigned int m = bw_parameters.m;
    unsigned int n = bw_parameters.n;

    std::vector<cxx_mpz> u(m);
    fill_random(u);
    std::vector<cxx_mpz> v(m + n);
    fill_random(v);

    cxx_mpz x;
    mpz_urandomm_nz(x, state, prime); /* random variable */
    if (verbose)
        gmp_printf("x=%Zd\n", (mpz_srcptr)x);

    /* Note that all the useful info is in the aux file for pi, really.
     * The one for E is stored at t0, and is not really useful.
     */
    cp_useful_info cp = read_cp_aux(pi_filename);

    unsigned long deg_pi = cp.deg;
    unsigned long t = cp.t;
    unsigned long t0 = cp.t0;
    unsigned long t1 = cp.t1;

    std::string check_name = fmt::sprintf(
      "check (seed=%lu, depth %d, t=%u, E*pi=O(X^*))",
                   seed,
                   cp.level,
                   cp.t);
    if (t < t1)
        check_name += " [truncated cp at end]";

    unsigned long deg_E = t - t0 - 1;

    matrix pi(m + n, m + n, k, deg_pi);
    matrix_reader Rpi(m + n, m + n, k, deg_pi, pi_filename, true);
    matrix E(m, m + n, k, deg_E);
    matrix_reader RE(m, m + n, k, deg_E, E_filename, false);

    /* We'll compute the evaluation at x of the short product of E*pi,
     * capped to degree deg_E
     *
     * Note that pi has degree smaller than E.
     *
     * We use the following expression of the result.
     *
     * res = (E_0 + ... + E_{deg_E} * x^deg_E) * pi_0
     *     + (E_0 + ... + E_{deg_E-1} * x^{deg_E-1}) * pi_1 * x
     *     + (E_0 + ... + E_{deg_E-2} * x^{deg_E-2}) * pi_2 * x^2
     *     + ...
     *     + (E_0 + ... + E_{deg_E-deg_pi} * x^{deg_E-deg_pi}) * pi_deg_pi *
     * x^deg_pi and the evaluation will be performed from the last line to the
     * first. We get a similarly significant result by multiplying
     * everything by x^(-deg_pi), which gives the following formula:
     *
     * res = (E_0 + ... + E_{deg_E-deg_pi} * x^{deg_E-deg_pi}) * pi_deg_pi
     *     + ...
     *     + (E_0 + ... + E_{deg_E-2} * x^{deg_E-2}) * pi_2 * xinv^{deg_pi-2}
     *     + (E_0 + ... + E_{deg_E-1} * x^{deg_E-1}) * pi_1 * xinv^{deg_pi-1}
     *     + (E_0 + ... + E_{deg_E} * x^deg_E) * pi_0 * xinv^{deg_pi}
     */

    cxx_mpz xinv;
    mpz_invert(xinv, x, prime);

    cxx_mpz res;

    cxx_mpz x_inc = 1;
    for (unsigned long k = 0; k < deg_E - deg_pi; k++) {
        /* invariant: x_power_k = x^k mod prime */
        RE.read1_accumulate(E, x_inc, k);
        mpz_mul(x_inc, x_inc, x);
    }
    cxx_mpz x_dec = 1;
    for (unsigned long k = 0; k <= deg_pi; k++) {
        RE.read1_accumulate(E, x_inc, k);
        pi.zero();
        Rpi.read1_accumulate(pi, x_dec, k);
        mpz_mul(x_inc, x_inc, x);
        mpz_mul(x_dec, x_dec, xinv);
        std::vector<cxx_mpz> u_E = u * E;
        std::vector<cxx_mpz> pi_v = pi * v;
        add_scalar_product(res, u_E, pi_v);
    }

    if (mpz_cmp_ui(res, 0) != 0) {
        fprintf(stderr, "FAILED %s\n", check_name.c_str());
        gmp_printf("res  = %Zd\n", (mpz_srcptr)res);
        ret = 0;
    } else {
        printf("ok %s\n", check_name.c_str());
        ret = 1;
    }

    return ret;
}

int
main(int argc, char* argv[])
{
    cxx_param_list pl;

    seed = getpid();

    declare_usage(pl);

    const char* argv0 = argv[0];

    param_list_configure_switch(pl, "-v", &verbose);
    for (argc--, argv++; argc;) {
        if (param_list_update_cmdline(pl, &argc, &argv)) {
            continue;
        }
        if (strcmp(argv[0], "--") == 0) {
            argc--, argv++;
            break;
        }
        fprintf(stderr, "Unhandled parameter %s\n", argv[0]);
        param_list_print_usage(pl, argv0, stderr);
        exit(EXIT_FAILURE);
    }

    if (!param_list_parse_mpz(pl, "prime", (mpz_ptr)prime)) {
        fprintf(stderr, "Missing parameter: prime\n");
        param_list_print_usage(pl, argv0, stderr);
        exit(EXIT_FAILURE);
    }
    int mpi_dims[2] = { 0, 0 };
    if (param_list_parse_int_and_int(pl, "mpi", mpi_dims, "x")) {
        ASSERT_ALWAYS(mpi_dims[0] == mpi_dims[0]);
        k = mpi_dims[0];
    }
    param_list_parse_ulong(pl, "seed", &seed);
    if (!param_list_parse_uint(pl, "m", &bw_parameters.m)) {
        fprintf(stderr, "Missing parameter: m\n");
        param_list_print_usage(pl, argv0, stderr);
        exit(EXIT_FAILURE);
    }
    if (!param_list_parse_uint(pl, "n", &bw_parameters.n)) {
        fprintf(stderr, "Missing parameter: n\n");
        param_list_print_usage(pl, argv0, stderr);
        exit(EXIT_FAILURE);
    }
    if (param_list_warn_unused(pl)) {
        param_list_print_usage(pl, argv0, stderr);
        exit(EXIT_FAILURE);
    }

    gmp_randinit_default(state);
    gmp_randseed_ui(state, seed);

    lingen_p = mpz_size(prime);
    if (verbose)
        printf("Number of limbs: %zu\n", lingen_p);

    int ret = 0;

    /* it doesn't seem to make sense to compare
     *      MP(E_{level,t0}, pi_{level+1,t0,t}) and E_{level+1,t}
     * ? This is covered by the test of the short product
     *      E_{level,t0} * pi_{level,t0}
     * since we can validate the fact that pi_{level,t0} is the
     * product of the two pi matrices at the level below, and that
     * the short product E_{level+1,t0}*pi_{level+1,t0} is also zero.
     */
    if (argc == 3) {
        ret = do_check_pi(argv[0], argv[1], argv[2]);
    } else if (argc == 2) {
        ret = do_check_E_short(argv[0], argv[1]);
    } else {
        fprintf(stderr, "bad argument list\n");
        exit(EXIT_FAILURE);
    }

    gmp_randclear(state);

    return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
/* vim: set sw=4 sta et: */
