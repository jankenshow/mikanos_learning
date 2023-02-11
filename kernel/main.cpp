#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "console.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"

// 配置new :
// メモリの確保は行わないが、指定したメモリ(buf)上にインスタンスを作成する
void *operator new(size_t size, void *buf) { return buf; }

// これを定義しないとエラーになるらしい (デストラクタが要求)
void operator delete(void *obj) noexcept {}

// edk2で利用しているツールチェイン(CLANGPDB)では、
// 完全仮想関数を呼び出そうとしてしまった場合に呼ばれるエラーハンドラの実装を提供する必要があるそう
extern "C" void __cxa_pure_virtual()
{
    while (1)
        __asm__("hlt");
}

// ピクセル描画クラスのメモリ確保
// (配列によるメモリ確保は言語に元々備わっているため利用できる)
char         pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter *pixel_writer;

// コンソール用のメモリ確保
char     console_buf[sizeof(Console)];
Console *console;

int printk(const char *format, ...)
{
    va_list ap;
    int     result;
    char    s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
};

// C++独自の参照渡し(参照型)で関数を定義しているが、C言語から呼び出す場合は
// ポインタを指定すればOK. (System V AMD64 ABI(コンパイラ)の仕様で決まっている)
// ABI = プログラム(関数など=呼出規約, Calling
// Convention)が動作するにあたり、必要なレジスタやメモリの使い方を定義したもの
// コンパイラはこのABIに従って機械語を生成する。
extern "C" void KernelMain(const FrameBufferConfig &frame_buffer_config)
{
    // ピクセルの形式で描画クラスを変更する (ポリモフィズム)
    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            // OSの機能が使えないため？C++のコンストラクタ呼び出しができない
            // (通常のnewも同様) そのため配置newを利用
            pixel_writer = new (pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new (pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
    }

    // ピクセル(背景)の描画
    for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
        for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
            pixel_writer->Write(x, y, {255, 255, 255});
        }
    }

    // コンソールクラスの初期化
    console = new (console_buf) Console{*pixel_writer, {0, 0, 0}, {255, 255, 255}};

    // コンソールへの描画
    for (int i = 0; i < 27; ++i) {
        printk("printk: %d\n", i);
    }

    while (1)
        // アセンブリを直接呼び出した方(インラインアセンブリ)が待機中のCPU使用率を節約できる
        // ただし、ニーモニックは GNU Assembly の文法でしか書けない
        __asm__("hlt");
}
