#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "DreamShare.h"
#include <functional>
#include <memory>

namespace BabyGirl
{
    /**
     * DreamApi Cloud Client
     * Talks to the DreamShare Cloudflare Worker (see /worker):
     *   GET  /api/presets?query=&tag=   -> { presets: [...] }
     *   POST /api/presets               -> { success: true, preset: {...} }
     * Requests run on a background thread; callbacks are delivered on the message thread.
     * A shared alive-flag makes callbacks safe if the editor closes while a request is in flight.
     */
    class DreamApi
    {
    public:
        explicit DreamApi(const juce::String& endpoint = "https://dreamshare-api.keganacummings.workers.dev")
            : apiBaseUrl(endpoint) {}

        ~DreamApi() { *alive = false; }

        void setBaseUrl(const juce::String& url) { apiBaseUrl = url; }
        const juce::String& getBaseUrl() const { return apiBaseUrl; }

        void fetchCloudPresets(const juce::String& query, const juce::String& tag,
                               std::function<void(std::vector<DreamPreset>, bool)> callback)
        {
            juce::URL url(apiBaseUrl + "/api/presets?query=" + juce::URL::addEscapeChars(query, true)
                          + "&tag=" + juce::URL::addEscapeChars(tag, true));
            auto flag = alive;
            juce::Thread::launch([url, callback, flag]()
            {
                std::vector<DreamPreset> results;
                bool ok = false;
                auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(8000);
                if (auto stream = url.createInputStream(opts))
                {
                    auto json = juce::JSON::parse(stream->readEntireStreamAsString());
                    if (auto* obj = json.getDynamicObject())
                        if (auto* list = obj->getProperty("presets").getArray())
                        {
                            for (auto& item : *list) results.push_back(DreamPreset::fromJson(item));
                            ok = true;
                        }
                }
                juce::MessageManager::callAsync([callback, results = std::move(results), ok, flag]()
                {
                    if (*flag) callback(results, ok);
                });
            });
        }

        void publishPreset(const DreamPreset& preset, std::function<void(bool)> callback)
        {
            juce::URL url(apiBaseUrl + "/api/presets");
            const juce::String body = juce::JSON::toString(preset.toJson(), true);
            auto flag = alive;
            juce::Thread::launch([url, body, callback, flag]()
            {
                bool ok = false;
                auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                .withExtraHeaders("Content-Type: application/json\r\n")
                                .withHttpRequestCmd("POST")
                                .withConnectionTimeoutMs(10000);
                if (auto stream = url.withPOSTData(body).createInputStream(opts))
                {
                    const auto response = stream->readEntireStreamAsString();
                    ok = response.contains("\"success\"") && response.contains("true");
                }
                juce::MessageManager::callAsync([callback, ok, flag]() { if (*flag) callback(ok); });
            });
        }

    private:
        juce::String apiBaseUrl;
        std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    };
}
