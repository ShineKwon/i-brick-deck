#!/usr/bin/env python3
# 키 배경용 치비 캐릭터를 코드로 렌더링한다 (외부 의존성 없음)
#
#   python3 tools/make_girl.py <출력.bmp> [미리보기.png]
#
# 4배로 그린 뒤 박스 다운샘플해서 안티에일리어싱을 얻는다.
# 그라데이션을 많이 써서 RGB565 색 평가에도 쓸 수 있게 했다.

import math, struct, sys, zlib

W, H, S = 148, 128, 4          # 최종 크기와 슈퍼샘플 배율
BW, BH = W * S, H * S
CORNER_R = 10                  # 키 라운드 반지름
BG_RGB = (24, 28, 24)          # 모서리를 채울 화면 배경색 (C_BG 0x18E3)

buf = bytearray(BW * BH * 3)


def put(x, y, c, a=1.0):
    if x < 0 or y < 0 or x >= BW or y >= BH:
        return
    i = (y * BW + x) * 3
    if a >= 0.999:
        buf[i], buf[i+1], buf[i+2] = c
    else:
        for k in range(3):
            buf[i+k] = int(buf[i+k] * (1 - a) + c[k] * a)


def ellipse(cx, cy, rx, ry, c, a=1.0, rot=0.0):
    cx, cy, rx, ry = cx*S, cy*S, rx*S, ry*S
    co, si = math.cos(-rot), math.sin(-rot)
    rad = int(max(rx, ry)) + 2
    for y in range(int(cy - rad), int(cy + rad) + 1):
        for x in range(int(cx - rad), int(cx + rad) + 1):
            dx, dy = x - cx, y - cy
            u, v = dx * co - dy * si, dx * si + dy * co
            if (u / rx) ** 2 + (v / ry) ** 2 <= 1.0:
                put(x, y, c, a)


def rect(x0, y0, x1, y1, c, a=1.0):
    for y in range(int(y0*S), int(y1*S)):
        for x in range(int(x0*S), int(x1*S)):
            put(x, y, c, a)


def lerp(c0, c1, t):
    return tuple(int(c0[k] + (c1[k] - c0[k]) * t) for k in range(3))


# ── 배경 — 세로 그라데이션 + 깅엄 체크 ──────────────────────────────────────
TOP, BOT = (214, 234, 252), (243, 249, 255)
for y in range(BH):
    c = lerp(TOP, BOT, y / (BH - 1))
    for x in range(BW):
        put(x, y, c)

CHK = (176, 210, 240)
cell = 9 * S
for y in range(BH):
    for x in range(BW):
        gx, gy = (x // cell) & 1, (y // cell) & 1
        if gx and gy:   put(x, y, CHK, 0.30)
        elif gx or gy:  put(x, y, CHK, 0.15)

# 장식 — 하트와 별
HEART, STAR = (255, 168, 190), (255, 214, 102)
ellipse(16, 19, 5, 5, HEART); ellipse(24, 19, 5, 5, HEART)
ellipse(20, 25, 7.5, 7.5, HEART, rot=math.radians(45))
for i in range(5):
    ang = math.radians(-90 + i * 72)
    ellipse(130 + 5*math.cos(ang), 20 + 5*math.sin(ang), 4, 4, STAR)
ellipse(130, 20, 5, 5, STAR)

# ── 후드 토끼 귀 ────────────────────────────────────────────────────────────
HOOD, HOOD_SH, EAR_IN = (252, 252, 255), (218, 228, 242), (255, 194, 208)
ellipse(35, 15, 13, 27, HOOD_SH, rot=math.radians(-16))
ellipse(113, 15, 13, 27, HOOD_SH, rot=math.radians(16))
ellipse(35, 14, 11, 25, HOOD,    rot=math.radians(-16))
ellipse(113, 14, 11, 25, HOOD,   rot=math.radians(16))
ellipse(35, 12, 5.5, 15, EAR_IN, rot=math.radians(-16))
ellipse(113, 12, 5.5, 15, EAR_IN, rot=math.radians(16))

# ── 후드 본체 ───────────────────────────────────────────────────────────────
ellipse(74, 48, 54, 43, HOOD_SH)
ellipse(74, 46, 52, 41, HOOD)
ellipse(74, 52, 45, 35, (241, 245, 253))      # 안쪽 그늘

# ── 머리카락 ────────────────────────────────────────────────────────────────
HAIR, HAIR_HI = (88, 60, 50), (140, 102, 84)
ellipse(74, 66, 35, 36, HAIR)                 # 뒷머리
ellipse(49, 72, 10, 24, HAIR)                 # 옆머리 — 후드 밖으로 흘러내림
ellipse(99, 72, 10, 24, HAIR)

# ── 얼굴 ────────────────────────────────────────────────────────────────────
SKIN, SKIN_SH = (255, 228, 212), (245, 200, 184)
ellipse(74, 65, 30, 31, SKIN_SH)
ellipse(74, 64, 29, 30, SKIN)

# ── 앞머리 ──────────────────────────────────────────────────────────────────
ellipse(74, 42, 30, 14, HAIR)
ellipse(58, 47, 10, 12, HAIR)                 # 갈라진 앞머리
ellipse(90, 47, 10, 12, HAIR)
ellipse(74, 38, 19, 7, HAIR_HI, 0.50)         # 앞머리 광택
ellipse(74, 57, 14, 5, SKIN, 0.85)            # 이마를 살짝 드러낸다

# ── 눈 ──────────────────────────────────────────────────────────────────────
def eye(ex):
    ellipse(ex, 67, 10.5, 12.5, (255, 255, 255))
    for i in range(9):
        t = i / 8
        ellipse(ex, 67 + t*1.6, 8.8 - t*2.3, 10.8 - t*2.7,
                lerp((58, 38, 32), (176, 118, 82), t))
    ellipse(ex, 65.5, 4.4, 5.2, (36, 23, 21))
    ellipse(ex - 3.2, 62, 3.6, 3.6, (255, 255, 255))
    ellipse(ex + 3.2, 71, 1.9, 1.9, (255, 255, 255), 0.85)
    ellipse(ex, 56.3, 11.0, 3.3, HAIR)        # 윗 속눈썹
eye(61); eye(87)

# ── 볼·입 ───────────────────────────────────────────────────────────────────
BLUSH = (255, 152, 164)
ellipse(46, 76, 7.5, 4.5, BLUSH, 0.55)
ellipse(102, 76, 7.5, 4.5, BLUSH, 0.55)
ellipse(74, 82, 5.5, 4.0, (206, 100, 104))
ellipse(74, 80.2, 5.5, 2.8, SKIN)             # 윗입술로 덮어 미소 모양

# ── 목·어깨 ─────────────────────────────────────────────────────────────────
CLOTH, CLOTH_SH, COLLAR = (166, 205, 245), (132, 178, 226), (236, 244, 255)
ellipse(74, 95, 11, 9, SKIN_SH)               # 목
ellipse(74, 128, 58, 27, CLOTH_SH)
ellipse(74, 130, 54, 25, CLOTH)
ellipse(74, 106, 13, 6, CLOTH_SH)             # 목 아래 그늘

# ── 라벨이 읽히도록 아래쪽 스크림 ───────────────────────────────────────────
for y in range(int(96*S), BH):
    t = (y - 96*S) / (BH - 96*S)
    a = 0.55 * t * t
    for x in range(BW):
        put(x, y, (16, 20, 26), a)

# ── 다운샘플 ────────────────────────────────────────────────────────────────
out = bytearray(W * H * 3)
n = S * S
for y in range(H):
    for x in range(W):
        r = g = b = 0
        for dy in range(S):
            row = (y*S + dy) * BW
            for dx in range(S):
                i = (row + x*S + dx) * 3
                r += buf[i]; g += buf[i+1]; b += buf[i+2]
        o = (y*W + x)*3
        out[o], out[o+1], out[o+2] = r//n, g//n, b//n

# ── 라운드 모서리를 배경색으로 마스킹 ───────────────────────────────────────
for y in range(H):
    for x in range(W):
        cx = CORNER_R if x < CORNER_R else (W-1-CORNER_R if x > W-1-CORNER_R else x)
        cy = CORNER_R if y < CORNER_R else (H-1-CORNER_R if y > H-1-CORNER_R else y)
        if math.hypot(x-cx, y-cy) > CORNER_R:
            o = (y*W + x)*3
            out[o], out[o+1], out[o+2] = BG_RGB

# ── 저장 ────────────────────────────────────────────────────────────────────
def write_bmp(path):
    rows = []
    for y in range(H-1, -1, -1):
        r = bytearray()
        for x in range(W):
            o = (y*W + x)*3
            r += bytes((out[o+2], out[o+1], out[o]))   # BGR
        r += b"\x00" * ((-len(r)) % 4)
        rows.append(bytes(r))
    px = b"".join(rows)
    hdr = b"BM" + struct.pack("<IHHI", 14+40+len(px), 0, 0, 14+40)
    info = struct.pack("<IiiHHIIiiII", 40, W, H, 1, 24, 0, len(px), 2835, 2835, 0, 0)
    open(path, "wb").write(hdr + info + px)


def write_png(path, zoom=3):
    zw, zh = W*zoom, H*zoom
    raw = bytearray()
    for y in range(zh):
        raw.append(0)
        sy = y // zoom
        for x in range(zw):
            o = (sy*W + x//zoom)*3
            raw += bytes((out[o], out[o+1], out[o+2]))
    def chunk(t, d):
        c = t + d
        return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", zw, zh, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)


write_bmp(sys.argv[1])
if len(sys.argv) > 2:
    write_png(sys.argv[2])
print(f"{W}x{H} 렌더 완료 -> {sys.argv[1]}" + (f", 미리보기 {sys.argv[2]}" if len(sys.argv) > 2 else ""))
