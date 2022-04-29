#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <omp.h>

struct simplex_t {
    int dim;
    double *pdf;
    double *cdf;
};

double rand_u (unsigned int *seed) {
    return rand_r(seed) / (RAND_MAX + 1.0);
}

double rand_exp (double lambda, unsigned int *seed) {
    return -log(1 - rand_u(seed)) / lambda;
}

int rand_int (int n, int m, unsigned int *seed) {
    // n <= ret < m

    return n + (int)(rand_u(seed) * (m - n));
}

struct simplex_t *init_simplex (int dim) {
    struct simplex_t *simplex = malloc(sizeof(struct simplex_t));
    double sum = 0.;
    int i;

    simplex->dim     = dim;
    simplex->pdf     = malloc(sizeof(double) * (dim + 1));
    simplex->cdf     = malloc(sizeof(double) * (dim + 1));

    simplex->pdf[0] = 0.;
    simplex->cdf[0] = 0.;

    return simplex;
}

void normalize_simplex (struct simplex_t *simplex) {
    int lo, hi, idx;
    int i;
    double pdf = 0;
    double sum = 0;

    // Normalize
    for (i = 1; i <= simplex->dim; ++i)
        sum += simplex->pdf[i];
    for (i = 1; i <= simplex->dim; ++i) {
        simplex->pdf[i] /= sum;
        simplex->cdf[i]  = simplex->cdf[i - 1] + simplex->pdf[i];
    }
}

void dest_simplex (struct simplex_t *simplex) {
    int i;

    free(simplex->cdf);
    free(simplex->pdf);
    free(simplex);
}

struct simplex_t *uniform_simplex (int dim) {
    struct simplex_t *simplex = init_simplex(dim);
    int i;

    for (i = 1; i <= dim; ++i)
        simplex->pdf[i] = 1;

    normalize_simplex(simplex);

    return simplex;
}

struct simplex_t *sample_simplex (int dim, unsigned int *seed) {
    struct simplex_t *simplex = init_simplex(dim);
    int i;

    for (i = 1; i <= dim; ++i)
        simplex->pdf[i] = rand_exp(1., seed);

    normalize_simplex(simplex);

    return simplex;
}

int guess (int lo, int hi, struct simplex_t *simplex, unsigned int *seed) {
    // Best guess lo < x < hi

    double cdf_t = (simplex->cdf[hi - 1] + simplex->cdf[lo]) * 0.5;
    int mid;

    // B-search
    while (true) {
        if (hi - lo <= 2)
            return lo + 1;

        mid = ((lo + hi) >> 1) + (hi & 1);
        if (simplex->cdf[mid] <= cdf_t)
            lo = mid;
        else
            hi = mid;
    }
}

int mid_guess (int lo, int hi, struct simplex_t *simplex, unsigned int *seed) {
    // mid guess lo + hi / 2

    return (lo + hi) >> 1;
}

int max_guess (int lo, int hi, struct simplex_t *simplex, unsigned int *seed) {
    // guess the lo < idx < hi which has max pdf
    int i, idx;
    double pdf = 0.;

    for (i = lo + 1; i < hi; ++i)
        if (simplex->pdf[i] > pdf) {
            idx = i;
            pdf = simplex->pdf[i];
        }

    return idx;
}

int min_guess (int lo, int hi, struct simplex_t *simplex, unsigned int *seed) {
    // guess the lo < idx < hi which has min pdf
    int i, idx;
    double pdf = 1.;

    for (i = lo + 1; i < hi; ++i)
        if (simplex->pdf[i] < pdf) {
            idx = i;
            pdf = simplex->pdf[i];
        }

    return idx;
}

int rand_guess (int lo, int hi, struct simplex_t *simplex, unsigned int *seed) {
    // Random guess

    return rand_int(lo + 1, hi, seed);
}

int game (int ans, struct simplex_t *simplex, 
        int (*guess_method)(int, int, struct simplex_t*, unsigned int*),
        unsigned int *seed) {
    // lets play a game

    int len = 1;
    int lo  = 0;
    int hi  = simplex->dim + 1;
    int mid;

    while (true) {
        len += 1;
        mid  = guess_method(lo, hi, simplex, seed);

        if (mid == ans)
            return len;
        else if (mid > ans)
            hi = mid;
        else
            lo = mid;
    }
}

double comp_avg_len (struct simplex_t *simplex, 
        int (*guess_method)(int, int, struct simplex_t*, unsigned int*),
        unsigned int *seed) {
    double len = 0.;
    int i;

    for (i = 1; i <= simplex->dim; ++i)
        len += simplex->pdf[i] * game(i, simplex, guess_method, seed);

    return len;
}

double _comp_avg_len_with_game_tree (struct simplex_t *simplex, 
        unsigned int lo, unsigned int hi,
        int (*guess_method)(int, int, struct simplex_t*, unsigned int*),
        unsigned int *seed) {
    double r_len, l_len;
    int mid;

    if (hi < lo + 2)
        return 0.;

    mid   = guess_method(lo, hi, simplex, seed);
    r_len = _comp_avg_len_with_game_tree(simplex, mid, hi, guess_method, seed);
    l_len = _comp_avg_len_with_game_tree(simplex, lo, mid, guess_method, seed);
    return (simplex->cdf[hi - 1] - simplex->cdf[lo]) + r_len + l_len;
}

double comp_avg_len_with_game_tree (struct simplex_t *simplex, 
        int (*guess_method)(int, int, struct simplex_t*, unsigned int*),
        unsigned int *seed) {
    return 1. + _comp_avg_len_with_game_tree(simplex, 0, simplex->dim + 1, guess_method, seed);
}

double comp_entropy (struct simplex_t *simplex) {
    double entropy = 0.;
    int i;

    for (i = 1; i <= simplex->dim; ++i)
        entropy += -simplex->pdf[i] * log2(simplex->pdf[i]);

    return entropy;
}

void simulate (int n, unsigned int *seed) {
    struct simplex_t *simplex = sample_simplex(n, seed);
    double avg_len, avg_len_m, avg_len_x, avg_len_n, avg_len_r, entropy;

    avg_len   = comp_avg_len_with_game_tree(simplex, guess, seed);
    avg_len_m = comp_avg_len_with_game_tree(simplex, mid_guess, seed);
    avg_len_x = comp_avg_len_with_game_tree(simplex, max_guess, seed);
    avg_len_n = comp_avg_len_with_game_tree(simplex, min_guess, seed);
    avg_len_r = comp_avg_len_with_game_tree(simplex, rand_guess, seed);
    entropy   = comp_entropy(simplex);

    dest_simplex(simplex);

    printf("%lf, %lf, %lf, %lf, %lf, %lf\n", entropy,
            avg_len, avg_len_m, avg_len_x, avg_len_n, avg_len_r);
}

int main (int argc, char *argv[]) {
    double n0, eps;
    int N, R;
    // n0 + eps * [N]
    
    if (argc != 5) {
        printf("Usage %s n0 eps N R\n", argv[0]);
        exit(-1);
    }
    n0  = atof(argv[1]);
    eps = atof(argv[2]);
    N   = atof(argv[3]);
    R   = atof(argv[4]);

    #pragma omp parallel default(none) shared(stderr, n0, eps, N, R)
    {
        int i, r, n;
        unsigned int seed = (unsigned)time(NULL) ^ omp_get_thread_num();
        #pragma omp for collapse(2)
        for (r = 0; r < R; ++r) {
            for (i = 0; i < N; ++i) {
                n = (int)(pow(2., n0 + eps * i));
                fprintf(stderr, "%d %d %d\n", r, i, n);
                simulate(n, &seed);
            }
        }
    }

    return 0;
}
