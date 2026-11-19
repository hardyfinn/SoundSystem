// Sound System: makes the input sound like it is coming out of a big physical
// sound system in a real space. SYSTEM picks the rig and the place, PUSH
// drives it, DISTANCE walks you from the front of the stack to the back.
// The sound engine is in SoundSystemDSP.hpp.
#include "plugin.hpp"
#include "SoundSystemDSP.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 6.0f, DISP_Y = 16.0f, DISP_W = 89.6f, DISP_H = 20.0f;
static const float SYS_X = 17.0f, BIG_Y = 53.0f;
static const float PUSH_X = 50.8f;
static const float DIST_X = 84.6f;
static const float COL5[5] = {12.7f, 31.75f, 50.8f, 69.85f, 88.9f};
static const float ROW2_Y = 77.0f;
static const float CV_Y = 95.0f;
static const float COL6[6] = {9.5f, 26.0f, 42.5f, 59.1f, 75.6f, 92.1f};
static const float IO_Y = 113.0f;

struct SoundSystem : Module {
	enum ParamIds {
		SYSTEM_PARAM, PUSH_PARAM, DIST_PARAM,
		WEIGHT_PARAM, ROOM_PARAM, TOPS_PARAM, CROWD_PARAM, MIX_PARAM,
		DOOR_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		SYSTEM_INPUT, PUSH_INPUT, DIST_INPUT, WEIGHT_INPUT, ROOM_INPUT,
		DOOR_INPUT, IN_L_INPUT, IN_R_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT, OUT_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		DOOR_LIGHT,
		NUM_LIGHTS
	};

	soundsystem::Engine engine;
	dsp::SchmittTrigger doorButton;
	bool doorLatched = false;

	// for the screen
	int dispRig = 0;
	float dispDist = 7.f, dispRt = 2.f, dispSub = 0.f, dispMid = 0.f, dispTop = 0.f, dispDoor = 0.f;

	SoundSystem() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		std::vector<std::string> names;
		for (int i = 0; i < soundsystem::NUM_RIGS; i++)
			names.push_back(string::f("%d %s", i + 1, soundsystem::RIG_NAMES[i]));
		configSwitch(SYSTEM_PARAM, 0.f, soundsystem::NUM_RIGS - 1, 0.f, "System", names);
		configParam(PUSH_PARAM, 0.f, 1.f, 0.5f, "Push (how hard the rig is driven)", "%", 0.f, 100.f);
		configParam(DIST_PARAM, 0.f, 1.f, 0.35f, "Distance (front of the stack to the back of the place)", "%", 0.f, 100.f);
		configParam(WEIGHT_PARAM, 0.f, 1.f, 0.5f, "Weight (sub bins and bass you feel)", "%", 0.f, 100.f);
		configParam(ROOM_PARAM, 0.f, 1.f, 0.5f, "Room (how much of the space you hear)", "%", 0.f, 100.f);
		configParam(TOPS_PARAM, 0.f, 1.f, 0.5f, "Tops (horns and tweeters)", "%", 0.f, 100.f);
		configParam(CROWD_PARAM, 0.f, 1.f, 0.3f, "Crowd (bodies soak up the room)", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix (dry to sound system)", "%", 0.f, 100.f);
		configButton(DOOR_PARAM, "Door (Through the wall: open/close)");
		configInput(SYSTEM_INPUT, "System CV (0-10 V steps through all 8)");
		configInput(PUSH_INPUT, "Push CV (10 V = full)");
		configInput(DIST_INPUT, "Distance CV (10 V = full)");
		configInput(WEIGHT_INPUT, "Weight CV (10 V = full)");
		configInput(ROOM_INPUT, "Room CV (10 V = full)");
		configInput(DOOR_INPUT, "Door gate (high = door open, flips the button)");
		configInput(IN_L_INPUT, "Left (or mono)");
		configInput(IN_R_INPUT, "Right (normalled to left)");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
		configLight(DOOR_LIGHT, "Door open");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}
	void onAdd(const AddEvent& e) override {
		engine.setSampleRate(APP->engine->getSampleRate());
	}
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		doorLatched = false;
		engine.setSampleRate(APP->engine->getSampleRate());
	}

	float knob(int p, int in) {
		return clamp(params[p].getValue() + inputs[in].getVoltage() * 0.1f, 0.f, 1.f);
	}

	void process(const ProcessArgs& args) override {
		if (doorButton.process(params[DOOR_PARAM].getValue()))
			doorLatched = !doorLatched;
		bool gate = inputs[DOOR_INPUT].getVoltage() >= 1.f;

		soundsystem::Controls c;
		float s = params[SYSTEM_PARAM].getValue() + inputs[SYSTEM_INPUT].getVoltage() * (soundsystem::NUM_RIGS / 10.f);
		c.rig = clamp((int) std::floor(s + 0.5f), 0, soundsystem::NUM_RIGS - 1);
		c.push = knob(PUSH_PARAM, PUSH_INPUT);
		c.dist = knob(DIST_PARAM, DIST_INPUT);
		c.weight = knob(WEIGHT_PARAM, WEIGHT_INPUT);
		c.room = knob(ROOM_PARAM, ROOM_INPUT);
		c.tops = params[TOPS_PARAM].getValue();
		c.crowd = params[CROWD_PARAM].getValue();
		c.mix = params[MIX_PARAM].getValue();
		c.door = doorLatched != gate;

		float inL = inputs[IN_L_INPUT].getVoltage();
		float inR = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() : inL;
		float oL, oR;
		engine.process(inL, inR, c, oL, oR);
		outputs[OUT_L_OUTPUT].setVoltage(oL);
		outputs[OUT_R_OUTPUT].setVoltage(oR);

		lights[DOOR_LIGHT].setBrightness(c.door ? 1.f : 0.f);
		dispRig = c.rig;
		dispDist = engine.showDist;
		dispRt = engine.showRt;
		dispSub = engine.subEnv;
		dispMid = engine.midEnvD;
		dispTop = engine.topEnvD;
		dispDoor = engine.sDoor;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "doorOpen", json_boolean(doorLatched));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "doorOpen"))) doorLatched = json_boolean_value(j);
	}
};

// The screen: system name and what it is, distance and reverb time, and a
// little speaker stack whose cones move with the music.
struct StackDisplay : Widget {
	SoundSystem* module = nullptr;
	float subPh = 0.f;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		int rig = 0;
		float dist = 7.1f, rt = 1.7f, sub = 0.25f, mid = 0.15f, top = 0.05f, door = 0.f;
		if (module) {
			rig = module->dispRig; dist = module->dispDist; rt = module->dispRt;
			sub = module->dispSub; mid = module->dispMid; top = module->dispTop; door = module->dispDoor;
		}
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font)
			return;
		float w = box.size.x, h = box.size.y;
		const NVGcolor ink = nvgRGB(240, 230, 210), soft = nvgRGB(170, 160, 145), dim = nvgRGB(115, 106, 96);
		const NVGcolor amber = nvgRGB(245, 170, 60), box_ = nvgRGB(52, 47, 43), cone = nvgRGB(24, 22, 21);
		float pad = mm2px(2.4f);
		nvgFontFaceId(args.vg, font->handle);

		nvgFontSize(args.vg, mm2px(4.6f));
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFillColor(args.vg, ink);
		std::string name = string::f("%d %s", rig + 1, soundsystem::RIG_NAMES[rig]);
		nvgText(args.vg, pad, mm2px(6.2f), name.c_str(), NULL);

		nvgFontSize(args.vg, mm2px(2.9f));
		nvgFillColor(args.vg, soft);
		nvgText(args.vg, pad, mm2px(11.4f), soundsystem::RIG_BLURBS[rig], NULL);

		nvgFontSize(args.vg, mm2px(2.9f));
		nvgFillColor(args.vg, amber);
		std::string info = dist < 10.f ? string::f("%.1f m from the stack", dist) : string::f("%d m from the stack", (int) (dist + 0.5f));
		nvgText(args.vg, pad, mm2px(16.4f), info.c_str(), NULL);
		nvgFillColor(args.vg, dim);
		std::string rts = string::f("room %.1f s", rt);
		if (rig == soundsystem::WALL)
			rts = door > 0.5f ? "door OPEN" : "door shut";
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, w - mm2px(17.f), mm2px(16.4f), rts.c_str(), NULL);

		// speaker stack: two scoop bins, a mid horn box, a row of tweeters
		float sx = w - mm2px(14.5f), sw = mm2px(12.f);
		float y0 = mm2px(1.6f), sh = h - mm2px(3.2f);
		float binH = sh * 0.3f;
		for (int b = 0; b < 2; b++) {
			float by = y0 + sh - binH * (b + 1);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, sx, by, sw, binH - mm2px(0.4f));
			nvgFillColor(args.vg, box_);
			nvgFill(args.vg);
			float r = binH * 0.34f * (1.f + clamp(sub * 1.4f, 0.f, 0.35f));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, sx + sw / 2, by + binH / 2 - mm2px(0.2f), r);
			nvgFillColor(args.vg, cone);
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, nvgLerpRGBA(dim, amber, clamp(sub * 3.f, 0.f, 1.f)));
			nvgStrokeWidth(args.vg, mm2px(0.35f));
			nvgStroke(args.vg);
		}
		float midH = sh * 0.24f, my = y0 + sh - 2 * binH - midH;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, sx, my, sw, midH - mm2px(0.4f));
		nvgFillColor(args.vg, box_);
		nvgFill(args.vg);
		// horn mouth
		float hm = clamp(mid * 5.f, 0.f, 1.f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, sx + mm2px(1.2f), my + mm2px(0.8f));
		nvgLineTo(args.vg, sx + sw - mm2px(1.2f), my + mm2px(0.8f));
		nvgLineTo(args.vg, sx + sw / 2 + mm2px(1.f), my + midH - mm2px(1.4f));
		nvgLineTo(args.vg, sx + sw / 2 - mm2px(1.f), my + midH - mm2px(1.4f));
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgLerpRGBA(cone, nvgRGB(90, 70, 40), hm));
		nvgFill(args.vg);
		float ty = my - mm2px(2.4f);
		float tl = clamp(top * 12.f, 0.f, 1.f);
		for (int i = 0; i < 3; i++) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, sx + sw * (0.2f + 0.3f * i), ty, mm2px(0.9f));
			nvgFillColor(args.vg, nvgLerpRGBA(box_, amber, tl));
			nvgFill(args.vg);
		}
		// the wall, when you're behind it
		if (rig == soundsystem::WALL && door < 0.98f) {
			nvgBeginPath(args.vg);
			nvgRect(args.vg, sx - mm2px(2.2f), y0, mm2px(1.4f) * (1.f - door) + mm2px(0.2f), sh);
			nvgFillColor(args.vg, nvgRGB(140, 128, 115));
			nvgFill(args.vg);
		}
		Widget::drawLayer(args, layer);
	}
};

struct SoundSystemWidget : ModuleWidget {
	SoundSystemWidget(SoundSystem* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SoundSystem.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		StackDisplay* d = createWidget<StackDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(SYS_X, BIG_Y)), module, SoundSystem::SYSTEM_PARAM));
		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(PUSH_X, BIG_Y)), module, SoundSystem::PUSH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(DIST_X, BIG_Y)), module, SoundSystem::DIST_PARAM));

		const int row2[5] = {SoundSystem::WEIGHT_PARAM, SoundSystem::ROOM_PARAM, SoundSystem::TOPS_PARAM, SoundSystem::CROWD_PARAM, SoundSystem::MIX_PARAM};
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL5[i], ROW2_Y)), module, row2[i]));

		const int cv[5] = {SoundSystem::SYSTEM_INPUT, SoundSystem::PUSH_INPUT, SoundSystem::DIST_INPUT, SoundSystem::WEIGHT_INPUT, SoundSystem::ROOM_INPUT};
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL5[i], CV_Y)), module, cv[i]));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL6[0], IO_Y)), module, SoundSystem::IN_L_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL6[1], IO_Y)), module, SoundSystem::IN_R_INPUT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(COL6[2], IO_Y)), module, SoundSystem::DOOR_PARAM, SoundSystem::DOOR_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL6[3], IO_Y)), module, SoundSystem::DOOR_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL6[4], IO_Y)), module, SoundSystem::OUT_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL6[5], IO_Y)), module, SoundSystem::OUT_R_OUTPUT));
	}
};

Model* modelSoundSystem = createModel<SoundSystem, SoundSystemWidget>("SoundSystem");
