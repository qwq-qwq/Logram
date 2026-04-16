#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>

struct DocumentChanges {
    uint32_t flags = 0;

    static constexpr uint32_t DataLoaded       = 1 << 0;
    static constexpr uint32_t FiltersChanged    = 1 << 1;
    static constexpr uint32_t SelectionChanged  = 1 << 2;
    static constexpr uint32_t StatisticsChanged = 1 << 3;
    static constexpr uint32_t ThemeChanged      = 1 << 4;
    static constexpr uint32_t LoadingProgress   = 1 << 5;

    bool Has(uint32_t flag) const { return (flags & flag) != 0; }
};

class IDocumentListener {
public:
    virtual ~IDocumentListener() = default;
    virtual void OnDocumentChanged(DocumentChanges changes) = 0;
};

class DocumentListeners {
public:
    void Add(IDocumentListener* listener) {
        listeners_.push_back(listener);
    }

    void Remove(IDocumentListener* listener) {
        listeners_.erase(
            std::remove(listeners_.begin(), listeners_.end(), listener),
            listeners_.end());
    }

    void Notify(DocumentChanges changes) {
        for (auto* l : listeners_) {
            l->OnDocumentChanged(changes);
        }
    }

private:
    std::vector<IDocumentListener*> listeners_;
};
