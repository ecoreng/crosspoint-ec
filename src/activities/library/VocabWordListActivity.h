#pragma once
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
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

 private:
  int listCount() const override { return getItemCount(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return bookTitle.c_str(); }

  int getItemCount() const;
  void rebuildRowItems();
  void startAddWord();
  void lookupWord(const std::string& word);

  int bookId;
  std::string bookTitle;
  std::string dictionaryFolder;
  std::vector<std::string> words;
  std::vector<freeink::ui::ListItem> rowItems_;

  Dictionary dict;
  bool dictOpenAttempted = false;
  bool dictOpenOk = false;
};
