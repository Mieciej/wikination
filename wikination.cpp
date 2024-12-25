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


struct s_ui_state {
  struct Window{
    int height;
    int width;
  };
  Window window;
  struct Query{
    bool changed;
    double *tf_idf;
  };
  Query query;
  struct Ranking{
    double *scores;
    int *order;
  };
  Ranking ranking;
  struct History {
    bool empty;
    bool *selected;
  };
  History history;
  struct SelectedDoc{
    int idx;
    bool changed;
    double *word_contrib;
    int *word_order;
  };
  SelectedDoc selected_doc;
};
typedef struct s_ui_state ui_state_t;

void draw_ui(ui_state_t &ui, std::vector<std::string>&doc_names, int n_docs, std::vector<std::string>& words, int n_terms);
void update(ui_state_t &ui, int n_docs, int n_terms, double**tf_idf, double** bag_of_words, std::vector<double>&idf, std::unordered_map<int, int> &word_id_map, std::vector<std::string> &doc_names);

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
  std::vector<double> idf;
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
      idf.push_back(it);
      words.push_back(word);
    }
    sqlite3_finalize(stmt);
  }
  std::cout << "Found " << n_terms << " terms." << std::endl;
  double *mem_bag_of_words = new double[n_docs*n_terms];
  double *mem_tf_idf = new double[n_docs*n_terms];
  double * bag_of_words[n_docs];
  // It is not really tf_idf but precomputed part of cosine similarity.
  double * tf_idf[n_docs];
  for (int i = 0; i < n_docs; i++) {
    bag_of_words[i] = &mem_bag_of_words[i*n_terms];
    tf_idf[i] = &mem_tf_idf[i*n_terms];
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
      bag_of_words[doc_id_map[doc_id]][word_id_map[word_id]] = (double)freq;
      tf_idf[doc_id_map[doc_id]][word_id_map[word_id]] = (double)freq * idf[word_id_map[word_id]];

      if (freq > max_freq[doc_id_map[doc_id]]) {
        max_freq[doc_id_map[doc_id]] = freq;
      }
    }
    sqlite3_finalize(stmt);
    for (int i = 0; i < n_docs; i++) {
      for (int j = 0; j < n_terms; j++) {
        tf_idf[i][j] /= max_freq[i];
      }
    }
    for (int i = 0; i < n_docs; i++){
      double l = length(tf_idf[i], n_terms);
      for (int j = 0; j < n_terms; j++){
        tf_idf[i][j] /= l;
      }
    }
    delete[] max_freq;
  }

  ui_state_t ui;
  ui.window.height = 720;
  ui.window.width = 1280;

  double* user_query = new double[n_terms]();
  ui.query.tf_idf = user_query;
  ui.query.changed = false;

  int document_order[n_docs];
  for (int i = 0; i < n_docs; i++){
    document_order[i] = i;
  }
  double *scores = new double[n_docs];
  ui.ranking.scores = scores;
  ui.ranking.order = document_order;

  bool selected_history[n_docs] = {false} ;
  ui.history.selected = selected_history;
  ui.history.empty = true;

  double *selected_document_word_contrib = new double[n_terms];
  int word_order[n_terms];
  for (int i = 0; i < n_terms; i++){
    word_order[i] = i;
  }
  ui.selected_doc.idx = -1;
  ui.selected_doc.changed = false;
  ui.selected_doc.word_contrib = selected_document_word_contrib;
  ui.selected_doc.word_order = word_order;

  SDL_Init(SDL_INIT_EVERYTHING);

  SDL_Window* window = SDL_CreateWindow(
      "SDL2 It's Works!",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      ui.window.width, ui.window.height,
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
        SDL_GetWindowSize(window, &ui.window.width, &ui.window.height);
      }
    }

    update(ui, n_docs, n_terms, tf_idf,bag_of_words, idf,  word_id_map, doc_names);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    draw_ui(ui, doc_names, n_docs, words, n_terms);

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
  delete[] mem_bag_of_words;
  delete[] mem_tf_idf;
  delete[] user_query;
  delete[] scores;
  delete[] selected_document_word_contrib;
  return 0;
}

void update(ui_state_t &ui, int n_docs, int n_terms, double**tf_idf, double** bag_of_words, std::vector<double>&idf, std::unordered_map<int, int> &word_id_map, std::vector<std::string> &doc_names){
  if(ui.query.changed){
    ui.query.changed = false;
    ui.history.empty = true;
    ui.selected_doc.idx = -1;
    for(int i = 0; i < n_terms; i++) {
      ui.query.tf_idf[i] = 0.0;
    }
    for (int i = 0; i < n_docs; i++){
      if(!ui.history.selected[i]) {
        continue;
      }
      ui.history.empty = false;
      for (int j = 0; j < n_terms; j++){
        ui.query.tf_idf[j] +=bag_of_words[i][j];
      }
    }
    double max_freq = -1.0;
    for(int i = 0; i < n_terms; i++) {
      double freq = ui.query.tf_idf[i];
      if (freq > max_freq) {
        max_freq = freq;
      }
      ui.query.tf_idf[i] = freq * idf[i];
    }
    for(int i = 0; i < n_terms; i++) {
      ui.query.tf_idf[i] /= max_freq;
    }
    double q_len = length(ui.query.tf_idf, n_terms);
    for (int i = 0; i < n_docs; i++){
      if(ui.history.selected[i]){
        continue;
      }
      double dot_prod = 0.0;
      for (int j = 0; j < n_terms; j++) {
        dot_prod += tf_idf[i][j] * ui.query.tf_idf[j];
      }
      ui.ranking.scores[i] = dot_prod / q_len;
    }
    std::sort(ui.ranking.order, ui.ranking.order+n_docs, [&ui](int a, int b){
        return ui.ranking.scores[a] > ui.ranking.scores[b];
        });
  }
  if (ui.selected_doc.changed) {
    ui.selected_doc.changed = false;
    double total = 0.0;
    for (int j = 0; j < n_terms; j++) {
      double dot = tf_idf[ui.selected_doc.idx][j] * ui.query.tf_idf[j];
      ui.selected_doc.word_contrib[j] = dot;
      total += dot;
    }
    for (int j = 0; j < n_terms; j++) {
      ui.selected_doc.word_contrib[j] /= total;
    }
    std::sort(ui.selected_doc.word_order, ui.selected_doc.word_order+n_terms, [&ui](int a, int b){
        return ui.selected_doc.word_contrib[a] > ui.selected_doc.word_contrib[b];
        });

  }

}

void draw_ui(ui_state_t &ui, std::vector<std::string>&doc_names, int n_docs, std::vector<std::string>& words, int n_terms){
  float child_width =  ui.window.width / 4.0f;
  float child_height = ui.window.height;
  int window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar;

  ImGui::SetNextWindowPos(ImVec2(0,0));
  ImGui::SetNextWindowSize(ImVec2(child_width,child_height));
  if(ImGui::Begin("Documents", NULL, window_flags)){
    for (int i = 0; i < n_docs; i++) {
      if(ui.history.selected[i]) {
        continue;
      }
      std::string button_label = "+##" + std::to_string(i);
      if (ImGui::Button(button_label.c_str())) {
        ui.history.selected[i] = true;
        ui.query.changed = true;
      }
      ImGui::SameLine();
      ImGui::Text("%s", doc_names[i].c_str());
    }
  }
  ImGui::End();

  if(!ui.history.empty){
    ImGui::SetNextWindowPos(ImVec2(child_width,0));
    ImGui::SetNextWindowSize(ImVec2(child_width,child_height));
    if(ImGui::Begin("History", NULL, window_flags)){
      for (int i = 0; i < n_docs; i++) {
        if(!ui.history.selected[i]) {
          continue;
        }
        std::string button_label = "x##" + std::to_string(i);
        if (ImGui::Button(button_label.c_str())) {
          ui.history.selected[i] = false;
          ui.query.changed = true;
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
        int idx = ui.ranking.order[i];
        if(ui.history.selected[idx]){
          continue;
        }
        char label[64];
        snprintf(label, sizeof(label), "%s %lf", doc_names[idx].c_str(), ui.ranking.scores[idx]);
        if(ImGui::Selectable(label, ui.selected_doc.idx == idx)) {
          if (ui.selected_doc.idx == i) {
            ui.selected_doc.idx = -1;
          } else {
            ui.selected_doc.idx = idx;
            ui.selected_doc.changed = true;
          }
        }
      }
    }
    ImGui::End();
  }
  if(ui.selected_doc.idx != -1) {
    ImGui::SetNextWindowPos(ImVec2(child_width * 3,0));
    ImGui::SetNextWindowSize(ImVec2(child_width,child_height));
    char window_name[128];
    snprintf(window_name, sizeof(window_name), "Word Score Contribution - %s", doc_names[ui.selected_doc.idx].c_str());
    if(ImGui::Begin(window_name, NULL, window_flags)){
      for (int i = 0; i < n_terms; i++) {
        int idx = ui.selected_doc.word_order[i];
        double contrib = ui.selected_doc.word_contrib[idx];
        if (!(contrib > 0.0)) {
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
}
