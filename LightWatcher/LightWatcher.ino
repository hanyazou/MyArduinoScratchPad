#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_APDS9960.h>

//#define SHOW_COLOR_NANME
//#define SHOW_NEW_LINE

#include "LedTypes.h"

// ==== センサー関連 ====
SparkFun_APDS9960 apds;

bool readRGB(uint16_t &r, uint16_t &g, uint16_t &b, uint16_t &c) {
  if (!apds.readRedLight(r))   return false;
  if (!apds.readGreenLight(g)) return false;
  if (!apds.readBlueLight(b))  return false;
  if (!apds.readAmbientLight(c)) return false;
  return true;
}

float bg_level = 0.0f;          // 環境光のベース（max(R,G,B)）
const float BG_ALPHA = 0.02f;   // 環境光を追従するスピード（小さいほどゆっくり）

// ヒステリシス用のしきい値（要調整）
const float TH_ON  = 400.0f;
const float TH_OFF = 250.0f;

// ==== ユーティリティ ====

// ANSI エスケープ: 色コードを返す（背景色）
const char *ansiColorBgClass(LedClass c) {
  // 16 色しか表示できない端末も考慮して、16 色の範囲内で選択
  switch (c) {
    case LedClass::Red:     return "\x1b[0;101m"; // 赤（高輝度）
    case LedClass::Green:   return "\x1b[0;102m"; // 緑（高輝度）
    case LedClass::Yellow:  return "\x1b[0;103m"; // 黄（高輝度）
    case LedClass::Blue:    return "\x1b[44m";    // 青
    case LedClass::Magenta: return "\x1b[0;105m"; // マゼンタ（高輝度）
    case LedClass::Cyan:    return "\x1b[0;106m"; // シアン（高輝度）
    case LedClass::Orange:  return "\x1b[43m";    // オレンジは黄（低輝度）で近似
    case LedClass::White:   return "\x1b[0;107m"; // 白（高輝度）
    case LedClass::Off:
    default:                return "\x1b[0;100m"; // 黒 / ほぼ見えない
  }
}

// ANSI エスケープ: 色コードを返す（背景色）
const char *ansiColorBgRGB(float r, float g, float b) {
  float maxc = max(r, max(g, b));
  if (maxc <= 0.0f) {
    return ansiColorBgRGB(0.0, 0.0, 0.0);
  }
  // R, G, B 各 0 ~ 5
  int rn = (r / maxc) * 5.5;
  int gn = (g / maxc) * 5.5;
  int bn = (b / maxc) * 5.5;
  static char seq[16];
  snprintf(seq, sizeof(seq), "\x1b[48;5;%dm", rn * 36 + gn * 6 + bn + 16);
  return seq;
}

// 色分類（線形っぽいルールベース）
// r, g, b: 各 0〜1 の正規化値
LedClass classifyColor(float r, float g, float b) {
  // 一応、最大成分で正規化しておく（多少の誤差に強くするため）
  float maxc = max(r, max(g, b));
  if (maxc <= 0.0f) {
    return LedClass::Off;
  }
  float rn = r / maxc;
  float gn = g / maxc;
  float bn = b / maxc;

  // --- R が最大のとき: RED / ORANGE / YELLOW を分ける ---
  if (rn >= gn && rn >= bn) {
    float rg = (rn > 0.0f) ? (gn / rn) : 0.0f; // G/R 比
    float rb = (rn > 0.0f) ? (bn / rn) : 0.0f; // B/R 比

    bool b_small = (rb < 0.4f);

    if (b_small) {
      if (rg < 0.20f) {
        // G がほとんど無い → 赤
        return LedClass::Red;
      } else if (rg < 0.99f) {
        // ★ 0.70 → 0.90 に拡大
        // G がそこそこ〜かなりあるが、Yellow と言い切るほどではない → オレンジ寄りに倒す
        return LedClass::Orange;
      } else {
        // R≒G かそれ以上 → 本当に黄色っぽいときだけ Yellow
        return LedClass::Yellow;
      }
    } else {
      // B もそこそこある → 白っぽい or マゼンタっぽい
      if (0.6f < rb) {
        return LedClass::White;
      } else {
        return LedClass::Yellow; // ここは保険的に Yellow 扱いのままでOK
      }
    }
  }

  // --- G が最大のとき: GREEN / YELLOW / CYAN あたり ---
  if (gn >= rn && gn >= bn) {
    float gr = (gn > 0.0f) ? (rn / gn) : 0.0f; // R/G 比
    float gb = (gn > 0.0f) ? (bn / gn) : 0.0f; // B/G 比

    // ★ Cyan 判定はかなり強い条件にする
    bool strong_cyan = (gb > 0.70f && gr < 0.20f); // G ≒ B かつ R がかなり小さいときだけ

    if (strong_cyan) {
      return LedClass::Cyan;
    }

    // それ以外は Green / Yellow 側に寄せる
    bool b_small = (gb < 0.60f);  // ★ 0.4 → 0.6 に緩めて「少し青い緑」も Green 側へ

    if (b_small) {
      if (gr > 0.70f) {
        // R ≒ G, B はそこまで大きくない → 黄寄り
        return LedClass::Yellow;
      } else {
        // それ以外は全部 Green に倒す
        return LedClass::Green;
      }
    } else {
      // B もそこそこあるけど、strong_cyan ほどではない → ここも Green 扱い
      if (0.8f < gb) {
        return LedClass::White;
      } else {
        return LedClass::Green;
      }
    }
  }

  // --- B が最大のとき: BLUE / MAGENTA / CYAN ---
  // （ここはあまりいじっていない）
  if (bn >= rn && bn >= gn) {
    float br = (bn > 0.0f) ? (rn / bn) : 0.0f;
    float bg = (bn > 0.0f) ? (gn / bn) : 0.0f;

    if (br > 0.5f && bg < 0.3f) {
      // R 多め、G 少ない → マゼンタ寄り
      return LedClass::Magenta;
    } else if (bg > 0.5f && br < 0.3f) {
      // G 多め、R 少ない → シアン寄り
      return LedClass::Cyan;
    } else if (bg > 0.5f && br > 0.5f) {
      return LedClass::White;
    } else {
      // 純粋な青 or ちょっと白っぽい青
      return LedClass::Blue;
    }
  }

  return LedClass::Off;
}

// センサー値 → LedState への変換
LedState estimateLedState(uint16_t rRaw, uint16_t gRaw, uint16_t bRaw, uint16_t cRaw) {
  LedState st{};
  float R = rRaw;
  float G = gRaw;
  float B = bRaw;

  float L = max(R, max(G, B));  // 明るさ指標

  // 背景レベル更新（現在オフっぽい時だけ）
  // ここでは簡単に「極端に暗い場合」を背景扱い
  if (L < TH_OFF * 0.8f) {
    bg_level = (1.0f - BG_ALPHA) * bg_level + BG_ALPHA * L;
  }

  float effL = L - bg_level;
  if (effL < 0.0f) effL = 0.0f;

  static bool led_on = false;

  if (!led_on && effL > TH_ON) {
    led_on = true;
  } else if (led_on && effL < TH_OFF) {
    led_on = false;
  }

  st.on = led_on;

  // 0〜1 に正規化した明るさ（上限を適当に 3000 と仮定）
  const float MAX_BRIGHT = 3000.0f;
  st.normBrightness = effL / MAX_BRIGHT;
  if (st.normBrightness > 1.0f) st.normBrightness = 1.0f;

  float sum = R + G + B;
  st.r = R / sum;
  st.g = G / sum;
  st.b = B / sum;

  if (!led_on || sum < 1.0f) {
    st.color = LedClass::Off;
    return st;
  }
  st.color = classifyColor(st.r, st.g, st.b);
  return st;
}

const char *colorNameForClass(LedClass c) {
  switch (c) {
    case LedClass::Red:     return "RED    ";
    case LedClass::Green:   return "GREEN  ";
    case LedClass::Blue:    return "BLUE   ";
    case LedClass::Orange:  return "ORANGE ";
    case LedClass::Yellow:  return "YELLOW ";
    case LedClass::Cyan:    return "CYAN   ";
    case LedClass::Magenta: return "MAGENTA";
    case LedClass::Off:
    default:                return "OFF    ";
  }
}

void print3d(int v) {
  if (v < 0)   v = 0;
  if (v > 999) v = 999;

  if (v < 10) {
    Serial.print("  ");  // 2桁分スペース
  } else if (v < 100) {
    Serial.print(" ");   // 1桁分スペース
  }
  Serial.print(v);
}

// TeraTerm / シリアルモニタへの描画
void renderToTerminal(const LedState &st) {
#ifdef SHOW_NEW_LINE
  Serial.println("");
#else
  // 行頭に戻る
  Serial.print("\r");
#endif

  Serial.print("\x1b[?25l");  // hide cursor
  if (!st.on || st.color == LedClass::Off) {
    // 消灯判定時: □ OFF 0%
    Serial.print("\x1b[0m"); // 念のため属性リセット
    Serial.print("__ "); // 末尾に空白を少し入れて上書き残りを消す
#ifdef SHOW_COLOR_NANME
    Serial.print("OFF       0%   "); // 末尾に空白を少し入れて上書き残りを消す
#endif
    return;
  }

  // 明るさを 0〜100% に変換
  int percent = (int)(st.normBrightness * 100.0f + 0.5f);
  if (percent > 100) percent = 100;
  if (percent < 0)   percent = 0;

  // ◾ の部分だけ ANSI カラーをかける
  //Serial.print(ansiColorBgClass(st.color)); // 色指定
  Serial.print(ansiColorBgRGB(st.r, st.g, st.b)); // RGB指定
  Serial.print("  ");                         // カラー付きブロック
  Serial.print("\x1b[0m");                   // ここで色をリセット
  Serial.print(" ");

#ifdef SHOW_COLOR_NANME
  // この後は通常文字（モノクロ）で色名とパーセンテージを表示
  Serial.print(colorNameForClass(st.color));
  Serial.print(" ");
  print3d(percent);
  Serial.print("%   ");  // 余白で前の表示を消しておく
#endif
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("RGB status monitor starting...");
  Serial.println("Make sure TeraTerm interprets ANSI escape sequences.");

  Wire.begin();

  if (!apds.init()) {
    Serial.println("APDS-9960 init failed!");
    while (1) {
      delay(1000);
    }
  }

  apds.enableLightSensor(false); // ALS/RGB 有効

  // 最初はbg_levelを現在値で初期化
  uint16_t r, g, b, c;
  if (readRGB(r, g, b, c)) {
    float L0 = max((float)r, max((float)g, (float)b));
    bg_level = L0;
  } else {
    bg_level = 0.0f;
  }

  // 画面クリア & カーソルホーム
  Serial.print("\x1b[2J\x1b[H");
}

void loop() {
  static uint32_t lastMs = 0;
  const uint32_t intervalMs = 20; // 50Hz

  uint32_t now = millis();
  if (now - lastMs < intervalMs) {
    return;
  }
  lastMs = now;

  uint16_t r, g, b, c;
  if (!readRGB(r, g, b, c)) {
    return;
  }

  LedState st = estimateLedState(r, g, b, c);
  renderToTerminal(st);
}
