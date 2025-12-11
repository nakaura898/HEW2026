@echo off
::============================================================================
:: @cleanup.cmd
:: ビルド成果物を削除してクリーンな状態に戻すスクリプト
::
:: 削除対象:
::   - build/  : VS2022ソリューション、bin/、obj/ を含む全ビルド成果物
::
:: 用途:
::   完全な再ビルドが必要な場合や、リポジトリをクリーンにしたい場合
::============================================================================
chcp 65001 >nul
cd /d "%~dp0"
echo Cleaning build artifacts...
if exist "build" rmdir /s /q "build" && echo   Removed: build/
echo [OK] Clean complete
