// Typed feature model. Every user-facing control is described by a FeatureDef
// (static metadata, usually loaded from the Lua menu definition) and backed by a
// FeatureValue stored in the FeatureRegistry. UI code never owns state itself.
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace osm {

enum class FeatureKind : uint8_t { Toggle, SliderFloat, SliderInt, Choice, Action };

// How a feature is backed at runtime. This is surfaced in the UI as a badge so
// simulated behaviour is never presented as a working integration.
enum class Support : uint8_t {
    Native,       // implemented by this application itself (UI, overlay, settings)
    Mock,         // simulated by MockAdapter; no effect outside this app
    Unsupported,  // requires an authorized integration API that does not exist yet
};

// bool for Toggle, int for SliderInt / Choice (option index), float for SliderFloat.
// Action features carry no persistent value (stored as false).
using FeatureValue = std::variant<bool, int, float>;

struct FeatureDef {
    std::string id;           // stable key used in JSON ("visual.show_fps")
    std::string label;
    std::string description;
    std::string category;     // category id
    std::string origin;       // name of the original Lua variable/menu entry, if any
    FeatureKind kind = FeatureKind::Toggle;
    Support support = Support::Mock;
    bool global = false;      // global features are not stored per profile
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step = 0.0f;        // 0 = continuous
    std::string format;       // printf format for sliders, e.g. "%.0f%%"
    std::vector<std::string> options;  // Choice labels
    FeatureValue defaultValue = false;

    // Precomputed lowercase haystack for search; filled by the registry.
    std::string searchText;
};

struct Category {
    std::string id;
    std::string label;
    std::string description;
    std::string icon;  // icon name understood by ui/Icons
};

const char* toString(FeatureKind kind);
const char* toString(Support support);
bool parseFeatureKind(const std::string& s, FeatureKind& out);
bool parseSupport(const std::string& s, Support& out);

}  // namespace osm
