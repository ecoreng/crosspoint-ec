#pragma once
#include <vector>

#include "VocabBooksStore.h"
#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"

// Lists declared vocabulary books (title + assigned dictionary). Selecting one
// opens its word list (VocabWordListActivity). "Add book" is a fixed button
// pinned above the list (title via KeyboardEntryActivity, then dictionary via
// OptionPopup over DictionaryRegistry::discover()), not a row, so it never
// scrolls out of reach as the book list grows. Navigation is a ring: position
// 0 is the Add book button, 1..N are the book rows (see VocabWordListActivity
// for the same pattern, and UiTabListActivity for the tab-bar variant).
class VocabLibraryActivity final : public UiListActivity {
 public:
  explicit VocabLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Add book button action; ACTION_ROW/ACTION_USER are base-owned.
  static constexpr freeink::ui::ActionId ACTION_ADD_BOOK = ACTION_USER;

  int listCount() const override { return getItemCount(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  void onRowLongPress(int index) override;
  void navigateButtons() override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  const char* headerTitle() const override;

  int getItemCount() const;
  void rebuildRowItems();
  void startAddBook();
  static void addBookActionTrampoline(const freeink::ui::ActionEvent& event, void* user);
  void promptDictionaryForNewBook(std::string title);
  void showDeleteConfirmation(int index);
  void deleteBook(int index);
  // Rows display newest-first (index 0 = most recently added) so a new book
  // stays visible under the pinned Add book button without scrolling; the
  // store's book list itself stays in addition order. Converts a display row
  // index to the matching index into VOCAB_BOOKS.getBooks().
  int storageIndexForRow(int rowIndex) const { return VOCAB_BOOKS.getCount() - 1 - rowIndex; }

  std::vector<freeink::ui::ListItem> rowItems_;
  OptionPopup optionPopup;
  // See VocabWordListActivity: picks up SETTINGS.orientation changes made via
  // the global control-center panel, which doesn't rotate this screen itself.
  uint8_t appliedOrientation = 0;
};
