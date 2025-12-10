# HEW2026

チーム「Nihonium113」の制作物です。

学校主催のゲーム開発コンテスト「HEW」に向けて制作中のゲームプロジェクトです。

## 環境

- OS: Windows 10 以上
- IDE: Visual Studio 2022
- 言語: C++20
- グラフィックス: DirectX 11

## ビルド方法

### クローン

```bash
git clone --recursive https://github.com/HEW2026-Nihonium113/HEW2026.git
cd HEW2026
```

### ビルド・実行

```bash
# プロジェクト生成 + ビルド
@make_project.cmd

# 実行
bin\Debug-windows-x64\game\game.exe
```

### その他のコマンド

| コマンド | 説明 |
|---------|------|
| `@make_project.cmd` | プロジェクト生成 + ビルド |
| `@open_project.cmd` | Visual Studioで開く |
| `@cleanup.cmd` | ビルド成果物を削除 |
| `build_debug.cmd` | Debugビルドのみ |
| `build_release.cmd` | Releaseビルドのみ |

## ディレクトリ構成

```
source/
├── dx11/                   # DirectX 11 ラッパーライブラリ
│   ├── gpu_common.h        # 共通ヘッダー（ComPtr, NonCopyable等）
│   ├── graphics_device     # D3D11デバイス管理（シングルトン）
│   ├── graphics_context    # D3D11コンテキスト管理
│   ├── swap_chain          # スワップチェーン
│   ├── gpu/                # GPUリソース（Buffer, Texture, Shader）
│   ├── graphics/           # シェーダーシステム
│   │   └── compile/        # シェーダーコンパイラ・キャッシュ
│   ├── texture/            # テクスチャ管理（ローダー、キャッシュ）
│   ├── view/               # リソースビュー（SRV, RTV, DSV, UAV）
│   ├── state/              # パイプラインステート
│   ├── format/             # DXGIフォーマットユーティリティ
│   ├── fs/                 # ファイルシステム抽象化
│   ├── platform/           # アプリケーションフレームワーク
│   ├── Input/              # 入力システム（キーボード、マウス、ゲームパッド）
│   ├── logging/            # ログ出力
│   └── utility/            # ユーティリティ
│
├── engine/                 # ゲームエンジン層
│   ├── engine.h            # エンジン初期化
│   ├── scene/              # シーン管理
│   ├── component/          # コンポーネント（Transform2D, SpriteRenderer等）
│   └── graphics2d/         # 2D描画（SpriteBatch）
│
└── game/                   # ゲーム本体
    ├── main.cpp            # エントリーポイント
    ├── game.h/.cpp         # ゲームクラス
    └── scenes/             # ゲームシーン
```

## 開発の流れ

### プログラマー向け（コード変更）

**masterへの直接pushは禁止されています。** 必ずPR（Pull Request）を作成してください。

```bash
# 1. masterから作業ブランチを作成
git checkout master
git pull
git checkout -b feature/作業内容

# 2. 作業してコミット
git add .
git commit -m "変更内容を日本語で書く"

# 3. pushしてPR作成
git push -u origin feature/作業内容
```

ブランチ名の例：
- `feature/player-movement` - 新機能
- `fix/collision-bug` - バグ修正
- `refactor/shader-manager` - リファクタリング

**PR作成後の流れ：**
1. **CodeRabbit** が自動でコードレビュー
2. チームメンバーがレビューを確認して **Approve**
3. マージ（作業ブランチは自動削除）
4. ローカルのmasterを更新：`git checkout master && git pull`

### デザイナー向け（アセット追加）

`assets` ブランチには直接pushできます。[GitHub Desktop](https://desktop.github.com/) を使用してください。

1. **ブランチ切り替え**: 上部の「Current Branch」→ `assets` を選択
2. **最新を取得**: 「Fetch origin」→「Pull origin」
3. **ファイル追加**: エクスプローラーでアセットを追加
4. **コミット**: 左下にメッセージを入力して「Commit to assets」
5. **プッシュ**: 「Push origin」

※ `assets` ブランチは定期的にmasterへマージされます。

### AI（Claude Code / Gemini CLI）にPRを任せる

以下のプロンプトをコピペして使用してください。

**新機能追加：**
```
変更をコミットしてPRを作成してください。
ブランチ名は feature/（機能名）、コミットメッセージとPRタイトルは日本語で。
```

**バグ修正：**
```
変更をコミットしてPRを作成してください。
ブランチ名は fix/（修正内容）、コミットメッセージとPRタイトルは日本語で。
```

**リファクタリング：**
```
変更をコミットしてPRを作成してください。
ブランチ名は refactor/（対象）、コミットメッセージとPRタイトルは日本語で。
```

## チーム

Nihonium113
