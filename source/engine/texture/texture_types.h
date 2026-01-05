//----------------------------------------------------------------------------
//! @file   texture_types.h
//! @brief  テクスチャ関連型定義（engine層からの再エクスポート）
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu/texture.h"

// dx11層の型をengine層から再エクスポート
// game層はこのヘッダーをincludeすることで、dx11への直接依存を避ける
using TexturePtr = std::shared_ptr<Texture>;
using TextureWeakPtr = std::weak_ptr<Texture>;
