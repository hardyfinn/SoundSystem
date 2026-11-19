// Sound System: the sound engine (no Rack code here, so test/render.cpp can
// run it offline).
//
// The signal goes through what happens between a record and your body at a
// dance:
//   1. AMP      the rack of power amps, pushed: loudness curve, limiter, valve-ish
//               saturation.
//   2. STACK    a 4-way crossover feeding SUB bins (scoop resonance, driver
//               excursion, mono), BASS bins, MID horns (honk, compression
//               driver grit) and TOPS (tweeters). Each band arrives a little
//               late or early, like a real stack.
//   3. BODY     sub harmonics so you "feel" the sub on small speakers, the sub
//               pushing everything else (pump), and things in the room rattling
//               on every big cone excursion.
//   4. ROOM     distance (air soaks up the top, direct sound vs room sound),
//               early reflections, a big reverb tail, room boom (modes),
//               slap echo off buildings, festival delay towers, wind.
//   5. WALL     for "Through the wall": the club heard from next door; DOOR
//               opens it.
//
// Levels: Rack audio is about +-5 V; internally 5 V = 1.0.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace soundsystem {

static const float PI = 3.14159265358979f;

inline float db2lin(float db) { return std::pow(10.f, db / 20.f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ---------------------------------------------------------------- filters
struct Biquad {
	enum Type { LP, HP, PEAK, LSHELF, HSHELF, BP };
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	inline float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
	// RBJ cookbook
	void set(Type t, float f, float q, float gainDb, float sr) {
		f = std::min(std::max(f, 5.f), sr * 0.45f);
		q = std::max(q, 0.05f);
		float w = 2 * PI * f / sr, cw = std::cos(w), sw = std::sin(w);
		float alpha = sw / (2 * q);
		float A = std::pow(10.f, gainDb / 40.f);
		float B0, B1, B2, A0, A1, A2;
		switch (t) {
			case LP: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = B0; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
			case HP: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = B0; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
			case BP: B0 = alpha; B1 = 0; B2 = -alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
			case PEAK: B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
			case LSHELF: {
				float s = 2 * std::sqrt(A) * alpha;
				B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
				A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s;
				break;
			}
			default: {  // HSHELF
				float s = 2 * std::sqrt(A) * alpha;
				B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
				A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s;
				break;
			}
		}
		b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
	}
};

// Linkwitz-Riley 4th order crossover (two Butterworths each side)
struct LR4 {
	Biquad lp1, lp2, hp1, hp2;
	void set(float f, float sr) {
		lp1.set(Biquad::LP, f, 0.7071f, 0, sr); lp2 = lp1; lp2.z1 = lp2.z2 = 0;
		hp1.set(Biquad::HP, f, 0.7071f, 0, sr); hp2 = hp1; hp2.z1 = hp2.z2 = 0;
	}
	// update coefficients only (keep filter state)
	void update(float f, float sr) {
		Biquad t;
		t.set(Biquad::LP, f, 0.7071f, 0, sr); copyCo(lp1, t); copyCo(lp2, t);
		t.set(Biquad::HP, f, 0.7071f, 0, sr); copyCo(hp1, t); copyCo(hp2, t);
	}
	static void copyCo(Biquad& d, const Biquad& s) { d.b0 = s.b0; d.b1 = s.b1; d.b2 = s.b2; d.a1 = s.a1; d.a2 = s.a2; }
	inline void split(float x, float& lo, float& hi) {
		lo = lp2.process(lp1.process(x));
		hi = hp2.process(hp1.process(x));
	}
};

// set coefficients but keep the running state (so filters can glide)
inline void setKeep(Biquad& b, Biquad::Type t, float f, float q, float g, float sr) {
	float z1 = b.z1, z2 = b.z2;
	b.set(t, f, q, g, sr);
	b.z1 = z1; b.z2 = z2;
}

struct OnePole {
	float y = 0, a = 1;
	void setHz(float f, float sr) { a = 1.f - std::exp(-2 * PI * std::min(f, sr * 0.45f) / sr); }
	inline float process(float x) { y += a * (x - y); return y; }
};

struct Delay {
	std::vector<float> buf;
	uint32_t mask = 0, w = 0;
	void init(int minSize) {
		uint32_t n = 1;
		while ((int) n < minSize) n <<= 1;
		buf.assign(n, 0.f);
		mask = n - 1; w = 0;
	}
	inline void push(float x) { buf[w] = x; w = (w + 1) & mask; }
	// d in samples (>= 1), linear interpolation
	inline float read(float d) const {
		d = std::min(std::max(d, 1.f), (float) mask - 2);
		float rp = (float) w - d;
		while (rp < 0) rp += (float) (mask + 1);
		uint32_t i = (uint32_t) rp;
		float fr = rp - (float) i;
		float a = buf[i & mask], b = buf[(i + 1) & mask];
		return a + (b - a) * fr;
	}
};

// ---------------------------------------------------------------- presets
// All floats, so two presets can be blended field by field when you switch
// systems (the change glides over about a third of a second).
struct Rig {
	float width;        // source stereo width (0 = mono like a real sound system)
	float subTune, subQ, subBoost;          // scoop bin resonance (Hz, Q, dB)
	float x1, x2, x3;                       // crossovers sub|bass|mid|top (Hz)
	float bassGain, midGain;                // dB
	float honkF, honkGain, honkQ;           // horn honk
	float topGain, topShelf, topLimit;      // dB, dB @ 9 kHz, tweeter roll-off Hz
	float drive, warmth, comp;              // amp push, even harmonics, limiter
	float subDelay, midDelay;               // ms, band arrival differences
	float distNear, distFar, critDist;      // metres
	float rt, damp, size, erLevel, predelay;// tail seconds, tail damping Hz, room scale, ER level, ms
	float outdoor;                          // 0..1: open air (no ceiling, no tail)
	float slapMs, slapLevel, slapFb;        // echo off buildings
	float tower;                            // festival delay towers
	float mode1, mode2, mode3, modeGain;    // room boom
	float rattle, rattleF;                  // things buzzing in the room
	float wind;                             // festival wind
	float wall;                             // through the wall
	float smile;                            // loudness curve (dB)
};
static const int RIG_FLOATS = sizeof(Rig) / sizeof(float);

enum { DUB, TECHNO, WAREHOUSE, BASEMENT, KINGSTON, LOFT, FESTIVAL, WALL, NUM_RIGS };

static const char* const RIG_NAMES[NUM_RIGS] = {
	"DUB HALL", "TECHNO CLUB", "WAREHOUSE", "BASEMENT",
	"KINGSTON STREET", "DISCO LOFT", "FESTIVAL", "THROUGH THE WALL"
};
static const char* const RIG_BLURBS[NUM_RIGS] = {
	"scoop bins, valve amps, a hall shaking",
	"clean stacks, tall concrete room",
	"rented rig pushed hard, brick + steel",
	"low ceiling, bodies, rig too big",
	"open air, home-built boxes, slap echo",
	"warm audiophile horns, wooden floor",
	"line arrays, a field, delay towers, wind",
	"the club from the toilets: DOOR opens",
};

//                  width sub  Q    boost  x1   x2   x3    bass mid  honkF honk hQ  top shelf limit   drive warm comp sDel mDel near far crit  rt   damp  size er   pre outd slap  sLv  sFb tower m1  m2  m3  mG  rat  ratF  wind wall smile
static const Rig RIGS[NUM_RIGS] = {
	{0.0f, 45, 1.4f, 9.f,  110, 450, 3800, 0.f, 0.f, 900,  4.f, 1.2f, 1.f, 3.f, 13000, 1.2f, 1.0f, 0.6f, 3.f, 1.0f, 3, 35, 6,   2.0f, 3500, 1.3f, 0.6f, 12, 0, 0,   0,    0,   0,   38, 57, 76, 4, 0.5f, 2300, 0, 0, 3},
	{0.5f, 48, 1.0f, 6.f,  100, 280, 4000, 2.f, 0.f, 1600, 1.5f,1.0f, 1.f, 2.f, 18000, 0.9f, 0.3f, 0.5f, 1.5f,0.5f, 3, 30, 7,   2.6f, 6000, 1.6f, 0.7f, 18, 0, 0,   0,    0,   0,   41, 62, 83, 3, 0.15f,3100, 0, 0, 2},
	{0.6f, 52, 1.2f, 7.f,  120, 500, 3500, 1.f, 1.f, 1200, 4.f, 1.5f, -1.f,-2.f, 11000, 1.8f, 0.7f, 1.0f, 4.f, 1.5f, 4, 50, 5,   3.8f, 2500, 2.0f, 0.8f, 25, 0, 0,   0,    0,   0,   31, 47, 63, 6, 0.8f, 2800, 0, 0, 4},
	{0.4f, 58, 1.6f, 8.f,  120, 450, 3500, 2.f, 0.f, 1000, 3.f, 1.2f, 0.f, -1.f, 12000, 1.6f, 0.8f, 0.9f, 1.f, 0.3f, 1, 8,  2.5f,0.7f, 2800, 0.45f,0.9f, 3,  0, 0,   0,    0,   0,   68, 95, 136,7, 0.6f, 2500, 0, 0, 4},
	{0.0f, 40, 1.8f, 11.f, 100, 500, 4500, -1.f,-1.f,1800, 5.f, 2.0f, 3.f, 5.f, 15000, 1.3f, 0.9f, 0.6f, 4.f, 1.0f, 4, 60, 25,  0.5f, 3000, 1.0f, 0.25f,20, 1, 115, 0.45f,0.3f,0,   0,  0,  0,  0, 1.0f, 1900, 0, 0, 3},
	{0.8f, 55, 0.8f, 4.f,  90,  350, 5000, 2.f, 0.f, 700,  1.5f,0.8f, 1.f, 2.5f,20000, 0.7f, 0.5f, 0.2f, 0.5f,0.2f, 2, 18, 5,   1.2f, 5000, 0.9f, 0.6f, 9,  0, 0,   0,    0,   0,   50, 72, 100,2, 0.05f,3500, 0, 0, 2.5f},
	{0.9f, 42, 1.0f, 7.f,  90,  300, 2500, 1.f, 0.f, 2500, 1.f, 1.0f, 0.f, 0.f, 18000, 1.0f, 0.3f, 0.8f, 6.f, 2.0f, 15,160,40,  0.9f, 2000, 2.0f, 0.2f, 40, 1, 260, 0.15f,0.1f,0.5f,0,  0,  0,  0, 0.1f, 3000, 1, 0, 3},
	{0.5f, 48, 1.0f, 6.f,  100, 280, 4000, 2.f, 0.f, 1600, 1.5f,1.0f, 1.f, 2.f, 18000, 0.9f, 0.3f, 0.5f, 1.5f,0.5f, 3, 30, 7,   2.6f, 6000, 1.6f, 0.7f, 18, 0, 0,   0,    0,   0,   41, 62, 83, 3, 0.7f, 1400, 0, 1, 2},
};

// ---------------------------------------------------------------- engine
struct Controls {
	int rig = DUB;
	float push = 0.5f;    // how hard the rig is driven
	float dist = 0.35f;   // near the stack .. back of the room
	float weight = 0.5f;  // sub/bass weight
	float room = 0.5f;    // how much room you hear
	float tops = 0.5f;    // horns + tweeters
	float crowd = 0.3f;   // bodies soak up the room
	float mix = 1.f;      // dry/wet
	bool door = false;    // open the door (Through the wall)
};

struct Engine {
	float sr = 48000.f;
	Rig cur = RIGS[DUB];          // blended rig actually used
	int lastRig = -1;

	// smoothed controls
	float sPush = 0.5f, sDist = 0.35f, sWeight = 0.5f, sRoom = 0.5f, sTops = 0.5f, sCrowd = 0.3f, sMix = 1.f, sDoor = 0.f;

	// amp
	Biquad smileLo[2], smileHi[2], dcBlock[2];
	float compEnv = 0.f;
	float drive = 1.f, compRatio = 1.f, warm = 0.f;
	// stack
	LR4 xo1[2], xo2[2], xo3[2];
	Biquad subHp1, subHp2, subScoop, harmBp, honk[2], topShelfF[2], topLp[2];
	float subGain = 1.f, bassG = 1.f, midG = 1.f, topG = 1.f, subDrive = 1.f, harmAmt = 0.f;
	Delay subDel, midDel[2];
	float subEnv = 0.f, midEnvD = 0.f, topEnvD = 0.f;
	// rattle
	uint32_t rng = 22222;
	Biquad rat1, rat2;
	float rattleAmt = 0.f;
	// room
	OnePole airLp[2];
	Delay srcLine[2];
	OnePole erLp[2];
	Delay slapLine;
	OnePole slapLp;
	OnePole towerLp[2];
	static const int NFDN = 8;
	Delay fdn[NFDN];
	OnePole fdnLp[NFDN];
	float fdnLen[NFDN], fdnG[NFDN];
	Biquad modes[2][3];
	float directG = 1.f, revG = 0.f, erG = 0.f, distM = 5.f, airHz = 20000.f, towerMs = 50.f;
	// wind
	float windT = 0.f, windV = 0.f, windTarget = 0.f, windCount = 0.f;
	// wall
	Biquad wallLp1[2], wallLp2[2], wallRes[2];
	Delay tileLine;
	float wallAmt = 0.f;

	int ctlCount = 0;

	// display
	float showDist = 5.f, showRt = 2.f;

	Engine() { setSampleRate(48000.f); }

	void setSampleRate(float s) {
		sr = s;
		int big = (int) (0.6f * sr) + 8;
		srcLine[0].init(big); srcLine[1].init(big);
		slapLine.init(big);
		for (int i = 0; i < NFDN; i++) fdn[i].init((int) (0.4f * sr) + 8);
		subDel.init((int) (0.02f * sr) + 8);
		midDel[0].init((int) (0.02f * sr) + 8); midDel[1].init((int) (0.02f * sr) + 8);
		tileLine.init((int) (0.1f * sr) + 8);
		for (int c = 0; c < 2; c++) {
			xo1[c].set(cur.x1, sr); xo2[c].set(cur.x2, sr); xo3[c].set(cur.x3, sr);
			dcBlock[c].set(Biquad::HP, 12.f, 0.7071f, 0, sr);
		}
		ctlCount = 0;
		updateControlRate(Controls(), true);
	}

	void reset() {
		Controls c;
		setSampleRate(sr);
	}

	inline float noise() {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
		return (rng >> 8) / 8388608.f - 1.f;
	}

	static inline float softClip(float x) {
		// tanh-ish, cheap
		if (x > 3.f) return 1.f;
		if (x < -3.f) return -1.f;
		float x2 = x * x;
		return x * (27.f + x2) / (27.f + 9.f * x2);
	}

	// Every 16 samples: glide rig + knobs, recompute filters.
	void updateControlRate(const Controls& c, bool snap) {
		float dt = 16.f / sr;
		float kRig = snap ? 1.f : 1.f - std::exp(-dt / 0.12f);
		float kKnob = snap ? 1.f : 1.f - std::exp(-dt / 0.03f);
		float kDoor = snap ? 1.f : 1.f - std::exp(-dt / 0.12f);
		int r = std::min(std::max(c.rig, 0), NUM_RIGS - 1);
		const float* tgt = (const float*) &RIGS[r];
		float* cu = (float*) &cur;
		for (int i = 0; i < RIG_FLOATS; i++) cu[i] += (tgt[i] - cu[i]) * kRig;
		lastRig = r;
		sPush += (c.push - sPush) * kKnob;
		sDist += (c.dist - sDist) * kKnob;
		sWeight += (c.weight - sWeight) * kKnob;
		sRoom += (c.room - sRoom) * kKnob;
		sTops += (c.tops - sTops) * kKnob;
		sCrowd += (c.crowd - sCrowd) * kKnob;
		sMix += (c.mix - sMix) * kKnob;
		sDoor += ((c.door ? 1.f : 0.f) - sDoor) * kDoor;

		const Rig& p = cur;
		// ---- amp
		drive = db2lin(-8.f + sPush * 22.f) * p.drive;          // push 0.5 -> +3 dB * rig
		compRatio = 1.f + p.comp * sPush * 10.f;
		warm = p.warmth;
		float smile = p.smile * (0.3f + sPush * 1.4f);
		for (int ch = 0; ch < 2; ch++) {
			setKeep(smileLo[ch], Biquad::LSHELF, 110.f, 0.7f, smile, sr);
			setKeep(smileHi[ch], Biquad::HSHELF, 7000.f, 0.7f, smile * 0.7f, sr);
			xo1[ch].update(p.x1, sr); xo2[ch].update(p.x2, sr); xo3[ch].update(p.x3, sr);
			setKeep(honk[ch], Biquad::PEAK, p.honkF, p.honkQ, p.honkGain, sr);
			setKeep(topShelfF[ch], Biquad::HSHELF, 9000.f, 0.7f, p.topShelf + (sTops - 0.5f) * 10.f, sr);
			setKeep(topLp[ch], Biquad::LP, p.topLimit, 0.6f, 0, sr);
		}
		// ---- stack
		setKeep(subHp1, Biquad::HP, p.subTune * 0.62f, 0.75f, 0, sr);
		setKeep(subHp2, Biquad::HP, p.subTune * 0.62f, 0.75f, 0, sr);
		// near the bins = more sub (proximity)
		float prox = 4.f * (0.6f - sDist);
		setKeep(subScoop, Biquad::PEAK, p.subTune, p.subQ, p.subBoost * (0.25f + 1.5f * sWeight), sr);
		setKeep(harmBp, Biquad::BP, p.subTune * 2.6f, 0.6f, 0, sr);
		subGain = db2lin(-2.f + (sWeight - 0.5f) * 10.f + prox);
		harmAmt = sWeight * 0.9f;
		subDrive = 1.f + sPush * 2.5f * p.drive;
		bassG = db2lin(p.bassGain + (sWeight - 0.5f) * 4.f);
		midG = db2lin(p.midGain + (sTops - 0.5f) * 4.f);
		topG = db2lin(p.topGain + (sTops - 0.5f) * 6.f);
		// ---- rattle
		setKeep(rat1, Biquad::BP, p.rattleF, 7.f, 0, sr);
		setKeep(rat2, Biquad::BP, p.rattleF * 1.63f, 9.f, 0, sr);
		rattleAmt = p.rattle * (0.25f + 1.2f * sPush);

		// ---- room
		distM = p.distNear * std::pow(p.distFar / p.distNear, sDist);
		float crit = p.critDist * (1.f + 1.2f * sCrowd);
		float dr = crit / distM;
		float roomScale = sRoom * 2.f;
		directG = dr / std::sqrt(1.f + dr * dr);
		revG = roomScale / std::sqrt(1.f + dr * dr);
		directG = std::max(directG, 0.25f);   // creative: never lose the stack completely
		// keep level steady-ish as the room grows
		float norm = 1.f / std::sqrt(directG * directG + 0.5f * revG * revG);
		directG *= std::min(norm, 2.f);
		revG *= std::min(norm, 2.f);
		erG = p.erLevel * (1.f - 0.5f * sCrowd);
		// air absorption, wind moves it about
		float windMod = 1.f + p.wind * windV * 0.45f;
		airHz = 20000.f / (1.f + distM / (25.f + 15.f * (1.f - p.outdoor))) * windMod;
		airHz = std::min(std::max(airHz, 1200.f), 20000.f);
		for (int ch = 0; ch < 2; ch++) {
			airLp[ch].setHz(airHz, sr);
			erLp[ch].setHz(std::min(airHz, p.damp * 1.6f), sr);
			towerLp[ch].setHz(std::min(airHz * 0.6f, 6000.f), sr);
		}
		slapLp.setHz(std::min(airHz * 0.5f, 4000.f), sr);
		towerMs = 40.f + distM * 0.75f;
		// tail
		float rt = p.rt * (1.f - 0.55f * sCrowd) * (0.4f + 1.2f * sRoom);
		rt = std::max(rt, 0.1f);
		showRt = rt;
		static const float base[NFDN] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f};
		float dampHz = p.damp * (1.f - 0.6f * sCrowd);
		for (int i = 0; i < NFDN; i++) {
			fdnLen[i] = base[i] * p.size * 0.001f * sr;
			fdnG[i] = std::pow(10.f, -3.f * (base[i] * p.size * 0.001f) / rt);
			fdnLp[i].setHz(dampHz, sr);
		}
		float mg = p.modeGain * (0.3f + 1.4f * sRoom) * (1.f - 0.4f * sCrowd);
		const float mf[3] = {p.mode1, p.mode2, p.mode3};
		for (int ch = 0; ch < 2; ch++)
			for (int m = 0; m < 3; m++)
				setKeep(modes[ch][m], Biquad::PEAK, std::max(mf[m], 20.f), 6.f + 2.f * m, mf[m] > 1.f ? mg / (1.f + 0.4f * m) : 0.f, sr);
		// wall
		wallAmt = p.wall * (1.f - sDoor);
		for (int ch = 0; ch < 2; ch++) {
			setKeep(wallLp1[ch], Biquad::LP, 210.f, 0.7f, 0, sr);
			setKeep(wallLp2[ch], Biquad::LP, 330.f, 0.9f, 0, sr);
			setKeep(wallRes[ch], Biquad::PEAK, 72.f, 3.f, 6.f, sr);
		}
		showDist = distM;
	}

	void process(float inL, float inR, const Controls& c, float& outL, float& outR) {
		if (ctlCount-- <= 0) {
			ctlCount = 15;
			// wind: slow random wander (target changes every ~1.5 s)
			windCount -= 16.f / sr;
			if (windCount <= 0) { windCount = 0.8f + 1.4f * (0.5f + 0.5f * noise()); windTarget = noise(); }
			windV += (windTarget - windV) * (1.f - std::exp(-16.f / sr / 0.7f));
			updateControlRate(c, false);
		}
		const Rig& p = cur;
		float dryL = inL, dryR = inR;
		float x[2] = {inL * 0.2f, inR * 0.2f};

		// mono-ish source: sound systems are usually mono
		float m = 0.5f * (x[0] + x[1]), s = 0.5f * (x[0] - x[1]) * p.width;
		x[0] = m + s; x[1] = m - s;

		// ---- 1. AMP
		float lvl = std::max(std::fabs(x[0]), std::fabs(x[1])) * drive;
		float aC = lvl > compEnv ? 1.f - std::exp(-1.f / (0.004f * sr)) : 1.f - std::exp(-1.f / (0.15f * sr));
		compEnv += (lvl - compEnv) * aC;
		float th = 0.6f;
		float gr = compEnv > th ? std::pow(compEnv / th, -(1.f - 1.f / compRatio)) : 1.f;
		float makeup = drive < 1.f ? 1.f / drive : 1.f;
		float sub = 0.f, bass[2], mid[2], top[2];
		for (int ch = 0; ch < 2; ch++) {
			float v = smileHi[ch].process(smileLo[ch].process(x[ch]));
			v *= drive * gr;
			v = softClip(v + warm * 0.18f * v * v);
			v = dcBlock[ch].process(v) * makeup;
			// ---- 2. STACK crossover
			float lo, hi, b, h2, mi, tp;
			xo1[ch].split(v, lo, hi);
			xo2[ch].split(hi, b, h2);
			xo3[ch].split(h2, mi, tp);
			sub += 0.5f * lo;
			bass[ch] = b; mid[ch] = mi; top[ch] = tp;
		}

		// SUB bins: mono, scoop resonance, excursion limit, arrive late
		float sb = subScoop.process(subHp2.process(subHp1.process(sub)));
		sb *= subGain;
		sb = softClip(sb * subDrive) / std::pow(subDrive, 0.75f);
		subDel.push(sb);
		sb = subDel.read(std::max(1.f, p.subDelay * 0.001f * sr));
		float as = std::fabs(sb);
		subEnv += (as - subEnv) * (as > subEnv ? 0.01f : 0.0005f);
		// harmonics you can hear on small speakers ("felt" bass)
		float harm = harmBp.process(softClip(3.f * as) - 0.5f * subEnv) * harmAmt;

		// ---- 3. BODY: the sub pushes everything else about a bit
		float pump = 1.f / (1.f + 0.6f * sWeight * std::min(subEnv * 3.f, 2.f));
		float rat = 0.f;
		if (rattleAmt > 0.001f) {
			float exc = std::max(0.f, as - 0.18f);
			float n = noise() * exc * 3.f;
			rat = (rat1.process(n) + 0.6f * rat2.process(n)) * rattleAmt;
		}

		float spk[2];
		for (int ch = 0; ch < 2; ch++) {
			float b = bass[ch] * bassG + harm;
			b = softClip(b * 1.3f) / 1.3f;
			float mi = honk[ch].process(mid[ch]) * midG;
			// compression driver grit (asymmetric)
			mi = softClip(mi * (1.f + 0.5f * warm) + 0.25f * warm * mi * mi) / (1.f + 0.5f * warm);
			midDel[ch].push(mi);
			mi = midDel[ch].read(std::max(1.f, p.midDelay * 0.001f * sr));
			float tp = topLp[ch].process(topShelfF[ch].process(top[ch])) * topG;
			tp = softClip(tp * 1.5f) / 1.5f;
			spk[ch] = sb + (b + mi + tp) * pump + rat * (ch ? 0.6f : 1.f);
		}
		float mEnv = std::fabs(spk[0] - sb), tEnv = std::fabs(top[0]);
		midEnvD += (mEnv - midEnvD) * 0.002f;
		topEnvD += (tEnv - topEnvD) * 0.002f;

		// ---- 4. ROOM
		float w[2];
		for (int ch = 0; ch < 2; ch++) srcLine[ch].push(spk[ch]);
		float msr = 0.001f * sr;
		// early reflections: 8 taps, crossing sides
		static const float erT[8] = {7.f, 11.f, 17.f, 23.f, 31.f, 37.f, 43.f, 53.f};
		static const float erA[8] = {0.8f, 0.7f, 0.62f, 0.55f, 0.45f, 0.4f, 0.32f, 0.27f};
		float er[2] = {0.f, 0.f};
		for (int i = 0; i < 8; i++) {
			int from = i & 1;
			float t = (p.predelay + erT[i] * p.size * (1.f - 0.3f * p.outdoor)) * msr;
			float v = srcLine[from].read(t) * erA[i];
			er[(i >> 1) & 1] += v;
		}
		for (int ch = 0; ch < 2; ch++) er[ch] = erLp[ch].process(er[ch]) * erG;
		// tail: 8-line feedback delay network
		float fin = 0.5f * (srcLine[0].read(p.predelay * msr + 1) + srcLine[1].read(p.predelay * msr + 1)) * (1.f - 0.85f * p.outdoor);
		float o[NFDN];
		for (int i = 0; i < NFDN; i++) o[i] = fdnLp[i].process(fdn[i].read(fdnLen[i])) * fdnG[i];
		// Hadamard mix
		float hm[NFDN];
		for (int i = 0; i < NFDN; i++) hm[i] = o[i];
		for (int len = 1; len < NFDN; len <<= 1)
			for (int i = 0; i < NFDN; i += len << 1)
				for (int j = i; j < i + len; j++) {
					float a = hm[j], b = hm[j + len];
					hm[j] = a + b; hm[j + len] = a - b;
				}
		const float hn = 0.35355339f;
		for (int i = 0; i < NFDN; i++) fdn[i].push(hm[i] * hn + fin * ((i & 1) ? 0.5f : -0.5f));
		float tail[2] = {0.f, 0.f};
		for (int i = 0; i < NFDN; i++) tail[i & 1] += o[i];
		tail[0] *= 0.42f; tail[1] *= 0.42f;
		// slap echo off buildings (Kingston, festival)
		float slap[2] = {0.f, 0.f};
		if (p.slapLevel > 0.001f) {
			float sIn = 0.5f * (spk[0] + spk[1]);
			float rd = slapLp.process(slapLine.read(std::max(1.f, p.slapMs * msr)));
			slapLine.push(sIn + rd * p.slapFb);
			float lv = p.slapLevel * (0.5f + sDist) * (0.6f + 0.8f * sRoom);
			slap[0] = rd * lv; slap[1] = rd * lv * 0.7f;
		}
		// festival delay towers
		float tow[2] = {0.f, 0.f};
		if (p.tower > 0.001f) {
			float lv = p.tower * sDist * (1.f + 0.4f * p.wind * windV);
			tow[0] = towerLp[0].process(srcLine[1].read(towerMs * msr)) * lv;
			tow[1] = towerLp[1].process(srcLine[0].read((towerMs + 9.f) * msr)) * lv;
		}
		for (int ch = 0; ch < 2; ch++) {
			float d = airLp[ch].process(spk[ch]) * directG;
			float v = d + (er[ch] + tail[ch]) * revG + slap[ch] + tow[ch];
			// room boom
			for (int k = 0; k < 3; k++) v = modes[ch][k].process(v);
			w[ch] = v;
		}

		// ---- 5. WALL
		if (p.wall > 0.001f) {
			float wl[2];
			for (int ch = 0; ch < 2; ch++) {
				float v = wallRes[ch].process(wallLp2[ch].process(wallLp1[ch].process(w[ch]))) * 0.9f;
				wl[ch] = v;
			}
			// small tiled room you're standing in
			float tm = 0.5f * (wl[0] + wl[1]);
			tileLine.push(tm);
			float tile0 = tileLine.read(9.f * msr) * 0.35f + tileLine.read(23.f * msr) * 0.25f;
			float tile1 = tileLine.read(13.f * msr) * 0.35f + tileLine.read(31.f * msr) * 0.2f;
			// door rattles in its frame
			float dr = rat * 0.5f;
			wl[0] += tile0 + dr; wl[1] += tile1 + dr * 0.8f;
			for (int ch = 0; ch < 2; ch++) w[ch] = lerpf(w[ch], wl[ch], wallAmt);
		}

		// back to volts, dry/wet, gentle output limit
		float wetL = w[0] * 2.8f, wetR = w[1] * 2.8f;   // trim: a big rig is loud, the output shouldn't be
		outL = outLimit(lerpf(dryL, wetL, sMix));
		outR = outLimit(lerpf(dryR, wetR, sMix));
		if (!std::isfinite(outL) || !std::isfinite(outR)) { outL = outR = 0.f; }
	}

	static inline float outLimit(float v) {
		float a = std::fabs(v);
		if (a < 8.f) return v;
		float y = 8.f + 3.f * std::tanh((a - 8.f) / 3.f);
		return v < 0.f ? -y : y;
	}
};

} // namespace soundsystem
