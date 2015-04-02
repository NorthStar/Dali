#include <algorithm>
#include <atomic>
#include <Eigen/Eigen>
#include <fstream>
#include <ostream>
#include <fstream>
#include <iterator>
#include <chrono>

#include "dali/core.h"
#include "dali/utils.h"
#include "dali/data_processing/SST.h"
#include "dali/data_processing/Glove.h"
#include "dali/models/StackedModel.h"

using std::vector;
using std::make_shared;
using std::shared_ptr;
using std::ifstream;
using std::istringstream;
using std::stringstream;
using std::string;
using std::ofstream;
using std::min;
using utils::Vocab;
using utils::tokenized_uint_labeled_dataset;
using std::atomic;
using std::chrono::seconds;
using SST::Databatch;
using utils::ConfusionMatrix;
using utils::assert2;

static int ADADELTA_TYPE = 0;
static int ADAGRAD_TYPE = 1;
static int SGD_TYPE = 2;
static int ADAM_TYPE = 3;
static int RMSPROP_TYPE = 4;

typedef float REAL_t;
typedef Mat<REAL_t> mat;

DEFINE_int32(minibatch,      100,        "What size should be used for the minibatches ?");
DEFINE_int32(patience,       5,          "How many unimproving epochs to wait through before witnessing progress ?");
DEFINE_double(dropout,       0.3,        "How much dropout noise to add to the problem ?");
DEFINE_bool(fast_dropout,    true,       "Use fast dropout?");
DEFINE_string(solver,        "adadelta", "What solver to use (adadelta, sgd, adam)");
DEFINE_string(test,          "",         "Where is the test set?");
DEFINE_double(root_weight,   1.0,        "By how much to weigh the roots in the objective function?");
DEFINE_string(pretrained_vectors, "",    "Load pretrained word vectors?");
DEFINE_double(learning_rate, 0.01,       "Learning rate for SGD and Adagrad.");
DEFINE_string(results_file, "",          "Where to save test performance.");
DEFINE_string(save_location, "",          "Where to save test performance.");

ThreadPool* pool;

Mat<REAL_t> apply_dropout(Mat<REAL_t> X, REAL_t drop_prob) {
    if (drop_prob > 0) {
        if (FLAGS_fast_dropout) {
            return MatOps<REAL_t>::fast_dropout(X);
        } else {
            return MatOps<REAL_t>::dropout_normalized(X, drop_prob);
        }
    } else {
        return X;
    }
}

template<typename Model>
std::tuple<REAL_t,REAL_t> average_recall(
    Model& model,
    std::vector<std::vector<std::tuple<std::vector<uint>, uint, bool>>>& dataset) {
    std::cout << "Getting average_recall" << std::endl;
    ReportProgress<REAL_t> journalist("Average recall", dataset.size());
    atomic<int> seen_minibatches(0);
    atomic<int> correct(0);
    atomic<int> correct_root(0);
    atomic<int> total_root(0);
    atomic<int> total(0);
    graph::NoBackprop nb;
    for (int batch_id = 0; batch_id < dataset.size(); ++batch_id) {
        pool->run([batch_id, &model, &dataset, &correct, &total, &correct_root, &total_root, &journalist, &seen_minibatches]() {
            graph::NoBackprop nb;
            auto& minibatch = dataset[batch_id];
            for (auto& example : minibatch) {
                auto prediction = model.decoder->activate(model.get_final_activation(
                    std::get<0>(example), // see an example
                    0.0                   // activate without dropout
                ).back().hidden).argmax();               // no softmax needed, simply get best guess
                if (prediction == std::get<1>(example)) {
                    correct += 1;
                    if (std::get<2>(example)) {
                        correct_root +=1;
                    }
                }
                total += 1;
                if (std::get<2>(example)) {
                    total_root +=1;
                }
            }
            seen_minibatches += 1;
            journalist.tick(seen_minibatches, 100.0 * ((REAL_t) correct / (REAL_t) total));
        });
    }
    pool->wait_until_idle();
    journalist.done();
    return std::tuple<REAL_t, REAL_t>(100.0 * ((REAL_t) correct / (REAL_t) total), 100.0 * (REAL_t) correct_root  / (REAL_t) total_root);
}

int main (int argc,  char* argv[]) {
    GFLAGS_NAMESPACE::SetUsageMessage(
        "\n"
        "Sentiment Analysis using multiple bidirectional LSTMs\n"
        "-----------------------------------------------------\n"
        "\n"
        " @author Jonathan Raiman\n"
        " @date March 13th 2015"
    );

    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

    auto epochs              = FLAGS_epochs;
    auto sentiment_treebank  = SST::load(FLAGS_train);


    auto embedding = Mat<REAL_t>(100, 0);
    auto word_vocab = Vocab();

    if (!FLAGS_pretrained_vectors.empty()) {
        glove::load(FLAGS_pretrained_vectors, embedding, word_vocab, 50000);
    } else {
        word_vocab = SST::get_word_vocab(sentiment_treebank, FLAGS_min_occurence);
    }
    auto vocab_size          = word_vocab.index2word.size();

    // Load Dataset of Trees:
    // Put trees into matrices:

    std::vector<std::vector<std::tuple<std::vector<uint>, uint, bool>>> dataset;

    auto to_index_pair = [&word_vocab](std::pair<std::vector<std::string>, uint>&& pair, bool&& is_root) {
        return std::tuple<std::vector<uint>, uint, bool>(
            word_vocab.transform(pair.first),
            pair.second,
            is_root);
    };

    auto add_to_dataset_in_minibatches = [&to_index_pair](
        std::vector<std::vector<std::tuple<std::vector<uint>, uint, bool>>>& dataset,
        std::vector<SST::AnnotatedParseTree::shared_tree>& trees
        ) {
        if (dataset.size() == 0)
            dataset.emplace_back(0);
        for (auto& tree : trees) {
            if (dataset[dataset.size()-1].size() == FLAGS_minibatch) {
                dataset.emplace_back(0);
                dataset.reserve(FLAGS_minibatch);
            }
            dataset[dataset.size()-1].emplace_back(
                to_index_pair(
                    tree->to_labeled_pair(),
                    true
                )
            );

            for (auto& child : tree->general_children) {
                if (dataset[dataset.size()-1].size() == FLAGS_minibatch) {
                    dataset.emplace_back(0);
                    dataset.reserve(FLAGS_minibatch);
                }
                dataset[dataset.size()-1].emplace_back(
                    to_index_pair(
                        child->to_labeled_pair(),
                        false
                    )
                );
            }
        }
    };

    add_to_dataset_in_minibatches(dataset, sentiment_treebank);

    std::vector<std::vector<std::tuple<std::vector<uint>, uint, bool>>> validation_set;

    {
        auto validation_treebank = SST::load(FLAGS_validation);
        add_to_dataset_in_minibatches(validation_set, validation_treebank);
    }


    std::cout << " Unique Trees Loaded : " << sentiment_treebank.size() << std::endl
              << "        Example tree : " << *sentiment_treebank[sentiment_treebank.size()-1] << std::endl
              << "     Vocabulary size : " << vocab_size << std::endl
              << "      minibatch size : " << FLAGS_minibatch << std::endl
              << "   number of threads : " << FLAGS_j << std::endl
              << "        Dropout type : " << (FLAGS_fast_dropout ? "fast" : "default") << std::endl
              << " Max training epochs : " << FLAGS_epochs << std::endl
              << "           LSTM type : " << (FLAGS_memory_feeds_gates ? "Graves 2013" : "Zaremba 2014") << std::endl
              << "          Stack size : " << std::max(FLAGS_stack_size, 1) << std::endl
              << " # training examples : " << dataset.size() * FLAGS_minibatch - (FLAGS_minibatch - dataset[dataset.size() - 1].size()) << std::endl;

    pool = new ThreadPool(FLAGS_j);

    /*
    Create a model with an embedding, and several stacks:
    */

    auto stack_size  = std::max(FLAGS_stack_size, 1);

    auto model = StackedModel<REAL_t>(
         FLAGS_pretrained_vectors.empty() ? word_vocab.index2word.size() : 0,
         FLAGS_pretrained_vectors.empty() ? FLAGS_hidden : embedding.dims(1),
         FLAGS_hidden,
         stack_size,
         SST::label_names.size(),
         FLAGS_shortcut,
         FLAGS_memory_feeds_gates);

    if (!FLAGS_pretrained_vectors.empty()) {
        model.embedding = embedding;
    }

    vector<vector<Mat<REAL_t>>> thread_params;

    // what needs to be optimized:
    vector<StackedModel<REAL_t>> thread_models;
    for (int i = 0; i < FLAGS_j; i++) {
        // create a copy for each training thread
        // (shared memory mode = Hogwild)
        thread_models.push_back(model.shallow_copy());
        thread_params.push_back(thread_models.back().parameters());
    }
    auto params = model.parameters();
    auto svd_init = weights<REAL_t>::svd(weights<REAL_t>::gaussian(0.0, 1.0));

    for (auto& param : params) {
        if (param.dims(0) < 1000) {
            svd_init(param);
        }
    }

    // Rho value, eps value, and gradient clipping value:
    std::shared_ptr<Solver::AbstractSolver<REAL_t>> solver;
    int solver_type;
    if (FLAGS_solver == "adadelta") {
        std::cout << "Using AdaDelta" << std::endl;
        solver = make_shared<Solver::AdaDelta<REAL_t>>(params, 0.95, 1e-9, 100.0);
        solver_type = ADADELTA_TYPE;
    } else if (FLAGS_solver == "adam") {
        std::cout << "Using Adam" << std::endl;
        solver = make_shared<Solver::Adam<REAL_t>>(params, 0.1, 0.001, 1e-9, 100.0);
        solver_type = ADAM_TYPE;
    } else if (FLAGS_solver == "sgd") {
        std::cout << "Using vanilla SGD" << std::endl;
        solver = make_shared<Solver::SGD<REAL_t>>(params, 1e-9, 100.0);
        solver_type = SGD_TYPE;
    } else if (FLAGS_solver == "adagrad") {
        std::cout << "Using Adagrad" << std::endl;
        solver = make_shared<Solver::AdaGrad<REAL_t>>(params, 1e-9, 100.0);
        solver_type = ADAGRAD_TYPE;
    } else {
        utils::exit_with_message("Did not recognize this solver type.");
    }

    std::tuple<REAL_t, REAL_t> best_validation_score({0.0, 0.0});
    int epoch = 0;
    int best_epoch = 0;
    double patience = 0;
    string best_file = "";

    while (patience < FLAGS_patience && epoch < epochs) {

        stringstream ss;
        ss << "Epoch " << ++epoch;
        atomic<int> batches_processed(0);

        ReportProgress<double> journalist(
            ss.str(),      // what to say first
            dataset.size() // how many steps to expect before being done with epoch
        );

        for (int batch_id = 0; batch_id < dataset.size(); ++batch_id) {
            pool->run([&solver_type, &thread_params, &thread_models, batch_id, &journalist, &solver, &dataset, &best_validation_score, &batches_processed]() {
                auto& thread_model  = thread_models[ThreadPool::get_thread_number()];
                auto& params        = thread_params[ThreadPool::get_thread_number()];
                auto& minibatch     = dataset[batch_id];
                // many forward steps here:
                // REAL_t err = 0.0;
                for (auto & example : minibatch) {
                    auto logprobs = thread_model.decoder->activate(
                        apply_dropout(
                            thread_model.get_final_activation(std::get<0>(example), FLAGS_dropout).back().hidden,
                            FLAGS_dropout
                        )
                    );
                    Mat<REAL_t> error;
                    error = MatOps<REAL_t>::softmax_cross_entropy(logprobs, std::get<1>(example));
                    // err += error.w()(0);
                    // auto error = MatOps<REAL_t>::softmax_cross_entropy(logprobs, std::get<1>(example));
                    if (std::get<2>(example) && FLAGS_root_weight != 1.0) {
                        error = error * FLAGS_root_weight;
                    }
                    error.grad();
                    graph::backward(); // backpropagate
                }
                if (solver_type == ADAGRAD_TYPE) {
                    dynamic_cast<Solver::AdaGrad<REAL_t>*>(solver.get())->step(params, FLAGS_learning_rate);
                } else if (solver_type == SGD_TYPE) {
                    dynamic_cast<Solver::SGD<REAL_t>*>(solver.get())->step(params, FLAGS_learning_rate);
                } else {
                    solver->step(params); // One step of gradient descent
                }
                journalist.tick(++batches_processed, std::get<0>(best_validation_score));
            });
        }
        pool->wait_until_idle();
        journalist.done();
        auto new_validation = average_recall(model, validation_set);
        std::cout << "Root recall=" << std::get<1>(new_validation) << std::endl;
        if (solver_type == ADAGRAD_TYPE) {
            solver->reset_caches(params);
        }
        if (std::get<0>(new_validation) + 1e-6 < std::get<0>(best_validation_score)) {
            // lose patience:
            patience += 1;
        } else {
            // recover some patience:
            patience = std::max(patience - 1, 0.0);
            best_validation_score = new_validation;
        }
        if (best_validation_score != new_validation) {
            std::cout << "Epoch (" << epoch << ") Best validation score = " << std::get<0>(best_validation_score) << "% ("<< std::get<0>(new_validation) << "%), patience = " << patience << std::endl;
        } else {
            std::cout << "Epoch (" << epoch << ") Best validation score = " << std::get<0>(best_validation_score) << "%, patience = " << patience << std::endl;
            best_epoch = epoch;
        }
        if (!FLAGS_save_location.empty()) {
            stringstream ss;
            ss << FLAGS_save_location;
            ss << "_" << epoch;
            model.save(ss.str());
            if (best_validation_score == new_validation) {
                best_file = ss.str();
            }
        }
    }

    if (!FLAGS_test.empty()) {
        std::vector<std::vector<std::tuple<std::vector<uint>, uint, bool>>> test_set;
        {
            auto test_treebank = SST::load(FLAGS_test);
            add_to_dataset_in_minibatches(test_set, test_treebank);
        }
        if (!FLAGS_save_location.empty() && !best_file.empty()) {
            auto params = model.parameters();
            utils::load_matrices(params, best_file);
        }
        auto a_r = average_recall(model, test_set);

        std::cout << "Done training" << std::endl;
        std::cout << "Test recall " << std::get<0>(a_r) << "%, root => " << std::get<1>(a_r)<< "%" << std::endl;
        if (!FLAGS_results_file.empty()) {
            ofstream fp;
            fp.open(FLAGS_results_file.c_str(), std::ios::out | std::ios::app);
            fp         << FLAGS_solver
               << "\t" << FLAGS_minibatch
               << "\t" << (FLAGS_fast_dropout ? "fast" : "std")
               << "\t" << FLAGS_dropout
               << "\t" << FLAGS_hidden
               << "\t" << std::get<0>(a_r)
               << "\t" << std::get<1>(a_r)
               << "\t" << best_epoch
               << std::endl;
        }
    }
}

