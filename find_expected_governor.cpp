/*
 * expected_governor_ytdu.cpp
 *
 *  Created on: May 12, 2016
 *      Author: ytdu
 */

#include<iostream>
#include<fstream>

#include "expected_governor.h"


std::string input_path = "data/test.predict";
std::string rule_path = "data/tcrf_rule";
std::string binary_headrule_path = "data/binary_headrules";
std::string output_path = "";
bool debug = false;

/* From file rule_path reads symbols.
 */
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

/* From file binary_headrule_path reads headrules.
 */
void read_headrules() {
	std::ifstream headrules_in(binary_headrule_path);
	std::string labels;
	int num_rules, head, x;
	headrules_in >> num_rules;
	for (int i = 0; i < num_rules; ++i) {
		headrules_in >> labels >> head >> x;
		nlu::headrules_map[labels] = head;
	}
	headrules_in.close();
}

/* Prints expected governors.
 * Also prints tokens if in debug mode.
 */
void print_expected_governors(std::ostream& out, const nlu::parse_forest& f, int n) {
	const nlu::parse_node& node = f.nodes[n];
	const nlu::expected_governor& eg = f.e_governors[n][0];
	if (debug) {
		for (int i = node.span_l; i < node.span_r; ++i) {
			out << f.tokens[i];
		}
		out << " ";
	}
	out << node.span_l << " " << node.span_r << " " << nlu::label_map[node.label_index] << " " << eg.size();
	out << std::endl;
	for (auto e: eg) {
		out << nlu::label_map[f.nodes[e.first.u].label_index] << " ";
		if (e.first.u_prime == -1) {
			out << START_SYMBOL << " " << START_SYMBOL;
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

void parse_options(int argc, char* argv[]) {
	for (int i = 1; i < argc; ++i) {
		std::string argi = std::string(argv[i]);
		if (argi == "-rule_path") {
			rule_path = argv[++i];
		} else if (argi == "-head_rule_path") {
			binary_headrule_path = argv[++i];
		} else if (argi == "-input") {
			input_path = argv[++i];
		} else if (argi == "-output") {
			output_path = argv[++i];
		} else if (argi == "-debug") {
			debug = true;
		} else {
			if (argi != "-h" && argi != "--help") {
				std::cout << "Unknown option: " << argi << "." << std::endl;
			}
			std::cout << "Usage:[\n-input <input path (Default=test.predict)>\n -output <output path (Default=stdout)>\n";
			std::cout << " -rule_path <binary rule path (Default=data/tcrf_rule)>\n -head_rule_path<binary headrule path (Default=data/binary_headrules)>\n -debug\n]"<< std::endl;
			exit(-1);
		}
	}
}

int main(int argc, char* argv[]) {
	parse_options(argc, argv);
	read_labels();
	read_headrules();

    // input file and output file (Default output is stdout)
	std::ifstream pred_in(input_path);
    std::ostream *pred_out = &std::cout;
    std::ofstream *f_out = NULL;
    if (output_path != "" && output_path != "stdout") {
    	f_out = new std::ofstream(output_path);
        pred_out = f_out;
    }

	nlu::parse_forest f;
	while (pred_in.peek() != EOF) {
		pred_in >> f;
		nlu::parse_forest headed_f = f.to_headed();
		headed_f.inside();
		headed_f.flow();
		headed_f.find_expected_governor();
		*pred_out << headed_f.n_basic_units <<std::endl;
		for (int i = 0; i < headed_f.nodes.size(); ++i) {
			if (headed_f.nodes[i].is_basic_unit) {
				print_expected_governors(*pred_out, headed_f, i);
			}
		}
	}
	std::cout<<std::endl;
	pred_in.close();
	if (f_out != NULL) {
		f_out->close();
	}
	return 0;
}
