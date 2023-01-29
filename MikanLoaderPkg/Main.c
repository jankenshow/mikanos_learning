#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>
#include "frame_buffer_config.hpp"
#include "elf.hpp"

// memory mapの構造
struct MemoryMap
{
    UINTN buffer_size;         // 割り当てられたバッファーのサイズ
    VOID *buffer;              // 割り当てられたバッファーの先頭アドレス
    UINTN map_size;            // 得られたmemory mapmのサイズ
    UINTN map_key;             // memory mapの(時系列)識別用ID
    UINTN descriptor_size;     // メモリマップの個々の行を表すディスクリプタのバイト数
    UINT32 descriptor_version; // メモリディスクリプタ構造体のバージョン
};

// メモリマップを得るための関数。
EFI_STATUS GetMemoryMap(struct MemoryMap *map)
{
    if (map->buffer == NULL)
    {
        return EFI_BUFFER_TOO_SMALL;
    }

    map->map_size = map->buffer_size;
    // ブートサービスを表すグローバル変数から、メモリマップを取得 (ランタイムサービスは gRT)
    return gBS->GetMemoryMap(
        &map->map_size,                       // (IN OUT) メモリマップ書き込み用のメモリ領域の大きさ
        (EFI_MEMORY_DESCRIPTOR *)map->buffer, // (IN OUT) メモリマップ書き込み用メモリ領域先頭アドレス
        &map->map_key,                        // (OUT) memory mapの(時系列)識別用ID
        &map->descriptor_size,                // (OUT) メモリマップの個々の行を表すディスクリプタのバイト数
        &map->descriptor_version);            // (OUT) メモリディスクリプタ構造体のバージョン
}

// ディスクリプタ自体の構造はUEFIの規格にある。(edk2/MdePkg/Include/Uefi/UefiSpec.h)
// UINT32                  Type            : メモリ領域の種別
// EFI_PHYSICAL_ADDRESS    PhysicalStart   : メモリ領域先頭の物理メモリアドレス
// EFI_VIRTUAL_ADDRESS     VirtualStart    : メモリ領域先頭の仮想メモリアドレス
// UINT64                  NumberOfPages   : メモリ領域の大きさ (4KiBページ単位 - 設定次第ではあるが、、、)
// UINT64                  Attribute       : メモリ領域が使える用途を示すビット集合

// メモリディスクリプタ種類(メモリ領域の種別)をテキスト化する関数
const CHAR16 *GetMemoryTypeUnicode(EFI_MEMORY_TYPE type)
{
    switch (type)
    {
    case EfiReservedMemoryType:
        return L"EfiReservedMemoryType";
    case EfiLoaderCode:
        return L"EfiLoaderCode"; // UEFIアプリケーションの実行コード
    case EfiLoaderData:
        return L"EfiLoaderData"; // UEFIアプリケーションが使うデータ領域
    case EfiBootServicesCode:
        return L"EfiBootServicesCode"; // ブートサービスドライバの実行コード
    case EfiBootServicesData:
        return L"EfiBootServicesData"; // ブートサービスドライバが使うデータ領域
    case EfiRuntimeServicesCode:
        return L"EfiRuntimeServicesCode"; // ランタイムサービスの実行コード
    case EfiRuntimeServicesData:
        return L"EfiRuntimeServicesData"; // ランタイムサービスが使うデータ領域
    case EfiConventionalMemory:
        return L"EfiConventionalMemory"; // 空き領域
    case EfiUnusableMemory:
        return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory:
        return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS:
        return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO:
        return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace:
        return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode:
        return L"EfiPalCode";
    case EfiPersistentMemory:
        return L"EfiPersistentMemory";
    case EfiMaxMemoryType:
        return L"EfiMaxMemoryType";
    default:
        return L"InvalidMemoryType";
    }
}

// 取得したメモリマップをファイルに保存する関数
EFI_STATUS SaveMemoryMap(struct MemoryMap *map, EFI_FILE_PROTOCOL *file)
{
    EFI_STATUS status;
    CHAR8 buf[256]; // ファイル書き込み用テキストを一時的に保存する変数
    UINTN len;      // 書き込むバイト数を一時的に保存する変数

    // ヘッダ保存の書き込み
    CHAR8 *header =
        "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    status = file->Write(file, &len, header);
    if (EFI_ERROR(status))
    {
        return status;
    }

    Print(L"map->buffer = %08lx, map->map_size = %08lx\n",
          map->buffer, map->map_size);

    // 取得したメモリマップを保存
    EFI_PHYSICAL_ADDRESS iter;
    int i;
    // メモリマップを書き込んだバッファに関して、全てのディスクリプタ(行)をイテレーションする
    for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
         iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
         iter += map->descriptor_size, i++)
    {
        // 型変換
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)iter;
        // 個々のメモリディスクリプタをテキスト化
        len = AsciiSPrint(
            buf, sizeof(buf),
            "%u, %x, %-ls, %08lx, %lx, %lx\n",
            i, desc->Type, GetMemoryTypeUnicode(desc->Type),
            desc->PhysicalStart, desc->NumberOfPages,
            desc->Attribute & 0xffffflu);
        // 書き込み
        status = file->Write(file, &len, buf);
        if (EFI_ERROR(status))
        {
            return status;
        }
    }

    return EFI_SUCCESS;
}

// ファイル操作プロトコル?(rootディレクトリ)を開くための関数
// (rootディレクトリを基準としたファイル操作プロトコルのポインタを返すため、ポインタのポインタを受け取る。)
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL **root)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;

    status = gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status))
    {
        return status;
    }

    return fs->OpenVolume(fs, root);
}

// グラフィック操作プロトコルの取得
EFI_STATUS OpenGOP(EFI_HANDLE image_handle,
                   EFI_GRAPHICS_OUTPUT_PROTOCOL **gop)
{
    EFI_STATUS status;
    UINTN num_gop_handles = 0;
    EFI_HANDLE *gop_handles = NULL;

    status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        &num_gop_handles,
        &gop_handles);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->OpenProtocol(
        gop_handles[0],
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **)gop,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status))
    {
        return status;
    }

    FreePool(gop_handles);

    return EFI_SUCCESS;
}
// gopを介して、フレームバッファの先頭アドレスや、解像度・非表示領域を含めた幅、
// 1ピクセルのデータ形式などを取得できる。

// 1ピクセルのデータ形式をテキストに変換する関数
const CHAR16 *GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt)
{
    switch (fmt)
    {
    case PixelRedGreenBlueReserved8BitPerColor:
        return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
        return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
        return L"PixelBitMask";
    case PixelBltOnly:
        return L"PixelBltOnly";
    case PixelFormatMax:
        return L"PixelFormatMax";
    default:
        return L"InvalidPixelFormat";
    }
}

// c言語からアセンブリの`hlt`を恒久的に実行するための関数 (ただwhile loopするだけよりcpu使用率が低い)
void Halt(void)
{
    while (1)
        __asm__("hlt");
}

// elfファイル(構造体)について、LOADセグメントに記載の仮想アドレスのと先頭と末尾を計算する関数
void CalcLoadAddressRange(Elf64_Ehdr *ehdr, UINT64 *first, UINT64 *last)
{
    // ELFファイルバッファのアドレスと、プログラムヘッダのオフセットを利用して、
    // プログラムヘッダ(構造体)の先頭アドレスを取得
    Elf64_Phdr *phdr = (Elf64_Phdr *)((UINT64)ehdr + ehdr->e_phoff);
    *first = MAX_UINT64;
    *last = 0;
    // プログラムヘッダ内の要素数でループを回して、LOADセグメントの先頭・末尾仮想アドレスを取得
    for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        // 先頭は、LOADセグメント仮想アドレスの中で一番小さいもの
        *first = MIN(*first, phdr[i].p_vaddr);
        // 末尾は、LOADセグメント仮想アドレス + LOADセグメントのメモリ上サイズ で一番大きいもの
        *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
    }
}

// elfファイル(構造体)について、LOADセグメントの内容から必要な情報を仮想アドレスにコピーする関数
void CopyLoadSegments(Elf64_Ehdr *ehdr)
{
    // プログラムヘッダの取得
    Elf64_Phdr *phdr = (Elf64_Phdr *)((UINT64)ehdr + ehdr->e_phoff);
    for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i)
    {
        // ロード領域以外はスルー
        if (phdr[i].p_type != PT_LOAD)
            continue;

        // 一時的に保存したファイルデータ(のLOADセグメント)を仮想アドレスの場所にコピーする
        UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
        CopyMem((VOID *)phdr[i].p_vaddr, (VOID *)segm_in_file, phdr[i].p_filesz);

        // セグメントのメモリ上のサイズが、ファイル上のサイズより大きい場合、メモリの余白部分について、0で埋める
        UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
        SetMem((VOID *)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
    }
}

// UEFIのアプリケーションとして実行(エントリポイント)
EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle,
                           EFI_SYSTEM_TABLE *system_table)
{
    EFI_STATUS status;

    Print(L"Hello, Mikan World!\n");

    // メモリマップの取得
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    status = GetMemoryMap(&memmap);
    if (EFI_ERROR(status))
    {
        Print(L"failed to get memory map: %r\n", status);
        Halt();
    }

    // ファイル操作プロトコルの取得
    EFI_FILE_PROTOCOL *root_dir;
    status = OpenRootDir(image_handle, &root_dir);
    if (EFI_ERROR(status))
    {
        Print(L"failed to open root directory: %r\n", status);
        Halt();
    }

    // メモリマップを書き込むためのファイルを開く(なければ作成)
    EFI_FILE_PROTOCOL *memmap_file;
    status = root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(status))
    {
        Print(L"failed to save memory map: %r\n", status);
        Halt();
    }
    // 後片付け(開いたファイルを閉じる)
    status = memmap_file->Close(memmap_file);
    if (EFI_ERROR(status))
    {
        Print(L"failed to close memory map: %r\n", status);
        Halt();
    }

    // グラフィック操作プロトコルの取得
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    status = OpenGOP(image_handle, &gop);
    if (EFI_ERROR(status))
    {
        Print(L"failed to open GOP: %r\n", status);
        Halt();
    }

    // グラフィック情報の取得
    Print(L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n",
          gop->Mode->Info->HorizontalResolution,
          gop->Mode->Info->VerticalResolution,
          GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
          gop->Mode->Info->PixelsPerScanLine);
    Print(L"Frame Buffer: 0x%0lx - 0x%0lx, Size: %lu bytes\n",
          gop->Mode->FrameBufferBase,
          gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
          gop->Mode->FrameBufferSize);

    // ローダから画面描画(白で塗りつぶす)
    UINT8 *frame_buffer = (UINT8 *)gop->Mode->FrameBufferBase;
    for (UINTN i = 0; i < gop->Mode->FrameBufferSize; ++i)
    {
        frame_buffer[i] = 255;
    }

    // カーネルファイルの読み込み
    EFI_FILE_PROTOCOL *kernel_file;
    status = root_dir->Open(
        root_dir, &kernel_file, L"\\kernel.elf",
        EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        Print(L"failed to open file '\\kernel.elf': %r\n", status);
        Halt();
    }

    // 読み込んだELFファイルのファイル情報について、ファイル名を含めたバイト数を計算し、
    // そのサイズでファイル情報を書き込むためのバッファを作成
    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
    UINT8 file_info_buffer[file_info_size];
    // ファイル情報バッファにファイル情報を書き込む
    status = kernel_file->GetInfo(
        kernel_file, &gEfiFileInfoGuid,
        &file_info_size, file_info_buffer);
    if (EFI_ERROR(status))
    {
        Print(L"failed to get file information: %r\n", status);
        Halt();
    }

    // ELFファイル情報バッファの型変換と、正確なファイルサイズの取得
    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *)file_info_buffer;
    UINTN kernel_file_size = file_info->FileSize;

    // カーネルファイルを一時的に保存するELFファイルバッファの確保
    VOID *kernel_buffer;
    // AllocatePageはページ単位でのメモリ確保、AllocatePoolはバイト単位でのメモリ確保
    status = gBS->AllocatePool(EfiLoaderData, kernel_file_size, &kernel_buffer);
    if (EFI_ERROR(status))
    {
        Print(L"failed to allocate pool: %r\n", status);
        Halt();
    }
    // カーネルファイルを一時ELFファイルバッファへ書き込む
    status = kernel_file->Read(kernel_file, &kernel_file_size, kernel_buffer);
    if (EFI_ERROR(status))
    {
        Print(L"error: %r\n", status);
        Halt();
    }

    // 一時ELFファイルバッファにあるファイルからヘッダ情報やLOADセグメント情報の取得
    // (ELFファイル先頭アドレスをファイルヘッダ構造体として変換)
    Elf64_Ehdr *kernel_ehdr = (Elf64_Ehdr *)kernel_buffer;
    // LOADセグメントから、ELFファイルが配備されるべき仮想アドレスの先頭と末尾(範囲)を取得
    UINT64 kernel_first_addr, kernel_last_addr;
    CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

    // ページ数の計算 (4KiB単位で換算)
    UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                                num_pages, &kernel_first_addr);
    if (EFI_ERROR(status))
    {
        Print(L"failed to allocate pages: %r\n", status);
        Halt();
    }

    // LOADセグメントのコピー
    CopyLoadSegments(kernel_ehdr);
    Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

    // 後片付け
    // 一時ELFファイルバッファの解放
    status = gBS->FreePool(kernel_buffer);
    if (EFI_ERROR(status))
    {
        Print(L"failed to free pool: %r\n", status);
        Halt();
    }
    // kernel_fileを閉じるとうまくいかない？
    // status = kernel_file->Close(kernel_file);
    // if (EFI_ERROR(status))
    // {
    //     Print(L"failed to close kernel_file: %r\n", status);
    //     Halt();
    // }

    // ブートサービスの終了
    // メモリマップに変更があると失敗する (memmap.map_keyで判断)
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if (EFI_ERROR(status))
    {
        // 再度メモリマップを取得する
        status = GetMemoryMap(&memmap);
        if (EFI_ERROR(status))
        {
            Print(L"failed to get memory map: %r\n", status);
            Halt();
        }
        // 再度ブートサービスを終了させる
        status = gBS->ExitBootServices(image_handle, memmap.map_key);
        if (EFI_ERROR(status))
        {
            Print(L"Could not exit boot service: %r\n", status);
            Halt();
        }
    }

    // コピー先の先頭仮想アドレスから、カーネルのエントリポイントを取得する
    // エントリポイントのアドレスはELF headerに格納されており、ベースアドレス(ファイルの先頭)を基準として
    // オフセット24バイトの位置から8バイト整数として書かれている。
    UINT64 entry_addr = *(UINT64 *)(kernel_first_addr + 24);

    // カーネルに渡す、フレームバッファコンフィグの取得
    struct FrameBufferConfig config = {
        (UINT8 *)gop->Mode->FrameBufferBase,
        gop->Mode->Info->PixelsPerScanLine,
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution,
        0};
    switch (gop->Mode->Info->PixelFormat)
    {
    case PixelRedGreenBlueReserved8BitPerColor:
        config.pixel_format = kPixelRGBResv8BitPerColor;
        break;
    case PixelBlueGreenRedReserved8BitPerColor:
        config.pixel_format = kPixelBGRResv8BitPerColor;
        break;
    default:
        Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
        Halt();
    }

    // エントリポイントの型定義と変換
    // (mac + edk2 ではCLANGPDB(Microsoft x64 ABI)でこのローダをビルドするように設定しているので、
    // この関数に関しては、ABIをSystem V AMD64 ABIに変更してビルドするように設定)
    typedef void __attribute__((sysv_abi)) EntryPointType(const struct FrameBufferConfig *);
    EntryPointType *entry_point = (EntryPointType *)entry_addr;
    // エントリポイントの実行
    entry_point(&config);

    // ここから先はあまり意味ない
    Print(L"All done\n");

    while (1)
        ;
    return EFI_SUCCESS;
}