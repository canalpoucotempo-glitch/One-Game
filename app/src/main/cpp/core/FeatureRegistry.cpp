#include "FeatureRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Log.h"

namespace osm {

namespace {
constexpr const char* kTag = "Registry";

bool validId(const std::string& id) {
    if (id.empty() || id.size() > 64) return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-';
    });
}
}  // namespace

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

const char* toString(FeatureKind kind) {
    switch (kind) {
        case FeatureKind::Toggle: return "toggle";
        case FeatureKind::SliderFloat: return "slider";
        case FeatureKind::SliderInt: return "slider_int";
        case FeatureKind::Choice: return "choice";
        case FeatureKind::Action: return "action";
    }
    return "?";
}

const char* toString(Support support) {
    switch (support) {
        case Support::Native: return "native";
        case Support::Mock: return "mock";
        case Support::Unsupported: return "unsupported";
    }
    return "?";
}

bool parseFeatureKind(const std::string& s, FeatureKind& out) {
    static const std::pair<const char*, FeatureKind> kMap[] = {
        {"toggle", FeatureKind::Toggle},      {"slider", FeatureKind::SliderFloat},
        {"slider_int", FeatureKind::SliderInt}, {"choice", FeatureKind::Choice},
        {"action", FeatureKind::Action},
    };
    for (const auto& [name, kind] : kMap)
        if (s == name) { out = kind; return true; }
    return false;
}

bool parseSupport(const std::string& s, Support& out) {
    if (s == "native") { out = Support::Native; return true; }
    if (s == "mock") { out = Support::Mock; return true; }
    if (s == "unsupported") { out = Support::Unsupported; return true; }
    return false;
}

bool FeatureRegistry::validate(const FeatureDef& def, FeatureValue& v) {
    switch (def.kind) {
        case FeatureKind::Toggle:
        case FeatureKind::Action:
            if (auto* i = std::get_if<int>(&v)) v = (*i != 0);
            else if (auto* f = std::get_if<float>(&v)) v = (*f != 0.0f);
            return std::holds_alternative<bool>(v);

        case FeatureKind::SliderFloat: {
            float f;
            if (auto* pf = std::get_if<float>(&v)) f = *pf;
            else if (auto* pi = std::get_if<int>(&v)) f = static_cast<float>(*pi);
            else return false;
            if (!std::isfinite(f)) return false;
            f = std::clamp(f, def.minValue, def.maxValue);
            if (def.step > 0.0f) f = def.minValue + std::round((f - def.minValue) / def.step) * def.step;
            v = std::clamp(f, def.minValue, def.maxValue);
            return true;
        }
        case FeatureKind::SliderInt: {
            int i;
            if (auto* pi = std::get_if<int>(&v)) i = *pi;
            else if (auto* pf = std::get_if<float>(&v)) {
                if (!std::isfinite(*pf)) return false;
                i = static_cast<int>(std::lround(*pf));
            } else return false;
            v = std::clamp(i, static_cast<int>(def.minValue), static_cast<int>(def.maxValue));
            return true;
        }
        case FeatureKind::Choice: {
            int i;
            if (auto* pi = std::get_if<int>(&v)) i = *pi;
            else if (auto* pf = std::get_if<float>(&v)) i = static_cast<int>(*pf);
            else return false;
            if (def.options.empty()) return false;
            v = std::clamp(i, 0, static_cast<int>(def.options.size()) - 1);
            return true;
        }
    }
    return false;
}

bool FeatureRegistry::add(FeatureDef def) {
    if (!validId(def.id)) {
        OSM_LOGW(kTag, "rejected feature with invalid id '%s'", def.id.c_str());
        return false;
    }
    if (index_.count(def.id)) {
        OSM_LOGW(kTag, "duplicate feature id '%s'", def.id.c_str());
        return false;
    }
    if (def.kind == FeatureKind::Choice && def.options.empty()) {
        OSM_LOGW(kTag, "choice feature '%s' has no options", def.id.c_str());
        return false;
    }
    if (def.minValue > def.maxValue) std::swap(def.minValue, def.maxValue);
    if (def.label.empty()) def.label = def.id;

    FeatureValue dv = def.defaultValue;
    if (!validate(def, dv)) {
        // Fall back to the type's natural default.
        switch (def.kind) {
            case FeatureKind::SliderFloat: dv = def.minValue; break;
            case FeatureKind::SliderInt: dv = static_cast<int>(def.minValue); break;
            case FeatureKind::Choice: dv = 0; break;
            default: dv = false; break;
        }
        OSM_LOGW(kTag, "feature '%s' had an invalid default; using fallback", def.id.c_str());
    }
    def.defaultValue = dv;
    def.searchText = toLower(def.label + "\n" + def.description + "\n" + def.id + "\n" + def.origin);

    index_.emplace(def.id, defs_.size());
    values_.push_back(dv);
    defs_.push_back(std::move(def));
    return true;
}

bool FeatureRegistry::addCategory(Category category) {
    if (!validId(category.id) || findCategory(category.id)) return false;
    if (category.label.empty()) category.label = category.id;
    categories_.push_back(std::move(category));
    return true;
}

const Category* FeatureRegistry::findCategory(const std::string& id) const {
    for (const auto& c : categories_)
        if (c.id == id) return &c;
    return nullptr;
}

void FeatureRegistry::clear() {
    defs_.clear();
    values_.clear();
    categories_.clear();
    index_.clear();
    ++revision_;
}

std::optional<size_t> FeatureRegistry::indexOf(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return std::nullopt;
    return it->second;
}

bool FeatureRegistry::set(size_t index, FeatureValue v) {
    if (index >= defs_.size()) return false;
    const FeatureDef& d = defs_[index];
    if (d.kind == FeatureKind::Action) return false;
    if (!validate(d, v)) {
        OSM_LOGW(kTag, "ignored invalid value for '%s'", d.id.c_str());
        return false;
    }
    if (values_[index] == v) return false;
    values_[index] = v;
    ++revision_;
    if (listener_) listener_(d, v);
    return true;
}

bool FeatureRegistry::set(const std::string& id, FeatureValue v) {
    auto i = indexOf(id);
    return i ? set(*i, v) : false;
}

void FeatureRegistry::trigger(size_t index) {
    if (index >= defs_.size() || defs_[index].kind != FeatureKind::Action) return;
    if (listener_) listener_(defs_[index], FeatureValue{true});
}

bool FeatureRegistry::getBool(const std::string& id, bool fallback) const {
    auto i = indexOf(id);
    if (!i) return fallback;
    if (auto* b = std::get_if<bool>(&values_[*i])) return *b;
    return fallback;
}

int FeatureRegistry::getInt(const std::string& id, int fallback) const {
    auto i = indexOf(id);
    if (!i) return fallback;
    if (auto* v = std::get_if<int>(&values_[*i])) return *v;
    return fallback;
}

float FeatureRegistry::getFloat(const std::string& id, float fallback) const {
    auto i = indexOf(id);
    if (!i) return fallback;
    if (auto* v = std::get_if<float>(&values_[*i])) return *v;
    return fallback;
}

bool FeatureRegistry::isDefault(size_t index) const {
    return index < defs_.size() && values_[index] == defs_[index].defaultValue;
}

void FeatureRegistry::resetToDefault(size_t index) {
    if (index < defs_.size()) set(index, defs_[index].defaultValue);
}

void FeatureRegistry::resetAll(bool includeGlobal) {
    for (size_t i = 0; i < defs_.size(); ++i)
        if (includeGlobal || !defs_[i].global) resetToDefault(i);
}

}  // namespace osm
