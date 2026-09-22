#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

// One user-declared vocabulary book: a title plus the on-device dictionary
// folder (DictionaryRegistry name) used to look up words added to it. Not
// tied to an actual book file — id is a stable key for the word-list file
// under /.crosspoint/vocab/<id>.txt (see VocabWordFile).
struct VocabBook {
  int id = 0;
  std::string title;
  std::string dictionaryFolder;
};

class VocabBooksStore : public PersistableStore<VocabBooksStore> {
  std::vector<VocabBook> books;
  int nextId = 1;

  VocabBooksStore() = default;
  ~VocabBooksStore() = default;

  friend class PersistableStore<VocabBooksStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/vocab_books.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::vector<VocabBook>& getBooks() const { return books; }
  int getCount() const { return static_cast<int>(books.size()); }
  const VocabBook* getBook(int id) const;

  // Assigns a fresh id, appends, and persists. Returns the new book's id.
  int addBook(const std::string& title, const std::string& dictionaryFolder);
};

#define VOCAB_BOOKS VocabBooksStore::getInstance()
