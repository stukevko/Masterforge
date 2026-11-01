#pragma once
// =============================================================================
//  MasterForge – DSP-Bausteine für das Mastering (Beat + Vocals auf der Summe)
//  Alles analysiert laufend das Signal; Regler bestimmen Stärke und Richtung.
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace mf
{
constexpr int kMaxChunk = 256;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-7f)); }
inline float powToDb (double p) noexcept  { return (float) (10.0 * std::log10 (std::max (p, 1.0e-14))); }
inline float coef (float ms, double fs) noexcept
{
    return (float) std::exp (-1.0 / (std::max (0.001, (double) ms) * 0.001 * fs));
}
inline float mixf (float a, float b, float t) noexcept { return a + (b - a) * t; }

// -----------------------------------------------------------------------------
//  Biquad (RBJ), Zustand für 2 Kanäle
// -----------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1[2] {}, s2[2] {};

    void reset() noexcept { s1[0] = s1[1] = s2[0] = s2[1] = 0.0; }
    inline float process (float x, int ch) noexcept
    {
        const double y = b0 * x + s1[ch];
        s1[ch] = b1 * x - a1 * y + s2[ch];
        s2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    void setPeak (double fs, double f, double q, double g) noexcept
    {
        const double A = std::pow (10.0, g / 40.0), w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowShelf (double fs, double f, double q, double g) noexcept
    {
        const double A = std::pow (10.0, g / 40.0), w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void setHighShelf (double fs, double f, double q, double g) noexcept
    {
        const double A = std::pow (10.0, g / 40.0), w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
    void setHighPass (double fs, double f, double q) noexcept
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setLowPass (double fs, double f, double q) noexcept
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setBandPass (double fs, double f, double q) noexcept
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }

private:
    static double w0 (double fs, double f) noexcept
    {
        return 2.0 * juce::MathConstants<double>::pi * std::clamp (f, 10.0, fs * 0.45) / fs;
    }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
};

// Linkwitz-Riley 4. Ordnung (phasengleiche Frequenzweiche)
struct LR4
{
    Biquad lp1, lp2, hp1, hp2;
    void setup (double fs, double f)
    {
        for (auto* b : { &lp1, &lp2 }) { b->setLowPass (fs, f, 0.70710678); b->reset(); }
        for (auto* b : { &hp1, &hp2 }) { b->setHighPass (fs, f, 0.70710678); b->reset(); }
    }
    inline void split (float x, int ch, float& lo, float& hi) noexcept
    {
        lo = lp2.process (lp1.process (x, ch), ch);
        hi = hp2.process (hp1.process (x, ch), ch);
    }
};

// -----------------------------------------------------------------------------
//  Gate: unterscheidet Musik von Stille/Ausklang, damit Analysen sauber bleiben
// -----------------------------------------------------------------------------
struct ActivityGate
{
    float ref = -60.0f, decay = 0.0f;
    void prepare (double rate) { ref = -60.0f; decay = (float) (4.0 / rate); }
    bool update (float lvlDb, float window) noexcept
    {
        ref = std::max ({ lvlDb, ref - decay, -70.0f });
        return lvlDb > -55.0f && lvlDb > ref - window;
    }
};

// -----------------------------------------------------------------------------
//  Auto-Kompressor (Gain-Rechner): Threshold folgt dem Langzeit-Pegel
// -----------------------------------------------------------------------------
class AutoComp
{
public:
    struct Ranges { float offLo, offHi, ratioLo, ratioHi, attSlow, attFast, relSlow, relFast, makeup; };

    void prepare (double sr, Ranges r)
    {
        fs = sr; rg = r; p = 0; gr = 0; longAvg = -24; avgGr = 0;
        gate.prepare (sr);
        aDet = coef (8.0f, fs); aLong = coef (4000.0f, fs); aMk = coef (3000.0f, fs);
        set (0.5f, 0.5f);
    }
    void set (float amount, float speed) noexcept
    {
        aAtt = coef (mixf (rg.attSlow, rg.attFast, speed), fs);
        aRel = coef (mixf (rg.relSlow, rg.relFast, speed), fs);
        offset = mixf (rg.offLo, rg.offHi, amount);
        slope = 1.0f - 1.0f / mixf (rg.ratioLo, rg.ratioHi, amount);
    }
    // x = Detektor-Eingang (Betrag), Rückgabe = linearer Gain inkl. Make-up
    inline float gain (float x) noexcept
    {
        p = aDet * p + (1 - aDet) * x * x;
        const float lvl = powToDb (p);
        const bool active = gate.update (lvl, 24.0f);
        if (active) longAvg = aLong * longAvg + (1 - aLong) * lvl;

        constexpr float knee = 6.0f;
        const float over = lvl - (longAvg + offset);
        float target;
        if (over <= -knee * 0.5f)     target = 0.0f;
        else if (over >= knee * 0.5f) target = slope * over;
        else                          target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

        gr = target > gr ? aAtt * gr + (1 - aAtt) * target : aRel * gr + (1 - aRel) * target;
        if (active) avgGr = aMk * avgGr + (1 - aMk) * gr;
        return dbToGain (avgGr * rg.makeup - gr);
    }
    float getGr() const noexcept { return gr; }

private:
    double fs = 44100;
    Ranges rg {};
    float p = 0, gr = 0, longAvg = -24, avgGr = 0, aDet = 0, aLong = 0, aMk = 0, aAtt = 0, aRel = 0, offset = 0, slope = 0;
    ActivityGate gate;
};

// -----------------------------------------------------------------------------
//  Auto Tonal-EQ: gleicht die Klangbalance des Mixes an eine Genre-Zielkurve an
// -----------------------------------------------------------------------------
class TonalEQ
{
public:
    static constexpr int N = 7;
    static constexpr float freqs[N] { 60.f, 150.f, 400.f, 1000.f, 2500.f, 6000.f, 12000.f };

    void prepare (double sr)
    {
        fs = sr; counter = 0; inMs = 0; activeTime = 0; active = false;
        gate.prepare (sr / 64.0);
        for (int b = 0; b < N; ++b)
        {
            ana[b].setBandPass (fs, freqs[b], 1.0); ana[b].reset();
            eq[b].reset(); bandMs[b] = 1.0e-7; gains[b] = 0;
        }
        aBand = coef (3000.0f, fs); aIn = coef (100.0f, fs); aGain = coef (1500.0f, fs / 64.0);
        update();
    }

    void process (float* L, float* R, int n, const float* target, float amount, float tilt,
                  float low, float mid, float air, bool on) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            inMs = aIn * inMs + (1 - aIn) * m * m;
            if (active)
                for (int b = 0; b < N; ++b)
                {
                    const float y = ana[b].process (m, 0);
                    bandMs[b] = aBand * bandMs[b] + (1 - aBand) * (double) y * y;
                }
            else
                for (int b = 0; b < N; ++b) ana[b].process (m, 0);

            for (int b = 0; b < N; ++b) { L[i] = eq[b].process (L[i], 0); R[i] = eq[b].process (R[i], 1); }
            if (++counter >= 64) { counter = 0; block (target, amount, tilt, low, mid, air, on); }
        }
    }
    float getGain (int b) const noexcept { return gains[b]; }

private:
    void block (const float* target, float amount, float tilt, float low, float mid, float air, bool on) noexcept
    {
        static constexpr float tiltW[N] { -1.f, -0.66f, -0.33f, 0.f, 0.33f, 0.66f, 1.f };
        active = gate.update (powToDb (inMs), 30.0f);
        if (active) activeTime += 64.0f / (float) fs;

        float rel[N], mean = 0, tMean = 0;
        for (int b = 0; b < N; ++b) { rel[b] = powToDb (bandMs[b]); mean += rel[b]; tMean += target[b]; }
        mean /= N; tMean /= N;
        const float conf = std::clamp (activeTime / 2.0f, 0.0f, 1.0f);

        for (int b = 0; b < N; ++b)
        {
            float t = 0;
            if (on)
            {
                t = std::clamp ((target[b] - tMean) - (rel[b] - mean), -6.0f, 6.0f) * amount * conf * 0.75f;
                t += tilt * tiltW[b] * 3.0f;
                if (b == 0) t += low;
                if (b == 4) t += mid;
                if (b == 6) t += air;
            }
            gains[b] = aGain * gains[b] + (1 - aGain) * t;
        }
        update();
    }
    void update() noexcept
    {
        eq[0].setLowShelf (fs, 80.0, 0.7, gains[0]);
        for (int b = 1; b < N - 1; ++b) eq[b].setPeak (fs, freqs[b], 0.9, gains[b]);
        eq[N - 1].setHighShelf (fs, 10000.0, 0.7, gains[N - 1]);
    }

    double fs = 44100;
    Biquad ana[N], eq[N];
    double bandMs[N] {};
    float gains[N] {}, inMs = 0, aBand = 0, aIn = 0, aGain = 0, activeTime = 0;
    bool active = false;
    int counter = 0;
    ActivityGate gate;
};

// -----------------------------------------------------------------------------
//  Auto Multiband-Kompressor (3 Bänder, LR4-Weichen 150 Hz / 2.5 kHz)
// -----------------------------------------------------------------------------
class Multiband
{
public:
    void prepare (double sr)
    {
        x1.setup (sr, 150.0); x2.setup (sr, 2500.0); ap.setup (sr, 2500.0);
        lowC.prepare  (sr, { 3.f, -8.f, 1.2f, 3.0f, 45.f, 12.f, 350.f, 120.f, 0.8f });
        midC.prepare  (sr, { 3.f, -8.f, 1.2f, 2.5f, 30.f,  6.f, 250.f,  70.f, 0.8f });
        highC.prepare (sr, { 3.f, -8.f, 1.2f, 2.5f, 15.f,  2.f, 200.f,  50.f, 0.8f });
    }
    void process (float* L, float* R, int n, float amount, float speed, bool on) noexcept
    {
        lowC.set (amount, speed); midC.set (amount, speed); highC.set (amount, speed);
        for (int i = 0; i < n; ++i)
        {
            float lo[2], mi[2], hi[2], rest[2];
            const float in[2] { L[i], R[i] };
            for (int c = 0; c < 2; ++c)
            {
                x1.split (in[c], c, lo[c], rest[c]);
                x2.split (rest[c], c, mi[c], hi[c]);
                float a, b; ap.split (lo[c], c, a, b); lo[c] = a + b;   // Allpass: Phase an Mitten/Höhen anpassen
            }
            if (on)
            {
                const float gl = lowC.gain  (std::max (std::abs (lo[0]), std::abs (lo[1])));
                const float gm = midC.gain  (std::max (std::abs (mi[0]), std::abs (mi[1])));
                const float gh = highC.gain (std::max (std::abs (hi[0]), std::abs (hi[1])));
                L[i] = lo[0] * gl + mi[0] * gm + hi[0] * gh;
                R[i] = lo[1] * gl + mi[1] * gm + hi[1] * gh;
            }
            else
            {
                L[i] = lo[0] + mi[0] + hi[0];
                R[i] = lo[1] + mi[1] + hi[1];
            }
        }
    }
    float grLow() const noexcept  { return lowC.getGr(); }
    float grMid() const noexcept  { return midC.getGr(); }
    float grHigh() const noexcept { return highC.getGr(); }

private:
    LR4 x1, x2, ap;
    AutoComp lowC, midC, highC;
};

// -----------------------------------------------------------------------------
//  Glue-Kompressor (Bus-Comp, klebt Beat und Vocals zusammen)
// -----------------------------------------------------------------------------
class Glue
{
public:
    void prepare (double sr) { c.prepare (sr, { 3.f, -9.f, 1.5f, 4.0f, 30.f, 10.f, 400.f, 100.f, 0.9f }); }
    void process (float* L, float* R, int n, float amount, float speed, float mix, bool on) noexcept
    {
        if (! on) return;
        c.set (amount, speed);
        for (int i = 0; i < n; ++i)
        {
            const float g = mixf (1.0f, c.gain (std::max (std::abs (L[i]), std::abs (R[i]))), mix);
            L[i] *= g; R[i] *= g;
        }
    }
    float getGr() const noexcept { return c.getGr(); }

private:
    AutoComp c;
};

// -----------------------------------------------------------------------------
//  Sättigung (Tape-artig, mit Lautheitsausgleich)
// -----------------------------------------------------------------------------
struct Saturator
{
    static void process (float* L, float* R, int n, float drive, bool on) noexcept
    {
        if (! on || drive <= 0.001f) return;
        const float g = 1.0f + 4.0f * drive, inv = 1.0f / g, wet = std::min (1.0f, drive * 1.2f);
        for (int i = 0; i < n; ++i)
        {
            const float l = std::tanh (g * L[i]) * inv;   // Kleinsignal-Gain = 1, Spitzen werden weich gerundet
            const float r = std::tanh (g * R[i]) * inv;
            L[i] += wet * (l - L[i]);
            R[i] += wet * (r - R[i]);
        }
    }
};

// -----------------------------------------------------------------------------
//  Stereo: Breite (M/S), Bass-Mono, Vocal Focus (Mitte präsenter, Seiten zurück)
// -----------------------------------------------------------------------------
class Stereo
{
public:
    void prepare (double sr) { fs = sr; lastMono = lastFocus = -1; monoHp1.reset(); monoHp2.reset(); fm.reset(); fs2.reset(); corrNum = 0; corrDen1 = corrDen2 = 1e-9; aC = coef (300.0f, fs); }
    void process (float* L, float* R, int n, float width, float monoFreq, float focus, bool on) noexcept
    {
        if (on)
        {
            if (! juce::exactlyEqual (monoFreq, lastMono))
            {
                monoHp1.setHighPass (fs, monoFreq, 0.70710678); monoHp2.setHighPass (fs, monoFreq, 0.70710678); lastMono = monoFreq;
            }
            if (! juce::exactlyEqual (focus, lastFocus))
            {
                fm.setPeak (fs, 2800.0, 0.8, 3.0 * focus); fs2.setPeak (fs, 2800.0, 0.8, -4.0 * focus); lastFocus = focus;
            }
            for (int i = 0; i < n; ++i)
            {
                float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]);
                s = monoHp2.process (monoHp1.process (s, 0), 0);   // Bass in Mono
                m = fm.process (m, 0);
                s = fs2.process (s, 0) * width;
                L[i] = m + s; R[i] = m - s;
            }
        }
        for (int i = 0; i < n; ++i)
        {
            corrNum  = aC * corrNum  + (1 - aC) * (double) L[i] * R[i];
            corrDen1 = aC * corrDen1 + (1 - aC) * (double) L[i] * L[i];
            corrDen2 = aC * corrDen2 + (1 - aC) * (double) R[i] * R[i];
        }
    }
    float correlation() const noexcept
    {
        const double d = std::sqrt (corrDen1 * corrDen2);
        return d > 1e-9 ? (float) std::clamp (corrNum / d, -1.0, 1.0) : 1.0f;
    }

private:
    double fs = 44100;
    float lastMono = -1, lastFocus = -1, aC = 0;
    Biquad monoHp1, monoHp2, fm, fs2;
    double corrNum = 0, corrDen1 = 1e-9, corrDen2 = 1e-9;
};

// -----------------------------------------------------------------------------
//  Loudness-Messung (K-Gewichtung nach ITU-R BS.1770, vereinfacht)
// -----------------------------------------------------------------------------
class LoudnessMeter
{
public:
    void prepare (double sr)
    {
        fs = sr; shelf.setHighShelf (fs, 1681.0, 0.7071, 4.0); hp.setHighPass (fs, 38.0, 0.5); shelf.reset(); hp.reset();
        mom = st = 0; aM = coef (400.0f / 2.0f, fs); aS = coef (3000.0f / 2.0f, fs);
        integSum = 0; integN = 0; blockCount = 0; longTerm = 0; ltN = 0;
    }
    // misst einen Stereo-Abschnitt
    void process (const float* L, const float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float l = hp.process (shelf.process (L[i], 0), 0);
            const float r = hp.process (shelf.process (R[i], 1), 1);
            const double e = (double) l * l + (double) r * r;
            mom = aM * mom + (1 - aM) * e;
            st  = aS * st  + (1 - aS) * e;
            if (++blockCount >= (int) (fs * 0.1))   // alle 100 ms
            {
                blockCount = 0;
                const float m = momentary();
                if (m > -70.0f)
                {
                    integSum += mom; ++integN;
                    // Langzeit-Lautheit: folgt lauteren Passagen zügig (~2,5 s), leiseren langsamer (~8 s),
                    // sehr leise Stellen (Intro, Breaks > 20 LU unter Langzeit) werden ignoriert
                    if (ltN == 0) longTerm = mom;
                    else if (m > longTermLufs() - 20.0f)
                        longTerm += (mom - longTerm) * (mom > longTerm ? 0.0392 : 0.01242);
                    ++ltN;
                }
            }
        }
    }
    void resetIntegrated() noexcept { integSum = 0; integN = 0; }
    float momentary() const noexcept  { return -0.691f + powToDb (mom); }
    float shortTerm() const noexcept  { return -0.691f + powToDb (st); }
    float integrated() const noexcept { return integN ? -0.691f + powToDb (integSum / integN) : -100.0f; }
    float longTermLufs() const noexcept { return ltN ? -0.691f + powToDb (longTerm) : -100.0f; }
    bool  hasLongTerm() const noexcept { return ltN > 5; }

private:
    double fs = 44100;
    Biquad shelf, hp;
    double mom = 0, st = 0, integSum = 0, longTerm = 0;
    float aM = 0, aS = 0;
    int integN = 0, blockCount = 0, ltN = 0;
};

// -----------------------------------------------------------------------------
//  Soft-Clipper: rundet kurze Spitzen (Kick/Snare) vor dem Limiter ab
//  -> mehr Lautheit bei weniger Limiter-Pumpen
// -----------------------------------------------------------------------------
struct SoftClipper
{
    static void process (float* L, float* R, int n, float amount, float ceilingDb) noexcept
    {
        if (amount <= 0.001f) return;
        const float T = dbToGain (ceilingDb + mixf (6.0f, 0.5f, amount));
        const float k = 0.7f * T, w = T - k;
        auto clip = [k, w] (float x) noexcept
        {
            const float a = std::abs (x);
            if (a <= k) return x;
            return std::copysign (k + w * std::tanh ((a - k) / w), x);
        };
        for (int i = 0; i < n; ++i) { L[i] = clip (L[i]); R[i] = clip (R[i]); }
    }
};

// -----------------------------------------------------------------------------
//  Lookahead-Limiter (kein Übersteuern, keine Klicks)
// -----------------------------------------------------------------------------
class Limiter
{
public:
    void prepare (double sr)
    {
        la = std::max (8, (int) std::round (sr * 0.0015));          // 1,5 ms Lookahead
        delayL.assign ((size_t) la, 0.0f); delayR.assign ((size_t) la, 0.0f);
        tgt.assign ((size_t) la, 1.0f); box.assign ((size_t) la, 1.0f);
        pos = 0; boxSum = la; rel = 1.0f; aRel = coef (70.0f, sr); gr = 0;
    }
    int latency() const noexcept { return la - 1; }

    void process (float* L, float* R, int n, float ceilingDb) noexcept
    {
        const float c = dbToGain (ceilingDb);
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            tgt[(size_t) pos] = pk > c ? c / pk : 1.0f;
            float mn = 1.0f;
            for (float v : tgt) mn = std::min (mn, v);              // Minimum über Lookahead-Fenster
            rel = mn < rel ? mn : aRel * rel + (1 - aRel) * mn;     // Release-Glättung
            boxSum += (double) rel - box[(size_t) pos];
            box[(size_t) pos] = rel;
            const float g = (float) (boxSum / la);                  // weiche Gain-Rampe

            // Audio um la-1 Samples verzögern
            const int rp = (pos + 1) % la;
            const float dl = delayL[(size_t) rp], dr = delayR[(size_t) rp];
            delayL[(size_t) pos] = L[i]; delayR[(size_t) pos] = R[i];
            L[i] = std::clamp (dl * g, -c, c);
            R[i] = std::clamp (dr * g, -c, c);
            gr = std::max (gr * 0.9995f, -gainToDb (g));
            if (++pos >= la) pos = 0;
        }
    }
    float getGr() const noexcept { return gr; }

private:
    std::vector<float> delayL, delayR, tgt, box;
    int la = 64, pos = 0;
    double boxSum = 0;
    float rel = 1, aRel = 0, gr = 0;
};

// Einfache Verzögerung (für Latenzausgleich im A/B-Vergleich)
class DelayLine
{
public:
    void prepare (int samples) { d = std::max (0, samples); bufL.assign ((size_t) d + 1, 0.f); bufR.assign ((size_t) d + 1, 0.f); w = 0; }
    inline void process (float& l, float& r) noexcept
    {
        if (d == 0) return;
        bufL[(size_t) w] = l; bufR[(size_t) w] = r;
        const int rp = (w + 1) % (d + 1);
        l = bufL[(size_t) rp]; r = bufR[(size_t) rp];
        w = rp;
    }

private:
    std::vector<float> bufL, bufR;
    int d = 0, w = 0;
};

} // namespace mf
