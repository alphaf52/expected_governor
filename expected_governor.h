// Copyright MISingularity.io
// All right reserved.
// Author: jiahua.liu@misigularity.io (Liu Jiahua)

// 
// Implementation of computation of expected governor given a sentence and the corresponding parsed forest.
//
// Assume that the nodes in parsed_forest.h is sorted by the length of the span 
// (e.g. (1,1), (2,2), ..., (n,n), (1,2), (2,3), ..., (n-1, n), ......, (1,n)) 
//

#ifndef NLU_CRF_EXPECTED_GOVERNOR_H__
#define NLU_CRF_EXPECTED_GOVERNOR_H__

#define LABEL_NOT_KNOWN_YET -1
#define HEADWORD_NOT_KNOWN_YET "TBD"

#include <vector>
#include <string>

#include "parse_forest.pb.h"

namespace nlu  {

struct GovernorMarkup {
  int label_u;
  int label_parent_of_u;
  std::string headword_parent_of_u;
  float probability;

  GovernorMarkup()
    : label_u(LABEL_NOT_KNOWN_YET), label_parent_of_u(LABEL_NOT_KNOWN_YET),
      headword_parent_of_u(HEADWORD_NOT_KNOWN_YET), probability(1.0) {}
  GovernorMarkup(int lu, int lup, std::string hup, float p)
    : label_u(lu), label_parent_of_u(lup),
      headword_parent_of_u(hup), probability(p) {}
}; 

bool operator==(const GovernorMarkup& lhs, const GovernorMarkup& rhs) {
    return lhs.label_u == rhs.label_u &&
           lhs.label_parent_of_u == rhs.label_parent_of_u &&
           lhs.headword_parent_of_u == rhs.headword_parent_of_u;
}


struct GovernorsPerWord {
  int idx;
  std::vector<GovernorMarkup> gms;
};


class GovernorFinder {
 public:
  GovernorFinder(ForestSentence* forestSentence, bool print_debug_info = false) {
    fs = forestSentence;
    // initialize
    for (int i = 0; i < fs->forest().nodes_size(); i++) {
      std::vector<GovernorsPerWord> v;
      for (int j = 0; j < fs->basic_units_size(); j++) {
        GovernorsPerWord gpw;
        gpw.idx = j;
        v.push_back(gpw);
      }
      governors.push_back(v);
    }

    for (int i = 0; i < fs->forest().nodes_size(); i++) {
      const NodeInfo& node = fs->forest().nodes(i);
      if (node.basic_unit() == 1 && node.upper() == 0) {
        int bu_idx = -1;
        for (int j = 0; j < fs->basic_units_size(); j++) {
          if (fs->basic_units(j).start() == node.start()
              && fs->basic_units(j).end() == node.end()) {
            bu_idx = j;
            break;
          }
        }
        GovernorMarkup m;
        governors[i][bu_idx].gms.push_back(m);
      }
    }

    // compute expected governor markup using a CKY-like algorithm
    for (int i = 0; i < fs->forest().nodes_size(); i++) {
      // compute expected governor for node i
      for (int j = fs->forest().starting_indexes(i);
           j < fs->forest().starting_indexes(i+1); j++) {
        // for each hyper edge (rule) expanding node i
        const HyperEdgeInfo& edge = fs->forest().edges(j);
        if (edge.tail_idx_size() == 2) {
          // binary rule
          int c1 = edge.tail_idx(0), c2 = edge.tail_idx(1);
          // left child
          for (int k = 0; k < fs->basic_units_size(); k++) {
            updateGovernorGivenChild(i, c1, k, true, edge.merit());
          }
          // right child
          for (int k = 0; k < fs->basic_units_size(); k++) {
            updateGovernorGivenChild(i, c2, k, true, edge.merit());
          }
        }
        else {
          // unary Rule
          int c = edge.tail_idx(0);
          for (int k = 0; k < fs->basic_units_size(); k++) {
            updateGovernorGivenChild(i, c, k, false, edge.merit());
          }
        }
      }

      for (size_t j = 0; j < governors[i].size(); j++) {
        float sum = 0.0;
        for (size_t k = 0; k < governors[i][j].gms.size(); k++) {
          sum += governors[i][j].gms[k].probability;
        }
        for (size_t k = 0; k < governors[i][j].gms.size(); k++) {
          governors[i][j].gms[k].probability /= sum;
        }
      }

      if (print_debug_info) {
        printf("\n");
        printf("idx=%d: stt=%d end=%d lbl=%d upper=%d head_stt=%d head_end=%d\n",
               i, fs->forest().nodes(i).start(), fs->forest().nodes(i).end(),
               fs->forest().nodes(i).label(), fs->forest().nodes(i).upper(),
               fs->forest().nodes(i).headword_stt(),
               fs->forest().nodes(i).headword_end());
        for (size_t j = 0; j < governors[i].size(); j++) {
          for (size_t k = 0; k < governors[i][j].gms.size(); k++) {
            printf("%zu: %d %d %s %f\n", j, governors[i][j].gms[k].label_u,
                   governors[i][j].gms[k].label_parent_of_u,
                   governors[i][j].gms[k].headword_parent_of_u.c_str(),
                   governors[i][j].gms[k].probability);
          }
        }
        printf("\n");
      }
    }
  }

  // update expected governor of a particular basic unit (identified by bu_idx)
  // for parent node given one child node
  void updateGovernorGivenChild(int pidx, int cidx, int bu_idx,
                                bool binary_rule, float weight) {
    const NodeInfo& parent = fs->forest().nodes(pidx);
    const NodeInfo& child = fs->forest().nodes(cidx);
    for (size_t i = 0; i < governors[cidx][bu_idx].gms.size(); i++) {
      // for each possible governor markup of this position for child node
      GovernorMarkup m;
      if (binary_rule) {
        // binary rule
        if ((governors[cidx][bu_idx].gms[i].label_parent_of_u == LABEL_NOT_KNOWN_YET)
           && (child.headword_stt() != parent.headword_stt()
               || child.headword_end() != parent.headword_end())) {
          // If the head word of child node is not the head word of parent node,
          // and the governor of this position for child node is not known yet,
          // (which means the word of this position is the head word of child
          // node) get the governor of this position for parent node according
          // to definition
          m.label_u = child.label();
          m.label_parent_of_u = parent.label();
          m.headword_parent_of_u = "";
          for (int j = parent.headword_stt(); j < parent.headword_end(); j++) {
            m.headword_parent_of_u += fs->tokens(j);
          }
          m.probability = governors[cidx][bu_idx].gms[i].probability * weight;
        }
        else {
          // Otherwise, the governor of this position remains the same
          m = governors[cidx][bu_idx].gms[i];
          m.probability *= weight;
        }
      }
      else {
        // unary rule
        if (child.headword_stt() != parent.headword_stt()
            || child.headword_end() != parent.headword_end()) {
          // Get governor markup for the main verb, when process the topmost
          // rule, like START_SYMBOL -> S
          m.label_u = child.label();
          m.label_parent_of_u = parent.label();
          m.headword_parent_of_u = "";
          for (int j = parent.headword_stt(); j < parent.headword_end(); j++) {
            m.headword_parent_of_u += fs->tokens(j);
          }
          m.probability = governors[cidx][bu_idx].gms[i].probability * weight;
        }
        else {
          // In normal situation, governor markup just remains the same for
          // unary rule
          m = governors[cidx][bu_idx].gms[i];
          m.probability *= weight;
        }
      }
      // update governor markup for parent node
      bool exist = false;
      for (size_t j = 0; j < governors[pidx][bu_idx].gms.size(); j++) {
        if (m == governors[pidx][bu_idx].gms[j]) {
          governors[pidx][bu_idx].gms[j].probability += m.probability;
          exist = true;
          break;
        }
      }
      if (!exist) {
        governors[pidx][bu_idx].gms.push_back(m);
      }
    }
  }

  std::vector<std::vector<GovernorsPerWord> > GetGovernors() {
    return governors;
  }

 private:
  ForestSentence* fs;
  // the first dimension indicates idx of node, and the second dimension
  // indicates idx of basic unit
  std::vector<std::vector<GovernorsPerWord> > governors;
};

} // namespace nlu

#endif
