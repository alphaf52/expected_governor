// Copyright MISingularity.io
// All right reserved.
// Author: jiahua.liu@misigularity.io (Liu Jiahua)

// 
// Find expected governor given a parse forset output.
//

#include <fstream>
#include <iostream>
#include <set>
#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <math.h>

#include "expected_governor.h"
#include "parse_forest.pb.h"

#define tcrf_prediction_path "data/tcrf_predict"
#define rule_path "data/tcrf_rule" 
#define binary_headrules_path "data/binary_headrules" 
#define output_path "data/tcrf_expected_governor"

using namespace nlu;

std::vector<std::string> &split(const std::string &s, char delim, 
                                std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
      elems.push_back(item);
  }
  return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, elems);
  return elems;
}

int main(int argc, char **argv) {
  std::map<std::string, int> label_map;
  std::vector<std::string> label_list;
  // read tcrf rules
  int num_of_labels, num_of_urules, num_of_brules;
  std::ifstream fin_rule(rule_path);
  fin_rule >> num_of_labels;
  fin_rule >> num_of_urules;
  fin_rule >> num_of_brules;
  for (int i = 0; i < num_of_labels; i++) {
    int idx;
    float f1, f2;
    std::string tmp, label;
    fin_rule >> tmp >> idx >> label >> f1 >> f2;
    label_map[label] = idx;
    label_list.push_back(label);
  }
  fin_rule.close();

  std::map<std::string, int> binary_headrules;
  // read binary headrules
  std::ifstream fin_bhr(binary_headrules_path);
  int num;
  fin_bhr >> num;
  for (int i = 0; i < num; i++) {
    std::string brules;
    int head_idx, count;
    fin_bhr >> brules >> head_idx >> count;
    binary_headrules[brules] = head_idx;
  }
  fin_bhr.close();

  // read prediction file
  std::ifstream fin(tcrf_prediction_path);
  FILE* outfile = fopen(output_path, "w");
  int num_of_tokens, num_of_basic_units, num_of_nodes, num_of_edges;
  while (fin >> num_of_tokens) {
    ForestSentence fs;
    for (int i = 0; i < num_of_tokens; i++) {
      std::string segment;
      fin >> segment;
      fs.add_tokens(segment);
    }

    ParseForest* forest = fs.mutable_forest();
    fin >> num_of_nodes;
    if (num_of_nodes == -1) {
      continue;
    }
    for (int i = 0; i < num_of_nodes; i++) {
      std::string index;
      int stt, end, tag, upper, basic_unit;
      float inside_score, outside_score;
      fin >> index >> stt >> end >> tag >> upper >> basic_unit 
          >> inside_score >> outside_score;

      NodeInfo* node = forest->add_nodes();
      node->set_start(stt);
      node->set_end(end);
      node->set_label(tag);
      node->set_upper(upper);
      node->set_basic_unit(basic_unit);
      node->set_inside_score(inside_score);
      node->set_outside_score(outside_score);

      if (basic_unit == 1 && upper == 0) {
        node->set_headword_stt(stt);
        node->set_headword_end(end);
        BasicUnit* bu = fs.add_basic_units();
        bu->set_start(stt);
        bu->set_end(end);
      } else {
        node->set_headword_stt(-1);
        node->set_headword_end(-1);
      }
    }

    int last_head = -1, edge_idx = 0;
    fin >> num_of_edges;
    std::string line;
    std::getline(fin, line);
    for (int i = 0; i < num_of_edges; i++) {
      std::getline(fin, line);
      std::vector<std::string> strs = split(line, ' ');
      if (strs.size() == 3) {
        // unary rule
        int head = std::stoi(strs[0]);
        int tail = std::stoi(strs[1]);
        float merit = std::stof(strs[2]);

        NodeInfo* head_node = forest->mutable_nodes(head);
        NodeInfo* tail_node = forest->mutable_nodes(tail);
        head_node->set_headword_stt(tail_node->headword_stt());
        head_node->set_headword_end(tail_node->headword_end());

        if (head != last_head) {
          for (int j = last_head + 1; j <= head; j++) {
            forest->add_starting_indexes(edge_idx);
          }
          last_head = head;
        }
        HyperEdgeInfo* edge = forest->add_edges();
        edge->set_merit(exp(merit));
        edge->set_head_idx(head);
        edge->add_tail_idx(tail);
        edge_idx++;
      } else {
        // binary rule
        int head = std::stoi(strs[0]);
        int tail0 = std::stoi(strs[1]);
        int tail1 = std::stoi(strs[2]);
        float merit = std::stof(strs[3]);

        NodeInfo* head_node = forest->mutable_nodes(head);
        NodeInfo* tail_node0 = forest->mutable_nodes(tail0);
        NodeInfo* tail_node1 = forest->mutable_nodes(tail1);
        std::string rule = label_list[head_node->label()] + "^" + 
                           label_list[tail_node0->label()] + "^" + 
                           label_list[tail_node1->label()];
        if (binary_headrules[rule] == 0) {
          head_node->set_headword_stt(tail_node0->headword_stt());
          head_node->set_headword_end(tail_node0->headword_end());
        }
        else {
          head_node->set_headword_stt(tail_node1->headword_stt());
          head_node->set_headword_end(tail_node1->headword_end());
        }

        if (head != last_head) {
          for (int j = last_head + 1; j <= head; j++) {
            forest->add_starting_indexes(edge_idx);
          }
          last_head = head;
        }
        HyperEdgeInfo* edge = forest->add_edges();
        edge->set_merit(exp(merit));
        edge->set_head_idx(head);
        edge->add_tail_idx(tail0);
        edge->add_tail_idx(tail1);
        edge_idx++;
      }
    }
    forest->add_starting_indexes(edge_idx);

    GovernorFinder gf(&fs);
    std::vector<GovernorsPerWord> result = 
        gf.GetGovernors()[fs.forest().nodes_size()-1];
    fprintf(outfile, "%d\n", fs.basic_units_size());
    for (int i = 0; i < fs.basic_units_size(); i++) {
      fprintf(outfile, "%d %d %d\n", fs.basic_units(i).start(), 
              fs.basic_units(i).end(), result[i].gms.size());
      for (size_t j = 0; j < result[i].gms.size(); j++) {
        std::string label_u = "ROOT";
        std::string label_parent_of_u = "NONE";
        if (result[i].gms[j].label_u != -1) {
          label_u = label_list[result[i].gms[j].label_u];
        }
        if (result[i].gms[j].label_parent_of_u != -1) {
          label_parent_of_u = label_list[result[i].gms[j].label_parent_of_u];
        }
        fprintf(outfile, "%s %s %s %f\n",
                label_u.c_str(), label_parent_of_u.c_str(),
                result[i].gms[j].headword_parent_of_u.c_str(),
                result[i].gms[j].probability);
      }
    }
  }
  fclose(outfile);
  fin.close();
}
