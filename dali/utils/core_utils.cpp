#include "core_utils.h"

#include <algorithm>
#include <dirent.h>
#include <iomanip>
#include <iterator>
#include <set>
#include <sys/stat.h>
#include <pwd.h>

#include "dali/utils/assert2.h"
#include "dali/utils/ThreadPool.h"
#include "dali/utils/vocab.h"
#include "dali/utils/print_utils.h"
#include "dali/utils/gzstream.h"

using std::vector;
using std::string;
using std::ifstream;
using std::stringstream;
using std::ofstream;
using std::set;
using std::make_shared;
using std::function;
using std::initializer_list;


namespace utils {
    const mode_t DEFAULT_MODE = S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH;

    #ifndef NDEBUG
    string explain_mat_bug(const string& mat_name, const char* file, const int& line) {
        stringstream ss;
        ss << "Matrix \"" << mat_name << "\" has NaNs in file:\"" << file << "\" and line: " << line;
        return ss.str();
    }
    template<typename T>
    bool contains_NaN(T val) {return !(val == val);}

    template bool contains_NaN(float);
    template bool contains_NaN(double);
    #endif

    vector<int> arange(int start, int end) {
        vector<int> indices(end - start);
        for (int i=0; i < indices.size();i++) indices[i] = i;
        return indices;
    }

    void ensure_directory (std::string& dirname) {
        if (dirname.back() != '/') dirname += "/";
    }

    vector<string> split(const std::string &s, char delim, bool keep_empty_strings) {
        std::vector<std::string> elems;
        std::stringstream ss(s);
        string item;
        while (std::getline(ss, item, delim))
            if (!item.empty() || keep_empty_strings)
                elems.push_back(item);
        return elems;
    }

    string join(const vector<string>& vs, const string& in_between) {
        std::stringstream ss;
        for (int i = 0; i < vs.size(); ++i) {
            ss << vs[i];
            if (i + 1 != vs.size())
                ss << in_between;
        }
        return ss.str();
    }


    template<typename T>
    bool add_to_set(vector<T>& set, T& el) {
        if(std::find(set.begin(), set.end(), el) != set.end()) {
                return false;
        } else {
                set.emplace_back(el);
                return true;
        }
    }

    template bool add_to_set(vector<int>&,    int&);
    template bool add_to_set(vector<uint>&,   uint&);
    template bool add_to_set(vector<string>&, string&);


    template<typename T>
    bool in_vector(const std::vector<T>& set, const T& el) {
        return std::find(set.begin(), set.end(), el) != set.end();
    }

    template bool in_vector(const vector<int>&,    const int&);
    template bool in_vector(const vector<uint>&,   const uint&);
    template bool in_vector(const vector<string>&, const string&);
    template bool in_vector(const vector<char>&, const char&);

    template<typename T>
    void tuple_sum(std::tuple<T, T>& A, std::tuple<T, T> B) {
            std::get<0>(A) += std::get<0>(B);
            std::get<1>(A) += std::get<1>(B);
    }

    template void tuple_sum(std::tuple<float, float>&, std::tuple<float, float>);
    template void tuple_sum(std::tuple<double, double>&, std::tuple<double, double>);
    template void tuple_sum(std::tuple<int, int>&, std::tuple<int, int>);
    template void tuple_sum(std::tuple<uint, uint>&, std::tuple<uint, uint>);

    template<typename T>
    void assert_map_has_key(std::unordered_map<string, T>& map, const string& key) {
            if (map.count(key) < 1) {
                    stringstream error_msg;
                    error_msg << "Map is missing the following key : \"" << key << "\".";
                    throw std::runtime_error(error_msg.str());
            }
    }

    template void assert_map_has_key(std::unordered_map<string, string>&, const string&);
    template void assert_map_has_key(std::unordered_map<string, vector<string>>&, const string&);

    vector<string> listdir(const string& folder) {
            vector<string> filenames;
            DIR *dp;
        struct dirent *dirp;
        if ((dp  = opendir(folder.c_str())) == nullptr) {
            stringstream error_msg;
                    error_msg << "Error: could not open directory \"" << folder << "\"";
                    throw std::runtime_error(error_msg.str());
        }
        // list all contents of directory:
        while ((dirp = readdir(dp)) != nullptr)
            // exclude "current directory" (.) and "parent directory" (..)
            if (std::strcmp(dirp->d_name, ".") != 0 && std::strcmp(dirp->d_name, "..") != 0)
                    filenames.emplace_back(dirp->d_name);
        closedir(dp);
        return filenames;
    }

    vector<string> split_str(const string& original, const string& delimiter) {
            std::vector<std::string> tokens;
            auto delimiter_ptr = delimiter.begin();
            stringstream ss(original);
            int inside = 0;
            std::vector<char> token;
            char ch;
            while (ss) {
                ch = ss.get();
                if (ch == *delimiter_ptr) {
                    delimiter_ptr++;
                    inside++;
                    if (delimiter_ptr == delimiter.end()) {
                        tokens.emplace_back(token.begin(), token.end());
                        token.clear();
                        inside = 0;
                        delimiter_ptr = delimiter.begin();
                    }
                } else {
                    if (inside > 0) {
                        token.insert(token.end(), delimiter.begin(), delimiter_ptr);
                        delimiter_ptr = delimiter.begin();
                        inside = 0;
                    }
                    token.push_back(ch);
                }
            }
            if (inside > 0) {
                    token.insert(token.end(), delimiter.begin(), delimiter_ptr);
                    tokens.emplace_back(token.begin(), token.end());
                    token.clear();
            } else {
                    if (token.size() > 0)
                            tokens.emplace_back(token.begin(), token.end()-1);
            }
            return tokens;
    }
    std::unordered_map<string, std::vector<string>> text_to_map(const string& fname) {
            ifstream infile(fname);
            string line;
            const char space = ' ';
            std::unordered_map<string, std::vector<string>> map;
            while (std::getline(infile, line)) {
                    if (*line.begin() != '=' && *line.begin() != '-' && *line.begin() != '#') {
                            const auto tokens = utils::split(line, space);
                            if (tokens.size() > 1) {
                                    auto ptr = tokens.begin() + 1;
                                    while( ptr != tokens.end()) {
                                            map[tokens[0]].emplace_back(*(ptr++));
                                    }
                            }
                    }
            }
            return map;
    }

    template<typename T, typename K>
    void stream_to_hashmap(T& infile, std::unordered_map<string, K>& map) {
            string line;
            const char space = ' ';
            while (std::getline(infile, line)) {
                    const auto tokens = utils::split(line, space);
                    if (tokens.size() > 1)
                            map[tokens[0]] = from_string<K>(tokens[1]);
            }
    }

    template<typename T>
    std::unordered_map<string, T> text_to_hashmap(const string& fname) {
            std::unordered_map<string, T> map;
            if (is_gzip(fname)) {
                    igzstream fpgz(fname.c_str(), std::ios::in | std::ios::binary);
                    stream_to_hashmap(fpgz, map);
            } else {
                    std::fstream fp(fname, std::ios::in | std::ios::binary);
                    stream_to_hashmap(fp, map);
            }
            return map;
    }

    template<typename T>
    void stream_to_list(T& fp, vector<string>& list) {
            string line;
            while (std::getline(fp, line))
                    list.emplace_back(line);
    }
    template void stream_to_list(stringstream&, vector<string>&);

    vector<string> load_list(const string& fname) {
        vector<string> list;
        if (is_gzip(fname)) {
            igzstream fpgz(fname.c_str(), std::ios::in | std::ios::binary);
            stream_to_list(fpgz, list);
        } else {
            std::fstream fp(fname, std::ios::in | std::ios::binary);
            stream_to_list(fp, list);
        }
        return list;
    }

    template<typename T>
    void save_list_to_stream(const vector<string>& list, T& fp) {
        for (auto& el : list) {
            fp << el << "\n";
        }
    }

    void save_list(const vector<string>& list, string fname, std::ios_base::openmode mode) {
        if (endswith(fname, ".gz")) {
            ogzstream fpgz(fname.c_str(), mode);
            save_list_to_stream(list, fpgz);
        } else {
            ofstream fp(fname, mode);
            save_list_to_stream(list, fp);
        }
    }

    template<typename T>
    void stream_to_redirection_list(T& fp, std::unordered_map<string, string>& mapping, std::function<std::string(std::string&&)>& preprocessor, int num_threads) {
        string line;
        const char dash = '-';
        const char arrow = '>';
        bool saw_dash = false;
        auto checker = [&saw_dash, &arrow, &dash](const char& ch) {
            if (saw_dash) {
                if (ch == arrow) {
                    return true;
                } else {
                    saw_dash = (ch == dash);
                    return false;
                }
            } else {
                saw_dash = (ch == dash);
                return false;
            }
        };
        if (num_threads > 1) {
            ThreadPool pool(num_threads);
            while (std::getline(fp, line)) {
                pool.run([&mapping, &preprocessor, &checker, line]() {
                    auto pos_end_arrow = std::find_if(line.begin(), line.end(), checker);
                    if (pos_end_arrow != line.end()) {
                        mapping.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(
                                preprocessor(
                                    std::string(
                                        line.begin(),
                                        pos_end_arrow-1
                                    )
                                )
                            ),
                            std::forward_as_tuple(
                                preprocessor(
                                    std::string(
                                        pos_end_arrow+1,
                                        line.end()
                                    )
                                )
                            )
                        );
                    }
                });
            }
        } else {
            while (std::getline(fp, line)) {
                auto pos_end_arrow = std::find_if(line.begin(), line.end(), checker);
                if (pos_end_arrow != line.end()) {
                    mapping.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple( preprocessor(std::string(line.begin(), pos_end_arrow-1))),
                        std::forward_as_tuple( preprocessor(std::string(pos_end_arrow+1, line.end())))
                    );
                }
            }
        }
    }

    template<typename T>
    void stream_to_redirection_list(T& fp, std::unordered_map<string, string>& mapping) {
        string line;
        const char dash = '-';
        const char arrow = '>';
        bool saw_dash = false;
        auto checker = [&saw_dash, &arrow, &dash](const char& ch) {
            if (saw_dash) {
                if (ch == arrow) {
                    return true;
                } else {
                    saw_dash = (ch == dash);
                    return false;
                }
            } else {
                saw_dash = (ch == dash);
                return false;
            }
        };
        while (std::getline(fp, line)) {
            auto pos_end_arrow = std::find_if(line.begin(), line.end(), checker);
            if (pos_end_arrow != line.end()) {
                mapping.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple( line.begin(), pos_end_arrow-1),
                    std::forward_as_tuple( pos_end_arrow+1, line.end())
                );
            }
        }
    }

    template void stream_to_redirection_list(stringstream&, std::unordered_map<string, string>&, std::function<std::string(std::string&&)>&, int);
    template void stream_to_redirection_list(stringstream&, std::unordered_map<string, string>&);

    std::unordered_map<string, string> load_redirection_list(const string& fname, std::function<std::string(std::string&&)>&& preprocessor, int num_threads) {
        std::unordered_map<string, string> mapping;
        if (is_gzip(fname)) {
            igzstream fpgz(fname.c_str(), std::ios::in | std::ios::binary);
            stream_to_redirection_list(fpgz, mapping, preprocessor, num_threads);
        } else {
            std::fstream fp(fname, std::ios::in | std::ios::binary);
            stream_to_redirection_list(fp, mapping, preprocessor, num_threads);
        }
        return mapping;
    }

    std::unordered_map<string, string> load_redirection_list(const string& fname) {
        std::unordered_map<string, string> mapping;
        if (is_gzip(fname)) {
            igzstream fpgz(fname.c_str(), std::ios::in | std::ios::binary);
            stream_to_redirection_list(fpgz, mapping);
        } else {
            std::fstream fp(fname, std::ios::in | std::ios::binary);
            stream_to_redirection_list(fp, mapping);
        }
        return mapping;
    }

    template std::unordered_map<string, string> text_to_hashmap(const string&);
    template std::unordered_map<string, int>    text_to_hashmap(const string&);
    template std::unordered_map<string, float>  text_to_hashmap(const string&);
    template std::unordered_map<string, double> text_to_hashmap(const string&);
    template std::unordered_map<string, uint>   text_to_hashmap(const string&);

    void map_to_file(const std::unordered_map<string, std::vector<string>>& map, const string& fname) {
        ofstream fp;
        fp.open(fname.c_str(), std::ios::out);
        for (auto& kv : map) {
            fp << kv.first;
            for (auto& v: kv.second)
                fp << " " << v;
            fp << "\n";
        }
    }

    vector<std::pair<string, string>> load_labeled_corpus(const string& fname) {
        ifstream fp(fname.c_str());
        string l;
        const char space = ' ';
        vector<std::pair<string, string>> pairs;
        string::size_type n;
        while (std::getline(fp, l)) {
            n = l.find(space);
            pairs.emplace_back(std::piecewise_construct,
                std::forward_as_tuple(l.begin() + n + 1, l.end()),
                std::forward_as_tuple(l.begin(), l.begin() + n)
            );
        }
        return pairs;
    }



    vector<string> tokenize(const string& s) {
        stringstream ss(s);
        std::istream_iterator<string> begin(ss);
        std::istream_iterator<string> end;
        return vector<string>(begin, end);
    }

    vector<vector<string>> load_tokenized_unlabeled_corpus(const string& fname) {
        ifstream fp(fname.c_str());
        string l;
        vector<vector<string>> list;
        while (std::getline(fp, l)) {
            auto tokenized = tokenize(string(l.begin(), l.end()));
            if (tokenized.size() > 0 ) {
                list.emplace_back(tokenized);
            }
        }
        return list;
    }

    vector<string> get_vocabulary(const tokenized_labeled_dataset& examples, int min_occurence, int data_column) {
        std::unordered_map<string, uint> word_occurences;
        string word;
        for (auto& example : examples)
            for (auto& word : example[data_column]) word_occurences[word] += 1;
        vector<string> list;
        for (auto& key_val : word_occurences)
            if (key_val.second >= min_occurence)
                list.emplace_back(key_val.first);
        list.emplace_back(utils::end_symbol);
        return list;
    }

    vector<string> get_vocabulary(const vector<vector<string>>& examples, int min_occurence) {
        std::unordered_map<string, uint> word_occurences;
        string word;
        for (auto& example : examples) {
            for (auto& word : example) {
                word_occurences[word] += 1;
            }
        }
        vector<string> list;
        for (auto& key_val : word_occurences) {
            if (key_val.second >= min_occurence) {
                list.emplace_back(key_val.first);
            }
        }
        list.emplace_back(utils::end_symbol);
        return list;
    }

    vector<string> get_vocabulary(const tokenized_uint_labeled_dataset& examples, int min_occurence) {
        std::unordered_map<string, uint> word_occurences;
        string word;
        for (auto& example : examples) {
            for (auto& word : example.first) {
                word_occurences[word] += 1;
            }
        }
        vector<string> list;
        for (auto& key_val : word_occurences) {
            if (key_val.second >= min_occurence) {
                list.emplace_back(key_val.first);
            }
        }
        list.emplace_back(utils::end_symbol);
        return list;
    }

    vector<string> get_label_vocabulary(const tokenized_labeled_dataset& examples) {
        std::set<string> labels;
        string word;
        for (auto& example : examples) {
            ASSERT2(example.size() > 1, "Examples must have at least 2 columns.");
            labels.insert(example[1].begin(), example[1].end());
        }
        return vector<string>(labels.begin(), labels.end());
    }

    // Trimming text from StackOverflow:
    // http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring

    // trim from start
    std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
    }

    // trim from end
    std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
    }

    // trim from both ends
    std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
    }

    // From this StackOverflow:
    // http://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
    bool makedirs(const char* path, mode_t mode) {
        // const cast for hack
        char* p = const_cast<char*>(path);

        // Do mkdir for each slash until end of string or error
        while (*p != '\0') {
            // Skip first character
            p++;

            // Find first slash or end
            while(*p != '\0' && *p != '/') p++;

            // Remember value from p
            char v = *p;

            // Write end of string at p
            *p = '\0';

            // Create folder from path to '\0' inserted at p
            if(mkdir(path, mode) == -1 && errno != EEXIST) {
                *p = v;
                return false;
            }

            // Restore path to it's former glory
            *p = v;
        }
        return true;
    }

    // From this StackOverflow:
    // http://stackoverflow.com/questions/3020187/getting-home-directory-in-mac-os-x-using-c-language
    std::string expanduser(const std::string& path) {
        if (path.size() == 0 || path[0] != '~') {
            return path;
        }
        // on windows a different environment variable
        // controls the home directory, but mac and linux
        // use HOME
        const char *homeDir = getenv("HOME");
        bool got_home_dir = false;
        if (!homeDir) {
            struct passwd* pwd = getpwuid(getuid());
            if (pwd) {
               homeDir = pwd->pw_dir;
               got_home_dir = true;
            }
        } else {
            got_home_dir = true;
        }
        if (got_home_dir) {
            return std::string(homeDir) + path.substr(1);
        } else {
            // path could not be expanded
            return path;
        }
    }

    template<typename T>
    T from_string(const std::string& s) {
        std::istringstream stream (s);
        T t;
        stream >> t;
        return t;
    }

    bool is_number(const std::string& s) {
        bool is_negative = *s.begin() == '-';
        bool is_decimal  = false;
        if (is_negative && s.size() == 1) {
            return false;
        }
        return !s.empty() && std::find_if(s.begin() + (is_negative ? 1 : 0),
            s.end(), [&is_decimal](char c) {
                if (is_decimal) {
                    if (c == '.') {
                        return true;
                    } else {
                        return !std::isdigit(c);
                    }
                } else {
                    if (c == '.') {
                        is_decimal = true;
                        return false;
                    } else {
                        return !std::isdigit(c);
                    }
                }
            }) == s.end();
    }

    template float from_string<float>(const std::string& s);
    template int from_string<int>(const std::string& s);
    template uint from_string<uint>(const std::string& s);
    template double from_string<double>(const std::string& s);
    template long from_string<long>(const std::string& s);

    bool is_gzip(const std::string& fname) {
        const unsigned char gzip_code = 0x1f;
        const unsigned char gzip_code2 = 0x8b;
        unsigned char ch;
        std::ifstream file;
        file.open(fname);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&ch), 1);
        if (ch != gzip_code)
                return false;
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&ch), 1);
        if (ch != gzip_code2)
                return false;
        return true;
    }

    template <typename T>
    vector<size_t> argsort(const vector<T> &v) {
            // initialize original index locations
            vector<size_t> idx(v.size());
            for (size_t i = 0; i != idx.size(); ++i) idx[i] = i;

            // sort indexes based on comparing values in v
            sort(idx.begin(), idx.end(),
               [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

            return idx;
    }
    template vector<size_t> argsort(const vector<size_t>&v);
    template vector<size_t> argsort(const vector<float>&v);
    template vector<size_t> argsort(const vector<double>&v);
    template vector<size_t> argsort(const vector<int>&v);
    template vector<size_t> argsort(const vector<uint>&v);
    template vector<size_t> argsort(const vector<std::string>&v);


    void exit_with_message(const std::string& message, int error_code) {
            std::cerr << message << std::endl;
            exit(error_code);
    }

    bool endswith(std::string const & full, std::string const & ending) {
        if (full.length() >= ending.length()) {
            return (0 == full.compare(full.length() - ending.length(), ending.length(), ending));
        } else {
            return false;
        }
    }

    bool startswith(std::string const & full, std::string const & beginning) {
        if (full.length() >= beginning.length()) {
            return (0 == full.compare(0, beginning.length(), beginning));
        } else {
            return false;
        }
    }

    bool file_exists (const std::string& fname) {
        struct stat buffer;
        return (stat (fname.c_str(), &buffer) == 0);
    }

    std::string dir_parent(const std::string& path, int levels_up) {
        auto file_path_split = split(path, '/');
        assert(levels_up < file_path_split.size());
        stringstream ss;
        if (path[0] == '/')
            ss << '/';
        for (int i = 0; i < file_path_split.size() - levels_up; ++i) {
            ss << file_path_split[i];
            if (i + 1 != file_path_split.size() - levels_up)
                ss << '/';
        }
        return ss.str();
    }

    std::string dir_join(const vector<std::string>& paths) {
        stringstream ss;
        for (int i = 0; i < paths.size(); ++i) {
            ss << paths[i];
            bool found_slash_symbol = false;
            if (paths[i].size() == 0 || paths[i][paths[i].size() - 1] == '/') found_slash_symbol = true;
            if (!found_slash_symbol && i + 1 != paths.size())
                ss << '/';
        }
        return ss.str();
    }

    template<typename T>
    std::vector<T> normalize_weights(const std::vector<T>& weights) {
        T minimum = weights[0];
        T sum = 0;
        for (int i=0; i<weights.size(); ++i) {
            minimum = std::min(minimum, weights[i]);
            sum += weights[i];
        }
        vector<T> res;
        T normalized_sum = sum - minimum * weights.size();
        for (int i=0; i<weights.size(); ++i) {
            res.push_back((weights[i] - minimum) / (normalized_sum));
        }
        return res;
    }

    template vector<float> normalize_weights(const std::vector<float>&);
    template vector<double> normalize_weights(const std::vector<double>&);

    string prefix_match(vector<string> candidates, string input) {
        ASSERT2(!candidates.empty(), "Empty set of candidates for prefix matching.");
        int best_match_idx = -1;
        for (auto& candidate: candidates) {
            if (candidate.size() < input.size())
                continue;
            if (startswith(candidate, input))
                return candidate;
        }
        ASSERT2(false, MS() << "Could not find match for " << input << " in " << candidates <<".");
        return "";
    }

    bool validate_flag_nonempty(const char* flagname, const std::string& value) {
        if (value.empty()) {
            std::cout << "Invalid value for --" << flagname << " (can't be empty)" << std::endl;
        }
        return not value.empty();
    }

    template<typename T>
    T vsum(const vector<T>& vec) {
        T res = 0;
        for(T item: vec) res += item;
        return res;
    }

    template float vsum(const vector<float>& vec);
    template double vsum(const vector<double>& vec);
    template int vsum(const vector<int>& vec);
    template uint vsum(const vector<uint>& vec);
    template unsigned long long vsum(const vector<unsigned long long >& vec);
    template long long vsum(const vector<long long>& vec);



    template<typename T>
    vector<T> reversed(const vector<T>& v) {
        vector<T> ret(v.rbegin(), v.rend());
        return ret;
    }

    std::string capitalize(const std::string& s) {
        std::string capitalized = s;
        // capitalize
        if (capitalized[0] >= 'a' && capitalized[0] <= 'z') {
            capitalized[0] += ('A' - 'a');
        }
        return capitalized;
    }

    template vector<float> reversed(const vector<float>& vec);
    template vector<double> reversed(const vector<double>& vec);
    template vector<int> reversed(const vector<int>& vec);
    template vector<uint> reversed(const vector<uint>& vec);
    template vector<size_t> reversed(const vector<size_t>& vec);
    template vector<string> reversed(const vector<string>& vec);
    template vector<vector<string>> reversed(const vector<vector<string>>& vec);
    // template vector<Mat<float>> reversed(const vector<Mat<float>>& vec);
    // template vector<Mat<double>> reversed(const vector<Mat<double>>& vec);

    ThreadAverage::ThreadAverage(int num_threads) :
            num_threads(num_threads),
            thread_error(num_threads),
            total_updates(0) {
        reset();
    }

    void ThreadAverage::update(double error) {
        thread_error[ThreadPool::get_thread_number()] += error;
        total_updates += 1;
    }

    double ThreadAverage::average() {
        return vsum(thread_error) / total_updates;
    }

    int ThreadAverage::size() {
        return total_updates;
    }

    void ThreadAverage::reset() {
        for (int tidx = 0; tidx < num_threads; ++tidx) {
            thread_error[tidx] = 0;
        }
        total_updates.store(0);
    }
}
