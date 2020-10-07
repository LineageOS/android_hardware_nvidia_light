/*
 * Copyright 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Light"

#include "Light.h"

#include <log/log.h>

#include <experimental/filesystem>

#define BACKLIGHT_DIR             "/sys/class/backlight"
#define BACKLIGHT_NODE            "brightness"
#define ROTHLED_NODE              "/sys/class/leds/roth-led/brightness"
#define LIGHTBAR_NODE             "/sys/clas/leds/led-lightbar/brightness"
#define LIGHTBAR_STATE            "/sys/clas/leds/led-lightbar/mode"
#define NVSHIELDLED_DIR           "/sys/class/nvshieldled"
#define NVSHIELDLED_POWER_NODE    "brightness"
#define NVSHIELDLED_POWER_STATE   "state"
#define NVSHIELDLED_BUTTONS_NODE  "brightness2"
#define NVSHIELDLED_BUTTONS_STATE "state2"

namespace {
using android::hardware::light::V2_0::LightState;

static uint32_t rgbToBrightness(const LightState& state) {
    uint32_t color = state.color & 0x00ffffff;
    return ((77 * ((color >> 16) & 0xff)) + (150 * ((color >> 8) & 0xff)) +
            (29 * (color & 0xff))) >> 8;
}
} // anonymous namespace

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

Light::Light() {
    auto backlightFn(std::bind(&Light::setBacklight,    this, std::placeholders::_1));
    auto buttonsFn  (std::bind(&Light::setButtonsLight, this, std::placeholders::_1));

    for (auto & node : std::experimental::filesystem::directory_iterator(BACKLIGHT_DIR)) {
        if (mBacklight.open(node.path().string() + "/" BACKLIGHT_NODE), mBacklight.is_open()) {
            ALOGI("Found backlight node: %s", node.path().string().c_str());
            mLights.emplace(std::make_pair(Type::BACKLIGHT, backlightFn));
	    break;
	}
    }

    if (mPowerLed.open(ROTHLED_NODE), mPowerLed.is_open()) {
        ALOGI("Found roth led node");
        mLights.emplace(std::make_pair(Type::BUTTONS, buttonsFn));
    } else if (mPowerLed.open(LIGHTBAR_NODE), mPowerLed.is_open()) {
        ALOGI("Found lightbar node");
        mPowerLedState.open(LIGHTBAR_STATE);
        mLights.emplace(std::make_pair(Type::BUTTONS, buttonsFn));
    } else if (std::experimental::filesystem::exists(NVSHIELDLED_DIR)) {
        for (auto & node : std::experimental::filesystem::directory_iterator(NVSHIELDLED_DIR)) {
            ALOGI("Found nvshieldled node: %s", node.path().string().c_str());
            mPowerLed.open(node.path().string() + "/" NVSHIELDLED_POWER_NODE);
            mPowerLedState.open(node.path().string() + "/" NVSHIELDLED_POWER_STATE);
            mButtonLeds.open(node.path().string() + "/" NVSHIELDLED_BUTTONS_NODE);
            mButtonLedsState.open(node.path().string() + "/" NVSHIELDLED_BUTTONS_STATE);
            if (mPowerLedState.is_open() || mButtonLedsState.is_open())
                mLights.emplace(std::make_pair(Type::BUTTONS, buttonsFn));
        }
    }
}

// Methods from ::android::hardware::light::V2_0::ILight follow.
Return<Status> Light::setLight(Type type, const LightState& state) {
    auto it = mLights.find(type);

    if (it == mLights.end()) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    it->second(state);

    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (auto const& light : mLights) {
        types.push_back(light.first);
    }

    _hidl_cb(types);

    return Void();
}

void Light::setBacklight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);

    int brightness = rgbToBrightness(state);

    if (mPowerLedState.is_open())
        mPowerLedState   << (brightness ? "normal" : "breathe") << std::endl;

    mBacklight << brightness << std::endl;
}

void Light::setButtonsLight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);

    int brightness = rgbToBrightness(state);

    // Do not turn off power led, only set its brightness
    if (mPowerLed.is_open() && brightness)
        mPowerLed << brightness << std::endl;

    if (mButtonLeds.is_open())
        mButtonLeds << brightness << std::endl;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
