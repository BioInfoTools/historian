#include "sampler.h"
#include "recon.h"
#include "util.h"

double SimpleTreePrior::coalescenceRate (int lineages) const {
  return (((double) lineages * (lineages-1)) / 2) / (double) populationSize;
}

LogProb SimpleTreePrior::treeLogLikelihood (const Tree& tree) const {
  tree.assertBinary();
  const auto distanceFromRoot = tree.distanceFromRoot();
  const auto nodesByDistanceFromRoot = orderedIndices (distanceFromRoot);
  size_t lineages = 0;
  LogProb lp = 0;
  double lastEventTime;
  for (auto iter = nodesByDistanceFromRoot.rbegin(); iter != nodesByDistanceFromRoot.rend(); ++iter) {
    const double eventTime = distanceFromRoot[*iter];
    if (lineages > 1)
      lp -= coalescenceRate(lineages) * (lastEventTime - eventTime);
    lastEventTime = eventTime;
    if (tree.isLeaf(*iter))
      ++lineages;
    else
      --lineages;
  }
  return lp;
}

void Sampler::History::swapNodes (TreeNodeIndex x, TreeNodeIndex y) {
  tree.swapNodes (x, y);
  iter_swap (gapped.begin() + x, gapped.begin() + y);
}

TreeNodeIndex Sampler::randomInternalNode (const Tree& tree, random_engine& generator) {
  vguard<TreeNodeIndex> intNodes;
  intNodes.reserve (tree.nodes() / 2);
  for (TreeNodeIndex n = 0; n < tree.nodes(); ++n)
    if (!tree.isLeaf(n))
      intNodes.push_back (n);
  return random_element (intNodes, generator);
}

TreeNodeIndex Sampler::randomChildNode (const Tree& tree, random_engine& generator) {
  Assert (tree.nodes() > 1, "No child nodes in tree");
  std::uniform_int_distribution<TreeNodeIndex> distribution (0, tree.nodes() - 2);
  return distribution (generator);
}

TreeNodeIndex Sampler::randomGrandchildNode (const Tree& tree, random_engine& generator) {
  vguard<TreeNodeIndex> grandkids;
  for (TreeNodeIndex n = 0; n < tree.root(); ++n)
    if (tree.parentNode(n) != tree.root())
      grandkids.push_back (n);
  Assert (grandkids.size() > 0, "No grandchild nodes found");
  return random_element (grandkids, generator);
}

TreeNodeIndex Sampler::randomContemporaneousNode (const Tree& tree, const vguard<TreeBranchLength>& dist, TreeNodeIndex node, random_engine& generator) {
  vguard<TreeNodeIndex> contemps;
  const TreeNodeIndex parent = tree.parentNode(node);
  Assert (parent >= 0, "Parent node not found");
  const TreeBranchLength distParent = dist[parent];
  for (TreeNodeIndex n = 0; n < tree.root(); ++n) {
    const TreeNodeIndex p = tree.parentNode(n);
    if (p != parent && dist[p] < distParent && dist[n] > distParent)
      contemps.push_back (n);
  }
  Assert (contemps.size() > 0, "No contemporaneous nodes found");
  return random_element (contemps, generator);
}

vguard<SeqIdx> Sampler::guideSeqPos (const AlignPath& path, AlignRowIndex row, AlignRowIndex guideRow) {
  vguard<SeqIdx> guidePos;
  const auto cols = alignPathColumns (path);
  guidePos.reserve (cols);
  const AlignRowPath& rowPath = path.at(row);
  const AlignRowPath& guideRowPath = path.at(guideRow);
  SeqIdx pos = 0;
  for (AlignColIndex col = 0; col < cols; ++col) {
    if (guideRowPath[col])
      ++pos;
    if (rowPath[col])
      guidePos.push_back (pos > 0 ? (pos - 1) : 0);
  }
  guidePos.push_back (pos > 0 ? (pos - 1) : 0);
  return guidePos;
}

AlignPath Sampler::cladePath (const AlignPath& path, const Tree& tree, TreeNodeIndex cladeRoot, TreeNodeIndex cladeRootParent) {
  AlignPath p;
  for (auto n : tree.rerootedPreorderSort(cladeRoot,cladeRootParent))
    p[n] = path.at(n);
  return alignPathRemoveEmptyColumns (p);
}

AlignPath Sampler::pairPath (const AlignPath& path, TreeNodeIndex node1, TreeNodeIndex node2) {
  AlignPath p;
  size_t nDel = 0;
  const AlignColIndex cols = alignPathColumns (path);
  const AlignRowPath& row1 = path.at (node1);
  const AlignRowPath& row2 = path.at (node2);
  AlignRowPath& r1 = p[node1];
  AlignRowPath& r2 = p[node2];
  for (AlignColIndex col = 0; col < cols; ++col) {
    const bool c1 = row1[col];
    const bool c2 = row2[col];
    if (!(c1 || c2))
      continue;
    const ProbModel::State state = ProbModel::getState (c1, c2);
    switch (state) {
    case ProbModel::Match:
      while (nDel > 0) {
	r1.push_back (false);
	r2.push_back (true);
	--nDel;
      }
    case ProbModel::Insert:
      r1.push_back (c1);
      r2.push_back (c2);
      break;
    case ProbModel::Delete:
      ++nDel;
    default:
      break;
    }
  }
  while (nDel > 0) {
    r1.push_back (false);
    r2.push_back (true);
    --nDel;
  }
  return p;
}

AlignPath Sampler::triplePath (const AlignPath& path, TreeNodeIndex lChild, TreeNodeIndex rChild, TreeNodeIndex parent) {
  AlignPath p;
  size_t nLeftIns = 0;
  const AlignColIndex cols = alignPathColumns (path);
  const AlignRowPath& lRow = path.at (lChild);
  const AlignRowPath& rRow = path.at (rChild);
  const AlignRowPath& pRow = path.at (parent);
  AlignRowPath& lr = p[lChild];
  AlignRowPath& rr = p[rChild];
  AlignRowPath& pr = p[parent];
  for (AlignColIndex col = 0; col < cols; ++col) {
    const bool lc = lRow[col];
    const bool rc = rRow[col];
    const bool pc = pRow[col];
    if (!(lc || rc || pc))
      continue;
    const auto state = Sampler::SiblingMatrix::getState (Sampler::SiblingMatrix::IMM, lc, rc, pc);
    switch (state) {
    case Sampler::SiblingMatrix::IMM:
    case Sampler::SiblingMatrix::IMD:
    case Sampler::SiblingMatrix::IDM:
    case Sampler::SiblingMatrix::IDD:
      while (nLeftIns > 0) {
	lr.push_back (true);
	rr.push_back (false);
	pr.push_back (false);
	--nLeftIns;
      }
    case Sampler::SiblingMatrix::IMI:
      lr.push_back (lc);
      rr.push_back (rc);
      pr.push_back (pc);
      break;
    case Sampler::SiblingMatrix::IIW:
      ++nLeftIns;
    default:
      Abort ("bad state");
      break;
    }
  }
  while (nLeftIns > 0) {
    lr.push_back (true);
    rr.push_back (false);
    pr.push_back (false);
    --nLeftIns;
  }
  return p;
}

AlignPath Sampler::branchPath (const AlignPath& path, const Tree& tree, TreeNodeIndex node) {
  const TreeNodeIndex parent = tree.parentNode(node);
  Assert (parent >= 0, "Parent node not found");
  return pairPath (path, parent, node);
}

map<TreeNodeIndex,Sampler::PosWeightMatrix> Sampler::getConditionalPWMs (const History& history, const map<TreeNodeIndex,TreeNodeIndex>& exclude) const {
  map<TreeNodeIndex,PosWeightMatrix> pwms;
  AlignColSumProduct colSumProd (model, history.tree, history.gapped);
  while (!colSumProd.alignmentDone()) {
    colSumProd.fillUp();
    colSumProd.fillDown();
    for (const auto& node_exclude : exclude)
      if (!colSumProd.isGap (node_exclude.first))
	pwms[node_exclude.first].push_back (colSumProd.logNodeExcludedPostProb (node_exclude.first, node_exclude.second));
    colSumProd.nextColumn();
  }
  return pwms;
}

LogProb Sampler::logLikelihood (const History& history, const char* suffix) const {
  const Alignment align (history.gapped);
  const LogProb lpTree = treePrior.treeLogLikelihood (history.tree);
  const double rootExt = rootExtProb (model);
  const LogProb lpRoot = log(1. - rootExt) + log(rootExt) * alignPathResiduesInRow (align.path.at (history.tree.root()));
  LogProb lpGaps = 0;
  for (TreeNodeIndex node = 0; node < history.tree.root(); ++node) {
    const TreeNodeIndex parent = history.tree.parentNode (node);
    const ProbModel probModel (model, history.tree.branchLength (node));
    const AlignPath path = pairPath (align.path, parent, node);
    lpGaps += logBranchPathLikelihood (probModel, path, parent, node);
  }
  AlignColSumProduct colSumProd (model, history.tree, history.gapped);
  LogProb lpSub = 0;
  while (!colSumProd.alignmentDone()) {
    colSumProd.fillUp();
    lpSub += colSumProd.colLogLike;
    colSumProd.nextColumn();
  }
  const LogProb lp = lpTree + lpRoot + lpGaps + lpSub;
  LogThisAt(6,"log(L" << suffix << ") = " << setw(10) << lpTree << " (tree) + " << setw(10) << lpRoot << " (root) + " << setw(10) << lpGaps << " (indels) + " << setw(10) << lpSub << " (substitutions) = " << lp << endl);
  return lp;
}

LogProb Sampler::logBranchPathLikelihood (const ProbModel& probModel, const AlignPath& path, TreeNodeIndex parent, TreeNodeIndex child) {
  const AlignColIndex cols = alignPathColumns (path);
  ProbModel::State state = ProbModel::Start;
  LogProb lp = 0;
  for (AlignColIndex col = 0; col < cols; ++col) {
    const ProbModel::State nextState = ProbModel::getState (path.at(parent)[col], path.at(child)[col]);
    lp += log (probModel.transProb (state, nextState));
    state = nextState;
  }
  lp += log (probModel.transProb (state, ProbModel::End));
  return lp;
}

Sampler::PosWeightMatrix Sampler::preMultiply (const PosWeightMatrix& child, const LogProbModel::LogProbMatrix& submat) {
  PosWeightMatrix pwm;
  pwm.reserve (child.size());
  for (const auto& lpp : child) {
    pwm.push_back (vguard<LogProb> (lpp.size(), -numeric_limits<double>::infinity()));
    auto& pre = pwm.back();
    for (AlphTok i = 0; i < pre.size(); ++i)
      for (AlphTok j = 0; j < lpp.size(); ++j)
	log_accum_exp (pre[i], submat[i][j] + lpp[j]);
  }
  return pwm;
}

vguard<LogProb> Sampler::calcInsProbs (const PosWeightMatrix& child, const LogProbModel::LogProbVector& insvec) {
  vguard<LogProb> ins;
  ins.reserve (child.size());
  for (const auto& lpp : child) {
    LogProb lp = -numeric_limits<double>::infinity();
    for (AlphTok i = 0; i < lpp.size(); ++i)
      log_accum_exp (lp, insvec[i] + lpp[i]);
    ins.push_back (lp);
  }
  return ins;
}

Sampler::Move::Move (Type type, const History& history)
  : type (type),
    oldHistory (history)
{ }

void Sampler::Move::initNewHistory (const Tree& tree, const vguard<FastSeq>& ungapped, const AlignPath& path) {
  const Alignment newAlign (ungapped, path);
  initNewHistory (tree, newAlign.gapped());
}

void Sampler::Move::initNewHistory (const Tree& tree, const vguard<FastSeq>& gapped) {
  newHistory.tree = tree;
  newHistory.gapped = gapped;
}

void Sampler::Move::initRatio (const Sampler& sampler) {
  oldLogLikelihood = sampler.logLikelihood (oldHistory, "_old");
  newLogLikelihood = sampler.logLikelihood (newHistory, "_new");

  LogThisAt(5,"log(L_old) = " << oldLogLikelihood << ", log(L_new) = " << newLogLikelihood << ", log(Q_fwd) = " << logForwardProposal << ", log(Q_rev) = " << logReverseProposal << endl);

  logHastingsRatio = (newLogLikelihood - oldLogLikelihood) - (logForwardProposal - logReverseProposal);
}

bool Sampler::Move::accept (random_engine& generator) const {
  bool a;
  if (logHastingsRatio > 0)
    a = true;
  else {
    bernoulli_distribution distribution (exp (logHastingsRatio));
    a = distribution (generator);
  }
  LogThisAt(4,"Move " << (a ? "accepted" : "rejected") << " with log-Hastings ratio " << logHastingsRatio << endl);
  return a;
}

Sampler::BranchAlignMove::BranchAlignMove (const History& history, const Sampler& sampler, random_engine& generator)
  : Move (BranchAlign, history)
{
  node = Sampler::randomChildNode (history.tree, generator);
  parent = history.tree.parentNode (node);

  LogThisAt(4,"Proposing branch realignment move between...\n   node #" << node << ": " << history.tree.seqName(node) << "\n parent #" << parent << ": " << history.tree.seqName(parent) << endl);

  const TreeBranchLength dist = history.tree.branchLength(parent,node);
  
  const TreeNodeIndex parentClosestLeaf = history.tree.closestLeaf (parent, node);
  const TreeNodeIndex nodeClosestLeaf = history.tree.closestLeaf (node, parent);

  const GuideAlignmentEnvelope branchEnv (sampler.guide.path, parentClosestLeaf, nodeClosestLeaf, sampler.maxDistanceFromGuide);

  const Alignment oldAlign (history.gapped);
  const AlignPath pCladePath = Sampler::cladePath (oldAlign.path, history.tree, parent, node);
  const AlignPath nCladePath = Sampler::cladePath (oldAlign.path, history.tree, node, parent);
  
  const vguard<SeqIdx> parentEnvPos = guideSeqPos (oldAlign.path, parent, parentClosestLeaf);
  const vguard<SeqIdx> nodeEnvPos = guideSeqPos (oldAlign.path, node, nodeClosestLeaf);

  map<TreeNodeIndex,TreeNodeIndex> exclude;
  exclude[node] = parent;
  exclude[parent] = node;
  const auto pwms = sampler.getConditionalPWMs (history, exclude);
  const PosWeightMatrix& pSeq = pwms.at (parent);
  const PosWeightMatrix& nSeq = pwms.at (node);

  const BranchMatrix branchMatrix (sampler.model, pSeq, nSeq, dist, branchEnv, parentEnvPos, nodeEnvPos, parent, node);

  const AlignPath newBranchPath = branchMatrix.sample (generator);
  const LogProb logPostNewBranchPath = branchMatrix.logPostProb (newBranchPath);

  const AlignPath oldBranchPath = Sampler::branchPath (oldAlign.path, history.tree, node);
  const LogProb logPostOldBranchPath = branchMatrix.logPostProb (oldBranchPath);

  const vguard<AlignPath> mergeComponents = { pCladePath, newBranchPath, nCladePath };
  const AlignPath newPath = alignPathMerge (mergeComponents);

  logForwardProposal = logPostNewBranchPath;
  logReverseProposal = logPostOldBranchPath;

  initNewHistory (oldHistory.tree, oldAlign.ungapped, newPath);
  initRatio (sampler);
}

Sampler::NodeAlignMove::NodeAlignMove (const History& history, const Sampler& sampler, random_engine& generator)
  : Move (NodeAlign, history)
{
  node = Sampler::randomInternalNode (history.tree, generator);

  Assert (history.tree.nChildren(node) == 2, "Non-binary tree");
  leftChild = history.tree.getChild (node, 0);
  rightChild = history.tree.getChild (node, 1);

  parent = history.tree.parentNode (node);

  LogThisAt(4,"Proposing node realignment move between...\n        node #" << node << ": " << history.tree.seqName(node) << "\n  left-child #" << leftChild << ": " << history.tree.seqName(leftChild) << "\n right-child #" << rightChild << ": " << history.tree.seqName(rightChild) << (parent >= 0 ? (string("\n      parent #") + to_string(parent) + ": " + history.tree.seqName(parent)) : string()) << endl);

  const TreeBranchLength lDist = history.tree.branchLength(node,leftChild);
  const TreeBranchLength rDist = history.tree.branchLength(node,rightChild);
  
  const TreeNodeIndex leftChildClosestLeaf = history.tree.closestLeaf (leftChild, node);
  const TreeNodeIndex rightChildClosestLeaf = history.tree.closestLeaf (rightChild, node);

  const Alignment oldAlign (history.gapped);
  const AlignPath lCladePath = Sampler::cladePath (oldAlign.path, history.tree, leftChild, node);
  const AlignPath rCladePath = Sampler::cladePath (oldAlign.path, history.tree, rightChild, node);

  const vguard<SeqIdx> leftChildEnvPos = guideSeqPos (oldAlign.path, leftChild, leftChildClosestLeaf);
  const vguard<SeqIdx> rightChildEnvPos = guideSeqPos (oldAlign.path, rightChild, rightChildClosestLeaf);

  const GuideAlignmentEnvelope siblingEnv (sampler.guide.path, leftChildClosestLeaf, rightChildClosestLeaf, sampler.maxDistanceFromGuide);

  map<TreeNodeIndex,TreeNodeIndex> exclude;
  exclude[leftChild] = node;
  exclude[rightChild] = node;
  if (parent >= 0) {
    exclude[node] = parent;
    exclude[parent] = node;
  }
  const auto pwms = sampler.getConditionalPWMs (history, exclude);
  const PosWeightMatrix& lSeq = pwms.at (leftChild);
  const PosWeightMatrix& rSeq = pwms.at (rightChild);

  const SiblingMatrix sibMatrix (sampler.model, lSeq, rSeq, lDist, rDist, siblingEnv, leftChildEnvPos, rightChildEnvPos, leftChild, rightChild, node);
  const AlignPath newSiblingPath = sibMatrix.sample (generator);
  const LogProb logPostNewSiblingPath = sibMatrix.logPostProb (newSiblingPath);

  const AlignPath oldSiblingPath = Sampler::triplePath (oldAlign.path, leftChild, rightChild, node);
  const LogProb logPostOldSiblingPath = sibMatrix.logPostProb (oldSiblingPath);

  logForwardProposal = logPostNewSiblingPath;
  logReverseProposal = logPostOldSiblingPath;

  vguard<AlignPath> mergeComponents = { lCladePath, rCladePath, newSiblingPath };
  AlignPath newPath = alignPathMerge (mergeComponents);

  vguard<FastSeq> newUngapped = oldAlign.ungapped;
  newUngapped[node].seq = string (alignPathResiduesInRow (newSiblingPath.at (node)), Alignment::wildcardChar);

  if (parent >= 0) {  // don't attempt to align to parent if node is root
    const PosWeightMatrix& pSeq = pwms.at (parent);
    const TreeBranchLength pDist = history.tree.branchLength(parent,node);

    const TreeNodeIndex nodeClosestLeaf = history.tree.closestLeaf (node, parent);
    const TreeNodeIndex parentClosestLeaf = history.tree.closestLeaf (parent, node);

    const GuideAlignmentEnvelope branchEnv (sampler.guide.path, parentClosestLeaf, nodeClosestLeaf, sampler.maxDistanceFromGuide);
    const AlignPath& nodeSubtreePath = newPath;
    const vguard<SeqIdx> newNodeEnvPos = guideSeqPos (nodeSubtreePath, node, nodeClosestLeaf);
    const vguard<SeqIdx> oldNodeEnvPos = guideSeqPos (oldAlign.path, node, nodeClosestLeaf);

    const AlignPath pCladePath = Sampler::cladePath (oldAlign.path, history.tree, parent, node);
    const vguard<SeqIdx> parentEnvPos = guideSeqPos (oldAlign.path, parent, parentClosestLeaf);

    const PosWeightMatrix newNodeSeq = sibMatrix.parentSeq (newSiblingPath);
    const BranchMatrix newBranchMatrix (sampler.model, pSeq, newNodeSeq, pDist, branchEnv, parentEnvPos, newNodeEnvPos, parent, node);

    const AlignPath newBranchPath = newBranchMatrix.sample (generator);
    const LogProb logPostNewBranchPath = newBranchMatrix.logPostProb (newBranchPath);

    const PosWeightMatrix& oldNodeSeq = sibMatrix.parentSeq (oldSiblingPath);
    const BranchMatrix oldBranchMatrix (sampler.model, pSeq, oldNodeSeq, pDist, branchEnv, parentEnvPos, oldNodeEnvPos, parent, node);
    const AlignPath oldBranchPath = Sampler::branchPath (oldAlign.path, history.tree, node);
    const LogProb logPostOldBranchPath = oldBranchMatrix.logPostProb (oldBranchPath);

    logForwardProposal += logPostNewBranchPath;
    logReverseProposal += logPostOldBranchPath;

    LogThisAt(6,"log(Q_new) = " << setw(10) << logPostNewSiblingPath << " (node:children) + " << setw(10) << logPostNewBranchPath << " (parent:node) = " << logForwardProposal << endl);
    LogThisAt(6,"log(Q_old) = " << setw(10) << logPostOldSiblingPath << " (node:children) + " << setw(10) << logPostOldBranchPath << " (parent:node) = " << logReverseProposal << endl);

    mergeComponents.push_back (pCladePath);
    mergeComponents.push_back (newBranchPath);
    newPath = alignPathMerge (mergeComponents);
  }

  initNewHistory (oldHistory.tree, newUngapped, newPath);
  initRatio (sampler);
}

Sampler::PruneAndRegraftMove::PruneAndRegraftMove (const History& history, const Sampler& sampler, random_engine& generator)
  : Move (PruneAndRegraft, history)
{
  const vguard<TreeBranchLength>& distanceFromRoot = history.tree.distanceFromRoot();

  node = Sampler::randomGrandchildNode (history.tree, generator);
  newSibling = Sampler::randomContemporaneousNode (history.tree, distanceFromRoot, node, generator);

  parent = history.tree.parentNode (node);
  Assert (parent >= 0, "Parent node not found");

  oldGrandparent = history.tree.parentNode (parent);
  Assert (oldGrandparent >= 0, "Grandparent node not found");

  newGrandparent = history.tree.parentNode (newSibling);
  Assert (newGrandparent >= 0, "Grandparent node not found");

  oldSibling = history.tree.getSibling (node);

  LogThisAt(4,"Proposing prune-and-regraft move at...\n            node #" << node << ": " << history.tree.seqName(node) << "\n          parent #" << parent << ": " << history.tree.seqName(parent) << "\n     old sibling #" << oldSibling << ": " << history.tree.seqName(oldSibling) << "\n old grandparent #" << oldGrandparent << ": " << history.tree.seqName(oldGrandparent) << "\n     new sibling #" << newSibling << ": " << history.tree.seqName(newSibling) << "\n new grandparent #" << newGrandparent << ": " << history.tree.seqName(newGrandparent) << endl);

  const Tree& oldTree = history.tree;
  
  const TreeBranchLength oldGranParentDist = oldTree.branchLength(oldGrandparent,parent);
  const TreeBranchLength parentNodeDist = oldTree.branchLength(parent,node);
  const TreeBranchLength parentOldSibDist = oldTree.branchLength(parent,oldSibling);

  const TreeBranchLength parentNewSibDist = distanceFromRoot[newSibling] - distanceFromRoot[parent];
  const TreeBranchLength newGranParentDist = distanceFromRoot[parent] - distanceFromRoot[newGrandparent];

  Tree newTree = history.tree;
  newTree.setParent (oldSibling, oldGrandparent, oldGranParentDist + parentOldSibDist);
  newTree.setParent (newSibling, parent, parentNewSibDist);
  newTree.setParent (parent, newGrandparent, newGranParentDist);
  
  const TreeNodeIndex nodeClosestLeaf = oldTree.closestLeaf (node, parent);
  const TreeNodeIndex oldSibClosestLeaf = oldTree.closestLeaf (oldSibling, parent);
  const TreeNodeIndex oldGranClosestLeaf = oldTree.closestLeaf (oldGrandparent, parent);
  const TreeNodeIndex newSibClosestLeaf = newTree.closestLeaf (newSibling, parent);
  const TreeNodeIndex newGranClosestLeaf = newTree.closestLeaf (newGrandparent, parent);
  const TreeNodeIndex oldParentClosestLeaf = oldTree.closestLeaf (parent, oldGrandparent);
  const TreeNodeIndex newParentClosestLeaf = newTree.closestLeaf (parent, newGrandparent);

  const Alignment oldAlign (history.gapped);
  const AlignPath oldSibCladePath = Sampler::cladePath (oldAlign.path, oldTree, oldSibling, parent);
  const AlignPath nodeCladePath = Sampler::cladePath (oldAlign.path, oldTree, node, parent);
  const AlignPath newSibCladePath = Sampler::cladePath (oldAlign.path, oldTree, newSibling, newGrandparent);
  const AlignPath oldGranCladePath = Sampler::cladePath (oldAlign.path, oldTree, oldGrandparent, parent);
  const AlignPath newGranCladePath = Sampler::cladePath (oldAlign.path, newTree, newGrandparent, parent);

  const AlignPath oldSiblingPath = Sampler::triplePath (oldAlign.path, node, oldSibling, parent);
  const AlignPath oldBranchPath = Sampler::branchPath (oldAlign.path, oldTree, parent);

  const AlignPath oldGranSibPath = Sampler::pairPath (oldAlign.path, oldGrandparent, oldSibling);

  const vguard<SeqIdx> nodeEnvPos = guideSeqPos (oldAlign.path, node, nodeClosestLeaf);
  const vguard<SeqIdx> oldSibEnvPos = guideSeqPos (oldAlign.path, oldSibling, oldSibClosestLeaf);
  const vguard<SeqIdx> oldGranEnvPos = guideSeqPos (oldAlign.path, oldGrandparent, oldGranClosestLeaf);
  const vguard<SeqIdx> newSibEnvPos = guideSeqPos (oldAlign.path, newSibling, newSibClosestLeaf);
  const vguard<SeqIdx> newGranEnvPos = guideSeqPos (oldAlign.path, newGrandparent, newGranClosestLeaf);

  const GuideAlignmentEnvelope newSibEnv (sampler.guide.path, nodeClosestLeaf, newSibClosestLeaf, sampler.maxDistanceFromGuide);
  const GuideAlignmentEnvelope oldSibEnv (sampler.guide.path, nodeClosestLeaf, oldSibClosestLeaf, sampler.maxDistanceFromGuide);

  map<TreeNodeIndex,TreeNodeIndex> exclude;
  exclude[node] = parent;
  exclude[oldSibling] = parent;
  exclude[oldGrandparent] = parent;
  exclude[newSibling] = newGrandparent;
  exclude[newGrandparent] = newSibling;
  const auto pwms = sampler.getConditionalPWMs (history, exclude);

  const PosWeightMatrix& nodeSeq = pwms.at (node);
  const PosWeightMatrix& oldSibSeq = pwms.at (oldSibling);
  const PosWeightMatrix& oldGranSeq = pwms.at (oldGrandparent);
  const PosWeightMatrix& newSibSeq = pwms.at (newSibling);
  const PosWeightMatrix& newGranSeq = pwms.at (newGrandparent);

  const SiblingMatrix newSibMatrix (sampler.model, nodeSeq, newSibSeq, parentNodeDist, parentNewSibDist, newSibEnv, nodeEnvPos, newSibEnvPos, node, newSibling, parent);
  const AlignPath newSiblingPath = newSibMatrix.sample (generator);
  const LogProb logPostNewSiblingPath = newSibMatrix.logPostProb (newSiblingPath);

  const SiblingMatrix oldSibMatrix (sampler.model, nodeSeq, oldSibSeq, parentNodeDist, parentOldSibDist, oldSibEnv, nodeEnvPos, oldSibEnvPos, node, oldSibling, parent);
  const LogProb logPostOldSiblingPath = oldSibMatrix.logPostProb (oldSiblingPath);

  vguard<AlignPath> mergeComponents = { nodeCladePath, newSibCladePath, newSiblingPath };
  const AlignPath newParentSubtreePath = alignPathMerge (mergeComponents);

  const GuideAlignmentEnvelope newBranchEnv (sampler.guide.path, newGranClosestLeaf, newParentClosestLeaf, sampler.maxDistanceFromGuide);
  const GuideAlignmentEnvelope oldBranchEnv (sampler.guide.path, oldGranClosestLeaf, oldParentClosestLeaf, sampler.maxDistanceFromGuide);

  const vguard<SeqIdx> newParentEnvPos = guideSeqPos (newParentSubtreePath, parent, newParentClosestLeaf);
  const vguard<SeqIdx> oldParentEnvPos = guideSeqPos (oldAlign.path, parent, oldParentClosestLeaf);

  const PosWeightMatrix newParentSeq = newSibMatrix.parentSeq (newSiblingPath);
  const BranchMatrix newBranchMatrix (sampler.model, newGranSeq, newParentSeq, newGranParentDist, newBranchEnv, newGranEnvPos, newParentEnvPos, newGrandparent, parent);

  const AlignPath newBranchPath = newBranchMatrix.sample (generator);
  const LogProb logPostNewBranchPath = newBranchMatrix.logPostProb (newBranchPath);

  const PosWeightMatrix oldParentSeq = oldSibMatrix.parentSeq (oldSiblingPath);
  const BranchMatrix oldBranchMatrix (sampler.model, oldGranSeq, oldParentSeq, oldGranParentDist, oldBranchEnv, oldGranEnvPos, oldParentEnvPos, oldGrandparent, parent);
  const LogProb logPostOldBranchPath = oldBranchMatrix.logPostProb (oldBranchPath);

  logForwardProposal = logPostNewSiblingPath + logPostNewBranchPath;
  logReverseProposal = logPostOldSiblingPath + logPostOldBranchPath;

  LogThisAt(6,"log(Q_new) = " << setw(10) << logPostNewSiblingPath << " (parent:node,newSibling) + " << setw(10) << logPostNewBranchPath << " (newGrandparent:parent) = " << logForwardProposal << endl);
  LogThisAt(6,"log(Q_old) = " << setw(10) << logPostOldSiblingPath << " (parent:node,oldSibling) + " << setw(10) << logPostOldBranchPath << " (oldGrandparent:parent) = " << logReverseProposal << endl);

  mergeComponents.push_back (newGranCladePath);
  mergeComponents.push_back (newBranchPath);
  const AlignPath newPath = alignPathMerge (mergeComponents);

  vguard<FastSeq> newUngapped = oldAlign.ungapped;
  newUngapped[parent].seq = string (alignPathResiduesInRow (newSiblingPath.at (parent)), Alignment::wildcardChar);

  initNewHistory (newTree, newUngapped, newPath);
  if (parent < newSibling)
    newHistory.swapNodes (parent, newSibling);
  else if (parent > newGrandparent)
    newHistory.swapNodes (parent, newGrandparent);

  initRatio (sampler);
}

Sampler::NodeHeightMove::NodeHeightMove (const History& history, const Sampler& sampler, random_engine& generator)
  : Move (NodeHeight, history)
{
  node = Sampler::randomInternalNode (history.tree, generator);
  Assert (history.tree.nChildren(node) == 2, "Tree is not binary");
  leftChild = history.tree.getChild (node, 0);
  rightChild = history.tree.getChild (node, 1);
  parent = history.tree.parentNode (node);

  LogThisAt(4,"Proposing node-height move at...\n        node #" << node << ": " << history.tree.seqName(node) << "\n  left-child #" << leftChild << ": " << history.tree.seqName(leftChild) << "\n right-child #" << rightChild << ": " << history.tree.seqName(rightChild) << (parent >= 0 ? (string("\n      parent #") + to_string(parent) + ": " + history.tree.seqName(parent)) : string()) << endl);

  const TreeBranchLength lChildDist = history.tree.branchLength(leftChild);
  const TreeBranchLength rChildDist = history.tree.branchLength(rightChild);
  const TreeBranchLength minChildDist = max (0., (double) min (lChildDist, rChildDist) - Tree::minBranchLength);

  Tree newTree = history.tree;

  if (parent < 0) {
    const double lambda = sampler.treePrior.coalescenceRate (2);
    exponential_distribution<TreeBranchLength> distribution (lambda);
    const TreeBranchLength coalescenceTime = distribution (generator);

    newTree.node[leftChild].d = (lChildDist - minChildDist) + coalescenceTime;
    newTree.node[rightChild].d = (rChildDist - minChildDist) + coalescenceTime;
    
    logForwardProposal = log(lambda) - lambda * coalescenceTime;
    logReverseProposal = log(lambda) - lambda * minChildDist;

    LogThisAt(6,"Sampled coalescence time of #" << leftChild << " and #" << rightChild << ": " << coalescenceTime << " (previously " << minChildDist << ")" << endl);

  } else {
    const TreeBranchLength pDist = max (0., (double) history.tree.branchLength(node) - Tree::minBranchLength);
    const TreeBranchLength pDistRange = pDist + minChildDist;

    uniform_real_distribution<TreeBranchLength> distribution (0, pDistRange);
    const TreeBranchLength pDistNew = distribution (generator);
    const TreeBranchLength cDistNew = pDistRange - pDistNew;

    newTree.node[node].d = pDistNew + Tree::minBranchLength;
    newTree.node[leftChild].d = (lChildDist - minChildDist) + cDistNew;
    newTree.node[rightChild].d = (rChildDist - minChildDist) + cDistNew;

    logForwardProposal = logReverseProposal = 0;

    LogThisAt(6,"Sampled coalescence time of #" << leftChild << " and #" << rightChild << ": " << cDistNew << " (previously " << minChildDist << ", maximum " << pDist << ")" << endl);
  }

  initNewHistory (newTree, oldHistory.gapped);
  initRatio (sampler);
}

Sampler::BranchMatrix::BranchMatrix (const RateModel& model, const PosWeightMatrix& xSeq, const PosWeightMatrix& ySeq, TreeBranchLength dist, const GuideAlignmentEnvelope& env, const vguard<SeqIdx>& xEnvPos, const vguard<SeqIdx>& yEnvPos, AlignRowIndex x, AlignRowIndex y)
  : SparseDPMatrix (env, xEnvPos, yEnvPos),
    model (model),
    probModel (model, dist),
    logProbModel (probModel),
    xRow (x),
    yRow (y),
    xSeq (xSeq),
    ySub (Sampler::preMultiply (ySeq, logProbModel.logSubProb)),
    yEmit (Sampler::calcInsProbs (ySeq, logProbModel.logInsProb))
{
  mm = lpTrans (ProbModel::Match, ProbModel::Match);
  mi = lpTrans (ProbModel::Match, ProbModel::Insert);
  md = lpTrans (ProbModel::Match, ProbModel::Delete);
  me = lpTrans (ProbModel::Match, ProbModel::End);

  im = lpTrans (ProbModel::Insert, ProbModel::Match);
  ii = lpTrans (ProbModel::Insert, ProbModel::Insert);
  id = lpTrans (ProbModel::Insert, ProbModel::Delete);
  ie = lpTrans (ProbModel::Insert, ProbModel::End);

  dm = lpTrans (ProbModel::Delete, ProbModel::Match);
  dd = lpTrans (ProbModel::Delete, ProbModel::Delete);
  de = lpTrans (ProbModel::Delete, ProbModel::End);

  ProgressLog (plog, 5);
  plog.initProgress ("Branch alignment matrix (%u*%u)", xSize, ySize);

  lpStart() = 0;
  for (SeqIdx xpos = 0; xpos < xSize; ++xpos) {

    plog.logProgress (xpos / (double) (xSize - 1), "row %d/%d", xpos + 1, xSize);

    for (SeqIdx ypos = 0; ypos < ySize; ++ypos)
      if (inEnvelope (xpos, ypos)) {
	XYCell& dest = xyCell (xpos, ypos);

	if (xpos > 0 && inEnvelope (xpos - 1, ypos)) {
	  const XYCell& dSrc = xyCell (xpos - 1, ypos);

	  dest(ProbModel::Delete) = log_sum_exp (dSrc(ProbModel::Match) + md,
						 dSrc(ProbModel::Insert) + id,
						 dSrc(ProbModel::Delete) + dd);
	}

      	if (ypos > 0 && inEnvelope (xpos, ypos - 1)) {
	  const XYCell& iSrc = xyCell (xpos, ypos - 1);
	  const LogProb yEmitScore = yEmit[ypos - 1];

	  dest(ProbModel::Insert) = yEmitScore + log_sum_exp (iSrc(ProbModel::Match) + mi,
							      iSrc(ProbModel::Insert) + ii);
	}

	if (xpos > 0 && ypos > 0 && inEnvelope (xpos - 1, ypos - 1)) {
	  const XYCell& mSrc = xyCell (xpos - 1, ypos - 1);
	  const LogProb xyEmitScore = logMatch (xpos, ypos);
	  
	  dest(ProbModel::Match) = xyEmitScore + log_sum_exp (mSrc(ProbModel::Match) + mm,
							      mSrc(ProbModel::Insert) + im,
							      mSrc(ProbModel::Delete) + dm);
	}
      }
  }

  const XYCell& endCell = xyCell (xSize - 1, ySize - 1);

  lpEnd = log_sum_exp (endCell(ProbModel::Match) + me,
		       endCell(ProbModel::Insert) + ie,
		       endCell(ProbModel::Delete) + de);

  if (LoggingThisAt(9))
    writeToLog(9);

  LogThisAt(6,"Forward log-likelihood is " << lpEnd << endl);
}

AlignPath Sampler::BranchMatrix::sample (random_engine& generator) const {
  CellCoords coords (xSize - 1, ySize - 1, ProbModel::End);
  AlignRowPath xPath, yPath;
  while (coords.xpos > 0 || coords.ypos > 0) {
    bool x, y;
    getColumn (coords, x, y);
    if (x || y) {
      xPath.push_back (x);
      yPath.push_back (y);
    }
    CellCoords src = coords;
    if (x) --src.xpos;
    if (y) --src.ypos;
    const XYCell& srcCell = xyCell (src.xpos, src.ypos);
    const LogProb e = lpEmit (coords);
    map<CellCoords,LogProb> srcLogProb;
    for (src.state = 0; src.state < (unsigned int) ProbModel::End; ++src.state)
      srcLogProb[src] = srcCell(src.state) + lpTrans ((State) src.state, (State) coords.state) + e;
    coords = random_key_log (srcLogProb, generator);
  }
  AlignPath path;
  path[xRow] = AlignRowPath (xPath.rbegin(), xPath.rend());
  path[yRow] = AlignRowPath (yPath.rbegin(), yPath.rend());

  LogThisAt(6,"Sampled parent-child alignment:" << endl << alignPathString(path));

  return path;
}

LogProb Sampler::BranchMatrix::logPostProb (const AlignPath& path) const {
  return logBranchPathLikelihood (probModel, path, xRow, yRow) - lpEnd;
}

LogProb Sampler::BranchMatrix::lpTrans (State src, State dest) const {
  return log (probModel.transProb (src, dest));
}

LogProb Sampler::BranchMatrix::lpEmit (const CellCoords& coords) const {
  switch ((State) coords.state) {
  case ProbModel::Match: return coords.xpos > 0 && coords.ypos > 0 ? logMatch (coords.xpos, coords.ypos) : -numeric_limits<double>::infinity();
  case ProbModel::Insert: return coords.ypos > 0 ? yEmit[coords.ypos - 1] : -numeric_limits<double>::infinity();
  default: break;
  }
  return 0;
}

void Sampler::BranchMatrix::getColumn (const CellCoords& coords, bool& x, bool& y) {
  x = y = false;
  switch ((State) coords.state) {
  case ProbModel::Match: if (coords.xpos > 0 && coords.ypos > 0) x = y = true; break;
  case ProbModel::Insert: y = true; break;
  case ProbModel::Delete: x = true; break;
  default: break;
  }
}

Sampler::SiblingMatrix::SiblingMatrix (const RateModel& model, const PosWeightMatrix& lSeq, const PosWeightMatrix& rSeq, TreeBranchLength plDist, TreeBranchLength prDist, const GuideAlignmentEnvelope& env, const vguard<SeqIdx>& lEnvPos, const vguard<SeqIdx>& rEnvPos, AlignRowIndex l, AlignRowIndex r, AlignRowIndex p)
  : SparseDPMatrix (env, lEnvPos, rEnvPos),
    model (model),
    lProbModel (model, plDist),
    rProbModel (model, prDist),
    lLogProbModel (lProbModel),
    rLogProbModel (rProbModel),
    logRoot (log_gsl_vector (model.insProb)),
    lRow (l),
    rRow (r),
    pRow (p),
    lSub (Sampler::preMultiply (lSeq, lLogProbModel.logSubProb)),
    rSub (Sampler::preMultiply (rSeq, rLogProbModel.logSubProb)),
    lEmit (Sampler::calcInsProbs (lSeq, lLogProbModel.logInsProb)),
    rEmit (Sampler::calcInsProbs (rSeq, rLogProbModel.logInsProb))
{
  imm_www = lNoIns() + rNoIns();
  imm_imi = rIns();
  imm_iiw = lIns() + rNoIns();

  imd_wwx = lNoIns();
  imd_iix = lIns();

  idm_wxw = rNoIns();
  idm_idi = rIns();

  idd_imm = iddExit() + rootExt() + lNoDelExt() + rNoDelExt();
  idd_imd = iddExit() + rootExt() + lNoDelExt() + rDelExt();
  idd_idm = iddExit() + rootExt() + lDelExt() + rNoDelExt();
  idd_eee = iddExit() + rootNoExt() + lNoDelExt() + rNoDelExt();

  www_imm = rootExt() + lNoDel() + rNoDel();
  www_imd = rootExt() + lNoDel() + rDel();
  www_idm = rootExt() + lDel() + rNoDel();
  www_idd = rootExt() + lDel() + rDel();
  www_eee = 0;

  wwx_imm = rootExt() + lNoDel() + rNoDelExt();
  wwx_imd = rootExt() + lNoDel() + rDelExt();
  wwx_idm = rootExt() + lDel() + rNoDelExt();
  wwx_idd = rootExt() + lDel() + rDelExt();
  wwx_eee = rNoDelExt();

  wxw_imm = rootExt() + lNoDelExt() + rNoDel();
  wxw_imd = rootExt() + lNoDelExt() + rDel();
  wxw_idm = rootExt() + lDelExt() + rNoDel();
  wxw_idd = rootExt() + lDelExt() + rDel();
  wxw_eee = lNoDelExt();

  imi_www = lNoIns() + rNoInsExt();
  imi_imi = rInsExt();
  imi_iiw = lIns() + rNoInsExt();

  iiw_www = lNoInsExt();
  iiw_iiw = lInsExt();

  idi_wxw = rNoInsExt();
  idi_idi = rInsExt();

  iix_wwx = lNoInsExt();
  iix_iix = lInsExt();

  ProgressLog (plog, 5);
  plog.initProgress ("Parent proposal matrix (%u*%u)", xSize, ySize);

  lpStart() = 0;
  for (SeqIdx xpos = 0; xpos < xSize; ++xpos) {

    plog.logProgress (xpos / (double) (xSize - 1), "row %d/%d", xpos + 1, xSize);

    for (SeqIdx ypos = 0; ypos < ySize; ++ypos)
      if (inEnvelope (xpos, ypos)) {
	XYCell& dest = xyCell (xpos, ypos);

	if (xpos > 0 && inEnvelope (xpos - 1, ypos)) {
	  const XYCell& lSrc = xyCell (xpos - 1, ypos);
	  const LogProb lEmitScore = lEmit[xpos - 1];

	  dest(IIW) = lEmitScore + log_sum_exp (lSrc(IMM) + imm_iiw,
						lSrc(IMI) + imi_iiw,
						lSrc(IIW) + iiw_iiw);

	  dest(IIX) = lEmitScore + log_sum_exp (lSrc(IMD) + imd_iix,
						lSrc(IIX) + iix_iix);

	  dest(IMD) = lEmitScore + log_sum_exp (lSrc(WWW) + www_imd,
						lSrc(WWX) + wwx_imd,
						lSrc(WXW) + wxw_imd,
						lSrc(IDD) + idd_imd);

	  dest(WWW) = dest(IIW) + imm_www;

	  dest(WWX) = log_sum_exp (dest(IIX) + iix_wwx,
				   dest(IMD) + imd_wwx);
	}

      	if (ypos > 0 && inEnvelope (xpos, ypos - 1)) {
	  const XYCell& rSrc = xyCell (xpos, ypos - 1);
	  const LogProb rEmitScore = rEmit[ypos - 1];

	  dest(IMI) = rEmitScore + log_sum_exp (rSrc(IMM) + imm_imi,
						rSrc(IMI) + imi_imi);

	  dest(IDI) = rEmitScore + log_sum_exp (rSrc(IDM) + idm_idi,
						rSrc(IDI) + idi_idi);

	  dest(IDM) = rEmitScore + log_sum_exp (rSrc(WWW) + www_idm,
						rSrc(WWX) + wwx_idm,
						rSrc(WXW) + wxw_idm,
						rSrc(IDD) + idd_idm);

	  dest(WWW) = log_sum_exp (dest(WWW),
				   dest(IMI) + imi_www);

	  dest(WXW) = log_sum_exp (dest(IDI) + idi_wxw,
				   dest(IDM) + idm_wxw);
	}

	if (xpos > 0 && ypos > 0 && inEnvelope (xpos - 1, ypos - 1)) {
	  const XYCell& lrSrc = xyCell (xpos - 1, ypos - 1);
	  const LogProb lrEmitScore = logMatch (xpos, ypos);
	  
	  dest(IMM) = lrEmitScore + log_sum_exp (lrSrc(WWW) + www_imm,
						 lrSrc(WWX) + wwx_imm,
						 lrSrc(WXW) + wxw_imm,
						 lrSrc(IDD) + idd_imm);

	  dest(WWW) = log_sum_exp (dest(WWW),
				   dest(IMM) + imm_www);
	}

	dest(IDD) = log_sum_exp (dest(WWW) + www_idd,
				 dest(WWX) + wwx_idd,
				 dest(WXW) + wxw_idd);
      }
  }

  const XYCell& endCell = xyCell (xSize - 1, ySize - 1);

  lpEnd = log_sum_exp (endCell(IDD) + idd_eee,
		       endCell(WWW) + www_eee,
		       endCell(WWX) + wwx_eee,
		       endCell(WXW) + wxw_eee);

  if (LoggingThisAt(9))
    writeToLog(9);

  LogThisAt(6,"Forward log-likelihood is " << lpEnd << endl);
}

AlignPath Sampler::SiblingMatrix::sample (random_engine& generator) const {
  CellCoords coords (xSize - 1, ySize - 1, EEE);
  AlignRowPath lPath, rPath, pPath;
  while (coords.xpos > 0 || coords.ypos > 0) {
    bool l, r, p;
    getColumn (coords, l, r, p);
    if (l || r || p) {
      lPath.push_back (l);
      rPath.push_back (r);
      pPath.push_back (p);
    }
    CellCoords src = coords;
    if (l) --src.xpos;
    if (r) --src.ypos;
    const XYCell& srcCell = xyCell (src.xpos, src.ypos);
    const LogProb e = lpEmit (coords);
    map<CellCoords,LogProb> srcLogProb;
    for (src.state = 0; src.state < (unsigned int) EEE; ++src.state)
      srcLogProb[src] = srcCell(src.state) + lpTrans ((State) src.state, (State) coords.state) + e;
    coords = random_key_log (srcLogProb, generator);
  }
  AlignPath path;
  path[lRow] = AlignRowPath (lPath.rbegin(), lPath.rend());
  path[rRow] = AlignRowPath (rPath.rbegin(), rPath.rend());
  path[pRow] = AlignRowPath (pPath.rbegin(), pPath.rend());

  LogThisAt(6,"Sampled sibling-parent alignment:" << endl << alignPathString(path));

  return path;
}

LogProb Sampler::SiblingMatrix::logPostProb (const AlignPath& lrpPath) const {
  LogProb lp = 0;
  State state = SSS;
  const AlignColIndex cols = alignPathColumns (lrpPath);
  for (AlignColIndex col = 0; col < cols; ++col) {
    const State nextState = getState (state, lrpPath.at(lRow)[col], lrpPath.at(rRow)[col], lrpPath.at(pRow)[col]);
    lp += lpTrans (state, nextState);
    state = nextState;
  }
  return lp;
}

LogProb Sampler::SiblingMatrix::lpEmit (const CellCoords& coords) const {
  switch ((State) coords.state) {
  case IMM: return coords.xpos > 0 && coords.ypos > 0 ? logMatch (coords.xpos, coords.ypos) : -numeric_limits<double>::infinity();
  case IDM: case IMI: case IDI: return coords.ypos > 0 ? rEmit[coords.ypos - 1] : -numeric_limits<double>::infinity();
  case IMD: case IIW: case IIX: return coords.xpos > 0 ? lEmit[coords.xpos - 1] : -numeric_limits<double>::infinity();
  default: break;
  }
  return 0;
}

Sampler::SiblingMatrix::State Sampler::SiblingMatrix::getState (State src, bool leftUngapped, bool rightUngapped, bool parentUngapped) {
  if (parentUngapped)
    return leftUngapped ? (rightUngapped ? IMM : IMD) : (rightUngapped ? IDM : IDD);
  if (leftUngapped)
    return (src == IMD || src == IIX) ? IIX : IIW;
  if (rightUngapped)
    return (src == IDM || src == IDI) ? IDI : IMI;
  if (src == IDM || src == IDD || src == IDI)
    return WXW;
  if (src == IMD || src == IIX)
    return WWX;
  return WWW;
}

void Sampler::SiblingMatrix::getColumn (const CellCoords& coords, bool& l, bool& r, bool& p) {
  p = l = r = false;
  switch ((State) coords.state) {
  case IMM: if (coords.xpos > 0 && coords.ypos > 0) p = l = r = true; break;
  case IMD: p = l = true; break;
  case IDM: p = r = true; break;
  case IIW: case IIX: if (coords.xpos > 0) l = true; break;
  case IMI: case IDI: if (coords.ypos > 0) r = true; break;
  default: break;
  }
}

LogProb Sampler::SiblingMatrix::lpTrans (State src, State dest) const {
  switch (src) {
  case IMM:
    switch (dest) {
    case WWW: return imm_www;
    case IMI: return imm_imi;
    case IIW: return imm_iiw;
    default: break;
    }
    break;

  case IMD:
    switch (dest) {
    case WWX: return imd_wwx;
    case IIX: return imd_iix;
    default: break;
    }
    break;

  case IDM:
    switch (dest) {
    case WXW: return idm_wxw;
    case IDI: return idm_idi;
    default: break;
    }
    break;

  case IDD:
    switch (dest) {
    case IMM: return idd_imm;
    case IMD: return idd_imd;
    case IDM: return idd_idm;
    case EEE: return idd_eee;
    default: break;
    }
    break;

  case WWW:
    switch (dest) {
    case IMM: return www_imm;
    case IMD: return www_imd;
    case IDM: return www_idm;
    case IDD: return www_idd;
    case EEE: return www_eee;
    default: break;
    }
    break;

  case WWX:
    switch (dest) {
    case IMM: return wwx_imm;
    case IMD: return wwx_imd;
    case IDM: return wwx_idm;
    case IDD: return wwx_idd;
    case EEE: return wwx_eee;
    default: break;
    }
    break;

  case WXW:
    switch (dest) {
    case IMM: return wxw_imm;
    case IMD: return wxw_imd;
    case IDM: return wxw_idm;
    case IDD: return wxw_idd;
    case EEE: return wxw_eee;
    default: break;
    }
    break;

  case IMI:
    switch (dest) {
    case WWW: return imi_www;
    case IMI: return imi_imi;
    case IIW: return imi_iiw;
    default: break;
    }
    break;

  case IIW:
    switch (dest) {
    case WWW: return iiw_www;
    case IIW: return iiw_iiw;
    default: break;
    }
    break;

  case IDI:
    switch (dest) {
    case WXW: return idi_wxw;
    case IDI: return idi_idi;
    default: break;
    }
    break;

  case IIX:
    switch (dest) {
    case WWX: return iix_wwx;
    case IIX: return iix_iix;
    default: break;
    }
    break;

  default:
    break;
  }
  return -numeric_limits<double>::infinity();
}

Sampler::PosWeightMatrix Sampler::SiblingMatrix::parentSeq (const AlignPath& lrpPath) const {
  PosWeightMatrix pwm;
  pwm.reserve (alignPathResiduesInRow (lrpPath.at (pRow)));
  const AlignColIndex cols = alignPathColumns (lrpPath);
  SeqIdx lPos = 0, rPos = 0;
  for (AlignColIndex col = 0; col < cols; ++col)
    if (lrpPath.at(pRow)[col]) {
      vguard<LogProb> prof (model.alphabetSize(), 0);
      if (lrpPath.at(lRow)[col]) {
	for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	  prof[i] += lSub[lPos][i];
	++lPos;
      }
      if (lrpPath.at(rRow)[col]) {
	for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	  prof[i] += rSub[rPos][i];
	++rPos;
      }
      LogProb norm = -numeric_limits<double>::infinity();
      for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	log_accum_exp (norm, prof[i]);
      for (AlphTok i = 0; i < model.alphabetSize(); ++i)
	prof[i] -= norm;
      pwm.push_back (prof);
    }
  return pwm;
}

Sampler::Sampler (const RateModel& model, const SimpleTreePrior& treePrior, const vguard<FastSeq>& gappedGuide)
  : model (model),
    treePrior (treePrior),
    moveRate (Move::TotalMoveTypes, 1.),
    guide (gappedGuide),
    maxDistanceFromGuide (DefaultMaxDistanceFromGuide)
{ }

Sampler::Move Sampler::proposeMove (const History& oldHistory, random_engine& generator) const {
  const Move::Type type = (Move::Type) random_index (moveRate, generator);
  switch (type) {
  case Move::BranchAlign: return BranchAlignMove (oldHistory, *this, generator);
  case Move::NodeAlign: return NodeAlignMove (oldHistory, *this, generator);
  case Move::PruneAndRegraft: return PruneAndRegraftMove (oldHistory, *this, generator);
  case Move::NodeHeight: return NodeHeightMove (oldHistory, *this, generator);
  default: break;
  }
  Abort ("Unknown move type");
  return Move();
}

void Sampler::addLogger (Logger& logger) {
  loggers.push_back (&logger);
}

Sampler::History Sampler::run (const History& initialHistory, random_engine& generator, unsigned int nSamples) {
  History history (initialHistory);
  for (unsigned int n = 0; n < nSamples; ++n) {
    const Move move = proposeMove (history, generator);
    if (move.accept (generator))
      history = move.newHistory;
    for (auto& logger : loggers)
      logger->logHistory (history);
  }
  return history;
}
