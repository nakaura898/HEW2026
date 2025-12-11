@echo off
::============================================================================
:: build_release.cmd
:: Releaseビルドのみを実行するスクリプト
::
:: 前提条件:
::   - build/HEW2026.sln が存在すること (@make_project.cmd で生成)
::
:: 処理内容:
::   1. UTF-8モード設定 & 作業ディレクトリ移動 (:init)
::   2. ソリューション存在確認 (:check_project)
::   3. VsDevCmd.batでMSBuild環境をセットアップ (:setup_msbuild)
::   4. MSBuildでReleaseビルド実行
::
:: 出力:
::   - bin/Release-windows-x64/game/game.exe
::   - bin/Release-windows-x64/tests/tests.exe
::============================================================================
call tools\_common.cmd :init
call tools\_common.cmd :check_project || exit /b 1
call tools\_common.cmd :setup_msbuild || exit /b 1

echo Release ビルド中...
msbuild build\HEW2026.sln -p:Configuration=Release -p:Platform=x64 -m -v:minimal
if errorlevel 1 (
    echo [ERROR] ビルド失敗
    exit /b 1
)
echo [OK] ビルド成功
