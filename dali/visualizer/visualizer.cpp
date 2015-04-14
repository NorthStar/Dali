#include "visualizer.h"
#include "dali/utils/core_utils.h"

using utils::MS;
using std::string;
using json11::Json;

DEFINE_string(visualizer_hostname, "127.0.0.1", "Default hostname to be used by visualizer.");
DEFINE_int32(visualizer_port, 6379, "Default port to be used by visualizer.");

namespace visualizable {
    const std::vector<double> FiniteDistribution::empty_vec;
}

Visualizer::Visualizer(std::string my_namespace, std::shared_ptr<redox::Redox> other_rdx):
        my_namespace(my_namespace) {
    if(!other_rdx) {
        // if no redox connection was passed we create
        // one on the fly:
        rdx = std::make_shared<redox::Redox>(std::cout, redox::log::Off);
        // next we try to connect:
        std::cout << "Visualizer connecting to redis "
                  << FLAGS_visualizer_hostname << ":" << FLAGS_visualizer_port << std::endl;
        connected = rdx->connect(FLAGS_visualizer_hostname, FLAGS_visualizer_port);
    } else {
        rdx = other_rdx;
        connected = true;
    }
    if (!connected) {
        throw std::runtime_error("VISUALIZER ERROR: can't connect to redis.");
        return;
    }
    std::string namespace_key = MS() << "namespace_" << this->my_namespace;
    auto& key_exists = rdx->commandSync<int>({"EXISTS", namespace_key});
    assert(key_exists.ok());
    if (key_exists.reply() == 1) {
        throw std::runtime_error(MS() << "VISUALIZER ERROR: visualizer name already in use: " << my_namespace);
    }
    key_exists.free();
    // then we ping the visualizer regularly:
    pinging = eq.run_every([this, namespace_key]() {
        if (!connected) return;
        // Expire at 2 seconds
        rdx->command<string>({"SET", namespace_key, "1", "EX", "2"});
    }, std::chrono::seconds(1));
}

void Visualizer::feed(const json11::Json& obj) {
    if (!connected) return;
    rdx->publish(MS() << "feed_" << my_namespace, obj.dump());
}

void Visualizer::feed(const std::string& str) {
    Json str_as_json = Json::object {
        { "type", "report" },
        { "data", str },
    };
    feed(str_as_json);
}
