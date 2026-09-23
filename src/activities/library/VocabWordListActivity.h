#pragma once
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"
#include "util/Dictionary.h"

// Word list for one vocabulary book: plain-text words stored via
// VocabWordFile. Selecting a word looks it up in the book's assigned
// dictionary and pushes DictionaryDefinitionActivity to show the result. "Add
// word" is a fixed button pinned above the list (via KeyboardEntryActivity),
// not a row, so it never scrolls out of reach as the list grows. Navigation
// is a ring: position 0 is the Add word button, 1..N are the word rows (see
// UiTabListActivity for the same pattern with a tab bar instead of a button).
class VocabWordListActivity final : public UiListActivity {
 public:
  explicit VocabWordListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int bookId,
                                 std::string bookTitle, std::string dictionaryFolder);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Add word button action; ACTION_ROW/ACTION_USER are base-owned.
  static constexpr freeink::ui::ActionId ACTION_ADD_WORD = ACTION_USER;

  int listCount() const override { return getItemCount(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  void onRowLongPress(int index) override;
  void navigateButtons() override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  const char* headerTitle() const override { return bookTitle.c_str(); }

  int getItemCount() const;
  void rebuildRowItems();
  void startAddWord();
  static void addWordActionTrampoline(const freeink::ui::ActionEvent& event, void* user);
  void lookupWord(const std::string& word);
  void showDeleteConfirmation(int index);
  void deleteWord(int index);

  int bookId;
  std::string bookTitle;
  std::string dictionaryFolder;
  std::vector<std::string> words;
  std::vector<freeink::ui::ListItem> rowItems_;

  Dictionary dict;
  bool dictOpenAttempted = false;
  bool dictOpenOk = false;
  OptionPopup optionPopup;
  // Mirrors EpubReaderActivity: swiping down anywhere opens the global
  // control-center panel, whose orientation tile only persists SETTINGS.orientation
  // (it doesn't rotate non-reader screens itself, see FrontlightPanelActivity's
  // ACTION_TILE case 2). Each screen that wants to honor it live has to pick up
  // the change itself; the reader does this in its own loop(), this does the same.
  uint8_t appliedOrientation = 0;
};
