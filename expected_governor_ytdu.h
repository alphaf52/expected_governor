/*
 * expected_governor_ytdu.h
 *
 *  Created on: May 12, 2016
 *      Author: ytdu
 */

#ifndef EXPECTED_GOVERNOR_YTDU_H_
#define EXPECTED_GOVERNOR_YTDU_H_

#include<vector>
#include<unordered_map>
#include<string>

#define EPS 1e-9
#define INIT_SCORE 1.0
namespace nlu {

extern std::vector<std::string> label_map;
extern std::unordered_map<std::string, int> headrules_map;

class governor {
public:
	int u;
	int u_head;
	int u_prime = -1;
	int u_prime_head = -1;
	governor(int u, int u_head) : u(u), u_head(u_head) {}
	governor(int u, int u_head, int u_prime, int u_prime_head) : u(u), u_head(u_head), u_prime(u_prime), u_prime_head(u_prime_head) {}

	bool operator==(const governor &other) const {
		return u == other.u && u_head == other.u_head && u_prime == other.u_prime && u_prime_head == other.u_prime_head;
	}
};

class governor_hash{
public:
    size_t operator()(const governor &g) const
    {
        size_t h1 = std::hash<int>()(g.u);
        size_t h2 = std::hash<int>()(g.u_head);
        size_t h3 = std::hash<int>()(g.u_prime);
        size_t h4 = std::hash<int>()(g.u_prime_head);
        return h1 ^ ( h2 << 1 ) ^ (h3 << 2) ^ (h4 << 3);
    }
};

typedef std::unordered_map<governor, double, governor_hash> expected_governor;
//class expected_governor{
//	std::unordered_map<governor, double, governor_hash> governors;
//};

class parse_node {
public:
	int index = -1;
	int span_l = -1;
	int span_r = -1;
	int label_index = -1;
	bool is_upper = 0;
	bool is_basic_unit = 0;
};
std::istream& operator>>(std::istream& in, parse_node& node);
std::ostream& operator<<(std::ostream& out, const parse_node& node);

class parse_rule {
public:
	int lhs = -1, lhs_head = -1;
	int rhs1 = -1, rhs1_head = -1;
	int rhs2 = -1, rhs2_head = -1;
	double log_prob = 0.0;
	double inside_score = 0.0;
	double flow_score = 0.0;
	expected_governor e_governor;
};
std::istream& operator>>(std::istream& in, parse_rule& node);
std::ostream& operator<<(std::ostream& out, const parse_rule& rule);

class parse_forest {
public:
	bool is_headed = false;
	int n_basic_units;
	std::vector<std::string> tokens;
	std::vector<parse_node> nodes;
	std::vector<parse_rule> rules;
	std::vector<std::vector<parse_node*> > heads;
	std::vector<std::vector<double> > inside_scores;
	std::vector<std::vector<double> > flow_scores;
	std::vector<std::vector<expected_governor> > e_governors;
	void clear();
	parse_forest to_headed();
	void inside();
	void flow();
	void find_expected_governor();
};
std::istream& operator>>(std::istream& in, parse_forest& forest);
std::ostream& operator<<(std::ostream& out, const parse_forest& forest);

}

#endif /* EXPECTED_GOVERNOR_YTDU_H_ */
