#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip> 
#include <stdexcept>
#include <thread>

// --- NVIDIA Management Library (NVML) ---
#include <nvml.h>

// --- Boost (Beast, JSON, ASIO) ---
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/asio.hpp>

namespace http = boost::beast::http;
namespace json = boost::json;
namespace net  = boost::asio;
using tcp = net::ip::tcp;

// =============================
// 1. 定数とNVML初期化ヘルパー
// =============================

constexpr int DEFAULT_GPU_INDEX = 0;

// NVMLのエラーチェック
void check_nvml(nvmlReturn_t result, const std::string& msg) {
    if (result != NVML_SUCCESS) {
        throw std::runtime_error(msg + ": " + std::string(nvmlErrorString(result)));
    }
}

// =============================
// 2. GPU電力データ取得関数
// =============================
json::object get_gpu_power_data() {
    nvmlReturn_t result;
    nvmlDevice_t device;
    unsigned int power_mW = 0;

    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        return {{"status", "error"}, {"message", "NVML init failed: " + std::string(nvmlErrorString(result))}};
    }

    json::object data;

    try {
        // GPUデバイス取得
        check_nvml(nvmlDeviceGetHandleByIndex(DEFAULT_GPU_INDEX, &device), "Failed to get device handle");

        // 🚨 修正: 時刻の取得をナノ秒単位に変換し、文字列として取得
        auto get_time_ns_str = []() -> std::string {
            auto now = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now()
            );
            return std::to_string(now.time_since_epoch().count());
        };

        std::string timestamp_ns_str = get_time_ns_str();

        // 電力取得
        check_nvml(nvmlDeviceGetPowerUsage(device, &power_mW), "Failed to get power usage");
        double power_watts = static_cast<double>(power_mW) / 1000.0;

        // JSON構築
        std::stringstream ss;
        ss << std::fixed << std::setprecision(6) << power_watts; 

        data = {
            {"status", "ok"},
            {"gpu_index", DEFAULT_GPU_INDEX},
            {"power_watts", ss.str()},
            {"timestamp_ns", timestamp_ns_str}
        };
    } catch (const std::exception& e) {
        data = {{"status", "error"}, {"message", e.what()}};
    }

    nvmlShutdown();
    return data;
}

// =============================
// 3. HTTPリクエストハンドラ
// =============================
http::response<http::string_body> handle_request(const http::request<http::string_body>& req) {
    http::response<http::string_body> res;
    res.set(http::field::server, "gpu-monitor/1.0");
    res.set(http::field::content_type, "application/json");

    if (req.method() != http::verb::get) {
        res.result(http::status::method_not_allowed);
        res.body() = R"({"error":"Method Not Allowed"})";
        res.prepare_payload();
        return res;
    }

    if (req.target() == "/health") {
        res.result(http::status::ok);
        res.body() = R"({"status":"healthy"})";
    } else if (req.target() == "/gpu/power") {
        auto json_data = get_gpu_power_data();
        res.result(json_data.at("status").as_string() == "ok"
                   ? http::status::ok
                   : http::status::internal_server_error);
        res.body() = json::serialize(json_data);
    } else {
        res.result(http::status::not_found);
        res.body() = R"({"error":"Not Found"})";
    }

    res.prepare_payload();
    return res;
}

// =============================
// 4. クライアントセッション
// =============================
void do_session(tcp::socket socket) {
    boost::beast::flat_buffer buffer;

    try {
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        auto res = handle_request(req);
        http::write(socket, res);
    } catch (const std::exception& e) {
        std::cerr << "[Session Error] " << e.what() << std::endl;
    }
}

// =============================
// 5. メインサーバーループ
// =============================
int main(int argc, char* argv[]) {
    int port = 9001;
    if (argc == 2) {
        try { port = std::stoi(argv[1]); }
        catch (...) { std::cerr << "Invalid port argument, using default 9001\n"; }
    }

    try {
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), static_cast<unsigned short>(port)}};

        std::cout << "[INFO] GPU Power Monitor running on port " << port << std::endl;

        while (true) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            std::thread{do_session, std::move(socket)}.detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
