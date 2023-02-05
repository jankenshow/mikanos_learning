TARGET = kernel.elf
OBJS = main.o graphics.o font.o hankaku.o newlib_support.o
DEPENDS = $(join $(dir $(OBJS)),$(addprefix .,$(notdir $(OBJS:.o=.d))))

CFLAGS   += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone
# CPPFLAGS += -I${HOME}/workspace/mikanos/osbook/devenv/x86_64-elf/include/c++/v1 \
# 			-I${HOME}/workspace/mikanos/osbook/devenv/x86_64-elf/include \
# 			-I${HOME}/workspace/mikanos/osbook/devenv/x86_64-elf/include/freetype2 \
# 			-I${HOME}/workspace/mikanos/edk2/MdePkg/Include \
# 			-I${HOME}/workspace/mikanos/edk2/MdePkg/Include/X64 \
# 			-nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS \
# 			-DEFIAPI='__attribute__((ms_abi))'
# shortやintなどはbit数が規格で決まっておらず、バグになりうる。
# こういったビット数の固定を行うため、<cstdint>を利用しているが、そのヘッダの場所を`-I`オプションで指定
# データの仕様を確定させるためのABI的な？

CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone \
			-fno-exceptions -fno-rtti -std=c++17
# -O2 					: 最適化
# -Wall 				: 警告
# --target=x86_64-elf	: x86_64向けのコンパイル + ELF形式での出力
# --ffreestanding		: フリースタンディング(=OSがない)環境向けにコンパイル
# -mno-red-zone			: レッドゾーン機能(スタック領域の手前など)を無効化
# -fno-exceptions		: C++の例外機能(OSのサポートが必要)を使わない
# -fno-rtti				: C++の動的型情報(OSのサポートが必要)を使わない
# -c					: コンパイルのみ

LDFLAGS  += --entry KernelMain -z norelro --image-base 0x100000 --static
# --entry KernelMain 	: KernelMain()をエントリポイントとする
# -z norelro 			: リロケーション情報読み込み専用にする機能を使わない
# --image-base 0x100000 : 出力されたバイナリのベースアドレスを0x100000番地にする
# --static				: 静的リンクを行う


.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o

kernel.elf: $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o $@ $(OBJS) -lc

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $<

.%.d: %.cpp
	clang++ $(CPPFLAGS) $(CXXFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.cpp=.o))
	sed -I '' -e 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $<

.%.d: %.c
	clang $(CPPFLAGS) $(CFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.c=.o))
	sed -I '' -e 's|$(notdir $(OBJ))|$(OBJ)|' $@

hankaku.bin: hankaku.txt
	python ../tools/makefont.py -o $@ $<

hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

.%.d: %.bin
	touch $@

.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

-include $(DEPENDS)