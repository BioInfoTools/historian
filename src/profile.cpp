#include <sstream>
#include <math.h>
#include "profile.h"
#include "jsonutil.h"
#include "forward.h"

ProfileTransition::ProfileTransition()
  : lpTrans(-numeric_limits<double>::infinity())
{ }

ProfileState::ProfileState (AlphTok alphSize)
  : lpAbsorb(alphSize,-numeric_limits<double>::infinity())
{ }

Profile::Profile (const string& alphabet, const FastSeq& seq, AlignRowIndex rowIndex)
  : alphSize ((AlphTok) alphabet.size()),
    state (seq.length() + 2, ProfileState ((AlphTok) alphabet.size())),
    trans (seq.length() + 1)
{
  name = seq.name;
  state.front() = state.back() = ProfileState();  // start and end are null states
  state.front().name = "START";
  state.back().name = "END";
  const vguard<AlphTok> dsq = seq.tokens (alphabet);
  for (size_t pos = 0; pos <= dsq.size(); ++pos) {
    ProfileTransition& t = trans[pos];
    t.src = pos;
    t.dest = pos + 1;
    t.lpTrans = 0;
    state[pos].out.push_back (pos);
    state[pos+1].in.push_back (pos);
    if (pos < dsq.size()) {
      state[pos+1].name = string(1,seq.seq[pos]) + to_string(pos+1);
      state[pos+1].lpAbsorb[dsq[pos]] = 0;
      state[pos+1].alignPath[rowIndex].push_back (true);
    }
  }
}

Profile Profile::leftMultiply (gsl_matrix* sub) const {
  Profile prof (*this);
  for (ProfileStateIndex i = 0; i < size(); ++i)
    if (!state[i].isNull()) {
      for (AlphTok c = 0; c < alphSize; ++c) {
	LogProb lp = -numeric_limits<double>::infinity();
	for (AlphTok d = 0; d < alphSize; ++d)
	  lp = log_sum_exp (lp, log (gsl_matrix_get(sub,c,d)) + state[i].lpAbsorb[d]);
	prof.state[i].lpAbsorb[c] = lp;
      }
    }
  return prof;
}

const ProfileTransition* Profile::getTrans (ProfileStateIndex src, ProfileStateIndex dest) const {
  for (auto t : state[dest].in)
    if (trans[t].src == src)
      return &trans[t];
  return NULL;
}

void Profile::calcSumPathAbsorbProbs (const vector<LogProb>& input, const char* tag) {
  vector<LogProb> lpCumAbs (state.size(), -numeric_limits<double>::infinity());
  lpCumAbs[0] = 0;
  for (ProfileStateIndex pos = 1; pos < state.size(); ++pos) {
    const LogProb lpAbs = state[pos].isNull() ? 0 : ForwardMatrix::logInnerProduct (input, state[pos].lpAbsorb);
    for (auto ti : state[pos].in) {
      const ProfileTransition& t = trans[ti];
      log_accum_exp (lpCumAbs[pos], lpCumAbs[t.src] + t.lpTrans + lpAbs);
    }
    state[pos].meta[string(tag)] = to_string(lpCumAbs[pos]);
  }
}

string alignPathJson (const AlignPath& a) {
  string s = "[";
  for (auto& row_path : a) {
    if (s.size() > 1)
      s += ",";
    s += " [ " + to_string(row_path.first) + ", \"";
    for (auto col : row_path.second)
      s += (col ? "*" : "-");
    s += "\" ]";
  }
  s += " ]";
  return s;
}

void Profile::writeJson (ostream& out) const {
  out << "{" << endl;
  if (name.size())
    out << " \"name\": \"" << name << "\"," << endl;
  if (meta.size())
    out << " \"meta\": " << JsonUtil::toString(meta,2) << "," << endl;
  out << " \"alphSize\": " << alphSize << "," << endl;
  out << " \"state\": [" << endl;
  for (ProfileStateIndex s = 0; s < state.size(); ++s) {
    out << "  {" << endl;
    out << "   \"n\": " << s << "," << endl;
    if (state[s].name.size())
      out << "   \"name\": \"" << state[s].name << "\"," << endl;
    if (state[s].meta.size())
      out << "   \"meta\": " << JsonUtil::toString(state[s].meta,4) << "," << endl;
    if (state[s].alignPath.size())
      out << "   \"path\": " << alignPathJson(state[s].alignPath) << "," << endl;
    if (!state[s].isNull()) {
      out << "   \"lpAbsorb\": [";
      for (AlphTok a = 0; a < alphSize; ++a)
	out << (a > 0 ? ", " : " ") << JsonUtil::toString (state[s].lpAbsorb[a]);
      out << " ]," << endl;
    }
    out << "   \"trans\": [";
    for (size_t nt = 0; nt < state[s].out.size(); ++nt) {
      const ProfileTransition& tr = trans[state[s].out[nt]];
      if (nt > 0)
	out << ",\n             ";     
      out << " { \"to\": " << tr.dest << ",";
      out << " \"lpTrans\": " << JsonUtil::toString (tr.lpTrans) << " }";
      if (tr.alignPath.size())
	out << ", \"path\": " << alignPathJson(tr.alignPath);
    }
    out << " ]" << endl;
    out << "  }";
    if (s < state.size() - 1)
      out << ",";
    out << endl;
  }
  out << " ]" << endl;
  out << "}" << endl;
}

string Profile::toJson() const {
  ostringstream s;
  writeJson (s);
  return s.str();
}
