## 【WSL2最終版】C++ GPU電力監視API (NVML) 導入ガイド

### 概要

このガイドは、WSL2環境で、NVIDIAのNVMLライブラリを直接使用した**超高速なGPU電力監視API**を構築し、Nginx経由で外部公開するための手順を統合したものです。

  * **プロジェクト名:** `gpu-monitor-service` (完全に独立)
  * **APIポート (内部):** `9001` (既存の`8000`番台と衝突回避)
  * **目的:** 0.001W精度、ミリ秒単位の応答速度の実現。

-----

### 1\. 開発環境のセットアップと権限設定

このステップで、プロジェクトフォルダの作成、必要なシステムのインストール、そして最も重要である**パスワード不要の権限設定**を行います。

#### 1-1. プロジェクトフォルダの作成とGitクローン

```bash
# 1. プロジェクトルートを定義 (Linuxネイティブ領域に設定)
export PROJECT_ROOT="/home/satoy/gpu-monitor-service" 
cd ~
mkdir -p $PROJECT_ROOT/src
mkdir -p $PROJECT_ROOT/build/release

# 2. リポジトリをクローン (このリポジトリに全てのソースコードが含まれている前提)
git clone https://github.com/koichi2426/gpu-monitor-service.git $PROJECT_ROOT
```

#### 1-2. システム依存関係のインストール

```bash
# C++コンパイラ、CMake、Boost、Nginx、Luaモジュールをすべてインストール
sudo apt update
sudo apt install -y build-essential cmake libboost-all-dev nginx-extras libnginx-mod-http-lua

# NVML (NVIDIA Management Library) の依存関係をインストール
# これが電力監視のコアです
sudo apt install -y cuda-compiler-12-4 libnvidia-compute-535 libnvidia-extra-535 libnvidia-ml-dev
```

#### 1-3. 【必須】サービス起動権限の解決 (NOPASSWD)

バックグラウンドサービスがパスワードなしで`systemctl`を使えるように設定します。

```bash
# 1. sudoers設定ファイルを編集
sudo visudo -f /etc/sudoers.d/010_gpu_monitor_nopasswd

# 2. エディタに以下の1行を貼り付け、保存して終了
# (ユーザー名は 'satoy' に置き換えてください)
# satoy ALL=(ALL) NOPASSWD: ALL
```

-----

### 2\. C++バイナリのビルドとサービス化

#### 2-1. ビルドの実行と権限付与

```bash
# 1. ビルド作業ディレクトリに移動
cd $PROJECT_ROOT/build/release

# 2. CMakeでビルド設定を生成 
cmake -DCMAKE_BUILD_TYPE=Release -S $PROJECT_ROOT -B .

# 3. Makeでコンパイル
make -j$(nproc)

# 4. 【最重要】実行権限を付与 
sudo chmod +x ./gpu_monitor
```

#### 2-2. systemd サービスの設定と起動 (Port 9001)

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

# 2. サービスのリロードと起動
sudo systemctl daemon-reload
sudo systemctl enable --now gpu-monitor.service

# 3. 状態確認: 'Active: active (running)' になれば成功
sudo systemctl status gpu-monitor.service
```

-----

### 3\. Nginx リバースプロキシの設定 (外部公開)

外部ポート`1721`からのリクエストを、内部の`9001`番に転送する設定を追加します。

```bash
# 1. Nginx設定ファイルをvimで開く
sudo vim /etc/nginx/sites-available/agenthub_proxy
```

**2. 既存の`server { ... }`ブロック内**に、以下の`location`ブロックを追加し、保存して終了します。

```nginx
location = /gpu/power {
    proxy_pass http://127.0.0.1:9001;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
}
```

**3. Nginxの有効化と再起動**

```bash
# Nginxを再起動して設定を反映
sudo nginx -t
sudo systemctl restart nginx
```

### ✅ 最終テスト

外部のPCから以下のコマンドを実行し、高精度なJSONが返ってくることを確認します。

```bash
# 外部PC（koichi@koxo）から実行 (IPはあなたのグローバルIPに置き換えてください)
curl http://118.9.7.134:1721/gpu/power
```