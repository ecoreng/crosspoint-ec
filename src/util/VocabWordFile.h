#pragma once
#include <string>
#include <vector>

// Per-book plain-text word list: one word per line under
// /.crosspoint/vocab/<bookId>.txt. Lists are expected to stay short (tens to
// low hundreds of words), so the whole file is read/rewritten rather than
// streamed.
namespace VocabWordFile {

// Loads the words for bookId. Cleared first; a missing file yields an empty
// list and returns false.
bool load(int bookId, std::vector<std::string>& words);

// Overwrites the word list for bookId, creating /.crosspoint/vocab as needed.
bool save(int bookId, const std::vector<std::string>& words);

// Loads, appends word if not already present (case-insensitive), and saves.
// Returns false if the word was already present or the save failed.
bool addWord(int bookId, const std::string& word);

}  // namespace VocabWordFile
