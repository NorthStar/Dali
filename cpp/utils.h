#ifndef RECURRENT_MAT_UTILS_H
#define RECURRENT_MAT_UTILS_H

#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <sys/stat.h>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
// Default writing mode useful for default argument to
// makedirs
#define DEFAULT_MODE S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH

std::ostream& operator<<(std::ostream&, const std::vector<std::string>&);
std::ostream& operator<<(std::ostream&, const std::unordered_map<std::string, uint>&);

template<typename T>
std::ostream& operator<<(std::ostream&, const std::vector<T>&);

namespace utils {
	/** Utility function to create directory tree */
	bool makedirs(const char* path, mode_t mode = DEFAULT_MODE);

	extern const char* end_symbol;
	extern const char* unknown_word_symbol;

	class Vocab {
		typedef uint ind_t;
		private:
			void construct_word2index();
			void add_unknown_word();
		public:
			ind_t unknown_word;
			std::unordered_map<std::string, ind_t> word2index;
			std::vector<std::string> index2word;
			Vocab();
			Vocab(std::vector<std::string>&);
			Vocab(std::vector<std::string>&, bool);
	};

	std::string& trim(std::string&);
	std::string& ltrim(std::string&);
	std::string& rtrim(std::string&);

	void map_to_file(const std::unordered_map<std::string, std::vector<std::string>>&, const std::string&);

	void ensure_directory(std::string&);

	std::vector<std::string> split_str(const std::string&, const std::string&);

	std::unordered_map<std::string, std::vector<std::string>> text_to_map(const std::string&);

	int randint(int, int);

	bool is_gzip(const std::string&);
	template<typename T>
	struct sigmoid_operator {
		T operator() (T) const;
	};
	template<typename T>
	struct tanh_operator {
		T operator() (T) const;
	};
	template<typename T>
	struct relu_operator {
		T operator() (T) const;
	};
	template<typename T>
	struct sign_operator {
		T operator() (T) const;
	};
	template<typename T>
	struct dtanh_operator {
		T operator() (T) const;
	};

	template<typename T>
    T from_string(const std::string&);

    template<typename T>
    void assert_map_has_key(std::unordered_map<std::string, T>&, const std::string&);

    std::vector<std::string> split(const std::string &, char);

	template <typename T>
	std::vector<size_t> argsort(const std::vector<T> &);

	template<typename T>
	void assign_cli_argument(char *, T&, T, std::string);

	template<typename T>
	void assign_cli_argument(char *, T&, std::string);

	template <class T> inline void hash_combine(std::size_t &, const T &);
	std::size_t get_random_id();
	namespace ops {
		static const uint add                             = 0;
		static const uint eltmul                          = 1;
		static const uint sigmoid                         = 2;
		static const uint tanh                            = 3;
		static const uint mul                             = 4;
		static const uint relu                            = 5;
		static const uint row_pluck                       = 6;
		static const uint add_broadcast                   = 7;
		static const uint eltmul_broadcast                = 8;
		static const uint mul_with_bias                   = 9;
		static const uint mul_add_mul_with_bias           = 10;
		static const uint mul_add_broadcast_mul_with_bias = 11;
		static const uint rows_pluck                      = 12;
		static const uint transpose                       = 13;
		static const uint eltmul_broadcast_rowwise        = 14;
		static const uint eltmul_rowwise                  = 15;
	}
}
#endif