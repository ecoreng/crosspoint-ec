#include "VocabWordListActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <utility>

#include "MappedInputManager.h"
#include "activities/reader/DictionaryDefinitionActivity.h"
#include "activities/reader/ReaderUtils.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "util/VocabWordFile.h"

namespace fui = freeink::ui;

namespace {
constexpr int ENTER_ACTIONS_MODE_MS = 700;
}  // namespace

VocabWordListActivity::VocabWordListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int bookId,
                                             std::string bookTitle, std::string dictionaryFolder)
    : UiListActivity("VocabWordList", renderer, mappedInput, /*wantsTouchLongPress=*/true),
      bookId(bookId),
      bookTitle(std::move(bookTitle)),
      dictionaryFolder(std::move(dictionaryFolder)) {}

void VocabWordListActivity::onEnter() {
  UiListActivity::onEnter();
  VocabWordFile::load(bookId, words);
  nav.selected = 0;
  rebuildRowItems();
  appliedOrientation = SETTINGS.orientation;
  ReaderUtils::applyOrientation(renderer, appliedOrientation);
  app.on(ACTION_ADD_WORD, &VocabWordListActivity::addWordActionTrampoline, this);
}

void VocabWordListActivity::loop() {
  if (SETTINGS.orientation != appliedOrientation) {
    appliedOrientation = SETTINGS.orientation;
    ReaderUtils::applyOrientation(renderer, appliedOrientation);
    requestUpdate(true);
  }
  UiListActivity::loop();
}

int VocabWordListActivity::getItemCount() const { return static_cast<int>(words.size()); }

void VocabWordListActivity::rebuildRowItems() {
  rowItems_.clear();
  rowItems_.reserve(words.size());

  for (size_t i = 0; i < words.size(); i++) {
    fui::ListItem item;
    item.label = words[storageIndexForRow(static_cast<int>(i))].c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }
}

void VocabWordListActivity::activateIndex(const int index) {
  if (optionPopup.isActive()) return;
  app.clearTapFlash();
  lookupWord(words[storageIndexForRow(index)]);
}

void VocabWordListActivity::onRowAction(const fui::ActionEvent& event) {
  nav.selected = event.value + 1;  // ring position, not row index (0 = Add word button)
  if (event.longPress) {
    onRowLongPress(event.value);
    return;
  }
  activateIndex(event.value);
}

void VocabWordListActivity::addWordActionTrampoline(const fui::ActionEvent&, void* user) {
  auto* self = static_cast<VocabWordListActivity*>(user);
  if (self->optionPopup.isActive()) return;
  self->nav.selected = 0;
  self->app.clearTapFlash();
  self->startAddWord();
}

void VocabWordListActivity::navigateButtons() {
  const int ringSize = static_cast<int>(words.size()) + 1;
  buttonNavigator.onNextRelease([this, ringSize] { moveSelectionTo(ButtonNavigator::nextIndex(nav.selected, ringSize)); });
  buttonNavigator.onPreviousRelease(
      [this, ringSize] { moveSelectionTo(ButtonNavigator::previousIndex(nav.selected, ringSize)); });
  buttonNavigator.onNextContinuous(
      [this, ringSize] { moveSelectionTo(ButtonNavigator::nextPageIndex(nav.selected, ringSize, nav.inputPageRows())); });
  buttonNavigator.onPreviousContinuous(
      [this, ringSize] { moveSelectionTo(ButtonNavigator::previousPageIndex(nav.selected, ringSize, nav.inputPageRows())); });
}

void VocabWordListActivity::onRowLongPress(const int index) {
  if (optionPopup.isActive()) return;
  if (index < 0 || index >= static_cast<int>(words.size())) return;
  app.clearTapFlash();
  nav.selected = index + 1;
  showDeleteConfirmation(index);
}

void VocabWordListActivity::startAddWord() {
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ADD_WORD), "", 63, InputType::Text);
  if (!keyboard) {
    LOG_ERR("VOCAB", "OOM: word entry keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    const std::string word = std::get<KeyboardResult>(result.data).text;
    if (word.empty()) return;
    if (VocabWordFile::addWord(bookId, word)) {
      VocabWordFile::load(bookId, words);
      rebuildRowItems();
    } else {
      optionPopup.show(StrId::STR_VOCAB_WORD_ALREADY_ADDED, std::vector<std::string>{tr(STR_OK_BUTTON)}, 0,
                       [](int) {});
    }
    requestUpdate();
  });
}

void VocabWordListActivity::showDeleteConfirmation(const int index) {
  if (index < 0 || index >= static_cast<int>(words.size()) || optionPopup.isActive()) return;
  const char* options[] = {tr(STR_CANCEL), tr(STR_DELETE)};
  optionPopup.show(tr(STR_CONFIRM_DELETE_WORD), options, 2, 0, [this, index](int idx) {
    if (idx == 1) deleteWord(index);
    requestUpdate();
  });
  requestUpdate();
}

void VocabWordListActivity::deleteWord(const int index) {
  if (index < 0 || index >= static_cast<int>(words.size())) return;
  words.erase(words.begin() + storageIndexForRow(index));
  if (!VocabWordFile::save(bookId, words)) {
    LOG_ERR("VOCAB", "Failed to save word list after delete");
  }
  rebuildRowItems();

  // nav.selected is a ring position (0 = Add word, 1..N = word rows); clamp
  // it into range if the deleted row was the last one, then let the next
  // buildScreen's syncListViewport() pull the viewport to it.
  if (nav.selected > static_cast<int>(words.size())) {
    nav.selected--;
  }
  nav.requestSelection(nav.selected);
  requestUpdate(true);
}

void VocabWordListActivity::lookupWord(const std::string& word) {
  if (!dictOpenAttempted) {
    dictOpenAttempted = true;
    dictOpenOk = dict.open(dictionaryFolder.c_str());
    if (dictOpenOk && dict.needsIndex()) {
      Dictionary::IndexResult indexResult = Dictionary::IndexResult::Ok;
      dictOpenOk = dict.buildIndex(nullptr, nullptr, &indexResult);
    }
  }
  if (!dictOpenOk) {
    LOG_ERR("VOCAB", "Failed to open/index dictionary %s", dictionaryFolder.c_str());
    optionPopup.show(StrId::STR_DICT_ERROR, std::vector<std::string>{tr(STR_OK_BUTTON)}, 0, [](int) {});
    requestUpdate();
    return;
  }

  std::string definition;
  std::string headword;
  Dictionary::LookupResult result = Dictionary::LookupResult::NotFound;
  if (!dict.lookup(word.c_str(), definition, headword, &result)) {
    LOG_DBG("VOCAB", "Word not found in %s: %s", dictionaryFolder.c_str(), word.c_str());
    optionPopup.show(StrId::STR_DICT_NOT_FOUND, std::vector<std::string>{tr(STR_OK_BUTTON)}, 0, [](int) {});
    requestUpdate();
    return;
  }

  startActivityForResult(
      makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, std::move(headword),
                                                       std::move(definition), dictionaryFolder,
                                                       dict.definitionsAreHtml()),
      [this](const ActivityResult&) { requestUpdate(); });
}

bool VocabWordListActivity::handleCustomInput() { return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); }); }

bool VocabWordListActivity::handleButtons() {
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, ENTER_ACTIONS_MODE_MS)) {
    if (nav.selected >= 1) showDeleteConfirmation(nav.selected - 1);
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (nav.selected == 0) {
      startAddWord();
    } else if (nav.selected >= 1 && nav.selected <= listCount()) {
      activateIndex(nav.selected - 1);
    }
    return true;
  }
  return false;
}

void VocabWordListActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}

void VocabWordListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  // "Add word" is a fixed row pinned above the list (ring position 0) so it
  // stays reachable as the word list grows, instead of scrolling with it.
  fui::ButtonProps addWord;
  addWord.label = tr(STR_ADD_WORD);
  addWord.action = ACTION_ADD_WORD;
  addWord.inputMask = fui::InputTouch;
  addWord.text = screen.theme().bodyText;
  addWord.styles = screen.theme().listRow;
  addWord.radius = static_cast<uint8_t>(metrics.listRowRadius);
  addWord.state = nav.selected == 0 ? fui::StateSelected : fui::StateNormal;
  fui::button(screen.frame(), screen.takeTop(static_cast<int16_t>(metrics.listRowHeight)), addWord);
  screen.spacer(static_cast<int16_t>(metrics.listRowGap));

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  // Tap looks the word up; long-press shows the delete confirmation
  // (physical buttons stay in handleButtons()).
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  syncListViewport(screen, props, /*selectionOffset=*/1);
  screen.list(props);
}
