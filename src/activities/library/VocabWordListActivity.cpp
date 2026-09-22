#include "VocabWordListActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <utility>

#include "activities/reader/DictionaryDefinitionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "util/VocabWordFile.h"

namespace fui = freeink::ui;

VocabWordListActivity::VocabWordListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int bookId,
                                             std::string bookTitle, std::string dictionaryFolder)
    : UiListActivity("VocabWordList", renderer, mappedInput),
      bookId(bookId),
      bookTitle(std::move(bookTitle)),
      dictionaryFolder(std::move(dictionaryFolder)) {}

void VocabWordListActivity::onEnter() {
  UiListActivity::onEnter();
  VocabWordFile::load(bookId, words);
  nav.selected = 0;
  rebuildRowItems();
}

int VocabWordListActivity::getItemCount() const { return static_cast<int>(words.size()) + 1; }

void VocabWordListActivity::rebuildRowItems() {
  rowItems_.clear();
  rowItems_.reserve(words.size() + 1);

  for (size_t i = 0; i < words.size(); i++) {
    fui::ListItem item;
    item.label = words[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }

  fui::ListItem addWord;
  addWord.label = tr(STR_ADD_WORD);
  addWord.actionValue = static_cast<int16_t>(words.size());
  rowItems_.push_back(addWord);
}

void VocabWordListActivity::activateIndex(const int index) {
  nav.selected = index;
  app.clearTapFlash();

  if (index >= 0 && index < static_cast<int>(words.size())) {
    lookupWord(words[index]);
    return;
  }

  startAddWord();
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
    }
    requestUpdate();
  });
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
    return;
  }

  std::string definition;
  std::string headword;
  Dictionary::LookupResult result = Dictionary::LookupResult::NotFound;
  if (!dict.lookup(word.c_str(), definition, headword, &result)) {
    LOG_DBG("VOCAB", "Word not found in %s: %s", dictionaryFolder.c_str(), word.c_str());
    return;
  }

  startActivityForResult(
      makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, std::move(headword),
                                                       std::move(definition), dict.definitionsAreHtml()),
      [this](const ActivityResult&) { requestUpdate(); });
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

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}
