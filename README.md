# WSL C++ GPU電力監視API導入ガイド

GPU電力監視を高速・軽量に行いたいエンジニアのための、
**NVMLベース C++ APIサービス構築ガイドの最終決定版** です。
既存のリポジトリに含まれるソースコードを利用して、
`systemd` + `Nginx` + `NVML` による **常時稼働型 GPUモニターAPI** を立ち上げます。

---

## 0. プロジェクトの定義と前提

このガイドでは、**既存のGitリポジトリに含まれるコード**を使用します。
GPUの消費電力をNVML（NVIDIA Management Library）から取得し、
HTTP APIとして提供するC++サービスを構築します。

| 項目             | 内容                                        |
| -------------- | ----------------------------------------- |
| **プロジェクト名**    | `gpu-monitor-service`                     |
| **APIポート（内部）** | `9001`                                    |
| **WSLユーザー**    | `satoy`                                   |
| **対象環境**       | Ubuntu on WSL2 + NVIDIA GPU (CUDA 12.4対応) |

---

## 1. 環境セットアップとファイルの取得

### 1-1. プロジェクトフォルダの作成とGitクローン

まずは、WSL上でプロジェクトを配置するフォルダを作成します。

```bash
# 1. プロジェクトルートを定義
export PROJECT_ROOT="/home/satoy/gpu-monitor" 

# 2. Gitリポジトリをクローン（Gitがフォルダを自動作成する）
git clone https://github.com/koichi2426/gpu-monitor-service.git $PROJECT_ROOT

echo "✅ プロジェクトフォルダが $PROJECT_ROOT に設定され、ソースコードを取得しました。"

# 3. ビルドに必要なフォルダを手動で作成
cd $PROJECT_ROOT
mkdir -p build/release
```

> 💡 `$PROJECT_ROOT` にすべてのソース (`src/gpu_monitor.cpp`, `CMakeLists.txt` など) が存在する前提です。

---

### 1-2. システム依存関係のインストール

次に、C++ビルド環境とNVMLライブラリを整えます。

```bash
# C++ビルドツール、CMake、Boost、Nginx、Luaモジュールをインストール
sudo apt update
sudo apt install -y build-essential cmake libboost-all-dev nginx-extras libnginx-mod-http-lua

# NVML (NVIDIA Management Library) の依存関係をインストール
sudo apt install -y cuda-compiler-12-4 libnvidia-compute-535 libnvidia-extra-535 libnvidia-ml-dev
```

---

## 2. C++バイナリのビルドと権限設定

ここでは、`gpu_monitor.cpp` からCMakeを使ってバイナリを生成します。

```bash
# 1. ビルドディレクトリへ移動
cd $PROJECT_ROOT/build/release

# 2. CMake構成を生成
cmake -DCMAKE_BUILD_TYPE=Release -S $PROJECT_ROOT -B .

# 3. 並列コンパイル
make -j$(nproc)

# 4. 実行権限を付与 (status=203/EXECエラー対策)
sudo chmod +x ./gpu_monitor
```

---

## 3. systemd サービスの設定と起動

GPUモニターを自動起動させるため、`systemd`サービスを設定します。

---

### 3-1. サービス定義ファイルの作成

```bash
# 1. サービス定義を /etc/systemd/system に配置
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
```

---

### 3-2. サービス実行権限の付与

```bash
# visudo設定ファイルを作成
sudo visudo -f /etc/sudoers.d/010_gpu_monitor_nopasswd
```

開いたエディタに以下を **そのまま** 貼り付け、保存して終了します。

```
satoy ALL=(ALL) NOPASSWD: ALL
```

---

### 3-3. サービス起動と確認

```bash
# 定義を再読み込み
sudo systemctl daemon-reload

# サービスが既に起動している場合は停止させる
sudo systemctl stop gpu-monitor.service 

# 有効化＆起動
sudo systemctl enable --now gpu-monitor.service

# 状態確認 (Active: active (running) であることを確認)
sudo systemctl status gpu-monitor.service
```

---

## 4. Nginx リバースプロキシ設定の統合

外部アクセスを許可するため、
外部ポート **1721** → 内部APIポート **9001** の転送を設定します。

---

### 4-1. 設定ファイルの編集

```bash
sudo vim /etc/nginx/sites-available/agenthub_proxy
```

既存の `server { ... }` ブロックの中に、次のブロックを追加します：

```nginx
location = /gpu/power {
    proxy_pass http://127.0.0.1:9001;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
}
```

---

### 4-2. Nginxの反映

```bash
sudo nginx -t
sudo systemctl restart nginx
```
