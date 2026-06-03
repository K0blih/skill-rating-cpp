#include "app/commands.hpp"
#include "app/json.hpp"

#include <httplib.h>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

struct Config {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::size_t workers = [] {
        const unsigned int detected = std::thread::hardware_concurrency();
        return detected == 0 ? std::size_t{4} : static_cast<std::size_t>(detected);
    }();
};

int parse_int(std::string_view value, std::string_view label) {
    std::size_t parsed = 0;
    int result = 0;
    try {
        result = std::stoi(std::string(value), &parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string(label) + " must be an integer");
    }
    if (parsed != value.size()) {
        throw std::invalid_argument(std::string(label) + " must be an integer");
    }
    return result;
}

Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--host" || arg == "-h") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            config.port = parse_int(argv[++i], "port");
        } else if ((arg == "--workers" || arg == "-w") && i + 1 < argc) {
            const int workers = parse_int(argv[++i], "workers");
            if (workers <= 0) {
                throw std::invalid_argument("workers must be positive");
            }
            config.workers = static_cast<std::size_t>(workers);
        } else if (arg == "--help") {
            std::cout << "Usage: skill_rating_http [--host 127.0.0.1] [--port 8080] [--workers N]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }
    if (config.port <= 0 || config.port > 65535) {
        throw std::invalid_argument("port must be in 1..65535");
    }
    return config;
}

void set_json(httplib::Response& response, int status, std::string body) {
    response.status = status;
    response.set_content(std::move(body), "application/json");
}

std::optional<std::string> request_id_from(std::string_view request_body) {
    const skill_rating::app::Json request_json = skill_rating::app::parse_json(request_body);
    if (!request_json.is_object()) {
        return std::nullopt;
    }
    const auto iter = request_json.find("request_id");
    if (iter == request_json.end()) {
        return std::nullopt;
    }
    if (!iter->is_string()) {
        throw std::invalid_argument("request_id must be a string");
    }
    return iter->get<std::string>();
}

std::string response_json(std::string body, const std::optional<std::string>& request_id) {
    if (!request_id.has_value()) {
        return body;
    }
    skill_rating::app::Json response = skill_rating::app::parse_json(body);
    response["request_id"] = *request_id;
    return response.dump();
}

std::string error_json(std::string_view message, const std::optional<std::string>& request_id) {
    skill_rating::app::Json response{{"error", std::string(message)}};
    if (request_id.has_value()) {
        response["request_id"] = *request_id;
    }
    return response.dump();
}

void register_command(httplib::Server& server, const char* route, const char* command) {
    server.Post(route, [command](const httplib::Request& request, httplib::Response& response) {
        std::optional<std::string> request_id;
        try {
            request_id = request_id_from(request.body);
            set_json(response, 200, response_json(skill_rating::app::run_command(command, request.body), request_id));
        } catch (const std::invalid_argument& error) {
            set_json(response, 400, error_json(error.what(), request_id));
        } catch (const std::exception& error) {
            set_json(response, 500, error_json(error.what(), request_id));
        }
    });
}

int run(int argc, char** argv) {
    const Config config = parse_args(argc, argv);
    httplib::Server server;
    server.new_task_queue = [workers = config.workers] {
        return new httplib::ThreadPool(workers);
    };

    server.Get("/health", [](const httplib::Request&, httplib::Response& response) {
        set_json(response, 200, "{\"status\":\"ok\"}");
    });
    register_command(server, "/rate", "rate");
    register_command(server, "/rate-1vs1", "rate-1vs1");
    register_command(server, "/quality", "quality");
    register_command(server, "/quality-1vs1", "quality-1vs1");
    register_command(server, "/expose", "expose");
    register_command(server, "/draw-probability", "draw-probability");

    server.set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if (response.status == 404) {
            set_json(response, 404, skill_rating::app::error_json("unknown route"));
        }
    });

    std::cout << "skill_rating_http listening on http://" << config.host << ':' << config.port
              << " with " << config.workers << " workers\n";
    if (!server.listen(config.host, config.port)) {
        throw std::runtime_error("failed to listen on " + config.host + ":" + std::to_string(config.port));
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
