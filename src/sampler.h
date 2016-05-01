#ifndef SAMPLER_INCLUDED
#define SAMPLER_INCLUDED

#include "model.h"
#include "tree.h"
#include "fastseq.h"
#include "forward.h"

struct SimpleTreePrior {
  double coalescenceRate;
  LogProb treeLogLikelihood (const Tree& tree) const;
};

struct Sampler {

  typedef DPMatrix::random_engine random_engine;

  // Sampler::SparseDPMatrix
  template <unsigned int CellStates>
  class SparseDPMatrix {
  private:
    struct XYCell {
      LogProb lp[CellStates];
      XYCell() {
	for (size_t s = 0; s < CellStates; ++s)
	  lp[s] = -numeric_limits<double>::infinity();
      }
      LogProb& operator() (unsigned int s) { return lp[s]; }
      LogProb operator() (unsigned int s) const { return lp[s]; }
    };
    vguard<map<SeqIdx,XYCell> > cellStorage;  // partial Forward sums by cell
    XYCell emptyCell;  // always -inf

    const GuideAlignmentEnvelope& env;
    const vguard<SeqIdx>& xEnvPos;
    const vguard<SeqIdx>& yEnvPos;

  public:
    const TokSeq& xSeq;
    const TokSeq& ySeq;

    LogProb lpEnd;

    // cell accessors
    inline XYCell& xyCell (SeqIdx xpos, SeqIdx ypos) { return cellStorage[xpos][ypos]; }
    inline const XYCell& xyCell (SeqIdx xpos, SeqIdx ypos) const {
      const auto& column = cellStorage[xpos];
      auto iter = column.find(ypos);
      return iter == column.end() ? emptyCell : iter->second;
    }

    inline LogProb& cell (SeqIdx xpos, SeqIdx ypos, unsigned int state)
    { return cellStorage[xpos][ypos].lp[state]; }
    inline LogProb cell (SeqIdx xpos, SeqIdx ypos, unsigned int state) const
    {
      const auto& column = cellStorage[xpos];
      auto iter = column.find(ypos);
      return iter == column.end() ? -numeric_limits<double>::infinity() : iter->second.lp[state];
    }

    inline LogProb& lpStart() { return cell(0,0,0); }
    inline const LogProb lpStart() const { return cell(0,0,0); }

    inline bool inEnvelope (SeqIdx xpos, SeqIdx ypos) const {
      return env.inRange (xEnvPos[xpos], yEnvPos[ypos]);
    }
    
    // constructor
    SparseDPMatrix (const TokSeq& xSeq, const TokSeq& ySeq, const GuideAlignmentEnvelope& env, const vguard<SeqIdx>& xEnvPos, const vguard<SeqIdx>& yEnvPos)
      : xSeq(xSeq), ySeq(ySeq), env(env), xEnvPos(xEnvPos), yEnvPos(yEnvPos), lpEnd(-numeric_limits<double>::infinity())
    { }
  };
  
  // Sampler::BranchMatrix
  class BranchMatrix : public SparseDPMatrix<3> {
  public:
    enum State { Start = 0,
		 Match = 0, Insert = 1, Delete = 2,
		 End = 3,
		 SourceStates = 3, DestStates = 4 };

    const RateModel& model;
    TreeBranchLength dist;

    LogProb lpTrans[SourceStates][DestStates];
    vguard<vguard<LogProb> > submat;  // log odds-ratio

    // cell accessors
    static inline AlignRowIndex xRow() { return 0; }
    static inline AlignRowIndex yRow() { return 1; }

    BranchMatrix (const RateModel& model, const TokSeq& xSeq, const TokSeq& ySeq, TreeBranchLength dist, const GuideAlignmentEnvelope& env, const vguard<SeqIdx>& xEnvPos, const vguard<SeqIdx>& yEnvPos);

    void sample (AlignPath& path, random_engine& generator) const;
    LogProb logPostProb (const AlignPath& path) const;
  };

  // Sampler::SiblingMatrix
  struct SiblingMatrix : public SparseDPMatrix<12> {
    enum State { SSS = 0, SSI = 5, SIW = 6,
		 IMM = 0, IMD = 1, IDM = 2, IDD = 3,
		 WWW = 4, WWX = 5, WXW = 6, WXX = 7,
		 IMI = 8, IIW = 9,
		 IDI = 10, IIX = 11,
		 EEE = 12,
		 SourceStates = 11, DestStates = 12 };

    const RateModel& model;
    const ProbModel lProbModel, rProbModel;

    // Transition log-probabilities.
    // The null cycle idd->wxx->idd is prevented by eliminating the transition idd->wxx.
    // To prevent the degeneracy between paths wxx->wxw->www and wxx->wwx->www,
    // the transition wxx->wwx is eliminated and the path wxx->wwx->imd folded into wxx->imd.
    // States {sss,ssi,siw} have same outgoing transition weights as states {imm,imi,iiw}.
    // Forward fill order: {emit states}, {wwx,wxx}, wxw, www, idd.
    // (34 transitions)
    //  To:     imm      imd      idm      idd      w**      imi      iiw      idi      iix      eee
    LogProb                                     imm_www, imm_imi, imm_iiw;
    LogProb                                     imd_wwx,                            imd_iix;
    LogProb                                     idm_wxw,                   idm_idi;
    LogProb idd_imm, idd_imd, idd_idm,                                                       idd_eee;
    LogProb www_imm, www_imd, www_idm, www_idd,                                              www_eee;
    LogProb          wwx_imd,          wwx_idd, wwx_www;
    LogProb                   wxw_idm, wxw_idd, wxw_www;
    LogProb          wxx_imd,          wxx_idd, wxx_wxw;
    LogProb                                     imi_www, imi_imi, imi_iiw;
    LogProb                                     iiw_www,          iiw_iiw;
    LogProb                                     idi_wxw,                  idi_idi;
    LogProb                                     iix_wwx,                           iix_iix;

    // This is 1.4* faster (48/34) but 1.5* fatter (12/8) than with w** eliminated:
    // (48 transitions)
    //        To:     imm      imd      idm      idd      imi      iiw      idi      iix      eee
    //    LogProb imm_imm, imm_imd, imm_idm, imm_idd, imm_imi, imm_iiw,                   imm_eee;
    //    LogProb imd_imm, imd_imd, imd_idm, imd_idd,                            imd_iix, imd_eee;
    //    LogProb idm_imm, idm_imd, idm_idm, idm_idd,                   idm_idi,          idm_eee;
    //    LogProb idd_imm, idd_imd, idd_idm,                                              idd_eee;
    //    LogProb imi_imm, imi_imd, imi_idm, imi_idd, imi_imi, imi_iiw,                   imi_eee;
    //    LogProb iiw_imm, iiw_imd, iiw_idm, iiw_idd,          iiw_iiw,                   iiw_eee;
    //    LogProb idi_imm, idi_imd, idi_idm, idi_idd,                   idi_idi,          idi_eee;
    //    LogProb iix_imm, iix_imd, iix_idm, iix_idd,                            iix_iix, iix_eee;
    
    // Within the DP, score substitutions with a log-odds-ratio matrix & gap emissions with zeroes, for speed.
    // When estimating the parent sequence conditioned on the alignment, we use the "proper" gap emissions.
    vguard<vguard<LogProb> > submat;

    static inline AlignRowIndex lRow() { return 0; }
    static inline AlignRowIndex rRow() { return 1; }
    static inline AlignRowIndex pRow() { return 2; }

    SiblingMatrix (const RateModel& model, const TokSeq& lSeq, const TokSeq& rSeq, TreeBranchLength plDist, TreeBranchLength prDist, const GuideAlignmentEnvelope& env, const vguard<SeqIdx>& xEnvPos, const vguard<SeqIdx>& yEnvPos);

    void sampleAlign (AlignPath& plrPath, random_engine& generator) const;
    LogProb logAlignPostProb (const AlignPath& plrPath) const;

    void sampleParent (TokSeq& pSeq, const AlignPath& plrPath, random_engine& generator) const;
    LogProb logParentPostProb (const TokSeq& pSeq, const AlignPath& plrPath) const;
  };

  // Sampler::History
  struct History {
    vguard<FastSeq> gapped;
    Tree tree;
  };

  // Sampler::Log
  struct Log {
    virtual void log (const History& history) = 0;
  };
  
  // Sampler::Move
  struct Move {
    enum Type { SampleBranch, SampleNode, PruneAndRegraft, SampleNodeHeight, SampleAncestralResidues };
    Type type;
    TreeNodeIndex node, parent, grandparent, leftChild, rightChild, oldSibling, newSibling;  // no single type of move uses all of these
    History oldHistory, newHistory;
    LogProb logProposal, logInverseProposal, logOldLikelihood, logNewLikelihood, logHastingsRatio;

    Move (Type type, const History& history);
    bool accept (random_engine& generator) const;
  };

  struct SampleBranchMove : Move {
    SampleBranchMove (const History&, Sampler&, random_engine&);
  };

  struct SampleNodeMove : Move {
    SampleNodeMove (const History&, Sampler&, random_engine&);
  };

  struct PruneAndRegraftMove : Move {
    PruneAndRegraftMove (const History&, Sampler&, random_engine&);
  };

  struct SampleNodeHeightMove : Move {
    SampleNodeHeightMove (const History&, Sampler&, random_engine&);
  };

  struct SampleAncestralResiduesMove : Move {
    SampleAncestralResiduesMove (const History&, Sampler&, random_engine&);
  };

  // Sampler member variables
  RateModel model;
  SimpleTreePrior treePrior;
  list<Log*> logs;
  map<Move::Type,double> moveRate;
  Alignment guide;
  int maxDistanceFromGuide;
  
  // Sampler constructor
  Sampler (const RateModel& model, const SimpleTreePrior& treePrior, const vguard<FastSeq>& gappedGuide);
  
  // Sampler methods
  void addLog (Log& log);
  Move proposeMove (const History& oldState, random_engine& generator) const;
  void run (History& state, random_engine& generator, int nSamples = 1);

  // Sampler helpers
  static TreeNodeIndex randomInternalNode (const Tree& tree, random_engine& generator);
  static vguard<SeqIdx> guideSeqPos (const AlignPath& path, AlignRowIndex row, AlignRowIndex guideRow);
  TokSeq removeGapsAndTokenize (const FastSeq& gapped) const;
  static AlignPath cladePath (const AlignPath& path, const Tree& tree, TreeNodeIndex cladeRoot, TreeNodeIndex cladeRootParent);
};

#endif /* SAMPLER_INCLUDED */
