#include <gsl/gsl_eigen.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_complex_math.h>
#include "sumprod.h"
#include "util.h"
#include "logger.h"

EigenModel::EigenModel (const RateModel& model)
  : model (model),
    eval (gsl_vector_complex_alloc (model.alphabetSize())),
    evec (gsl_matrix_complex_alloc (model.alphabetSize(), model.alphabetSize())),
    evecInv (gsl_matrix_complex_alloc (model.alphabetSize(), model.alphabetSize())),
    ev (model.alphabetSize()),
    ev_t (model.alphabetSize()),
    exp_ev_t (model.alphabetSize())
{
  gsl_eigen_nonsymmv_workspace *workspace = gsl_eigen_nonsymmv_alloc (model.alphabetSize());
  CheckGsl (gsl_eigen_nonsymmv (model.subRate, eval, evec, workspace));
  gsl_eigen_nonsymmv_free (workspace);

  gsl_matrix_complex *LU = gsl_matrix_complex_alloc (model.alphabetSize(), model.alphabetSize());
  gsl_permutation *perm = gsl_permutation_alloc (model.alphabetSize());
  int permSig = 0;
  gsl_matrix_complex_memcpy (LU, evec);
  CheckGsl (gsl_linalg_complex_LU_decomp (LU, perm, &permSig));
  CheckGsl (gsl_linalg_complex_LU_invert (LU, perm, evecInv));
  gsl_matrix_complex_free (LU);
  gsl_permutation_free (perm);

  for (AlphTok i = 0; i < model.alphabetSize(); ++i)
    ev[i] = gsl_vector_complex_get (eval, i);

  if (LoggingThisAt(8)) {
    LogThisAt(8,"Eigenvalues:");
    for (AlphTok i = 0; i < model.alphabetSize(); ++i)
      LogThisAt(8," (" << GSL_REAL(ev[i]) << "," << GSL_IMAG(ev[i]) << ")");
    LogThisAt(8,endl << "Right eigenvector matrix:" << endl);
    for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
      for (AlphTok j = 0; j < model.alphabetSize(); ++j) {
	const gsl_complex c = gsl_matrix_complex_get (evec, i, j);
	LogThisAt(8," (" << GSL_REAL(c) << "," << GSL_IMAG(c) << ")");
      }
      LogThisAt(8,endl);
    }
    LogThisAt(8,endl << "Left eigenvector matrix:" << endl);
    for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
      for (AlphTok j = 0; j < model.alphabetSize(); ++j) {
	const gsl_complex c = gsl_matrix_complex_get (evecInv, i, j);
	LogThisAt(8," (" << GSL_REAL(c) << "," << GSL_IMAG(c) << ")");
      }
      LogThisAt(8,endl);
    }
    LogThisAt(8,endl << "Reconstituted rate matrix:" << endl);
    for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
      for (AlphTok j = 0; j < model.alphabetSize(); ++j) {
	gsl_complex rij = gsl_complex_rect (0, 0);
	for (AlphTok k = 0; k < model.alphabetSize(); ++k) {
	  gsl_complex rij_term = gsl_complex_mul (gsl_complex_mul (gsl_matrix_complex_get (evec, i, k),
								   gsl_matrix_complex_get (evecInv, k, j)),
						  ev[k]);
	  rij = gsl_complex_add (rij, rij_term);
	}
	LogThisAt(8," (" << GSL_REAL(rij) << "," << GSL_IMAG(rij) << ")");
      }
      LogThisAt(8,endl);
    }
  }
}

EigenModel::~EigenModel() {
  gsl_vector_complex_free (eval);
  gsl_matrix_complex_free (evec);
  gsl_matrix_complex_free (evecInv);
}

gsl_matrix* EigenModel::getSubProb (double t) const {
  gsl_matrix* sub = gsl_matrix_alloc (model.alphabetSize(), model.alphabetSize());
  ((EigenModel&) *this).compute_exp_ev_t (t);
  for (AlphTok i = 0; i < model.alphabetSize(); ++i)
    for (AlphTok j = 0; j < model.alphabetSize(); ++j) {
      gsl_complex p = gsl_complex_rect (0, 0);
      for (AlphTok k = 0; k < model.alphabetSize(); ++k)
	p = gsl_complex_add
	  (p,
	   gsl_complex_mul (gsl_complex_mul (gsl_matrix_complex_get (evec, i, k),
					     gsl_matrix_complex_get (evecInv, k, j)),
			    exp_ev_t[k]));
      Assert (GSL_IMAG(p) == 0, "Probability has imaginary part: p=(%g,%g)", GSL_REAL(p), GSL_IMAG(p));
      gsl_matrix_set (sub, i, j, min (1., max (0., GSL_REAL(p))));
    }
  return sub;
}

void EigenModel::compute_exp_ev_t (double t) {
  for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
    ev_t[i] = gsl_complex_mul_real (ev[i], t);
    exp_ev_t[i] = gsl_complex_exp (ev_t[i]);
  }
}

void EigenModel::accumSubCount (gsl_matrix* count, AlphTok a, AlphTok b, double weight, const gsl_matrix* sub, const gsl_matrix_complex* eSubCount) {
  for (AlphTok i = 0; i < model.alphabetSize(); ++i)
    for (AlphTok j = 0; j < model.alphabetSize(); ++j) {
      gsl_complex c = gsl_complex_rect (0, 0);
      for (AlphTok k = 0; k < model.alphabetSize(); ++k) {
	gsl_complex ck = gsl_complex_rect (0, 0);
	for (AlphTok l = 0; l < model.alphabetSize(); ++l)
	  ck = gsl_complex_add
	    (ck,
	     gsl_complex_mul
	     (gsl_complex_mul
	      (gsl_matrix_complex_get (eSubCount, k, l),
	       gsl_matrix_complex_get (evecInv, l, b)),
	      gsl_matrix_complex_get (evec, j, l)));
	c = gsl_complex_add (c,
			     gsl_complex_mul
			     (gsl_complex_mul
			      (gsl_matrix_complex_get (evec, a, k),
			       gsl_matrix_complex_get (evecInv, k, i)),
			      ck));
      }
      Assert (GSL_IMAG(c) == 0, "Count has imaginary part: c=(%g,%g)", GSL_REAL(c), GSL_IMAG(c));
      *(gsl_matrix_ptr (count, i, j)) += GSL_REAL(c) * weight;
    }
}

gsl_matrix_complex* EigenModel::eigenSubCount (double t) const {
  gsl_matrix_complex* esub = gsl_matrix_complex_alloc (model.alphabetSize(), model.alphabetSize());
  ((EigenModel&) *this).compute_exp_ev_t (t);
  for (AlphTok i = 0; i < model.alphabetSize(); ++i)
    for (AlphTok j = 0; j < model.alphabetSize(); ++j)
      gsl_matrix_complex_set
	(esub, i, j,
	 GSL_COMPLEX_EQ (ev[i], ev[j])
	 ? gsl_complex_mul_real (exp_ev_t[i], t)
	 : gsl_complex_div (gsl_complex_sub (exp_ev_t[i], exp_ev_t[j]),
			    gsl_complex_sub (ev[i], ev[j])));
  return esub;
}

AlignColSumProduct::AlignColSumProduct (const RateModel& model, const Tree& tree, const vguard<FastSeq>& gapped)
  : model (model),
    tree (tree),
    gapped (gapped),
    eigen (model),
    logInsProb (log_gsl_vector (model.insProb)),
    branchLogSubProb (tree.nodes()),
    branchEigenSubCount (tree.nodes()),
    col (0),
    logE (tree.nodes(), vguard<LogProb> (model.alphabetSize())),
    logF (tree.nodes(), vguard<LogProb> (model.alphabetSize())),
    logG (tree.nodes(), vguard<LogProb> (model.alphabetSize()))
{
  Require (tree.nodes() == gapped.size(), "Every tree node must have an alignment row");

  for (AlignRowIndex r = 0; r < gapped.size() - 1; ++r) {
    ProbModel pm (model, tree.branchLength(r));
    LogProbModel lpm (pm);
    branchLogSubProb[r].swap (lpm.logSubProb);
  }

  initColumn();
  
  for (AlignRowIndex r = 0; r < gapped.size() - 1; ++r)
    branchEigenSubCount[r] = eigen.eigenSubCount (tree.branchLength(r));
}

AlignColSumProduct::~AlignColSumProduct() {
  for (auto m : branchEigenSubCount)
    gsl_matrix_complex_free (m);
}

void AlignColSumProduct::initColumn() {
  ungappedRows.clear();
  vguard<int> ungappedKids (tree.nodes(), 0);
  vguard<TreeNodeIndex> roots;
  for (TreeNodeIndex r = 0; r < tree.nodes(); ++r)
    if (!isGap(r)) {
      ungappedRows.push_back (r);
      Assert (isWild(r) || ungappedKids[r] == 0, "Internal node sequences must be wildcards (%c)", Alignment::gapChar);
      const TreeNodeIndex rp = tree.parentNode(r);
      if (rp < 0 || isGap(rp))
	roots.push_back (r);
      else
	++ungappedKids[rp];
    }
  Assert (roots.size() == 1, "Multiple root nodes");
}

bool AlignColSumProduct::alignmentDone() const {
  return col >= gapped.front().length();
}

void AlignColSumProduct::nextColumn() {
  ++col;
  initColumn();
}

void AlignColSumProduct::fillUp() {
  colLogLike = -numeric_limits<double>::infinity();
  for (auto r : ungappedRows) {
    if (isWild(r))
      for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
	LogProb logFi = 0;
	for (size_t nc = 0; nc < tree.nChildren(r); ++nc)
	  logFi += logE[tree.getChild(r,nc)][i];
	logF[r][i] = logFi;
      }
    else {
      for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	logF[r][i] = -numeric_limits<double>::infinity();
      logF[r][model.tokenize(gapped[r].seq[col])] = 0;
    }

    if (r == root())
      colLogLike = logInnerProduct (logF[r], logInsProb);
    else {
      const TreeNodeIndex rp = tree.parentNode(r);
      for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
	LogProb logEi = -numeric_limits<double>::infinity();
	for (AlphTok j = 0; j < model.alphabetSize(); ++j)
	  log_accum_exp (logEi, branchLogSubProb[r][i][j] + logF[r][j]);
	logE[r][i] = logEi;
      }
    }
  }
}

void AlignColSumProduct::fillDown() {
  if (!columnEmpty()) {
    vector<AlignRowIndex>::const_reverse_iterator iter = ungappedRows.rbegin();
    logG[*iter] = logInsProb;
    for (++iter; iter != ungappedRows.rend(); ++iter) {
      const TreeNodeIndex r = *iter;
      const TreeNodeIndex rp = tree.parentNode(r);
      const TreeNodeIndex rs = tree.getSibling(r);
      for (AlphTok j = 0; j < model.alphabetSize(); ++j) {
	LogProb logGj = -numeric_limits<double>::infinity();
	for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	  log_accum_exp (logGj, logG[rp][i] + branchLogSubProb[r][i][j] + logE[rs][i]);
	logG[r][j] = logGj;
      }
    }
  }
}

vguard<LogProb> AlignColSumProduct::logNodePostProb (AlignRowIndex node) const {
  vguard<LogProb> lpp (model.alphabetSize());
  for (AlphTok i = 0; i < model.alphabetSize(); ++i)
    lpp[i] = logF[node][i] + logG[node][i] - colLogLike;
  return lpp;
}

LogProb AlignColSumProduct::logBranchPostProb (AlignRowIndex node, AlphTok parentState, AlphTok nodeState) const {
  const TreeNodeIndex parent = tree.parentNode(node);
  const TreeNodeIndex sibling = tree.getSibling(node);
  return logG[parent][parentState] + branchLogSubProb[node][parentState][nodeState] + logF[node][nodeState] + logE[sibling][parentState] - colLogLike;
}

AlphTok AlignColSumProduct::maxPostState (AlignRowIndex node) const {
  const auto lpp = logNodePostProb (node);
  return (AlphTok) (max_element(lpp.begin(),lpp.end()) - lpp.begin());
}

void AlignColSumProduct::accumulateCounts (gsl_vector* rootCounts, gsl_matrix_complex* eigenCounts) const {
  for (AlphTok i = 0; i < model.alphabetSize(); ++i)
    *(gsl_vector_ptr(rootCounts,i)) += exp (logF[root()][i] - colLogLike);

  vguard<double> U (model.alphabetSize()), D (model.alphabetSize());
  vguard<gsl_complex> Ubasis (model.alphabetSize()), Dbasis (model.alphabetSize());
  for (auto node : ungappedRows)
    if (node != root()) {
      const TreeNodeIndex parent = tree.parentNode(node);
      const TreeNodeIndex sibling = tree.getSibling(node);
      const vguard<LogProb>& logU = logF[node];
      vguard<LogProb> logD (model.alphabetSize());
      for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	logD[i] = logG[parent][i] + logE[sibling][i];
      const LogProb minLogU = *min_element (logU.begin(), logU.end());
      const LogProb minLogD = *min_element (logD.begin(), logD.end());
      const double norm = exp (colLogLike - minLogU - minLogD);
      // U[k] = exp(logU[k]-minLogU); Ubasis[i] = sum_k U[k] * evecInv[i][k]
      for (AlphTok k = 0; k < model.alphabetSize(); ++k)
	U[k] = exp (logU[k] - minLogU);
      for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
	gsl_complex Ubasis_i = gsl_complex_rect (0, 0);
	for (AlphTok k = 0; k < model.alphabetSize(); ++k)
	  Ubasis_i = gsl_complex_add
	    (Ubasis_i,
	     gsl_complex_mul_real (gsl_matrix_complex_get (eigen.evecInv, i, k),
				   U[k]));
      }
      // D[k] = exp(logD[k]-minLogD); Dbasis[i] = sum_k D[k] * evec[k][i]
      for (AlphTok k = 0; k < model.alphabetSize(); ++k)
	D[k] = exp (logD[k] - minLogD);
      for (AlphTok i = 0; i < model.alphabetSize(); ++i) {
	gsl_complex Dbasis_i = gsl_complex_rect (0, 0);
	for (AlphTok k = 0; k < model.alphabetSize(); ++k)
	  Dbasis_i = gsl_complex_add
	    (Dbasis_i,
	     gsl_complex_mul_real (gsl_matrix_complex_get (eigen.evec, k, i),
				   D[k]));
      }
      // eigenCounts[i][j] += Dbasis[i] * eigenSub[i][j] * Ubasis[j] / norm
      for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	for (AlphTok j = 0; j < model.alphabetSize(); ++j)
	  gsl_matrix_complex_set
	    (eigenCounts, i, j,
	     gsl_complex_add
	     (gsl_matrix_complex_get (eigenCounts, i, j),
	      gsl_complex_div_real
	      (gsl_complex_mul
	       (Dbasis[i],
		gsl_complex_mul
		(gsl_matrix_complex_get (branchEigenSubCount[node], i, j),
		 Ubasis[j])),
	       norm)));
    }
}

gsl_matrix* AlignColSumProduct::getSubCounts (gsl_matrix_complex* eigenCounts) const {
  gsl_matrix* counts = gsl_matrix_alloc (model.alphabetSize(), model.alphabetSize());
  for (AlphTok i = 0; i < model.alphabetSize(); ++i)
    for (AlphTok j = 0; j < model.alphabetSize(); ++j) {
      gsl_complex c = gsl_complex_rect (0, 0);
      for (AlphTok k = 0; k < model.alphabetSize(); ++k) {
	gsl_complex ck = gsl_complex_rect (0, 0);
	for (AlphTok l = 0; l < model.alphabetSize(); ++l)
	  ck = gsl_complex_add
	    (ck,
	     gsl_complex_mul (gsl_matrix_complex_get (eigenCounts, k, l),
			      gsl_matrix_complex_get (eigen.evec, j, l)));
	c = gsl_complex_add
	  (c,
	   gsl_complex_mul (gsl_matrix_complex_get (eigen.evecInv, k, i),
			    ck));
      }
      if (i == j)
	gsl_matrix_set (counts, i, j, GSL_REAL(c));
      else
	gsl_matrix_set (counts, i, j, GSL_REAL(c) * gsl_matrix_get (model.subRate, i, j));
    }
  return counts;
}
