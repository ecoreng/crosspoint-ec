#include "VocabLibraryActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "VocabBooksStore.h"
#include "activities/reader/ReaderUtils.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "util/DictionaryRegistry.h"
#include "VocabWordListActivity.h"

namespace fui = freeink::ui;

VocabLibraryActivity::VocabLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("VocabLibrary", renderer, mappedInput) {}

void VocabLibraryActivity::onEnter() {
  UiListActivity::onEnter();
  VOCAB_BOOKS.loadFromFile();
  nav.selected = 0;
  rebuildRowItems();
  appliedOrientation = SETTINGS.orientation;
  ReaderUtils::applyOrientation(renderer, appliedOrientation);
}

void VocabLibraryActivity::loop() {
  if (SETTINGS.orientation != appliedOrientation) {
    appliedOrientation = SETTINGS.orientation;
    ReaderUtils::applyOrientation(renderer, appliedOrientation);
    requestUpdate(true);
  }
  UiListActivity::loop();
}

void VocabLibraryActivity::onExit() {
  // See VocabWordListActivity::onExit: restore Portrait so Home doesn't
  // inherit a rotation it was never exercised in.
  ReaderUtils::applyOrientation(renderer, CrossPointSettings::ORIENTATION::PORTRAIT);
  UiListActivity::onExit();
}

int VocabLibraryActivity::getItemCount() const { return VOCAB_BOOKS.getCount() + 1; }

void VocabLibraryActivity::rebuildRowItems() {
  rowItems_.clear();
  const auto& books = VOCAB_BOOKS.getBooks();
  rowItems_.reserve(books.size() + 1);

  for (size_t i = 0; i < books.size(); i++) {
    fui::ListItem item;
    item.label = books[i].title.c_str();
    item.subtitle = books[i].dictionaryFolder.c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }

  fui::ListItem addBook;
  addBook.label = tr(STR_ADD_VOCAB_BOOK);
  addBook.actionValue = static_cast<int16_t>(books.size());
  rowItems_.push_back(addBook);
}

const char* VocabLibraryActivity::headerTitle() const { return tr(STR_VOCABULARY); }

bool VocabLibraryActivity::handleCustomInput() { return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); }); }

void VocabLibraryActivity::activateIndex(const int index) {
  nav.selected = index;
  app.clearTapFlash();

  const auto& books = VOCAB_BOOKS.getBooks();
  if (index >= 0 && index < static_cast<int>(books.size())) {
    const VocabBook& book = books[index];
    startActivityForResult(
        makeUniqueNoThrow<VocabWordListActivity>(renderer, mappedInput, book.id, book.title, book.dictionaryFolder),
        [this](const ActivityResult&) {
          rebuildRowItems();
          requestUpdate();
        });
    return;
  }

  startAddBook();
}

void VocabLibraryActivity::startAddBook() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_BOOK_TITLE), "", 63,
                                                           InputType::Text);
  if (!keyboard) {
    LOG_ERR("VOCAB", "OOM: book title keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    std::string title = std::get<KeyboardResult>(result.data).text;
    if (title.empty()) return;
    promptDictionaryForNewBook(std::move(title));
  });
}

void VocabLibraryActivity::promptDictionaryForNewBook(std::string title) {
  std::vector<DictionaryEntry> dictionaries;
  DictionaryRegistry::discover(dictionaries);
  if (dictionaries.empty()) {
    LOG_ERR("VOCAB", "No dictionaries found on SD card");
    optionPopup.show(StrId::STR_VOCAB_NO_DICTIONARIES, std::vector<std::string>{tr(STR_OK_BUTTON)}, 0, [](int) {});
    requestUpdate();
    return;
  }

  std::vector<std::string> names;
  names.reserve(dictionaries.size());
  for (const auto& d : dictionaries) names.push_back(d.name);

  optionPopup.show(StrId::STR_SELECT_DICTIONARY, names, 0,
                   [this, title = std::move(title), names](int idx) {
                     VOCAB_BOOKS.addBook(title, names[idx]);
                     rebuildRowItems();
                     nav.selected = VOCAB_BOOKS.getCount() - 1;
                     requestUpdate();
                   });
  requestUpdate();
}

void VocabLibraryActivity::buildScreen(UiScreen& screen) {
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

void VocabLibraryActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
