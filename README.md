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
├── dx11/     # DirectX 11 ラッパーライブラリ
├── engine/   # ゲームエンジン層
└── game/     # ゲーム本体
```

## チーム

Nihonium113
