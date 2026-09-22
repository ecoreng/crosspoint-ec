#pragma once
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"
#include "util/Dictionary.h"

// Word list for one vocabulary book: plain-text words stored via
// VocabWordFile. Selecting a word looks it up in the book's assigned
// dictionary and pushes DictionaryDefinitionActivity to show the result. The
// last row adds a new word via KeyboardEntryActivity.
class VocabWordListActivity final : public UiListActivity {
 public:
  explicit VocabWordListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int bookId,
                                 std::string bookTitle, std::string dictionaryFolder);

  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  int listCount() const override { return getItemCount(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowLongPress(int index) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  const char* headerTitle() const override { return bookTitle.c_str(); }

  int getItemCount() const;
  void rebuildRowItems();
  void startAddWord();
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
};
