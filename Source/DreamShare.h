#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace BabyGirl
{
    /** One captured parameter: numeric value, or (for machine choosers) the machine name. */
    struct PresetSetting
    {
        juce::String id;
        float value = 0.0f;
        juce::String text;
    };

    /**
     * DreamShare Cloud & Preset Data Structure.
     * The first 15 fields are the original layout (kept for compatibility); `settings` carries
     * everything else (machine rack, synth, drums, drum pattern) so shared patches restore completely.
     */
    struct DreamPreset
    {
        juce::String id;
        juce::String name;
        juce::String author;
        juce::String category;
        juce::String tags;
        float tubeDrive = 3.8f;
        float filterCutoff = 1850.0f;
        float filterReso = 0.48f;
        float tapeTime = 0.36f;
        float tapeFlutter = 0.32f;
        float vinylCrackle = 0.25f;
        float tapeWarp = 0.28f;
        float shimmerMix = 0.35f;
        float stereoWidth = 1.25f;
        float ironDrive = 0.45f;
        std::vector<PresetSetting> settings;
        int stars = 0;
        bool cloud = false;
        bool legacyOnly = true;      // true: apply the 10 legacy fields (and enable Hush/Kyoto/Master)

        juce::var toJson() const
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", id);
            obj->setProperty("name", name);
            obj->setProperty("author", author);
            obj->setProperty("category", category);
            juce::Array<juce::var> tagArr;
            for (auto& t : juce::StringArray::fromTokens(tags, ",", ""))
                if (t.trim().isNotEmpty()) tagArr.add(t.trim());
            obj->setProperty("tags", tagArr);

            auto* patch = new juce::DynamicObject();
            patch->setProperty("tubeDrive", tubeDrive);
            patch->setProperty("filterCutoff", filterCutoff);
            patch->setProperty("filterReso", filterReso);
            patch->setProperty("tapeTime", tapeTime);
            patch->setProperty("tapeFlutter", tapeFlutter);
            patch->setProperty("vinylCrackle", vinylCrackle);
            patch->setProperty("tapeWarp", tapeWarp);
            patch->setProperty("shimmerMix", shimmerMix);
            patch->setProperty("stereoWidth", stereoWidth);
            patch->setProperty("ironDrive", ironDrive);
            obj->setProperty("patch", juce::var(patch));

            juce::Array<juce::var> arr;
            for (auto& s : settings)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty("id", s.id);
                if (s.text.isNotEmpty()) o->setProperty("text", s.text); else o->setProperty("v", s.value);
                arr.add(juce::var(o));
            }
            obj->setProperty("settings", arr);
            return juce::var(obj);
        }

        static DreamPreset fromJson(const juce::var& json)
        {
            DreamPreset p;
            p.cloud = true;
            auto* obj = json.getDynamicObject();
            if (obj == nullptr) return p;

            p.id = obj->getProperty("id").toString();
            p.name = obj->getProperty("name").toString();
            p.author = obj->getProperty("author").toString();
            p.category = obj->getProperty("category").toString();
            p.stars = (int)obj->getProperty("stars");

            const auto tg = obj->getProperty("tags");
            if (auto* arr = tg.getArray())
            {
                juce::StringArray sa;
                for (auto& t : *arr) sa.add(t.toString());
                p.tags = sa.joinIntoString(", ");
            }
            else p.tags = tg.toString();

            // The worker nests numeric fields under "patch"; older clients sent them flat.
            juce::var src = obj->getProperty("patch");
            auto* po = src.getDynamicObject();
            if (po == nullptr) po = obj;
            auto num = [&](const char* key, float& dst) { if (po->hasProperty(key)) dst = (float)po->getProperty(key); };
            num("tubeDrive", p.tubeDrive); num("filterCutoff", p.filterCutoff); num("filterReso", p.filterReso);
            num("tapeTime", p.tapeTime); num("tapeFlutter", p.tapeFlutter); num("vinylCrackle", p.vinylCrackle);
            num("tapeWarp", p.tapeWarp); num("shimmerMix", p.shimmerMix); num("stereoWidth", p.stereoWidth); num("ironDrive", p.ironDrive);

            if (auto* arr = obj->getProperty("settings").getArray())
            {
                for (auto& item : *arr)
                {
                    if (auto* o = item.getDynamicObject())
                    {
                        PresetSetting s;
                        s.id = o->getProperty("id").toString();
                        if (o->hasProperty("text")) s.text = o->getProperty("text").toString();
                        else s.value = (float)o->getProperty("v");
                        if (s.id.isNotEmpty()) p.settings.push_back(s);
                    }
                }
                p.legacyOnly = p.settings.empty();
            }
            return p;
        }
    };
}
