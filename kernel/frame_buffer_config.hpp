#pragma once

#include <stdint.h>

// 本OSでサポートするピクセルフォーマットの種類
enum PixelFormat
{
    kPixelRGBResv8BitPerColor,
    kPixelBGRResv8BitPerColor,
};

// ピクセル描画に必要なフレームバッファなどの情報の構造体
struct FrameBufferConfig
{
    uint8_t *frame_buffer;          // フレームバッファの先頭アドレス
    uint32_t pixels_per_scan_line;  // 一行あたりのピクセル数(幅) >= 解像度(横) (横方向に余白がある場合がある)
    uint32_t horizontal_resolution; // 解像度(横)
    uint32_t vertical_resolution;   // 解像度(縦)
    enum PixelFormat pixel_format;  // ピクセル形式
};
