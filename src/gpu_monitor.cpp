#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip> 
#include <stdexcept> 

// NVIDIA Management Library (NVML) Headers
#include <nvml.h>

// Boost Headers (ASIO/Beast for networking, JSON for data formatting)
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/asio.hpp>

namespace http = boost::beast::http;
namespace json = boost::json;
namespace net  = boost::asio;
using tcp = net::ip::tcp;

// --- 1. 定数とグローバル状態 ---
const int DEFAULT_GPU_INDEX = 0; // 監視対象のGPUインデックス (通常は0)

// --- 2. NVMLヘルパー関数 ---

// NVMLエラーチェックと例外スロー
void check_nvml_error(nvmlReturn_t result, const std::string& message) {
    if (result != NVML_SUCCESS) {
        std::string error_str = "NVML Error: " + message + " (" + nvmlErrorString(result) + ")";
        throw std::runtime_error(error_str);
    }
}

// GPUの電力と時刻を取得するコア関数
json::object get_gpu_power_data() {
    nvmlReturn_t result;
    nvmlDevice_t device;
    unsigned int power_mW; 
    
    // --- 1. NVMLの初期化 ---
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        // 初期化失敗の場合は、そのままエラーを返す
        return json::object{
            {"status", "error"},
            {"message", "NVML initialization failed: " + std::string(nvmlErrorString(result))}
        };
    }
    
    try {
        // --- 2. GPUデバイスの取得 ---
        result = nvmlDeviceGetHandleByIndex(DEFAULT_GPU_INDEX, &device);
        check_nvml_error(result, "Failed to get device handle");

        // --- 3. 時刻の取得 (ミリ秒UNIXタイムスタンプ) ---
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now()
        );
        long long timestamp_ms = now.time_since_epoch().count();

        // --- 4. 電力情報の取得 (ミリワット単位) ---
        result = nvmlDeviceGetPowerUsage(device, &power_mW); 
        check_nvml_error(result, "Failed to get power usage");

        // --- 5. 値の整形 (ワット単位に変換し、精度を確保) ---
        // 0.001W精度を満たすため、小数点以下6桁で表示
        double power_watts = static_cast<double>(power_mW) / 1000.0; 
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(6) << power_watts;

        // --- 6. JSONレスポンス構築 ---
        return json::object{
            {"status", "ok"},
            {"gpu_index", DEFAULT_GPU_INDEX},
            {"power_watts", ss.str()},
            {"timestamp_ms", timestamp_ms}
        };

    } catch (const std::runtime_error& e) {
        // NVML操作中のエラー
        return json::object{
            {"status", "error"},
            {"message", e.what()}
        };
    }
    
    // --- 7. NVMLの終了処理 ---
    nvmlShutdown();
    return json::object{};
}


// --- 3. HTTPセッションハンドラ ---
http::response<http::string_body> handle_power_request() {
    json::object data = get_gpu_power_data();
    
    http::response<http::string_body> res;
    res.set(http::field::content_type, "application/json");

    if (data.at("status").as_string() == "error") {
        res.result(http::status::internal_server_error);
    } else {
        res.result(http::status::ok);
    }

    res.body() = json::serialize(data);
    res.prepare_payload();
    return res;
}

// HTTPリクエストを処理する関数
void do_session(tcp::socket socket) {
    boost::beast::flat_buffer buffer;
    
    try {
        http::request<http::string_body> req;
        http::read(socket, buffer, req); 
        
        http::response<http::string_body> res;
        
        if (req.method() == http::verb::get) {
            
            if (req.target() == "/health") {
                res.result(http::status::ok);
                res.body() = "OK";
            } else if (req.target() == "/gpu/power") {
                res = handle_power_request();
            } else {
                res.result(http::status::not_found);
                res.body() = "404 Not Found";
            }
        } else {
            res.result(http::status::method_not_allowed);
            res.body() = "Method Not Allowed";
        }

        res.set(http::field::server, "GPU-Monitor/1.0");
        res.prepare_payload();
        http::write(socket, res); 

    } catch (const boost::system::system_error& se) {
        if (se.code() != http::error::end_of_stream) {
            std::cerr << "Server Session Error: " << se.what() << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "General Error: " << e.what() << "\n";
    }
}


// --- 4. メインサーバーループ ---
int main(int argc, char* argv[]) {
    int port = 9001; // デフォルトポートは9001

    if (argc == 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid port argument. Using default port 9001.\n";
        }
    }
    
    try {
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), static_cast<unsigned short>(port)}};
        
        std::cout << "[INFO] GPU Power Monitor Server listening on port " << port << "\n";

        while (true) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            
            // クライアント接続を新しいスレッドで処理
            std::thread{do_session, std::move(socket)}.detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "Server Initialization Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
