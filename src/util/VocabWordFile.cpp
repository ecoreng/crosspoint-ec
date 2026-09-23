#include "VocabWordFile.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "StringUtils.h"

namespace VocabWordFile {
namespace {

std::string pathFor(int bookId) {
  char buf[48];
  snprintf(buf, sizeof(buf), "/.crosspoint/vocab/%d.txt", bookId);
  return buf;
}

}  // namespace

bool load(int bookId, std::vector<std::string>& words) {
  words.clear();
  const std::string path = pathFor(bookId);
  if (!Storage.exists(path.c_str())) return false;

  const String content = Storage.readFile(path.c_str());
  std::string line;
  for (size_t i = 0; i < content.length(); i++) {
    const char c = content[i];
    if (c == '\n') {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (!line.empty()) words.push_back(line);
      line.clear();
    } else {
      line.push_back(c);
    }
  }
  if (!line.empty()) words.push_back(line);

  LOG_DBG("VOCAB", "Loaded %zu words for book %d", words.size(), bookId);
  return true;
}

bool save(int bookId, const std::vector<std::string>& words) {
  Storage.mkdir("/.crosspoint/vocab");

  std::string content;
  content.reserve(words.size() * 12);
  for (const auto& word : words) {
    content += word;
    content += '\n';
  }
  return Storage.writeFile(pathFor(bookId).c_str(), content.c_str());
}

bool addWord(int bookId, const std::string& word) {
  std::vector<std::string> words;
  load(bookId, words);

  const bool alreadyPresent =
      std::any_of(words.begin(), words.end(),
                  [&](const std::string& w) { return StringUtils::asciiCaseCmp(w.c_str(), word.c_str()) == 0; });
  if (alreadyPresent) return false;

  words.push_back(word);
  return save(bookId, words);
}

void remove(int bookId) {
  const std::string path = pathFor(bookId);
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
}

}  // namespace VocabWordFile
