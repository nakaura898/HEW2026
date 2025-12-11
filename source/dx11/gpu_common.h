//----------------------------------------------------------------------------
//! @file   gpu_common.h
//! @brief  DirectX11汎用クラス 共通ヘッダー
//! @note   全てのgpu関連ヘッダーファイルがこのファイルをインクルードします
//----------------------------------------------------------------------------
#pragma once

//--------------------------------------------------------------
// DirectX11関連
//--------------------------------------------------------------
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>

//--------------------------------------------------------------
// COM管理
//--------------------------------------------------------------
#include <wrl/client.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

//--------------------------------------------------------------
// 標準ライブラリ
//--------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include <string>
#include <cassert>

//--------------------------------------------------------------
// ユーティリティ
//--------------------------------------------------------------
#include "common/utility/non_copyable.h"

//--------------------------------------------------------------
// ヘルパー関数
//--------------------------------------------------------------

//! D3D11_USAGEからCPUアクセスフラグを自動推論
//! @param [in] usage 使用法
//! @return CPUアクセスフラグ
inline D3D11_CPU_ACCESS_FLAG GetCpuAccessFlags(D3D11_USAGE usage) noexcept
{
    switch (usage) {
        case D3D11_USAGE_DYNAMIC:
            return D3D11_CPU_ACCESS_WRITE;
        case D3D11_USAGE_STAGING:
            return static_cast<D3D11_CPU_ACCESS_FLAG>(
                D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE
            );
        case D3D11_USAGE_IMMUTABLE:
        case D3D11_USAGE_DEFAULT:
        default:
            return static_cast<D3D11_CPU_ACCESS_FLAG>(0);
    }
}

