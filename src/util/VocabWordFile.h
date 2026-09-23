#pragma once
#include <string>
#include <vector>

// Per-book plain-text word list: one word per line under
// /vocab/words/<bookId>.txt (a plain, visible folder -- see VocabBooksStore's
// getFilePath() comment for why). Lists are expected to stay short (tens to
// low hundreds of words), so the whole file is read/rewritten rather than
// streamed.
namespace VocabWordFile {

// Loads the words for bookId. Cleared first; a missing file yields an empty
// list and returns false.
bool load(int bookId, std::vector<std::string>& words);

// Overwrites the word list for bookId, creating /vocab/words as needed.
bool save(int bookId, const std::vector<std::string>& words);

// Loads, appends word if not already present (case-insensitive), and saves.
// Returns false if the word was already present or the save failed.
bool addWord(int bookId, const std::string& word);

// Deletes the word-list file for bookId, if any. A missing file is not an error.
void remove(int bookId);

}  // namespace VocabWordFile
