/*
 * expected_governor_ytdu.cpp
 *
 *  Created on: May 12, 2016
 *      Author: ytdu
 *
 *  The algorithms implemented in this code (parse_forest::to_headed(), parse_forest::inside(), parse_forest::flow(), parse_forest::find_expected_governor()) are from:
 * Helmut Schmid and Mats Rooth: Parse forest Computation of Expected Governors. ACL'2001
 */

#include<iostream>
#include<sstream>
#include<cmath>

#include "expected_governor.h"

namespace nlu {
std::vector<std::string> label_map;
std::unordered_map<std::string, int> headrules_map;

std::istream& operator>>(std::istream& in, parse_node& node) {
	char c;
	in >> node.index >> c >> node.span_l >> node.span_r >> node.label_index >> node.is_upper >> node.is_basic_unit;
	return in;
}
std::ostream& operator<<(std::ostream& out, const parse_node& node){
	out << node.index << ": " << node.span_l << " " << node.span_r << " " << node.label_index << " " << node.is_upper << " " << node.is_basic_unit << std::endl;
	return out;
}

std::istream& operator>>(std::istream& in, parse_rule& rule) {
	rule.rhs2 = -1;
	std::string s;
	std::getline(in, s);
	int num_space = 0;
	int i = 0;
	while (s[i]) {
		if (s[i++] == ' ') {
			++num_space;
		}
	}
	std::istringstream isstream(s);
	if (num_space == 2) {
		isstream >> rule.lhs >> rule.rhs1 >> rule.log_prob;
	}
	else if (num_space == 3) {
		isstream >> rule.lhs >> rule.rhs1 >> rule.rhs2 >> rule.log_prob;
	} else {
		std::cerr << "unknown rule format: " << s << "." << std::endl;
		exit(-1);
	}
	return in;
}
std::ostream& operator<<(std::ostream& out, const parse_rule& rule) {
	out << rule.lhs << " " << rule.rhs1 << " ";
	if (rule.rhs2 != -1) {
		out << rule.rhs2 << " ";
	}
	out << rule.log_prob << std::endl;
	return out;
}

void parse_forest::clear() {
	is_headed = false;
	n_basic_units = 0;
	tokens.clear();
	nodes.clear();
	rules.clear();
	heads.clear();
	inside_scores.clear();
	flow_scores.clear();
	e_governors.clear();
}

std::istream& operator>>(std::istream& in, parse_forest& forest) {
	forest.clear();
	int n_words, n_nodes, n_rules;
	in >> n_words;
//	std::cout<<n_words<<std::endl;
	std::string w;
	for (int i = 0; i < n_words; ++i) {
		in >> w;
		forest.tokens.push_back(w);
//		std::cout<<w<<std::endl;
	}
	in >> n_nodes;
//	std::cout<<n_nodes<<std::endl;
	parse_node n;
	int prev_span = 0;
	int prev_upper = 0;
	for (int i = 0; i < n_nodes; ++i) {
		in >> n;
		if (n.span_r - n.span_l < prev_span || (n.span_r - n.span_l == prev_span && n.is_upper > prev_upper)) {
			std::cerr << "Warning: nodes not sorted." << std::endl;
			prev_span = n.span_r - n.span_l;
			prev_upper = n.is_upper;
		}
		forest.nodes.push_back(n);
		if (n.is_basic_unit) {
			++forest.n_basic_units;
		}
	}
	in >> n_rules;
//	std::cout<<n_rules<<std::endl;
	in.get();
	parse_rule r;
	for (int i = 0; i < n_rules; ++i) {
		in >> r;
		n = forest.nodes[r.lhs];
		if (n.span_r - n.span_l < prev_span || (n.span_r - n.span_l == prev_span && n.is_upper > prev_upper)) {
			std::cerr << "Warning: rules not sorted." << std::endl;
			prev_span = n.span_r - n.span_l;
			prev_upper = n.is_upper;
		}
		if (r.log_prob > 0) {
			std::cerr << "Warning: rule log probability greater than 0." << std::endl;
		}
		forest.rules.push_back(r);
	}
	return in;
}
std::ostream& operator<<(std::ostream& out, const parse_forest& forest) {
	out << forest.tokens.size() << std::endl;
	for (int i = 0; i < forest.tokens.size(); ++i) {
		out << forest.tokens[i] << std::endl;
	}
	out << forest.nodes.size() << std::endl;
	for (int i = 0; i < forest.nodes.size(); ++i) {
		out << forest.nodes[i];
	}
	out << forest.rules.size() << std::endl;
	for (int i = 0; i < forest.rules.size(); ++i) {
		out << forest.rules[i];
	}
	return out;
}

template<class T>
int find_in_vector(const std::vector<T>& v, const T& x) {
	for (int i = 0; i < v.size(); ++i) {
		if (v[i] == x) {
			return i;
		}
	}
	return -1;
}

parse_forest parse_forest::to_headed(){
	parse_forest ret;
	ret.is_headed = true;
	ret.n_basic_units = n_basic_units;
	ret.tokens = tokens;
	ret.nodes = nodes;
	ret.heads = std::vector<std::vector<parse_node*> >(ret.nodes.size(), std::vector<parse_node*>());

	// for each terminal symbol, the head is itself.
	for (int i = 0; i < ret.nodes.size(	); ++i) {
		if (ret.nodes[i].is_basic_unit) {
			ret.heads[i].push_back(&ret.nodes[i]);
		}
	}

	// for each rule in bottom-up order, split the parent symbol for all the possible combination of its children.
	for (parse_rule r : rules) {
		int head_rhs = 0;
		if (r.rhs2 != -1) {
			std::string str_r = label_map[nodes[r.lhs].label_index] + "^" + label_map[nodes[r.rhs1].label_index] + "^" + label_map[nodes[r.rhs2].label_index];
			head_rhs = headrules_map[str_r];
		}
		for (int i = 0; i < ret.heads[r.rhs1].size(); ++i) {
			parse_node* rhs_head = ret.heads[r.rhs1][i];
			if (r.rhs2 == -1) {
				parse_rule new_rule = r;
				new_rule.rhs1_head = i;
				int idx = find_in_vector(ret.heads[r.lhs], rhs_head);
				if (idx == -1) {
					idx = ret.heads[r.lhs].size();
					ret.heads[r.lhs].push_back(rhs_head);
				}
				new_rule.lhs_head = idx;
				ret.rules.push_back(new_rule);
			} else {
				for (int j = 0; j < ret.heads[r.rhs2].size(); ++j) {
					if (head_rhs == 1) {
						rhs_head = ret.heads[r.rhs2][j];
					}
					parse_rule new_rule = r;
					new_rule.rhs1_head = i;
					new_rule.rhs2_head = j;
					int idx = find_in_vector(ret.heads[r.lhs], rhs_head);
					if (idx == -1) {
						idx = ret.heads[r.lhs].size();
						ret.heads[r.lhs].push_back(rhs_head);
					}
					new_rule.lhs_head = idx;
					ret.rules.push_back(new_rule);
				}
			}
		}
	}
	return ret;
}

void parse_forest::inside() {
	if (!is_headed) {
		std::cerr<< "Not headed forest.  -- parse_forest.inside()" << std::endl;
		return;
	}
	inside_scores = std::vector<std::vector<double> >(heads.size(), std::vector<double>());
	// the inside scores of the terminals are 0.
	for (int i = 0; i < heads.size(); ++i) {
		inside_scores[i] = std::vector<double>(heads[i].size(), INIT_SCORE);
		if (nodes[i].is_basic_unit) {
			inside_scores[i][0] = 0.0;
		}
	}
	// inside algorithm
	for (parse_rule& r : rules) {
		r.inside_score = r.log_prob + inside_scores[r.rhs1][r.rhs1_head];
		if (r.rhs2 != -1) {
			r.inside_score += inside_scores[r.rhs2][r.rhs2_head];
		}
		if (inside_scores[r.lhs][r.lhs_head] == INIT_SCORE) {
			inside_scores[r.lhs][r.lhs_head] = r.inside_score;
		} else {
			inside_scores[r.lhs][r.lhs_head] = std::log(std::exp(inside_scores[r.lhs][r.lhs_head]) + std::exp(r.inside_score));
		}
	}
}

void parse_forest::flow(){
	if (inside_scores.empty() && !rules.empty()) {
		std::cerr<< "parse_forest.inside() should be called first.  -- parse_forest.flow()" << std::endl;
		return;
	}
	flow_scores = std::vector<std::vector<double> >(heads.size(), std::vector<double>());
	// accumulate all the root inside scores.
    double all_score = 0;
	for (int i = 0; i < heads.size(); ++i) {
		flow_scores[i] = std::vector<double>(heads[i].size(), INIT_SCORE);
		if (nodes[i].is_upper && nodes[i].span_l == 0 && nodes[i].span_r == tokens.size()) {
            for (int j = 0; j < inside_scores[i].size(); ++j) {
                all_score += std::exp(inside_scores[i][j]);
            }
		}
	}
	// normalize the root inside scores for flow scores.
    all_score = std::log(all_score);
    for (int i = 0; i < heads.size(); ++i) {
		if (nodes[i].is_upper && nodes[i].span_l == 0 && nodes[i].span_r == tokens.size()) {
            for (int j = 0; j < inside_scores[i].size(); ++j) {
                flow_scores[i][j] = inside_scores[i][j] - all_score;
            }
        }
    }
    // flow algorithm
	for (int i = rules.size() - 1; i >= 0; --i) {
		parse_rule& r = rules[i];
		r.flow_score = r.inside_score - inside_scores[r.lhs][r.lhs_head] + flow_scores[r.lhs][r.lhs_head];

//		if (debug) {
//			std::cout << r.flow_score << std::endl;
//			parse_node n = nodes[r.lhs];
//			std::cout << "r inside:" << r.inside_score << std::endl;
//			std::cout << "lhs inside:" << inside_scores[r.lhs][r.lhs_head] << std::endl;
//			std::cout << "lhs flow:" << flow_scores[r.lhs][r.lhs_head] << std::endl;
//			std::cout << "<" << n.span_l << " " << n.span_r << ">: " << r.lhs << std::endl;
//		}

		if (flow_scores[r.rhs1][r.rhs1_head] == INIT_SCORE) {
			flow_scores[r.rhs1][r.rhs1_head] = r.flow_score;
		} else {
            double tmp = flow_scores[r.rhs1][r.rhs1_head];
			flow_scores[r.rhs1][r.rhs1_head] = std::log(std::exp(flow_scores[r.rhs1][r.rhs1_head]) + std::exp(r.flow_score));

//			if (debug) {
//				if (flow_scores[r.rhs1][r.rhs1_head] > EPS) {
//					std::cout << flow_scores[r.rhs1][r.rhs1_head] << std::endl;
//					parse_node n = nodes[r.lhs];
//					std::cout << "r flow:" << r.flow_score << std::endl;
//					std::cout << "rhs1 flow:" << tmp << std::endl;
//					std::cout << "<" << n.span_l << " " << n.span_r << ">" << std::endl;
//					exit(-1);
//				}
//			}
		}
		if (r.rhs2 != -1) {
			if (flow_scores[r.rhs2][r.rhs2_head] == INIT_SCORE) {
				flow_scores[r.rhs2][r.rhs2_head] = r.flow_score;
			} else {
                double tmp = flow_scores[r.rhs2][r.rhs2_head];
				flow_scores[r.rhs2][r.rhs2_head] = std::log(std::exp(flow_scores[r.rhs2][r.rhs2_head]) + std::exp(r.flow_score));

//				if(debug) {
//					if (flow_scores[r.rhs2][r.rhs2_head] > EPS) {
//						std::cout << flow_scores[r.rhs2][r.rhs2_head] << std::endl;
//						parse_node n = nodes[r.lhs];
//						std::cout << "r flow:" << r.flow_score << std::endl;
//						std::cout << "rhs2 flow:" << tmp << std::endl;
//						std::cout << "<" << n.span_l << " " << n.span_r << ">" << std::endl;
//						exit(-1);
//					}
//				}
			}
		}
	}

	if (debug) {
		std::cout << "flow scores:" << std::endl;
		for (int i = 0; i < flow_scores.size(); ++i) {
			auto v = flow_scores[i];
			std::cout << "<" << nodes[i].span_l << ", " << nodes[i].span_r << ">: " << std::endl;
			for (auto t : v) {
				std::cout << t <<std::endl;
			}
		}
	}
}

void add_governors(expected_governor& eg1, const expected_governor& eg2) {
	for (auto g : eg2) {
		if (eg1.find(g.first) != eg1.end()) {
			eg1[g.first] += g.second;
		} else {
			eg1[g.first] = g.second;
		}
	}
}
void add_one_governor(expected_governor & eg, const parse_rule& r, int node, int node_head) {
	governor g(node, node_head, r.lhs, r.lhs_head);
	if (eg.find(g) != eg.end()) {
		eg[g] += std::exp(r.flow_score);
	} else {
		eg[g] = std::exp(r.flow_score);
	}
}

void parse_forest::find_expected_governor(){
	if (flow_scores.empty() && !rules.empty()) {
		std::cerr<< "parse_forest.flow() should be called first.  -- parse_forest.find_expected_governor()" << std::endl;
		return;
	}
	e_governors = std::vector<std::vector<expected_governor> >(heads.size(), std::vector<expected_governor>());
	// initialize the expected governor of the roots
	for (int i = 0; i < heads.size(); ++i) {
		e_governors[i] = std::vector<expected_governor>(heads[i].size(), expected_governor());
		if (nodes[i].is_upper && nodes[i].span_l == 0 && nodes[i].span_r == tokens.size()) {
			for (int j = 0; j < heads[i].size(); ++j) {
				governor root(i, j);
				e_governors[i][j][root] = std::exp(flow_scores[i][j]);
			}
		}
	}
	// for each rule in top-down order, pass down the expected governor
	for (int i = rules.size() - 1; i >= 0; --i) {
		parse_rule& r = rules[i];
		expected_governor new_e_governor;
		double frac = std::exp(r.inside_score - inside_scores[r.lhs][r.lhs_head]);
		for (std::pair<governor, double> entry : e_governors[r.lhs][r.lhs_head]) {
			new_e_governor[entry.first] = entry.second * frac;
		}
		r.e_governor = new_e_governor;
		if (heads[r.lhs][r.lhs_head] == heads[r.rhs1][r.rhs1_head]) {
			add_governors(e_governors[r.rhs1][r.rhs1_head], r.e_governor);
		} else {
			add_one_governor(e_governors[r.rhs1][r.rhs1_head], r, r.rhs1, r.rhs1_head);
		}
		if (r.rhs2 != -1) {
			if (heads[r.lhs][r.lhs_head] == heads[r.rhs2][r.rhs2_head]) {
				add_governors(e_governors[r.rhs2][r.rhs2_head], r.e_governor);
			} else {
				add_one_governor(e_governors[r.rhs2][r.rhs2_head], r, r.rhs2, r.rhs2_head);
			}
		}
	}
}

}
