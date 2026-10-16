// Central store for feature definitions and their current values.
//
// Threading: the registry is owned by the render thread (android_main). All
// reads/writes, including adapter notifications, happen on that thread. Other
// threads communicate through the platform event queue, never directly.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Feature.h"

namespace osm {

class FeatureRegistry {
public:
    using Listener = std::function<void(const FeatureDef&, const FeatureValue&)>;

    // Registers a definition. Returns false (and logs) on duplicate/invalid ids.
    bool add(FeatureDef def);
    bool addCategory(Category category);
    void clear();

    size_t size() const { return defs_.size(); }
    const FeatureDef& def(size_t index) const { return defs_[index]; }
    const FeatureValue& value(size_t index) const { return values_[index]; }
    const std::vector<Category>& categories() const { return categories_; }
    const Category* findCategory(const std::string& id) const;

    std::optional<size_t> indexOf(const std::string& id) const;

    // Sets a value after validating/clamping it against the definition.
    // Returns true if the stored value changed. Notifies listeners on change.
    bool set(size_t index, FeatureValue v);
    bool set(const std::string& id, FeatureValue v);
    void trigger(size_t index);  // for Action features

    bool getBool(const std::string& id, bool fallback = false) const;
    int getInt(const std::string& id, int fallback = 0) const;
    float getFloat(const std::string& id, float fallback = 0.0f) const;

    bool isDefault(size_t index) const;
    void resetToDefault(size_t index);
    void resetAll(bool includeGlobal);

    // Coerces `v` into the type/range required by `def`. Returns false if the
    // value is unusable (wrong type that cannot be converted).
    static bool validate(const FeatureDef& def, FeatureValue& v);

    void setListener(Listener l) { listener_ = std::move(l); }
    // Monotonic counter bumped on every value change; used for autosave.
    uint64_t revision() const { return revision_; }

private:
    std::vector<FeatureDef> defs_;
    std::vector<FeatureValue> values_;
    std::vector<Category> categories_;
    std::unordered_map<std::string, size_t> index_;
    Listener listener_;
    uint64_t revision_ = 0;
};

std::string toLower(std::string s);

}  // namespace osm
