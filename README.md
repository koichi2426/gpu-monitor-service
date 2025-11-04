# 【WSL2最終版】C++ GPU電力監視API導入ガイド (Port 9001)

本記事では、**C++でNVMLを直接利用してGPU電力情報を取得する独立APIサーバー**を構築する手順を紹介します。
既存プロジェクトとは切り離された、**軽量かつシンプルな最終版セットアップガイド**です。

---

## 0. プロジェクト概要

このガイドでは、NVIDIAの **NVML (NVIDIA Management Library)** をC++から直接呼び出し、GPU電力や温度、使用率などをリアルタイムで取得します。

| 項目                    | 内容                    |
| --------------------- | --------------------- |
| **プロジェクト名**           | `gpu-monitor-service` |
| **APIポート (内部)**       | `9001`                |
| **外部公開ポート (Nginx経由)** | `1721`                |
| **ユーザー**              | `satoy`（WSLユーザー）      |

---

## 1. システム準備

### 1-1. プロジェクトフォルダ作成とクローン

```bash
# プロジェクトルートを定義
export PROJECT_ROOT="/home/satoy/gpu-monitor-service"

# ディレクトリ作成
mkdir -p $PROJECT_ROOT/src
mkdir -p $PROJECT_ROOT/build/release

# ソース取得
git clone https://github.com/koichi2426/gpu-monitor-service.git $PROJECT_ROOT
```

---

### 1-2. 依存パッケージのインストール

```bash
sudo apt update
sudo apt install -y build-essential cmake libboost-all-dev nginx-extras libnginx-mod-http-lua
sudo apt install -y cuda-compiler-12-4 libnvidia-compute-535 libnvidia-extra-535 libnvidia-ml-dev
```

※ `libnvidia-ml-dev` に NVML ライブラリが含まれます。

---

## 2. C++ソース構成

### 2-1. `src/gpu_monitor.cpp`

GPU電力・温度・使用率を取得し、JSON形式で返すHTTPサーバーを実装。
（ソースは `git clone` に含まれています）

### 2-2. `CMakeLists.txt`

NVMLライブラリとBoost.Asioをリンクする設定。

---

## 3. ビルドと実行権限

```bash
cd $PROJECT_ROOT/build/release
cmake -DCMAKE_BUILD_TYPE=Release -S $PROJECT_ROOT -B .
make -j$(nproc)
sudo chmod +x ./gpu_monitor
```

---

## 4. systemdサービス化

### 4-1. sudo権限の自動化

```bash
sudo visudo -f /etc/sudoers.d/010_gpu_monitor_nopasswd
# 以下を追記
satoy ALL=(ALL) NOPASSWD: ALL
```

### 4-2. サービス定義と起動

```bash
sudo tee /etc/systemd/system/gpu-monitor.service > /dev/null <<EOL
[Unit]
Description=NVIDIA GPU Power Monitoring API (Port 9001)
After=network.target

[Service]
Environment="PROJECT_ROOT=${PROJECT_ROOT}"
User=$(whoami)
ExecStart=$PROJECT_ROOT/build/release/gpu_monitor 9001
Restart=always
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOL

sudo systemctl daemon-reload
sudo systemctl enable --now gpu-monitor.service
```

---

## 5. Nginxリバースプロキシ設定

### 5-1. 設定追加

```bash
sudo vim /etc/nginx/sites-available/agenthub_proxy
```

`server { ... }` 内に以下を追記：

```nginx
location = /gpu/power {
    proxy_pass http://127.0.0.1:9001;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
}
```

### 5-2. Nginx再起動

```bash
sudo nginx -t
sudo systemctl restart nginx
```

---

## ✅ 最終テスト

外部のPCから、GPU電力情報APIを叩いて応答を確認します。

```bash
# 外部PCから実行
curl http://<あなたのグローバルIP>:1721/gpu/power
```

正常であれば、以下のようなJSONが返ります：

```json
{
  "power_watts": 68.25,
  "temperature_c": 57,
  "gpu_utilization": 42
}
```

---

## まとめ

これで、WSL2上に**独立したGPU電力監視APIサービス**が構築されました。
Nginx経由で安全に公開でき、他のシステムからも軽量にGPU情報を取得できます。

---

（完）
