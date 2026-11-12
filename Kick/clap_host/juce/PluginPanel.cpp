// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginPanel.h"

#include <clap/ext/params.h>
#include <kick_clap/PluginManager.h>
#include <kick_clap/RemotePages.h>

namespace kick::clap {
namespace {
// A touch-sized file chooser that lives inside the panel (no native window): the MPC has no windowing system for juce::FileChooser.
class FilePicker final : public juce::Component {
public:
    FilePicker(const BitmapGuiFileRequest& request, juce::File start, std::function<void(uint32_t, const juce::String&)> answer)
        : save(request.mode == SAFIAN_BITMAP_GUI_FILE_SAVE), done(std::move(answer)),
          filter(request.wildcards.empty() ? "*" : juce::String(request.wildcards), "*", "files"),
          browser((save ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
                  save && !request.suggestedName.empty() ? start.getChildFile(juce::String(request.suggestedName)) : start, &filter, nullptr) {
        title.setText(juce::String(request.title.empty() ? (save ? "Save file" : "Open file") : request.title), juce::dontSendNotification);
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        ok.setButtonText(save ? "Save" : "Open");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &browser, &ok, &cancel}) addAndMakeVisible(c);
        if (auto* list = dynamic_cast<juce::ListBox*>(browser.getDisplayComponent())) list->setRowHeight(44);
        ok.onClick = [this] {
            const auto file = browser.getSelectedFile(0);
            if (file == juce::File() || (!save && !file.existsAsFile())) return;
            finish(SAFIAN_BITMAP_GUI_RESULT_OK, file.getFullPathName());
        };
        cancel.onClick = [this] { finish(SAFIAN_BITMAP_GUI_RESULT_CANCELLED, {}); };
    }
    ~FilePicker() override { if (done) { auto d = std::move(done); done = nullptr; d(SAFIAN_BITMAP_GUI_RESULT_CANCELLED, {}); } }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xf015191e)); }
    void resized() override {
        auto b = getLocalBounds().reduced(8);
        title.setBounds(b.removeFromTop(34));
        auto bar = b.removeFromBottom(48);
        cancel.setBounds(bar.removeFromRight(120).reduced(4)); ok.setBounds(bar.removeFromRight(120).reduced(4));
        browser.setBounds(b);
    }
    std::function<void()> onFinished;
private:
    void finish(uint32_t result, const juce::String& path) {
        auto d = std::move(done); done = nullptr;
        if (d) d(result, path);
        if (onFinished) { auto f = onFinished; juce::MessageManager::callAsync(f); }
    }
    bool save;
    std::function<void(uint32_t, const juce::String&)> done;
    juce::WildcardFileFilter filter;
    juce::FileBrowserComponent browser;
    juce::Label title;
    juce::TextButton ok, cancel{"Cancel"};
};
} // namespace
struct PluginPanel::ParameterRow : juce::Component {
    std::shared_ptr<PluginSession> session;
    Parameter parameter;
    juce::Label label, value;
    juce::Slider slider;
    ParameterRow(std::shared_ptr<PluginSession> owner, Parameter p) : session(std::move(owner)), parameter(std::move(p)) {
        addAndMakeVisible(label); addAndMakeVisible(value); addAndMakeVisible(slider);
        label.setInterceptsMouseClicks(false, false);
        value.setInterceptsMouseClicks(false, false);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        value.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.onValueChange = [this] { session->host().setParameter(parameter.id, slider.getValue()); updateText(); };
        slider.onDragStart = [this] { session->host().parameterGesture(parameter.id, true); };
        slider.onDragEnd = [this] { session->host().parameterGesture(parameter.id, false); };
        bind(parameter);
    }
    void bind(Parameter p) {
        const juce::String nextText = p.module.empty() ? p.name : p.module + " / " + p.name;
        const auto nextMax = p.maximum > p.minimum ? p.maximum : p.minimum + 1;
        const auto nextStep = (p.flags & CLAP_PARAM_IS_STEPPED) ? 1. : 0.;
        const bool sameRow = (parameter.id == p.id && label.getText() == nextText && slider.getMinimum() == p.minimum && slider.getMaximum() == nextMax);
        parameter = std::move(p);
        if (!sameRow) {
            label.setText(nextText, juce::dontSendNotification);
            slider.setRange(parameter.minimum, nextMax, nextStep);
            slider.setEnabled(!(parameter.flags & CLAP_PARAM_IS_READONLY) && parameter.maximum > parameter.minimum);
        }
        refresh();
    }
    void refresh() {
        if (!slider.isMouseButtonDown()) slider.setValue(session->host().parameterValue(parameter.id), juce::dontSendNotification);
        updateText();
    }
    void updateText() { value.setText(session->host().parameterText(parameter.id, slider.getValue()), juce::dontSendNotification); }
    void resized() override {
        auto bounds = getLocalBounds().reduced(8, 2);
        label.setBounds(bounds.removeFromLeft(getWidth() * 35 / 100));
        value.setBounds(bounds.removeFromRight(130)); slider.setBounds(bounds);
    }
};
std::filesystem::path PluginPanel::scannerPath() {
    if (const auto* overridePath = std::getenv("KICK_CLAP_SCANNER")) return overridePath;
    auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto sibling = executable.getSiblingFile("kick-clap-scan");
    if (sibling.existsAsFile()) return sibling.getFullPathName().toStdString();
    if (juce::File("/usr/bin/kick-clap-scan").existsAsFile()) return "/usr/bin/kick-clap-scan";
    return "/usr/local/bin/kick-clap-scan";
}
PluginPanel::PluginPanel(std::shared_ptr<PluginSession> owner, bool automaticallyScan) : session(std::move(owner)), roots(defaultSearchPaths()) {
    for (auto* component : std::initializer_list<juce::Component*>{&scanButton, &folderButton, &loadButton, &removeButton,
                                                                  &panicButton, &errorsButton, &pluginList, &search, &statusLabel, &title, &parameterList}) addAndMakeVisible(component);
    title.setText("CLAP instruments", juce::dontSendNotification);
#if JUCE_MAJOR_VERSION >= 8
    title.setFont(juce::Font(juce::FontOptions(22.f)));
#else
    title.setFont(juce::Font(22.f));
#endif
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setText(session->status(), juce::dontSendNotification);
    pluginList.setTextWhenNothingSelected("Scan and choose an instrument");
    search.setTextToShowWhenEmpty("Filter parameters...", juce::Colours::grey);
    search.onTextChange = [this] { rebuildParameters(); };
    parameterList.setRowHeight(56);
    parameterList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff15191e));
    if (auto* vp = parameterList.getViewport()) {
        vp->setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
        vp->setScrollBarThickness(12);
    }
    errorsButton.setEnabled(false);
    errorsButton.onClick = [this] { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Plugin scan errors", scanErrors); };
    scanButton.onClick = [this] { scanPlugins(); };
    folderButton.onClick = [this] { chooseFolder(); };
    loadButton.onClick = [this] {
        auto selected = pluginList.getSelectedId() - 1;
        if (selected >= 0 && selected < static_cast<int>(catalog.size())) { session->select(catalog[static_cast<size_t>(selected)]); rebuildParameters(); }
    };
    removeButton.onClick = [this] { session->remove(); rebuildParameters(); };
    panicButton.onClick = [this] { session->panic(); };
    for (int i = 0; i < 4; ++i) {
        auto& macro = macros[static_cast<size_t>(i)]; addAndMakeVisible(macro);
        macro.addMouseListener(this, true);
        macro.setTextWhenNothingSelected("Knob " + juce::String(i + 1));
        macro.onChange = [this, i] {
            const auto selected = macros[static_cast<size_t>(i)].getSelectedId() - 2;
            const auto& parameters = session->host().parameters();
            session->assignMacro(i, selected >= 0 && selected < static_cast<int>(parameters.size()) ? parameters[static_cast<size_t>(selected)].id : CLAP_INVALID_ID);
        };
    }
    pluginList.addMouseListener(this, true);
    auto extraPaths = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("KICK/clap-paths.txt");
    juce::StringArray lines; lines.addLines(extraPaths.loadFileAsString());
    for (const auto& line : lines) {
        if (line.isNotEmpty()) {
            std::filesystem::path p(line.toStdString());
            if (std::find(roots.begin(), roots.end(), p) == roots.end())
                roots.push_back(p);
        }
    }
    for (auto* component : std::initializer_list<juce::Component*>{&viewToggle, &pagePrev, &pageNext, &pageTitle}) addChildComponent(component);
    pageTitle.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    pageTitle.setJustificationType(juce::Justification::centred);
    viewToggle.setClickingTogglesState(false);
    viewToggle.onClick = [this] { showGeneric = !showGeneric; updatePluginGui(); resized(); };
    pagePrev.onClick = [this] { setRemoteFocus(remote::stepPage(remoteFocusIndex, -1, static_cast<int>(session->host().remotePages().size())), true); };
    pageNext.onClick = [this] { setRemoteFocus(remote::stepPage(remoteFocusIndex, +1, static_cast<int>(session->host().remotePages().size())), true); };
    setSize(960, 540); rebuildParameters(); startTimerHz(10);
    if (automaticallyScan) scanPlugins();
}
PluginPanel::~PluginPanel() { stopTimer(); gui.reset(); filePicker.reset(); parameterList.setModel(nullptr); if (scanJob) scanJob->cancelled.store(true); }
void PluginPanel::scanPlugins() {
    if (scanJob && !scanJob->complete.load()) return;
    scanJob = std::make_shared<ScanJob>();
    auto job = scanJob; auto paths = roots; auto scanner = scannerPath();
    statusLabel.setText("Scanning instruments...", juce::dontSendNotification); scanButton.setEnabled(false);
    // The worker owns only shared job data. Closing the panel cancels the child
    // process and cannot leave a callback pointing at a deleted component.
    std::thread([job, paths, scanner] {
        try { job->result = scan(paths, scanner, 5000, &job->cancelled); }
        catch (const std::exception& e) { job->result.failures.push_back({scanner.string(), e.what()}); }
        job->complete.store(true);
    }).detach();
}
void PluginPanel::chooseFolder() {
    chooser = std::make_unique<juce::FileChooser>("Choose a folder containing Linux CLAP instruments");
    juce::Component::SafePointer<PluginPanel> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [safe](const juce::FileChooser& selection) {
        if (!safe || selection.getResult() == juce::File{}) return;
        const auto path = selection.getResult().getFullPathName();
        std::filesystem::path fsPath(path.toStdString());
        if (std::find(safe->roots.begin(), safe->roots.end(), fsPath) == safe->roots.end()) {
            safe->roots.push_back(fsPath);
            auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("KICK/clap-paths.txt");
            file.getParentDirectory().createDirectory();
            juce::StringArray existing;
            existing.addLines(file.loadFileAsString());
            if (!existing.contains(path))
                file.appendText(path + "\n");
        }
        safe->scanPlugins();
    });
}
void PluginPanel::rebuildParameters() {
    visibleParameters.clear();
    auto& host = session->host();
    const auto filter = search.getText();
    for (const auto& p : host.parameters()) {
        if (!(p.flags & CLAP_PARAM_IS_HIDDEN) && (filter.isEmpty() || juce::String(p.name + " " + p.module).containsIgnoreCase(filter))) {
            visibleParameters.push_back(p);
        }
    }
    std::sort(visibleParameters.begin(), visibleParameters.end(), [](const Parameter& a, const Parameter& b) {
        return juce::String(a.module + "/" + a.name).compareNatural(juce::String(b.module + "/" + b.name)) < 0;
    });
    parameterRevision = host.metadataRevision();
    parameterList.updateContent();
    title.setText(host.loaded() ? "CLAP - " + juce::String(host.descriptor().name) : "CLAP instruments", juce::dontSendNotification);
    std::vector<size_t> order;
    for (size_t i = 0; i < host.parameters().size(); ++i) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto& lhs = host.parameters()[a]; const auto& rhs = host.parameters()[b];
        return juce::String(lhs.module + "/" + lhs.name).compareNatural(juce::String(rhs.module + "/" + rhs.name)) < 0;
    });
    for (int i = 0; i < 4; ++i) {
        if (session->macro(i) == CLAP_INVALID_ID && i < static_cast<int>(order.size())) {
            session->assignMacro(i, host.parameters()[order[static_cast<size_t>(i)]].id);
        }
        auto& macro = macros[static_cast<size_t>(i)]; macro.clear(juce::dontSendNotification); macro.addItem("Knob " + juce::String(i+1) + ": unassigned", 1);
        int selected = 1;
        for (const auto index : order) {
            const auto& p = host.parameters()[index];
            const auto item = static_cast<int>(index + 2);
            if (!(p.flags & (CLAP_PARAM_IS_HIDDEN | CLAP_PARAM_IS_READONLY))) macro.addItem(p.name, item);
            if (session->macro(i) == p.id) selected = item;
        }
        macro.setSelectedId(selected, juce::dontSendNotification);
    }
    resized();
}
juce::Component* PluginPanel::refreshComponentForRow(int row, bool, juce::Component* existing) {
    if (row < 0 || row >= static_cast<int>(visibleParameters.size())) {
        delete existing;
        return nullptr;
    }
    const auto& p = visibleParameters[static_cast<size_t>(row)];
    if (auto* rowComp = dynamic_cast<ParameterRow*>(existing)) {
        rowComp->bind(p);
        return rowComp;
    }
    return new ParameterRow(session, p);
}
void PluginPanel::timerCallback() {
    if (scanJob && scanJob->complete.load()) {
        catalog.clear();
        PluginManager::getInstance().publishDiscovered(scanJob->result.plugins); // a binary of the same id wins over the built-in
        for (auto& p : scanJob->result.plugins)
            if (std::find(p.features.begin(), p.features.end(), CLAP_PLUGIN_FEATURE_INSTRUMENT) != p.features.end()) catalog.push_back(std::move(p));
        for (const auto& p : PluginManager::getInstance().getInstrumentDescriptors()) {
            if (std::find_if(catalog.begin(), catalog.end(), [&](const Descriptor& d){ return d.id == p.id; }) == catalog.end()) {
                catalog.push_back(p);
            }
        }
        pluginList.clear(juce::dontSendNotification);
        for (size_t i = 0; i < catalog.size(); ++i) {
            pluginList.addItem(catalog[i].name + " - " + catalog[i].vendor, static_cast<int>(i + 1));
            if (catalog[i].id == session->host().descriptor().id) pluginList.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
        }
        if (pluginList.getSelectedId() == 0 && !catalog.empty()) {
            pluginList.setSelectedId(1, juce::dontSendNotification);
        }
        juce::String failures;
        for (const auto& f : scanJob->result.failures) failures += juce::String(f.path) + ": " + f.message + "\n";
        scanButton.setTooltip(failures);
        statusLabel.setTooltip(failures);
        scanErrors = failures; errorsButton.setEnabled(failures.isNotEmpty());
        title.setText("CLAP instruments - " + juce::String(catalog.size()) + " found, " + juce::String(scanJob->result.failures.size()) + " scan errors", juce::dontSendNotification);
        scanJob.reset(); scanButton.setEnabled(true);
    }
    auto& host = session->host();
    if (host.metadataRevision() != parameterRevision) rebuildParameters();
    updatePluginGui();
    if (host.remotePagesRevision() != remoteRevision) {
        remoteRevision = host.remotePagesRevision();
        const int count = static_cast<int>(host.remotePages().size());
        setRemoteFocus(count == 0 ? -1 : std::max(0, std::min(remoteFocusIndex, count - 1)), false);
        resized();
    }
    if (const auto suggested = host.takeSuggestedRemotePage(); suggested != CLAP_INVALID_ID)
        for (const auto& page : host.remotePages()) if (page.id == suggested) { setRemoteFocus(static_cast<int>(page.index), true); break; }
    if (gui && !showGeneric) return; // the plug-in's GUI is shown: the generic list and its status line cost nothing
    const auto* viewport = parameterList.getViewport();
    const auto first = viewport ? viewport->getViewPositionY() / 56 : 0;
    const auto last = std::min(getNumRows(), first + parameterList.getHeight() / 56 + 2);
    for (int i = std::max(0, first); i < last; ++i)
        if (auto* row = dynamic_cast<ParameterRow*>(parameterList.getComponentForRowNumber(i))) row->refresh();
    const auto stats = host.statistics();
    if (!scanJob) statusLabel.setText(session->status() + "  |  latency " + juce::String(host.latency()) + " samples  |  peak " +
                                     juce::String(stats.maximumMicroseconds, 0) + " us  |  missed " + juce::String(stats.deadlineMisses) +
                                     "  |  paused " + juce::String(stats.controlSilences) + "  |  dropped " + juce::String(stats.droppedEvents), juce::dontSendNotification);
}
void PluginPanel::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff15191e)); }
void PluginPanel::mouseDown(const juce::MouseEvent& e) {
    if (e.eventComponent == &pluginList || pluginList.isParentOf(e.eventComponent)) {
        const auto num = pluginList.getNumItems();
        if (num > 0) {
            const auto cur = pluginList.getSelectedId();
            const auto next = (cur % num) + 1;
            pluginList.setSelectedId(next, juce::dontSendNotification);
        }
        return;
    }
    for (int i = 0; i < 4; ++i) {
        auto& macro = macros[static_cast<size_t>(i)];
        if (e.eventComponent == &macro || macro.isParentOf(e.eventComponent)) {
            const auto num = macro.getNumItems();
            if (num > 0) {
                const auto cur = macro.getSelectedId();
                const auto next = (cur % num) + 1;
                macro.setSelectedId(next, juce::sendNotificationSync);
            }
            return;
        }
    }
}
bool PluginPanel::hasStrip() const { return gui != nullptr || !session->host().remotePages().empty(); }
juce::Rectangle<int> PluginPanel::guiBounds() const {
    auto b = getLocalBounds();
    if (hasStrip()) b.removeFromTop(28);
    return b;
}
void PluginPanel::layoutStrip(juce::Rectangle<int>& all) {
    const bool strip = hasStrip();
    for (auto* c : std::initializer_list<juce::Component*>{&pagePrev, &pageNext, &pageTitle}) c->setVisible(strip && !session->host().remotePages().empty());
    viewToggle.setVisible(strip && gui != nullptr);
    if (!strip) return;
    auto bar = all.removeFromTop(28);
    viewToggle.setBounds(bar.removeFromLeft(110).reduced(1));
    pageNext.setBounds(bar.removeFromRight(40).reduced(1)); pagePrev.setBounds(bar.removeFromRight(40).reduced(1));
    pageTitle.setBounds(bar);
}
void PluginPanel::resized() {
    auto all = getLocalBounds();
    layoutStrip(all);
    if (gui && !showGeneric) gui->setBounds(all);
    if (filePicker) filePicker->setBounds(getLocalBounds().reduced(24));
    auto bounds = all.reduced(12);
    title.setBounds(bounds.removeFromTop(30));
    auto toolbar = bounds.removeFromTop(52);
    scanButton.setBounds(toolbar.removeFromLeft(76).reduced(2)); folderButton.setBounds(toolbar.removeFromLeft(108).reduced(2));
    errorsButton.setBounds(toolbar.removeFromLeft(100).reduced(2));
    panicButton.setBounds(toolbar.removeFromRight(76).reduced(2)); removeButton.setBounds(toolbar.removeFromRight(86).reduced(2));
    loadButton.setBounds(toolbar.removeFromRight(76).reduced(2)); pluginList.setBounds(toolbar.reduced(2));
    auto macroRow = bounds.removeFromTop(48);
    const auto width = macroRow.getWidth() / 4;
    for (auto& macro : macros) macro.setBounds(macroRow.removeFromLeft(width).reduced(2));
    search.setBounds(bounds.removeFromTop(40).reduced(2));
    statusLabel.setBounds(bounds.removeFromBottom(36)); parameterList.setBounds(bounds);
}

juce::String PluginPanel::remoteTitle() const {
    const auto& pages = session->host().remotePages();
    if (remoteFocusIndex < 0 || remoteFocusIndex >= static_cast<int>(pages.size())) return {};
    const auto& page = pages[static_cast<size_t>(remoteFocusIndex)];
    return "Q-Link " + juce::String(remoteFocusIndex + 1) + "/" + juce::String(static_cast<int>(pages.size())) + "  " + juce::String(page.name) +
           (page.section.empty() ? juce::String() : "  (" + juce::String(page.section) + ")");
}

void PluginPanel::setRemoteFocus(int index, bool tellPlugin) {
    const auto& pages = session->host().remotePages();
    remoteFocusIndex = index < 0 || pages.empty() ? -1 : std::min(index, static_cast<int>(pages.size()) - 1);
    pageTitle.setText(remoteTitle(), juce::dontSendNotification);
    if (tellPlugin && gui && remoteFocusIndex >= 0) gui->showRemotePage(pages[static_cast<size_t>(remoteFocusIndex)].id);
}

bool PluginPanel::stepRemotePage(int direction) {
    const auto count = static_cast<int>(session->host().remotePages().size());
    if (count == 0) return false;
    setRemoteFocus(remote::stepPage(remoteFocusIndex, direction, count), true);
    return true;
}

bool PluginPanel::scrollGui(int steps) {
    if (!gui || showGeneric || steps == 0) return false;
    gui->scrollBy(-1.5 * steps);
    return true;
}

bool PluginPanel::pluginUndoRedo(bool redo) {
    if (!gui || showGeneric) return false;
    const int flags = juce::ModifierKeys::ctrlModifier | (redo ? juce::ModifierKeys::shiftModifier : 0);
    return gui->keyPressed(juce::KeyPress('z', juce::ModifierKeys(flags), 0));
}

bool PluginPanel::adjustKnob(int knobOffset, double delta) {
    const auto& pages = session->host().remotePages();
    if (!pages.empty()) {
        const auto target = remote::resolveKnob(remoteFocusIndex, knobOffset, static_cast<int>(pages.size()));
        if (!target.valid()) return false;
        session->adjustParameter(pages[static_cast<size_t>(target.page)].parameters[static_cast<size_t>(target.slot)], delta);
        return true;
    }
    if (knobOffset >= 0 && knobOffset < 4) { adjustMacro(knobOffset, delta); return true; }
    return false;
}

void PluginPanel::chooseGuiFile(const BitmapGuiFileRequest& request, std::function<void(uint32_t, const juce::String&)> done) {
    static std::map<std::string, juce::File> lastFolder;
    const auto purpose = request.purpose;
    auto start = lastFolder.count(purpose) ? lastFolder[purpose] : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto remember = [purpose, done](uint32_t result, const juce::String& path) {
        if (result == SAFIAN_BITMAP_GUI_RESULT_OK) lastFolder[purpose] = juce::File(path).getParentDirectory();
        done(result, path);
    };
#if JUCE_MAC || JUCE_WINDOWS || JUCE_IOS || JUCE_ANDROID
    const bool save = request.mode == SAFIAN_BITMAP_GUI_FILE_SAVE;
    if (save && !request.suggestedName.empty()) start = start.getChildFile(juce::String(request.suggestedName));
    chooser = std::make_unique<juce::FileChooser>(juce::String(request.title), start, juce::String(request.wildcards));
    chooser->launchAsync((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting
                               : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles),
                         [remember, save](const juce::FileChooser& c) {
        const auto file = c.getResult();
        if (file == juce::File() || (!save && !file.existsAsFile())) remember(SAFIAN_BITMAP_GUI_RESULT_CANCELLED, {});
        else remember(SAFIAN_BITMAP_GUI_RESULT_OK, file.getFullPathName());
    });
#else
    auto picker = std::make_unique<FilePicker>(request, start, remember);
    juce::Component::SafePointer<PluginPanel> safe(this);
    picker->onFinished = [safe] { if (safe) { safe->filePicker.reset(); safe->resized(); } };
    filePicker = std::move(picker);
    addAndMakeVisible(filePicker.get());
    filePicker->setBounds(getLocalBounds().reduced(24));
    filePicker->toFront(true);
#endif
}

void PluginPanel::updatePluginGui() {
    auto& host = session->host();
    const juce::String id = host.loaded() ? juce::String(host.descriptor().id) : juce::String();
    if (id != guiPluginId) { guiPluginId = id; guiRefused = false; showGeneric = false; remoteRevision = ~uint64_t{0}; }
    if (gui && !gui->isOpen()) gui.reset(); // the plug-in was unloaded or replaced
    const bool want = host.loaded() && !guiRefused && BitmapGuiClient::supported(host);
    if (!want && gui) { gui.reset(); filePicker.reset(); resized(); }
    if (want && !gui) {
        auto created = std::make_unique<BitmapGuiComponent>(host);
        created->hooks = guiHooks;
        juce::Component::SafePointer<PluginPanel> safe(this);
        if (!created->hooks.chooseFile)
            created->hooks.chooseFile = [safe](const BitmapGuiFileRequest& r, std::function<void(uint32_t, const juce::String&)> done) {
                if (!safe) return false;
                safe->chooseGuiFile(r, std::move(done));
                return true;
            };
        created->hooks.remotePage = [safe](clap_id page) {
            if (!safe) return;
            for (const auto& p : safe->session->host().remotePages()) if (p.id == page) { safe->setRemoteFocus(static_cast<int>(p.index), false); break; }
        };
        created->hooks.detached = [safe] { juce::MessageManager::callAsync([safe] { if (safe) safe->updatePluginGui(); }); };
        addAndMakeVisible(created.get());
        gui = std::move(created);
        auto b = guiBounds();
        gui->setBounds(b);
        if (!gui->open(b.getWidth(), b.getHeight())) { gui.reset(); guiRefused = true; }
        else {
            if (gui && session->host().remotePages().size() > 0 && remoteFocusIndex < 0) setRemoteFocus(0, false);
            gui->toBack();
        }
        resized();
    }
    const bool genericVisible = !gui || showGeneric;
    for (auto* c : std::initializer_list<juce::Component*>{&scanButton, &folderButton, &loadButton, &removeButton, &panicButton, &errorsButton, &pluginList, &search,
                                                           &statusLabel, &title, &parameterList})
        c->setVisible(genericVisible);
    for (auto& macro : macros) macro.setVisible(genericVisible);
    if (gui) gui->setVisible(!showGeneric);
    viewToggle.setButtonText(showGeneric ? "Plug-in GUI" : "Parameters");
}
} // namespace kick::clap
