#pragma once
#include <vector>

#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"

// Lists declared vocabulary books (title + assigned dictionary). Selecting one
// opens its word list (VocabWordListActivity); the last row adds a new book:
// title via KeyboardEntryActivity, then dictionary via OptionPopup over
// DictionaryRegistry::discover().
class VocabLibraryActivity final : public UiListActivity {
 public:
  explicit VocabLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  int listCount() const override { return getItemCount(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  const char* headerTitle() const override;

  int getItemCount() const;
  void rebuildRowItems();
  void startAddBook();
  void promptDictionaryForNewBook(std::string title);

  std::vector<freeink::ui::ListItem> rowItems_;
  OptionPopup optionPopup;
};
