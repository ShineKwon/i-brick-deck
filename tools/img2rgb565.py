#!/usr/bin/env python3
# 임의의 이미지를 LCD 에 바로 올릴 수 있는 RGB565 C 헤더로 바꾸는 변환기
#
#   python3 tools/img2rgb565.py <입력이미지> <출력.h> <가로> <세로> [배열이름]
#
# macOS 내장 sips 만 쓰므로 Pillow 같은 외부 의존성이 없다.
# 비율은 잘라내기(aspect-fill)로 맞춘다 — 찌그러뜨리지 않고 가운데를 남긴다.

import os, subprocess, sys, tempfile


def sips(*args):
    r = subprocess.run(["sips", *args], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"sips 실패: {r.stderr.strip()}")
    return r.stdout


def size_of(path):
    out = sips("-g", "pixelWidth", "-g", "pixelHeight", path)
    w = h = None
    for line in out.splitlines():
        if "pixelWidth:" in line:  w = int(line.split(":")[1])
        if "pixelHeight:" in line: h = int(line.split(":")[1])
    if not w or not h:
        sys.exit(f"크기를 읽지 못했다: {path}")
    return w, h


def to_bmp(src, dst, tw, th):
    sw, sh = size_of(src)
    scale = max(tw / sw, th / sh)            # 덮기(cover) — 짧은 변이 목표를 채우게
    rw, rh = max(tw, round(sw * scale)), max(th, round(sh * scale))
    sips("-s", "format", "bmp", src, "--out", dst)
    sips("--resampleHeightWidth", str(rh), str(rw), dst)
    sips("-c", str(th), str(tw), dst)        # 가운데 기준 잘라내기
    return sw, sh, rw, rh


def read_bmp(path):
    d = open(path, "rb").read()
    if d[:2] != b"BM":
        sys.exit("BMP 가 아니다")
    off = int.from_bytes(d[10:14], "little")
    w   = int.from_bytes(d[18:22], "little", signed=True)
    h   = int.from_bytes(d[22:26], "little", signed=True)
    bpp = int.from_bytes(d[28:30], "little")
    if bpp not in (24, 32):
        sys.exit(f"24/32bpp 만 지원한다 (현재 {bpp}bpp)")
    top_down = h < 0
    h = abs(h)
    stride = ((bpp * w + 31) // 32) * 4
    px = []
    for row in range(h):
        src = row if top_down else (h - 1 - row)
        base = off + src * stride
        line = []
        for col in range(w):
            p = base + col * (bpp // 8)
            b, g, r = d[p], d[p + 1], d[p + 2]      # BMP 는 BGR 순서
            line.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
        px.append(line)
    return w, h, px


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__ or "인자가 부족하다")
    src, dst, tw, th = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    name = sys.argv[5] if len(sys.argv) > 5 else "IMG"
    if not os.path.isfile(src):
        sys.exit(f"입력 파일이 없다: {src}")

    with tempfile.TemporaryDirectory() as tmp:
        bmp = os.path.join(tmp, "work.bmp")
        sw, sh, rw, rh = to_bmp(src, bmp, tw, th)
        w, h, px = read_bmp(bmp)

    if (w, h) != (tw, th):
        sys.exit(f"크기가 맞지 않는다: {w}x{h} (기대 {tw}x{th})")

    flat = [v for line in px for v in line]
    with open(dst, "w") as f:
        f.write(f"// {os.path.basename(src)} ({sw}x{sh}) -> {tw}x{th} RGB565, "
                f"{len(flat) * 2:,} 바이트\n")
        f.write("// tools/img2rgb565.py 로 생성. 직접 고치지 말 것.\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write(f"static constexpr int {name}_W = {tw};\n")
        f.write(f"static constexpr int {name}_H = {th};\n")
        f.write(f"const uint16_t {name}_DATA[{len(flat)}] = {{\n")
        for i in range(0, len(flat), 12):
            f.write("  " + ",".join(f"0x{v:04X}" for v in flat[i:i + 12]) + ",\n")
        f.write("};\n")

    uniq = len(set(flat))
    print(f"{src} {sw}x{sh} -> 리샘플 {rw}x{rh} -> 잘라내기 {tw}x{th}")
    print(f"{dst}  {len(flat):,}픽셀  {len(flat)*2:,}바이트  고유색 {uniq:,}개")


if __name__ == "__main__":
    main()
