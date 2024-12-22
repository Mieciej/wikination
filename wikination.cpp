#include <iostream>
#include <sqlite3.h>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <map>
#include <string>

sqlite3* db;
void check_resut(int rc) {
  if (rc != SQLITE_OK) {
    std::cerr << "Failed to execute query: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    std::exit(1);
  }
}
double length(double* v, int size) {
  double total = 0.0;
  for(int i = 0; i < size; i++) {
    total += v[i] * v[i];
  }
  return sqrt(total);
}


int main(int argc, char** argv){
  int rc = sqlite3_open("bow.db", &db);
  if (rc) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return 1;
  }

  int n_docs = 0;
  std::unordered_map<int, int> doc_id_map;
  std::vector<std::string> doc_names;
  {
    const char* query = "SELECT doc_id, doc_name FROM documents;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    check_resut(rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int doc_id = sqlite3_column_int(stmt, 0);
      std::string doc_name((const char*)sqlite3_column_text(stmt, 1));
      doc_names.push_back(doc_name);
      doc_id_map[doc_id] = n_docs++;

    }
    sqlite3_finalize(stmt);
  }
  std::cout << "Found " << n_docs << " documents." << std::endl;
  int n_terms = 0;
  std::unordered_map<int, int> word_id_map;
  std::vector<double> inv_tf;
  {
    const char* query = "SELECT word_id, frequency FROM words;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    check_resut(rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int word_id = sqlite3_column_int(stmt, 0);
      int freq = sqlite3_column_int(stmt, 1);
      word_id_map[word_id] = n_terms++;
      double it = log((double)n_docs/(double)freq);
      inv_tf.push_back(it);
    }
    sqlite3_finalize(stmt);
  }
  std::cout << "Found " << n_terms << " terms." << std::endl;
  double *mem = new double[n_docs*n_terms];
  double * bag_of_words[n_docs];
  for (int i = 0; i < n_docs; i++) {
    bag_of_words[i] = &mem[i*n_terms];
  }
  {
    double* max_freq = new double[n_docs]();
    const char* query = "SELECT doc_id, word_id, frequency FROM bag_of_words;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    check_resut(rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int doc_id = sqlite3_column_int(stmt, 0);
      int word_id = sqlite3_column_int(stmt, 1);
      int freq = sqlite3_column_int(stmt, 2);
      bag_of_words[doc_id_map[doc_id]][word_id_map[word_id]] = (double)freq * inv_tf[word_id_map[word_id]];
      if (freq > max_freq[doc_id_map[doc_id]]) {
        max_freq[doc_id_map[doc_id]] = freq;
      }
    }
    sqlite3_finalize(stmt);
    for (int i = 0; i < n_docs; i++) {
      for (int j = 0; j < n_terms; j++) {
        bag_of_words[i][j] /= max_freq[i];
      }
    }
    delete[] max_freq;
  }
  double* user_query = new double[n_terms]();
  {
    for (int i = 1; i < argc; i++){
      const char* query = "SELECT t1.word_id, t1.frequency \
                           FROM bag_of_words t1 JOIN documents t2 \
                           WHERE t1.doc_id = t2.doc_id and t2.doc_name = ?;";
      sqlite3_stmt* stmt;
      rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
      check_resut(rc);
      sqlite3_bind_text(stmt, 1, argv[i], -1, SQLITE_STATIC);
      bool found = false;
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        int word_id = sqlite3_column_int(stmt, 0);
        int freq = sqlite3_column_int(stmt, 1);
        user_query[word_id_map[word_id]] +=freq;
      }
      sqlite3_finalize(stmt);
      if (!found) {
        std::cerr << "Document " << argv[i] << " not found." << std::endl;
      }
    }
    double max_freq = -1.0;
    for(int i = 0; i < n_terms; i++) {
      double freq = user_query[i];
      if (freq > max_freq) {
        max_freq = freq;
      }
      user_query[i] = freq * inv_tf[i];
    }
    for(int i = 0; i < n_terms; i++) {
      user_query[i] /= max_freq;
    }
  }
  double *scores = new double[n_docs];
  double q_len = length(user_query, n_terms);
  for (int i = 0; i < n_docs; i++){
    double dot_prod = 0.0;
    double d_len = length(bag_of_words[i], n_terms);
    for (int j = 0; j < n_terms; j++) {
      dot_prod += bag_of_words[i][j] * user_query[j];
    }
    scores[i] = dot_prod / (d_len * q_len);
  }
  for (int i = 0; i < n_docs; i++){
    if (scores[i] > 0) {
      std::cout << doc_names[i] <<": "<< scores[i] << std::endl;
    }
  }
  sqlite3_close(db);
  delete[] mem;
  delete[] user_query;
  delete[] scores;
  return 0;
}
