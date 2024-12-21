#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <map>
#include <filesystem>
#include <cassert>
#include <cmath>

int main(){
  std::ifstream file("lemma/dict.txt");
  if(!file.is_open()) {
    std::cerr << "Failed to open lemma/dict.txt file" << std::endl;
    return 1;
  }
  std::string line;
  std::getline(file, line);
  int n_unique_words = std::stoi(line);
  std::unordered_map<std::string, int> dict;
  std::unordered_map<int, std::string> inv_dict;
  for(int i = 0; i < n_unique_words; i++) {
    std::getline(file, line);
    dict[line]=i;
    inv_dict[i]=line;
  }
  file.close();
  double* term_freq = new double[n_unique_words]();
  std::map<std::string, double*> index;
  int n_docs = 0;
  for (const auto& entry : std::filesystem::directory_iterator("lemma/")) {
    if (entry.path() == "lemma/dict.txt"){
      continue;
    }
    std::ifstream docfile(entry.path());
    if(!docfile.is_open()) {
      std::cerr << "Failed to open a file" << std::endl;
      return 1;
    }
    n_docs++;
    double* bag_of_words = new double[n_unique_words]();
    bool has_seen_doc[n_unique_words] = {false};
    while(std::getline(docfile, line)){
      int idx = dict[line];
      if (!has_seen_doc[idx]){
        term_freq[idx] += 1;
        has_seen_doc[idx] = true;
      }
      bag_of_words[idx] +=1;
    }
    docfile.close();
    index[entry.path()] = bag_of_words;
  }
  for (int i = 0; i < n_unique_words; i++) {
    double t = term_freq[i];
    assert(t<=n_docs);
    term_freq[i] = log((double)n_docs/t);
  }
  for (auto it = index.begin(); it != index.end(); ++it) {
    double *bow = it->second;
    for (int i = 0; i < n_unique_words; ++i) {
      bow[i]*=term_freq[i];
    }
  }
  std::string q = "\"lemma/C_(programming_language)\"";
  dist
  for (auto it = index.begin(); it != index.end(); ++it) {
  return 0;
}k
