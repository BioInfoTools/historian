#ifndef LOGSUMEXP_INCLUDED
#define LOGSUMEXP_INCLUDED

#include <gsl/gsl_vector.h>
#include <vector>

/* uncomment to disable lookup table */
/*
#define LOGSUMEXP_DEBUG
*/

/* uncomment to catch NaN errors */
/*
#define NAN_DEBUG
*/

typedef double LogProb;
std::vector<LogProb> log_gsl_vector (gsl_vector* v);

double log_sum_exp (double a, double b);  /* returns log(exp(a) + exp(b)) */
double log_sum_exp (double a, double b, double c);
double log_sum_exp (double a, double b, double c, double d);
double log_sum_exp (double a, double b, double c, double d, double e);
double log_sum_exp (double a, double b, double c, double d, double e, double f);
double log_sum_exp (double a, double b, double c, double d, double e, double f, double g);

double log_sum_exp_slow (double a, double b);  /* does not use lookup table */
double log_sum_exp_slow (double a, double b, double c);
double log_sum_exp_slow (double a, double b, double c, double d);

double log_sum_exp_unary (double x);  /* returns log(1 + exp(-x)) for nonnegative x */
double log_sum_exp_unary_slow (double x);  /* does not use lookup table */

void log_accum_exp (double& a, double b);  /* equivalent to a = log_sum_exp(a,b) */

#endif /* LOGSUMEXP_INCLUDED */
