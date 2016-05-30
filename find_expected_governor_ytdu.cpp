/*
 * expected_governor_ytdu.cpp
 *
 *  Created on: May 12, 2016
 *      Author: ytdu
 */

#include<iostream>
#include<fstream>

#include "expected_governor_ytdu.h"

#define tcrf_prediction_path "data/test.predict"
#define rule_path "data/tcrf_rule"
#define binary_headrules_path "data/binary_headrules"
#define output_path "data/tcrf_expected_governor"

void read_labels() {
	std::ifstream rules_in(rule_path);
	int num_labels, x;
	rules_in >> num_labels >> x >> x;
	std::string l, s;
	for (int i = 0; i < num_labels; ++i) {
		rules_in >> s >> s >> l >> s >> s;
		nlu::label_map.push_back(l);
	}
	rules_in.close();
}

void read_headrules() {
	std::ifstream headrules_in(binary_headrules_path);
	std::string labels;
	int num_rules, head, x;
	headrules_in >> num_rules;
	for (int i = 0; i < num_rules; ++i) {
		headrules_in >> labels >> head >> x;
		nlu::headrules_map[labels] = head;
	}
	headrules_in.close();
}

void print_expected_governors(std::ostream& out, const nlu::parse_forest& f, int n) {
	const nlu::parse_node& node = f.nodes[n];
	const nlu::expected_governor& eg = f.e_governors[n][0];
	for (int i = node.span_l; i < node.span_r; ++i) {
		out << f.tokens[i];
	}
	out << " " << node.span_l << " " << node.span_r << " " << nlu::label_map[node.label_index] << " " << eg.size();
	out << std::endl;
	for (auto e: eg) {
		out << nlu::label_map[f.nodes[e.first.u].label_index] << " ";
		if (e.first.u_prime == -1) {
			out << "#START# #START#";
		} else {
			out << nlu::label_map[f.nodes[e.first.u_prime].label_index] << " ";
			int span_l = f.heads[e.first.u_prime][e.first.u_prime_head]->span_l;
			int span_r = f.heads[e.first.u_prime][e.first.u_prime_head]->span_r;
			for (int i = span_l; i < span_r; ++i) {
				out << f.tokens[i];
			}
		}
		out << " " << e.second << std::endl;
	}
}

int main() {
	read_labels();
	read_headrules();

	std::ifstream pred_in(tcrf_prediction_path);
	nlu::parse_forest f;
	while (pred_in.peek() != EOF) {
		pred_in >> f;
		nlu::parse_forest headed_f = f.to_headed();
//		std::cout<< headed_f;
		headed_f.inside();
		headed_f.flow();
		headed_f.find_expected_governor();
		std::cout << headed_f.n_basic_units <<std::endl;
		for (int i = 0; i < headed_f.nodes.size(); ++i) {
			if (headed_f.nodes[i].is_basic_unit) {
				print_expected_governors(std::cout, headed_f, i);
			}
		}
	}
	std::cout<<std::endl;
	pred_in.close();
	return 0;
}
