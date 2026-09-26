#include "VocabLibraryActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "VocabBooksStore.h"
#include "MappedInputManager.h"
#include "activities/reader/ReaderUtils.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "util/DictionaryRegistry.h"
#include "util/VocabWordFile.h"
#include "VocabWordListActivity.h"

namespace fui = freeink::ui;

namespace {
constexpr int ENTER_ACTIONS_MODE_MS = 700;
}  // namespace

VocabLibraryActivity::VocabLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const bool selectionMode, const int initialBookId)
    : UiListActivity("VocabLibrary", renderer, mappedInput, /*wantsTouchLongPress=*/true),
      selectionMode(selectionMode),
      initialBookId(initialBookId) {}

void VocabLibraryActivity::onEnter() {
  UiListActivity::onEnter();
  VOCAB_BOOKS.loadFromFile();
  nav.selected = selectionMode && initialBookId != 0 ? ringPositionForBook(initialBookId) : 0;
  rebuildRowItems();
  appliedOrientation = SETTINGS.orientation;
  ReaderUtils::applyOrientation(renderer, appliedOrientation);
  app.on(ACTION_ADD_BOOK, &VocabLibraryActivity::addBookActionTrampoline, this);
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
  // inherit a rotation it was never exercised in. Selection mode returns to
  // a picker's caller (e.g. DictionaryDefinitionActivity), which may be
  // mid-reading in a non-Portrait orientation -- forcing Portrait there would
  // fight the screen it's about to resume rather than protect Home.
  if (!selectionMode) {
    ReaderUtils::applyOrientation(renderer, CrossPointSettings::ORIENTATION::PORTRAIT);
  }
  UiListActivity::onExit();
}

int VocabLibraryActivity::getItemCount() const { return VOCAB_BOOKS.getCount(); }

void VocabLibraryActivity::rebuildRowItems() {
  rowItems_.clear();
  const auto& books = VOCAB_BOOKS.getBooks();
  rowItems_.reserve(books.size());

  for (size_t i = 0; i < books.size(); i++) {
    const VocabBook& book = books[storageIndexForRow(static_cast<int>(i))];
    fui::ListItem item;
    item.label = book.title.c_str();
    item.subtitle = book.dictionaryFolder.c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }
}

const char* VocabLibraryActivity::headerTitle() const {
  return selectionMode ? tr(STR_SAVE_TO_VOCAB) : tr(STR_VOCABULARY);
}

int VocabLibraryActivity::ringPositionForBook(const int bookId) const {
  const auto& books = VOCAB_BOOKS.getBooks();
  for (size_t storageIndex = 0; storageIndex < books.size(); storageIndex++) {
    if (books[storageIndex].id == bookId) {
      const int rowIndex = static_cast<int>(books.size()) - 1 - static_cast<int>(storageIndex);
      return rowIndex + 1;
    }
  }
  return 0;
}

bool VocabLibraryActivity::handleCustomInput() { return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); }); }

bool VocabLibraryActivity::handleButtons() {
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
      startAddBook();
    } else if (nav.selected >= 1 && nav.selected <= listCount()) {
      activateIndex(nav.selected - 1);
    }
    return true;
  }
  return false;
}

void VocabLibraryActivity::activateIndex(const int index) {
  if (optionPopup.isActive()) return;
  app.clearTapFlash();

  const VocabBook& book = VOCAB_BOOKS.getBooks()[storageIndexForRow(index)];
  if (selectionMode) {
    setResult(VocabBookResult{book.id});
    finish();
    return;
  }
  startActivityForResult(
      makeUniqueNoThrow<VocabWordListActivity>(renderer, mappedInput, book.id, book.title, book.dictionaryFolder),
      [this](const ActivityResult&) {
        rebuildRowItems();
        requestUpdate();
      });
}

void VocabLibraryActivity::onRowAction(const fui::ActionEvent& event) {
  nav.selected = event.value + 1;  // ring position, not row index (0 = Add book button)
  if (event.longPress) {
    onRowLongPress(event.value);
    return;
  }
  activateIndex(event.value);
}

void VocabLibraryActivity::addBookActionTrampoline(const fui::ActionEvent&, void* user) {
  auto* self = static_cast<VocabLibraryActivity*>(user);
  if (self->optionPopup.isActive()) return;
  self->nav.selected = 0;
  self->app.clearTapFlash();
  self->startAddBook();
}

void VocabLibraryActivity::navigateButtons() {
  const int ringSize = VOCAB_BOOKS.getCount() + 1;
  buttonNavigator.onNextRelease([this, ringSize] { moveSelectionTo(ButtonNavigator::nextIndex(nav.selected, ringSize)); });
  buttonNavigator.onPreviousRelease(
      [this, ringSize] { moveSelectionTo(ButtonNavigator::previousIndex(nav.selected, ringSize)); });
  buttonNavigator.onNextContinuous(
      [this, ringSize] { moveSelectionTo(ButtonNavigator::nextPageIndex(nav.selected, ringSize, nav.inputPageRows())); });
  buttonNavigator.onPreviousContinuous(
      [this, ringSize] { moveSelectionTo(ButtonNavigator::previousPageIndex(nav.selected, ringSize, nav.inputPageRows())); });
}

void VocabLibraryActivity::onRowLongPress(const int index) {
  if (optionPopup.isActive()) return;
  if (index < 0 || index >= VOCAB_BOOKS.getCount()) return;
  app.clearTapFlash();
  nav.selected = index + 1;
  showDeleteConfirmation(index);
}

void VocabLibraryActivity::showDeleteConfirmation(const int index) {
  if (index < 0 || index >= VOCAB_BOOKS.getCount() || optionPopup.isActive()) return;
  const char* options[] = {tr(STR_CANCEL), tr(STR_DELETE)};
  optionPopup.show(tr(STR_CONFIRM_DELETE_VOCAB_BOOK), options, 2, 0, [this, index](int idx) {
    if (idx == 1) deleteBook(index);
    requestUpdate();
  });
  requestUpdate();
}

void VocabLibraryActivity::deleteBook(const int index) {
  const auto& books = VOCAB_BOOKS.getBooks();
  if (index < 0 || index >= static_cast<int>(books.size())) return;
  const int id = books[storageIndexForRow(index)].id;
  VocabWordFile::remove(id);
  VOCAB_BOOKS.deleteBook(id);
  rebuildRowItems();

  // nav.selected is a ring position (0 = Add book, 1..N = book rows); clamp
  // it into range if the deleted row was the last one, then let the next
  // buildScreen's syncListViewport() pull the viewport to it.
  if (nav.selected > VOCAB_BOOKS.getCount()) {
    nav.selected--;
  }
  nav.requestSelection(nav.selected);
  requestUpdate(true);
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
                     const int newId = VOCAB_BOOKS.addBook(title, names[idx]);
                     if (selectionMode) {
                       setResult(VocabBookResult{newId});
                       finish();
                       return;
                     }
                     rebuildRowItems();
                     nav.selected = 1;  // ring position of the newest book, now displayed on top
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

  // "Add book" is a fixed row pinned above the list (ring position 0) so it
  // stays reachable as the book list grows, instead of scrolling with it.
  fui::ButtonProps addBook;
  addBook.label = tr(STR_ADD_VOCAB_BOOK);
  addBook.action = ACTION_ADD_BOOK;
  addBook.inputMask = fui::InputTouch;
  addBook.text = screen.theme().bodyText;
  addBook.styles = screen.theme().listRow;
  addBook.radius = static_cast<uint8_t>(metrics.listRowRadius);
  addBook.state = nav.selected == 0 ? fui::StateSelected : fui::StateNormal;
  fui::button(screen.frame(), screen.takeTop(static_cast<int16_t>(metrics.listRowHeight)), addBook);
  screen.spacer(static_cast<int16_t>(metrics.listRowGap));

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  // Tap opens the book; long-press shows the delete confirmation (physical
  // buttons stay in handleButtons()).
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  syncListViewport(screen, props, /*selectionOffset=*/1);
  screen.list(props);
}

void VocabLibraryActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
