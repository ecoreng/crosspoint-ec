#include "VocabBooksStore.h"

#include <Logging.h>

#include <algorithm>

void VocabBooksStore::toJson(JsonDocument& doc) const {
  doc["nextId"] = nextId;
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& book : books) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = book.id;
    obj["title"] = book.title;
    obj["dictionaryFolder"] = book.dictionaryFolder;
  }
}

bool VocabBooksStore::fromJson(JsonVariantConst doc) {
  books.clear();
  nextId = doc["nextId"] | 1;
  JsonArrayConst arr = doc["books"].as<JsonArrayConst>();
  books.reserve(arr.size());
  for (JsonObjectConst obj : arr) {
    VocabBook book;
    book.id = obj["id"] | 0;
    book.title = obj["title"] | "";
    book.dictionaryFolder = obj["dictionaryFolder"] | "";
    books.push_back(std::move(book));
  }
  LOG_DBG("VOCAB", "Vocab books loaded from file (%d entries)", getCount());
  return true;
}

const VocabBook* VocabBooksStore::getBook(int id) const {
  auto it = std::find_if(books.begin(), books.end(), [id](const VocabBook& b) { return b.id == id; });
  return it == books.end() ? nullptr : &(*it);
}

int VocabBooksStore::addBook(const std::string& title, const std::string& dictionaryFolder) {
  const int id = nextId++;
  books.push_back(VocabBook{id, title, dictionaryFolder});
  if (!saveToFile()) {
    LOG_ERR("VOCAB", "Failed to persist new vocab book %d", id);
  }
  return id;
}
