#include "dali/data_processing/Glove.h"
#include "dali/tensor/__MatMacros__.h"

using std::string;
using utils::Vocab;
using std::make_tuple;
using std::vector;
using utils::from_string;

namespace glove {
    template<typename T>
    void load(string fname, Mat<T>* underlying_mat, Vocab* vocab, int threshold) {
        ASSERT2(utils::file_exists(fname), "Cannot open file with glove vectors.");

        std::fstream fp(fname);
        std::string line;

        int observed_size = underlying_mat->dims(1);
        int capacity      = underlying_mat->dims(0);

        if (capacity == 0) {
            underlying_mat->resize(1, std::max(observed_size, 1));
            capacity = 1;
        }
        auto& mat         = underlying_mat->w();
        int vocab_size = 0;
        vector<string> vocabulary;
        std::string item;
        // // use mat for assigning elements
        while (std::getline(fp, line)) {
            bool found_name = false;
            int i = 0; // count how many numbers are in this row
            std::stringstream tokenizer(line);
            while (std::getline(tokenizer, item, ' ')) {
                if (!found_name) {
                    vocabulary.emplace_back(item);
                    vocab_size += 1;
                    if (vocab_size > capacity) {
                        // increase matrix by 10%
                        mat.resize(mshadow::Shape2(std::max((int)(capacity * 1.1), capacity + 1), observed_size));
                        capacity = std::max((int)(capacity * 1.1), capacity + 1);
                    }
                    found_name = true;
                } else {
                    if (!item.empty()) {
                        if (observed_size == 0 && i+1 > mat.shape[1]) {
                            mat.resize(mshadow::Shape2(capacity, std::max(100, (int) (mat.shape[1] * 1.5))));
                            mat(vocab_size-1, i) = from_string<T>(item);
                        } else {
                            mat(vocab_size-1, i) = from_string<T>(item);
                        }
                        i += 1;
                    }
                }
            }

            if (observed_size == 0) {
                // actualize size of matrix
                observed_size = i;
                mat.resize(mshadow::Shape2(capacity, observed_size));
            }
            ASSERT2(i == observed_size,
                utils::MS() << "Vectors in Glove file are of different sizes. Expected " << observed_size << " but found " << i
            );
            // got all the vocab needed.
            if (vocab_size == threshold) {
                break;
            }
        }
        (*vocab) = Vocab(vocabulary);
        if (observed_size > 0) {
            // // now final update is made to matrix
           underlying_mat->resize(vocab_size + 1, observed_size);
           mat[vocab_size].clear();
        } else {
            underlying_mat->forget_w();
            underlying_mat->forget_dw();
        }
    }

    template<typename T>
    std::tuple<Mat<T>, Vocab> load(string fname, int threshold) {
        auto pair = make_tuple<Mat<T>, Vocab>(Mat<T>(threshold > 0 ? threshold : 100, 0, false), Vocab());
        load(fname, &std::get<0>(pair), &std::get<1>(pair), threshold);
        return pair;
    }

    template<typename T>
    int load_relevant_vectors(std::string fname,
                              Mat<T>* target,
                              const utils::Vocab& vocab,
                              int threshold) {
        ASSERT2(utils::file_exists(fname), "Cannot open file with glove vectors.");

        SmartParser sp = SmartParser::from_path(fname);
        int embedding_size = 0;
        int words_read_so_far = 0;
        int words_matched_so_far = 0;
        try {
            while(true) {
                string line = sp.next_line();
                vector<string> tokens = utils::split(line, ' ', false);
                utils::assert2(tokens.size() > 0, "Glove file is damaged");
                const string& word = tokens[0];
                if (vocab.word2index.find(word) != vocab.word2index.end()) {
                    int word_index = vocab.word2index.at(word);
                    vector<T> embedding;
                    // parse embeddings
                    for (int tidx = 1; tidx < tokens.size(); ++tidx)
                        embedding.push_back(from_string<T>(tokens[tidx]));
                    // ensure matrix is the right size
                    if (embedding_size == 0) {
                        embedding_size = embedding.size();

                        if (target->dims(0) != vocab.word2index.size() ||
                                target->dims(1) !=  embedding_size) {
                            *target = Mat<T>(vocab.size(), embedding_size,
                                            weights<T>::uniform(1.0/embedding_size));
                        }
                    }
                    // store the embedding
                    for (int eidx = 0; eidx < embedding_size; ++ eidx)
                        target->w(word_index, eidx) = embedding[eidx];
                    ++words_matched_so_far;
                }
                if (threshold != -1 && ++words_read_so_far >= threshold) break;
            }
        } catch(...) {
            // we done
        }
        return words_matched_so_far;
    }

    template std::tuple<Mat<float>, Vocab> load(string, int );
    template std::tuple<Mat<double>, Vocab> load(string, int );

    template void load(string fname, Mat<float>* underlying_mat, Vocab* vocab, int );
    template void load(string fname, Mat<double>* underlying_mat, Vocab* vocab, int );
    template int load_relevant_vectors(std::string, Mat<float>*,const utils::Vocab& vocab, int);
    template int load_relevant_vectors(std::string, Mat<double>*,const utils::Vocab& vocab, int);
}
