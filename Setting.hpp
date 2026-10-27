
#pragma once

#include <imgui_internal.h>
#include <imgui.h>

#include <string>
#include <vector>
#include <functional>
#include <utility>
#include <cmath>
#include <cstdint>

#include <nlohmann/json.hpp>

// -------------------------------------------------------------
// Setting types
// -------------------------------------------------------------

enum class SettingType
{
    Bool,
    Number,
    Enum,
    Color,
    String
};

// -------------------------------------------------------------
// Base setting
// -------------------------------------------------------------

class Setting
{
public:
    virtual ~Setting() = default;

    bool parse(const std::string& value);

    std::string mName;
    std::string mDescription;
    SettingType mType;

    bool mDisplay = false;
    bool* mHideOnClickGUI = nullptr;

    std::function<bool()> mIsVisible = []()
        {
            return true;
        };

    Setting(
        std::string name,
        std::string description,
        SettingType type
    )
        : mName(std::move(name)),
        mDescription(std::move(description)),
        mType(type)
    {
    }

    float sliderEase = 0.0f;
    float boolScale = 0.0f;
    bool isDragging = false;

    bool enumExtended = false;
    bool colourExtended = false;
    float enumSlide = 0.0f;
    float colourSlide = 0.0f;

    virtual nlohmann::json serialize()
    {
        nlohmann::json j;
        j["name"] = mName;
        j["type"] = static_cast<int>(mType);
        return j;
    }
};

// -------------------------------------------------------------
// Visibility condition
// -------------------------------------------------------------

#define VISIBILITY_CONDITION(setting, condition) \
    setting.mIsVisible = std::function<bool()>([&]() { return condition; })

// -------------------------------------------------------------
// Shared enum interface
// -------------------------------------------------------------

class EnumSettingBase : public Setting
{
public:
    EnumSettingBase(
        std::string name,
        std::string description
    )
        : Setting(
            std::move(name),
            std::move(description),
            SettingType::Enum
        )
    {
    }

    virtual int getIndex() const = 0;

    virtual int getOptionCount() const = 0;

    virtual const std::string& getOptionName(int index) const = 0;

    virtual void setIndex(int index) = 0;
};

// -------------------------------------------------------------
// Boolean setting
// -------------------------------------------------------------

class BoolSetting : public Setting
{
public:
    bool mValue = false;
    int mKey = -1;

    BoolSetting(
        std::string name,
        std::string description,
        bool value,
        int key = -1
    )
        : Setting(
            std::move(name),
            std::move(description),
            SettingType::Bool
        ),
        mValue(value),
        mKey(key)
    {
    }

    void setValue(bool value)
    {
        mValue = value;
    }

    std::string getName()
    {
        return mName;
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["boolValue"] = mValue;
        j["key"] = mKey;
        return j;
    }

    explicit operator bool() const
    {
        return mValue;
    }
};

// -------------------------------------------------------------
// Number setting
// -------------------------------------------------------------

class NumberSetting : public Setting
{
public:
    float mValue = 0.0f;
    float mMin = 0.0f;
    float mMax = 0.0f;
    float mStep = 0.0f;

    NumberSetting(
        std::string name,
        std::string description,
        float value,
        float min,
        float max,
        float step
    )
        : Setting(
            std::move(name),
            std::move(description),
            SettingType::Number
        ),
        mValue(value),
        mMin(min),
        mMax(max),
        mStep(step)
    {
    }

    void setValue(float value)
    {
        if (mStep > 0.0f)
            mValue = std::round(value / mStep) * mStep;
        else
            mValue = value;

        mValue = std::clamp(mValue, mMin, mMax);
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["numberValue"] = mValue;
        return j;
    }

    template <typename T>
    T as() const
    {
        return static_cast<T>(mValue);
    }
};

// -------------------------------------------------------------
// Integer-based enum setting
// -------------------------------------------------------------

class EnumSetting : public EnumSettingBase
{
public:
    int mValue = 0;
    std::vector<std::string> mValues;

    EnumSetting(
        std::string name,
        std::string description,
        int index,
        std::vector<std::string> values
    )
        : EnumSettingBase(
            std::move(name),
            std::move(description)
        ),
        mValue(index),
        mValues(std::move(values))
    {
    }

    template <typename IndexType, typename... Args>
    EnumSetting(
        std::string name,
        std::string description,
        IndexType index,
        Args... values
    )
        : EnumSettingBase(
            std::move(name),
            std::move(description)
        ),
        mValue(static_cast<int>(index)),
        mValues{ values... }
    {
    }

    void setValue(int value)
    {
        if (!mValues.empty())
            mValue = std::clamp(
                value,
                0,
                static_cast<int>(mValues.size()) - 1
            );
        else
            mValue = 0;
    }

    int getIndex() const override
    {
        return mValue;
    }

    int getOptionCount() const override
    {
        return static_cast<int>(mValues.size());
    }

    const std::string& getOptionName(int index) const override
    {
        return mValues.at(static_cast<std::size_t>(index));
    }

    void setIndex(int index) override
    {
        setValue(index);
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["enumValue"] = mValue;
        return j;
    }

    template <typename T>
    explicit operator T() const
    {
        return static_cast<T>(mValue);
    }

    template <typename T>
    T as() const
    {
        return static_cast<T>(mValue);
    }
};

// -------------------------------------------------------------
// Typed enum setting
// -------------------------------------------------------------

template <typename T>
class EnumSettingT : public EnumSettingBase
{
public:
    T mValue;
    std::vector<std::string> mValues;

    EnumSettingT(
        std::string name,
        std::string description,
        T index,
        std::vector<std::string> values
    )
        : EnumSettingBase(
            std::move(name),
            std::move(description)
        ),
        mValue(index),
        mValues(std::move(values))
    {
    }

    template <typename... Args>
    EnumSettingT(
        std::string name,
        std::string description,
        T index,
        Args... values
    )
        : EnumSettingBase(
            std::move(name),
            std::move(description)
        ),
        mValue(index),
        mValues{ values... }
    {
    }

    void setValue(T value)
    {
        mValue = value;
    }

    int getIndex() const override
    {
        return static_cast<int>(mValue);
    }

    int getOptionCount() const override
    {
        return static_cast<int>(mValues.size());
    }

    const std::string& getOptionName(int index) const override
    {
        return mValues.at(static_cast<std::size_t>(index));
    }

    void setIndex(int index) override
    {
        if (!mValues.empty())
        {
            index = std::clamp(
                index,
                0,
                static_cast<int>(mValues.size()) - 1
            );

            mValue = static_cast<T>(index);
        }
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["enumValue"] = static_cast<int>(mValue);
        return j;
    }

    template <typename Type>
    Type as() const
    {
        return static_cast<Type>(mValue);
    }
};

// -------------------------------------------------------------
// Color setting
// -------------------------------------------------------------

class ColorSetting : public Setting
{
public:
    float mValue[4] = {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    bool mIsExtended = false;
    float mSlide = 0.0f;

    static inline float mColorPickerSize = 400.0f;

    ColorSetting(
        std::string name,
        std::string description,
        float r,
        float g,
        float b,
        float a
    )
        : Setting(
            std::move(name),
            std::move(description),
            SettingType::Color
        )
    {
        mValue[0] = r;
        mValue[1] = g;
        mValue[2] = b;
        mValue[3] = a;
    }

    ColorSetting(
        std::string name,
        std::string description,
        uint64_t color
    )
        : Setting(
            std::move(name),
            std::move(description),
            SettingType::Color
        )
    {
        mValue[0] = ((color >> 16) & 0xFF) / 255.0f;
        mValue[1] = ((color >> 8) & 0xFF) / 255.0f;
        mValue[2] = (color & 0xFF) / 255.0f;
        mValue[3] = ((color >> 24) & 0xFF) / 255.0f;
    }

    void setValue(float r, float g, float b, float a)
    {
        mValue[0] = r;
        mValue[1] = g;
        mValue[2] = b;
        mValue[3] = a;
    }

    ImColor getAsImColor()
    {
        return {
            mValue[0],
            mValue[1],
            mValue[2],
            mValue[3]
        };
    }

    void setFromImColor(const ImColor& color)
    {
        mValue[0] = color.Value.x;
        mValue[1] = color.Value.y;
        mValue[2] = color.Value.z;
        mValue[3] = color.Value.w;
    }

    void setFromImVec4(const ImVec4& color)
    {
        mValue[0] = color.x;
        mValue[1] = color.y;
        mValue[2] = color.z;
        mValue[3] = color.w;
    }

    ImVec4 getAsImVec4()
    {
        return {
            mValue[0],
            mValue[1],
            mValue[2],
            mValue[3]
        };
    }

    void setColor(float r, float g, float b, float a)
    {
        setValue(r, g, b, a);
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();

        j["colorValue"] = {
            mValue[0],
            mValue[1],
            mValue[2],
            mValue[3]
        };

        return j;
    }

    void setFromHex(unsigned long val)
    {
        mValue[0] = ((val >> 16) & 0xFF) / 255.0f;
        mValue[1] = ((val >> 8) & 0xFF) / 255.0f;
        mValue[2] = (val & 0xFF) / 255.0f;
        mValue[3] = ((val >> 24) & 0xFF) / 255.0f;
    }
};
