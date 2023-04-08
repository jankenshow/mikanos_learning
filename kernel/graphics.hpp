#pragma once

#include "frame_buffer_config.hpp"

struct PixelColor
{
    uint8_t r, g, b;
};

// フレームバッファを介して、ピクセル描画を行うベースクラス
class PixelWriter {
  public:
    PixelWriter(const FrameBufferConfig &config);
    virtual ~PixelWriter() = default;
    virtual void Write(int x, int y,
                       const PixelColor &c) = 0;    // 純粋仮想関数 (オーバーライドされないとエラーになる)

  protected:
    // 指定された座標のピクセルに関して、フレームバッファ上のアドレスを返す
    // 一つのピクセルあたり4バイトの大きさをもつ。
    uint8_t *PixelAt(int x, int y);

  private:
    const FrameBufferConfig &config_;
};

// RGB形式のピクセルフォーマットに対する、ピクセル描画クラス
class RGBResv8BitPerColorPixelWriter : public PixelWriter {
  public:
    using PixelWriter::PixelWriter;

    // override はつけなくても動くが、わかりやすくなるのでつけた方が良い
    virtual void Write(int x, int y, const PixelColor &c) override;
};

// BGR形式のピクセルフォーマットに対する、ピクセル描画クラス
class BGRResv8BitPerColorPixelWriter : public PixelWriter {
  public:
    using PixelWriter::PixelWriter;

    virtual void Write(int x, int y, const PixelColor &c) override;
};

template <typename T> struct Vector2D
{
    T x, y;

    template <typename U> Vector2D<T> &operator+=(const Vector2D<U> &rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
};

void DrawRectangle(PixelWriter &writer, const Vector2D<int> &pos, const Vector2D<int> &size, const PixelColor &c);
void FillRectangle(PixelWriter &writer, const Vector2D<int> &pos, const Vector2D<int> &size, const PixelColor &c);
