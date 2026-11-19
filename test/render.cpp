// Offline checks + demo WAV for the Sound System engine (no Rack needed).
//   make test    (writes test/soundsystem-demo.wav)
// A little techno loop (kick, sub bass, hats, chord stab) goes through each
// of the 8 systems for 8 seconds: 4 s dry-ish first bar comparison is left
// to the ear; the checks are no NaNs, sane peaks, sane loudness.
// Extra passes: DISTANCE sweep on Festival, DOOR opening on Through the wall.
#include "../src/SoundSystemDSP.hpp"
#include <cstdio>
#include <string>
using namespace soundsystem;

static_assert(sizeof(Rig) == 42 * sizeof(float), "Rig has 42 fields");

static void writeWav(const char* path, const std::vector<float>& lr, int sr) {
	FILE* f = fopen(path, "wb");
	if (!f) return;
	int n = (int) lr.size();
	int bytes = n * 2;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (float v : lr) { float s = std::max(-1.f, std::min(1.f, v / 10.f)); int16_t q = (int16_t) (s * 32767); fwrite(&q, 2, 1, f); }
	fclose(f);
}

struct Loop {
	float sr;
	float kph = 0, bph = 0, cph[3] = {0, 0, 0}, hp = 0;
	uint32_t rs = 7;
	float noise() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs >> 8) / 8388608.f - 1.f; }
	// returns stereo volts
	void next(float t, float& L, float& R) {
		const float bpm = 128.f;
		float beat = std::fmod(t * bpm / 60.f, 1.f);
		float step = std::fmod(t * bpm / 15.f, 1.f);       // 16ths
		int s16 = (int) std::fmod(t * bpm / 15.f, 16.f);
		// kick
		kph += (48.f + 140.f * std::exp(-beat * 35.f)) / sr; if (kph >= 1) kph -= 1;
		float kick = 5.f * std::exp(-beat * 7.f) * std::sin(2 * PI * kph);
		// rolling sub bass on the offbeat 16ths (F1 / Ab1)
		static const float notes[16] = {0, 0, 43.65f, 43.65f, 0, 0, 43.65f, 51.91f, 0, 0, 43.65f, 43.65f, 0, 0, 38.89f, 43.65f};
		float bf = notes[s16];
		bph += (bf > 0 ? bf : 43.65f) / sr; if (bph >= 1) bph -= 1;
		float benv = bf > 0 ? std::exp(-step * 2.5f) : 0.f;
		float bass = 2.6f * benv * std::sin(2 * PI * bph + 0.8f * std::sin(2 * PI * bph));
		// hats
		float n = noise(); float hh = n - hp; hp = n;
		float hat = 1.2f * hh * std::exp(-std::fmod(beat + 0.5f, 1.f) * 40.f);
		// chord stab every 3 16ths (Fm7)
		static const float ch[3] = {349.2f, 415.3f, 523.3f};
		float stab = 0;
		float sp = std::fmod(t * bpm / 15.f, 3.f) / 3.f;
		for (int i = 0; i < 3; i++) { cph[i] += ch[i] / sr; if (cph[i] >= 1) cph[i] -= 1; stab += 2 * cph[i] - 1; }
		stab *= 0.7f * std::exp(-sp * 9.f);
		L = kick + bass + hat * 0.8f + stab * 1.1f;
		R = kick + bass + hat * 1.1f + stab * 0.8f;
	}
};

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	const int SR = 48000;
	std::vector<float> wav;
	int fails = 0;

	// dry reference first
	{
		Loop lp{(float) SR};
		for (int i = 0; i < SR * 4; i++) { float L, R; lp.next(i / (float) SR, L, R); wav.push_back(L); wav.push_back(R); }
		for (int i = 0; i < SR; i++) { wav.push_back(0); wav.push_back(0); }
	}

	auto run = [&](const char* label, int rig, float secs, auto ctlFn) {
		Engine e;
		e.setSampleRate(SR);
		Loop lp{(float) SR};
		Controls c; c.rig = rig;
		double sIn = 0, sOut = 0; float peak = 0; bool nan = false;
		int N = (int) (SR * secs);
		for (int i = 0; i < N; i++) {
			float t = i / (float) SR;
			ctlFn(c, t);
			float L, R, oL, oR;
			lp.next(t, L, R);
			e.process(L, R, c, oL, oR);
			if (!std::isfinite(oL) || !std::isfinite(oR)) nan = true;
			peak = std::max(peak, std::max(std::fabs(oL), std::fabs(oR)));
			if (t > 1.f) { sIn += L * L + R * R; sOut += oL * oL + oR * oR; }
			wav.push_back(oL); wav.push_back(oR);
		}
		for (int i = 0; i < SR / 2; i++) { wav.push_back(0); wav.push_back(0); }
		float db = 10.f * std::log10((sOut + 1e-12) / (sIn + 1e-12));
		bool ok = !nan && peak < 11.5f && db > -12.f && db < 8.f;
		printf("%-18s %-22s peak %5.2f V  level %+5.1f dB  dist %5.1f m  RT %.2f s  %s\n", RIG_NAMES[rig], label, peak, db, e.showDist, e.showRt, ok ? "ok" : "FAIL");
		if (!ok) fails++;
	};

	for (int r = 0; r < NUM_RIGS; r++)
		run("default", r, 8.f, [](Controls&, float) {});
	run("distance sweep", FESTIVAL, 10.f, [](Controls& c, float t) { c.dist = std::min(1.f, t / 9.f); });
	run("door opens at 4 s", WALL, 9.f, [](Controls& c, float t) { c.door = t > 4.f && t < 7.f; });
	run("push full", WAREHOUSE, 6.f, [](Controls& c, float) { c.push = 1.f; c.weight = 1.f; c.room = 1.f; });
	run("push zero", LOFT, 6.f, [](Controls& c, float) { c.push = 0.f; c.weight = 0.f; c.room = 0.f; c.crowd = 1.f; });
	run("system sweep", DUB, 16.f, [](Controls& c, float t) { c.rig = std::min(7, (int) (t / 2.f)); });

	writeWav((dir + "/test/soundsystem-demo.wav").c_str(), wav, SR);
	printf(fails ? "%d FAILED\n" : "all ok\n", fails);
	return fails ? 1 : 0;
}
