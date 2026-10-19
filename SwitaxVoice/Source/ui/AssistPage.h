#pragma once

#include "Widgets.h"

namespace sx::ui
{
//==============================================================================
/** One recommendation: evidence, interpretation, proposal, prediction, confidence, limitations,
    and audition / amount / accept / reject controls. */
class RecCard : public juce::Component
{
public:
    enum class Status { Pending, Accepted, Rejected };

    explicit RecCard (const sx::analysis::Recommendation& r);

    int heightForWidth (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    void setStatus (Status s);
    void setAuditioning (bool a);

    sx::analysis::Recommendation rec;
    Status status = Status::Pending;
    bool expanded = false;
    float amount = 1.0f;

    juce::TextButton auditionButton { "AUDITION" }, acceptButton { "ACCEPT" }, rejectButton { "REJECT" }, detailsButton { "DETAILS" };
    juce::Slider amountSlider;

    std::function<void (RecCard&)> onAudition, onAccept, onReject, onLayoutChanged;

private:
    float layoutText (juce::Graphics* g, int width) const;   // draws when g != nullptr; returns height
};

//==============================================================================
class AssistPage : public juce::Component
{
public:
    explicit AssistPage (SwitaxVoiceAudioProcessor& p);
    ~AssistPage() override;

    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called with the parameter IDs a hovered/auditioned recommendation would change. */
    std::function<void (const juce::StringArray&)> onHighlight;

    // public for tests / screenshots
    void rebuildFromReport();
    int getNumCards() const { return (int) cards.size(); }
    RecCard* getCard (int i) { return juce::isPositiveAndBelow (i, (int) cards.size()) ? cards[(size_t) i].get() : nullptr; }
    void accept (RecCard&);
    void reject (RecCard&);
    void toggleAudition (RecCard&);
    void acceptAll();
    void undo();
    void stopAudition();
    void startLearn();

    juce::ComboBox contextBox, targetBox, lengthBox;
    juce::TextButton learnButton { "LEARN" }, reanalyseButton { "RE-ANALYSE" }, loadRefButton { "LOAD REFERENCE" }, clearRefButton { "X" };
    juce::TextButton auditionAllButton { "AUDITION ALL" }, acceptAllButton { "ACCEPT ALL" }, undoButton { "UNDO" };

private:
    struct Graph : public juce::Component
    {
        explicit Graph (AssistPage& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        AssistPage& owner;
    };
    struct Tiles : public juce::Component
    {
        explicit Tiles (AssistPage& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        AssistPage& owner;
    };

    sx::analysis::Context currentContext() const;
    void refreshTargetMenu();
    void syncSettingsFromProcessor();
    void writeSettingsToProcessor();
    void layoutCards();
    static void applyScaled (sx::ParamValues& v, const sx::analysis::Recommendation& r, float amount);

    SwitaxVoiceAudioProcessor& proc;
    std::shared_ptr<const sx::analysis::Report> report;
    int shownVersion = -1;
    Graph graph { *this };
    Tiles tiles { *this };
    juce::Viewport viewport;
    juce::Component listContent;
    std::vector<std::unique_ptr<RecCard>> cards;
    RecCard* auditioning = nullptr;
    bool auditioningAll = false;

    struct UndoEntry { sx::ParamValues values; std::vector<RecCard*> cards; };
    std::vector<UndoEntry> undoStack;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String refStatus;
    AnalysisEngine::State lastState = AnalysisEngine::State::Idle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AssistPage)
};

} // namespace sx::ui
