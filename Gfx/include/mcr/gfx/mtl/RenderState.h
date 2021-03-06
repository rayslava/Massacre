#pragma once

#include <mcr/Types.h>

namespace mcr {
namespace gfx {
namespace mtl {

struct DepthFn
{
    enum Fn: uint
    {
        Never,
        Less,
        Equal,
        LEqual,
        Greater,
        NotEqual,
        GEqual,
        Always,
        NumFns
    }
    fn;

    DepthFn(): fn(Less) {}
    DepthFn(Fn func): fn(func) {}
    explicit DepthFn(uint func): fn(Fn(func)) {}

    operator Fn() const { return fn; }
};

struct BlendFn
{
    enum Factor: uint
    {
        Zero, One,
        SrcColor, OneMinusSrcColor,
        SrcAlpha, OneMinusSrcAlpha,
        DstAlpha, OneMinusDstAlpha,
        DstColor, OneMinusDstColor,
        SrcAlphaSaturate,
        NumFactors
    }
    srcFactor, dstFactor;

    BlendFn(): srcFactor(One), dstFactor(Zero) {}
    BlendFn(Factor src, Factor dst): srcFactor(src), dstFactor(dst) {}
    explicit BlendFn(uint src, uint dst): srcFactor(Factor(src)), dstFactor(Factor(dst)) {}
};

struct RenderState
{
    bool    depthTest, depthWrite;
    DepthFn depthFunc;
    bool    alphaTest;
    bool    blend;
    BlendFn blendFunc;
    bool    cullFace;
    bool    polygonOffset;

    RenderState();
    explicit RenderState(uint hash);

    ~RenderState() {}

    uint hash() const;
};

} // ns mtl
} // ns gfx
} // ns mcr

#include "RenderState.inl"
