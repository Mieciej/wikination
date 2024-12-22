#include <iostream>
#include <sqlite3.h>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <map>
#include <string>
#include <algorithm>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL2/SDL.h>


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
  std::vector<std::string> words;
  {
    const char* query = "SELECT word_id, frequency, word FROM words;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    check_resut(rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int word_id = sqlite3_column_int(stmt, 0);
      int freq = sqlite3_column_int(stmt, 1);
      std::string word((const char*)sqlite3_column_text(stmt, 2));
      word_id_map[word_id] = n_terms++;
      double it = log((double)n_docs/(double)freq);
      inv_tf.push_back(it);
      words.push_back(word);
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
  double *scores = new double[n_docs];
  std::vector<int> document_order;
  for (int i = 0; i < n_docs; i++){
    document_order.push_back(i);
  }
  double *bow_lengths = new double[n_terms]();
  for (int i = 0; i < n_docs; i++){
    bow_lengths[i] = length(bag_of_words[i], n_terms);
  }
  bool selected_history[n_docs] = {false} ;
  bool new_query = false;
  bool something_in_history = false;
  int selected_document = -1;
  bool selected_document_changed = false;
  double *selected_document_word_contrib = new double[n_terms];
  std::vector<int> word_order;
  for (int i = 0; i < n_terms; i++){
    word_order.push_back(i);
  }
  SDL_Init(SDL_INIT_EVERYTHING);

  int window_height = 720;
  int window_width = 1280;
  SDL_Window* window = SDL_CreateWindow(
      "SDL2 It's Works!",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      window_width, window_height,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
      );

  SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  ImGui::StyleColorsLight();
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);
  while(true){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
      ImGui_ImplSDL2_ProcessEvent(&event);
      if( event.type == SDL_QUIT ){
        goto finish;
      } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        SDL_GetWindowSize(window, &window_width, &window_height);
      }
    }
    if(new_query){
      new_query = false;
      something_in_history = false;
      selected_document = -1;
      for(int i = 0; i < n_terms; i++) {
        user_query[i] = 0.0;
      }
      for (int i = 0; i < n_docs; i++){
        if(!selected_history[i]) {
          continue;
        }
        something_in_history = true;
        const char* query = "SELECT t1.word_id, t1.frequency \
                             FROM bag_of_words t1 JOIN documents t2 \
                             WHERE t1.doc_id = t2.doc_id and t2.doc_name = ?;";
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
        check_resut(rc);
        sqlite3_bind_text(stmt, 1, doc_names[i].c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          int word_id = sqlite3_column_int(stmt, 0);
          int freq = sqlite3_column_int(stmt, 1);
          user_query[word_id_map[word_id]] +=freq;
        }
        sqlite3_finalize(stmt);
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
      double q_len = length(user_query, n_terms);
      for (int i = 0; i < n_docs; i++){
        if(selected_history[i]){
          continue;
        }
        double dot_prod = 0.0;
        for (int j = 0; j < n_terms; j++) {
          dot_prod += bag_of_words[i][j] * user_query[j];
        }
        scores[i] = dot_prod / (bow_lengths[i] * q_len);
      }
      std::sort(document_order.begin(), document_order.end(), [&scores](int a, int b){
          return scores[a] > scores[b];
          });
    }
    if (selected_document_changed) {
      selected_document_changed = false;
      double total = 0.0;
      for (int j = 0; j < n_terms; j++) {
        double dot = bag_of_words[selected_document][j] * user_query[j];
        selected_document_word_contrib[j] = dot;
        total += dot;
      }
      for (int j = 0; j < n_terms; j++) {
        selected_document_word_contrib[j] /= total;
      }
      std::sort(word_order.begin(), word_order.end(), [&selected_document_word_contrib](int a, int b){
          return selected_document_word_contrib[a] > selected_document_word_contrib[b];
          });

    }


    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    float child_width =  window_width / 4.0f;
    float child_height = window_height;
    int window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar;

    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(child_width,child_height));
    if(ImGui::Begin("Documents", NULL, window_flags)){
      for (int i = 0; i < n_docs; i++) {
        if(selected_history[i]) {
          continue;
        }
        std::string button_label = "+##" + std::to_string(i);
        if (ImGui::Button(button_label.c_str())) {
          selected_history[i] = true;
          new_query = true;
        }
        ImGui::SameLine();
        ImGui::Text("%s", doc_names[i].c_str());
      }
    }
    ImGui::End();

    if(something_in_history){
      ImGui::SetNextWindowPos(ImVec2(child_width,0));
      ImGui::SetNextWindowSize(ImVec2(child_width,child_height));
      if(ImGui::Begin("History", NULL, window_flags)){
        for (int i = 0; i < n_docs; i++) {
          if(!selected_history[i]) {
            continue;
          }
          std::string button_label = "x##" + std::to_string(i);
          if (ImGui::Button(button_label.c_str())) {
            selected_history[i] = false;
            new_query = true;
          }
          ImGui::SameLine();
          ImGui::Text("%s", doc_names[i].c_str());
        }
      }
      ImGui::End();
      ImGui::SetNextWindowPos(ImVec2(child_width * 2,0));
      ImGui::SetNextWindowSize(ImVec2(child_width,child_height));
      if(ImGui::Begin("Ranking", NULL, window_flags)){
        for (int i = 0; i < n_docs; i++) {
          if(selected_history[document_order[i]]){
            continue;
          }
          char label[64];
          snprintf(label, sizeof(label), "%s %lf", doc_names[document_order[i]].c_str(), scores[document_order[i]]);
          if(ImGui::Selectable(label, selected_document == i)) {
            if (selected_document == i) {
              selected_document = -1;
            } else {
              selected_document = i;
              selected_document_changed = true;
            }
          }
        }
      }
      ImGui::End();
    }
    if(selected_document != -1) {
      ImGui::SetNextWindowPos(ImVec2(child_width * 3,0));
      ImGui::SetNextWindowSize(ImVec2(child_width,child_height));
      char window_name[128];
      snprintf(window_name, sizeof(window_name), "Word Score Contribution - %s", doc_names[selected_document].c_str());
      if(ImGui::Begin(window_name, NULL, window_flags)){
        for (int i = 0; i < n_terms; i++) {
          int idx = word_order[i];
          double contrib = selected_document_word_contrib[idx];
          if (contrib == 0.0) {
            break;
          }
          ImGui::Text("%s", words[idx].c_str());
          ImGui::SameLine();
          float margin = 125.0f;
          ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - margin);
          ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f-contrib, 0.2f + contrib, 0.0f, 1.0f)); 
          char text[32];
          snprintf(text, sizeof(text), "%.3lf%%", contrib * 100.0);
          ImGui::ProgressBar(contrib, ImVec2(margin, 20), text);
          ImGui::PopStyleColor();
        }
      }
      ImGui::End();
    }


    ImGui::Render();
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }
finish:
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  sqlite3_close(db);
  delete[] mem;
  delete[] user_query;
  delete[] scores;
  delete[] selected_document_word_contrib;
  return 0;
}
