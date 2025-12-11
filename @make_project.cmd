@echo off
::============================================================================
:: @make_project.cmd
:: プロジェクト生成 + Debugビルドを一括実行するスクリプト
::
:: 処理内容:
::   1. UTF-8モード設定 & 作業ディレクトリ移動 (:init)
::   2. Premake5でVisual Studio 2022ソリューション生成 (:generate_project)
::   3. VsDevCmd.batでMSBuild環境をセットアップ (:setup_msbuild)
::   4. MSBuildでDebugビルド実行
::
:: 出力:
::   - build/HEW2026.sln (Visual Studioソリューション)
::   - build/bin/Debug-windows-x86_64/game/game.exe (ゲーム実行ファイル)
::============================================================================
call tools\_common.cmd :init

echo Visual Studio 2022 プロジェクトを生成中...
call tools\_common.cmd :generate_project
if errorlevel 1 exit /b 1

echo.
echo Debug ビルド中...
call tools\_common.cmd :setup_msbuild
if errorlevel 1 exit /b 1

msbuild build\HEW2026.sln -p:Configuration=Debug -p:Platform=x64 -m -v:minimal
if errorlevel 1 (
    echo [ERROR] ビルド失敗
    exit /b 1
)
echo [OK] ビルド成功
