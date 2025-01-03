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
    double *tf_idf;   // tf_idf representaiton of the query
  };
  Query query;
  struct Ranking{
    double *scores;   // query similarity to other documents
    int *order;
  };
  Ranking ranking;
  struct History {
    bool empty;
    bool *selected;   // what documents are present in user history
  };
  History history;
  struct SelectedDoc{
    int idx;
    bool changed;
    double *word_contrib;
    int *word_order;
  };
  SelectedDoc selected_doc;

  struct StatDoc{
    int idx;
    bool changed;
    int *word_order;
    int *doc_order;
    double *scores;
  };
  StatDoc stat_doc;
};
typedef struct s_ui_state ui_state_t;

struct s_doc_table_t {
  int n;
  std::unordered_map<int, int> id_map;
  std::vector<std::string> names;
};

typedef struct s_doc_table_t doc_table_t;

struct s_word_table_t{
  int n;
  std::unordered_map<int, int> id_map;
  std::vector<std::string> names;
};

typedef struct s_word_table_t word_table_t;

struct s_bag_of_words_t{
  double **bow;
  double **tf_idf;
  std::vector<double> idf;
};

typedef struct s_bag_of_words_t bag_of_words_table_t;

void draw_ui(ui_state_t &ui, doc_table_t& doc_table, word_table_t& word_table, bag_of_words_table_t &bow_table);
void update(ui_state_t &ui, doc_table_t &doc_table, word_table_t &word_table, bag_of_words_table_t &bow_table);

int main(int argc, char** argv){
  int rc = sqlite3_open("bow.db", &db);
  doc_table_t doc_table;
  word_table_t word_table;
  bag_of_words_table_t bow_table;
  ui_state_t ui;
  if (rc) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return 1;
  }
  // query information about documents
  doc_table.n = 0;
  {
    const char* query = "SELECT doc_id, doc_name FROM documents;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    check_resut(rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int doc_id = sqlite3_column_int(stmt, 0);
      std::string doc_name((const char*)sqlite3_column_text(stmt, 1));
      doc_table.names.push_back(doc_name);
      doc_table.id_map[doc_id] = doc_table.n++;

    }
    sqlite3_finalize(stmt);
  }
  std::cout << "Found " << doc_table.n << " documents." << std::endl;
  word_table.n= 0;

  {
    const char* query = "SELECT word_id, frequency, word FROM words;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    check_resut(rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int word_id = sqlite3_column_int(stmt, 0);
      int freq = sqlite3_column_int(stmt, 1);
      std::string word((const char*)sqlite3_column_text(stmt, 2));
      word_table.id_map[word_id] = word_table.n++;
      double it = log((double)doc_table.n/(double)freq);
      bow_table.idf.push_back(it);
      word_table.names.push_back(word);
    }
    sqlite3_finalize(stmt);
  }
  std::cout << "Found " << word_table.n << " terms." << std::endl;

  double *mem_bag_of_words = new double[doc_table.n*word_table.n];
  double *mem_tf_idf = new double[doc_table.n*word_table.n];
  double * repoint_bag_of_words[doc_table.n];
  double * repoint_tf_idf[doc_table.n];
  for (int i = 0; i < doc_table.n; i++) {
    repoint_bag_of_words[i] = &mem_bag_of_words[i*word_table.n];
    repoint_tf_idf[i] = &mem_tf_idf[i*word_table.n];
  }
  bow_table.bow = repoint_bag_of_words;
  bow_table.tf_idf = repoint_tf_idf;
  {
    double* max_freq = new double[doc_table.n]();
    const char* query = "SELECT doc_id, word_id, frequency FROM bag_of_words;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    check_resut(rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int doc_id = sqlite3_column_int(stmt, 0);
      int word_id = sqlite3_column_int(stmt, 1);
      int freq = sqlite3_column_int(stmt, 2);
      bow_table.bow[doc_table.id_map[doc_id]][word_table.id_map[word_id]] = (double)freq;
      bow_table.tf_idf[doc_table.id_map[doc_id]][word_table.id_map[word_id]] = (double)freq * bow_table.idf[word_table.id_map[word_id]];

      if (freq > max_freq[doc_table.id_map[doc_id]]) {
        max_freq[doc_table.id_map[doc_id]] = freq;
      }
    }
    sqlite3_finalize(stmt);
    for (int i = 0; i < doc_table.n; i++) {
      for (int j = 0; j < word_table.n; j++) {
        bow_table.tf_idf[i][j] /= max_freq[i];
      }
    }
    for (int i = 0; i < doc_table.n; i++){
      double l = length(bow_table.tf_idf[i], word_table.n);
      for (int j = 0; j < word_table.n; j++){
        bow_table.tf_idf[i][j] /= l;
      }
    }
    delete[] max_freq;
  }

  ui.window.height = 720;
  ui.window.width = 1280;

  double* user_query = new double[word_table.n]();
  ui.query.tf_idf = user_query;
  ui.query.changed = false;

  int recommend_doc_order[doc_table.n];
  int stat_doc_order[doc_table.n];
  for (int i = 0; i < doc_table.n; i++){
    recommend_doc_order[i] = i;
    stat_doc_order[i] = i;
  }
  double *rec_scores = new double[doc_table.n];
  ui.ranking.scores = rec_scores;
  ui.ranking.order = recommend_doc_order;

  bool selected_history[doc_table.n] = {false} ;
  ui.history.selected = selected_history;
  ui.history.empty = true;

  double *selected_document_word_contrib = new double[word_table.n];
  int selected_doc_word_order[word_table.n];
  int stat_doc_word_order[word_table.n];
  for (int i = 0; i < word_table.n; i++){
    selected_doc_word_order[i] = i;
    stat_doc_word_order[i] = i;
  }
  ui.selected_doc.idx = -1;
  ui.selected_doc.changed = false;
  ui.selected_doc.word_contrib = selected_document_word_contrib;
  ui.selected_doc.word_order = selected_doc_word_order;

  ui.stat_doc.idx = 0;
  ui.stat_doc.changed = true;
  ui.stat_doc.word_order = stat_doc_word_order;
  ui.stat_doc.doc_order = stat_doc_order;
  double stat_scores[doc_table.n] = {0.0};
  ui.stat_doc.scores = stat_scores;

  SDL_Init(SDL_INIT_EVERYTHING);

  SDL_Window* window = SDL_CreateWindow(
      "Wikination",
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

    update(ui, doc_table, word_table, bow_table);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    draw_ui(ui, doc_table, word_table, bow_table);

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
  delete[] rec_scores;
  delete[] selected_document_word_contrib;
  return 0;
}

void update(ui_state_t &ui, doc_table_t &doc_table, word_table_t &word_table, bag_of_words_table_t &bow_table){
  if(ui.query.changed){
    ui.query.changed = false;
    ui.history.empty = true;
    ui.selected_doc.idx = -1;
    for(int i = 0; i < word_table.n; i++) {
      ui.query.tf_idf[i] = 0.0;
    }
    for (int i = 0; i < doc_table.n; i++){
      if(!ui.history.selected[i]) {
        continue;
      }
      ui.history.empty = false;
      for (int j = 0; j < word_table.n; j++){
        ui.query.tf_idf[j] += bow_table.bow[i][j];
      }
    }
    double max_freq = -1.0;
    for(int i = 0; i < word_table.n; i++) {
      double freq = ui.query.tf_idf[i];
      if (freq > max_freq) {
        max_freq = freq;
      }
      ui.query.tf_idf[i] = freq * bow_table.idf[i];
    }
    for(int i = 0; i < word_table.n; i++) {
      ui.query.tf_idf[i] /= max_freq;
    }
    double q_len = length(ui.query.tf_idf, word_table.n);
    for (int i = 0; i < doc_table.n; i++){
      if(ui.history.selected[i]){
        ui.ranking.scores[i] = -1.0;
      }
      double dot_prod = 0.0;
      for (int j = 0; j < word_table.n; j++) {
        dot_prod += bow_table.tf_idf[i][j] * ui.query.tf_idf[j];
      }
      ui.ranking.scores[i] = dot_prod / q_len;
    }
    std::sort(ui.ranking.order, ui.ranking.order+doc_table.n, [&ui](int a, int b){
        return ui.ranking.scores[a] > ui.ranking.scores[b];
        });
  }
  if (ui.selected_doc.changed) {
    ui.selected_doc.changed = false;
    double total = 0.0;
    for (int j = 0; j < word_table.n; j++) {
      double dot = bow_table.tf_idf[ui.selected_doc.idx][j] * ui.query.tf_idf[j];
      ui.selected_doc.word_contrib[j] = dot;
      total += dot;
    }
    for (int j = 0; j < word_table.n; j++) {
      ui.selected_doc.word_contrib[j] /= total;
    }
    std::sort(ui.selected_doc.word_order, ui.selected_doc.word_order+word_table.n, [&ui](int a, int b){
        return ui.selected_doc.word_contrib[a] > ui.selected_doc.word_contrib[b];
        });
  }
  if (ui.stat_doc.changed) {
    ui.stat_doc.changed = false;
    std::sort(ui.stat_doc.word_order, ui.stat_doc.word_order+word_table.n, [&bow_table, &ui](int a, int b){
        return bow_table.bow[ui.stat_doc.idx][a] > bow_table.bow[ui.stat_doc.idx][b];
        });
    for (int i = 0; i < doc_table.n; i++) {
      if (i == ui.stat_doc.idx) {
        continue;
      }
      double s = 0.0;
      for (int j = 0; j < word_table.n; j++) {
         s += bow_table.tf_idf[ui.stat_doc.idx][j] * bow_table.tf_idf[i][j];
      }
      ui.stat_doc.scores[i] = s;
    }
    std::sort(ui.stat_doc.doc_order, ui.stat_doc.doc_order+doc_table.n, [&ui](int a, int b){
        return ui.stat_doc.scores[a] > ui.stat_doc.scores[b];
        });
  }

}

void draw_ui(ui_state_t &ui, doc_table_t& doc_table, word_table_t& word_table, bag_of_words_table_t &bow_table) {
  int window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar;

  ImGui::SetNextWindowPos(ImVec2(0,0));
  ImGui::SetNextWindowSize(ImVec2(ui.window.width,ui.window.height));
  if(ImGui::Begin("MainWindow", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse )) {
    if(ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)){
      if(ImGui::BeginTabItem("Recommender System")){
      float child_width =  ImGui::GetContentRegionAvail().x / 4.0f;
      float child_height = ImGui::GetContentRegionAvail().y;
      if(ImGui::BeginChild("Documents", ImVec2(child_width, child_height), ImGuiChildFlags_None, window_flags)){
        ImGui::SeparatorText("Documents");
        ImGui::BeginChild("Scrollable");
        for (int i = 0; i < doc_table.n; i++) {
          if(ui.history.selected[i]) {
            continue;
          }
          std::string button_label = "+##" + std::to_string(i);
          if (ImGui::Button(button_label.c_str())) {
            ui.history.selected[i] = true;
            ui.query.changed = true;
          }
          ImGui::SameLine();
          ImGui::Text("%s", doc_table.names[i].c_str());
        }
        ImGui::EndChild();
      }
      ImGui::EndChild();
      ImGui::SameLine();
      if(!ui.history.empty){
        if(ImGui::BeginChild("History", ImVec2(child_width, child_height), ImGuiChildFlags_None, window_flags)){
          ImGui::SeparatorText("History");
          ImGui::BeginChild("Scrollable");
          for (int i = 0; i < doc_table.n; i++) {
            if(!ui.history.selected[i]) {
              continue;
            }
            std::string button_label = "x##" + std::to_string(i);
            if (ImGui::Button(button_label.c_str())) {
              ui.history.selected[i] = false;
              ui.query.changed = true;
            }
            ImGui::SameLine();
            ImGui::Text("%s", doc_table.names[i].c_str());
          }
          ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if(ImGui::BeginChild("Ranking", ImVec2(child_width, child_height), ImGuiChildFlags_None, window_flags)){
          ImGui::SeparatorText("Recommendation");
          ImGui::BeginChild("Scrollable");
          for (int i = 0; i < doc_table.n; i++) {
            int idx = ui.ranking.order[i];
            if(ui.history.selected[idx]){
              continue;
            }
            char label[64];
            snprintf(label, sizeof(label), "%s %lf", doc_table.names[idx].c_str(), ui.ranking.scores[idx]);
            if(ImGui::Selectable(label, ui.selected_doc.idx == idx)) {
              if (ui.selected_doc.idx == i) {
                ui.selected_doc.idx = -1;
              } else {
                ui.selected_doc.idx = idx;
                ui.selected_doc.changed = true;
              }
            }
          }
          ImGui::EndChild();
        }
        ImGui::EndChild();
      }
      ImGui::SameLine();
      if(ui.selected_doc.idx != -1) {
        if(ImGui::BeginChild("Word score contribution", ImVec2(child_width, child_height), ImGuiChildFlags_None, window_flags)){
          std::string t =  doc_table.names[ui.selected_doc.idx] + " - term importance ";
          ImGui::SeparatorText(t.c_str());
          ImGui::BeginChild("Scrollable");
          for (int i = 0; i < word_table.n; i++) {
            int idx = ui.selected_doc.word_order[i];
            double contrib = ui.selected_doc.word_contrib[idx];
            if (!(contrib > 0.0)) {
              break;
            }
            ImGui::Text("%s", word_table.names[idx].c_str());
            ImGui::SameLine();
            float margin = child_width * 0.5;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - margin);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f-contrib, 0.2f + contrib, 0.0f, 1.0f)); 
            char text[32];
            snprintf(text, sizeof(text), "%.3lf%%", contrib * 100.0);
            ImGui::ProgressBar(contrib, ImVec2(margin*0.9, 20), text);
            ImGui::PopStyleColor();
          }
          ImGui::EndChild();
        }
        ImGui::EndChild();
      }
      ImGui::EndTabItem();
    }
    }
    if(ImGui::BeginTabItem("Statistics")){
      float child_width =  ImGui::GetContentRegionAvail().x / 3.0f;
      float child_height = ImGui::GetContentRegionAvail().y;
      if(ImGui::BeginChild("FreqTerms", ImVec2(child_width, child_height), ImGuiChildFlags_None, window_flags)){
        ImGui::SeparatorText("Select a document");
        ImGui::BeginChild("Scrollable");
        for (int i = 0; i < doc_table.n; i++) {
          if(ImGui::Selectable(doc_table.names[i].c_str(), ui.stat_doc.idx == i)){
            if (i!=ui.stat_doc.idx){
              ui.stat_doc.changed = true;
            }
            ui.stat_doc.idx = i;
          }
        }
        ImGui::EndChild();
      }
      ImGui::EndChild();
      ImGui::SameLine();
      if(ImGui::BeginChild("Documents", ImVec2(child_width, child_height), ImGuiChildFlags_None, window_flags)){
        ImGui::SeparatorText("Most frequent terms");
        ImGui::BeginChild("Scrollable");
        double top_freq = bow_table.bow[ui.stat_doc.idx][ui.stat_doc.word_order[0]];
        for (int i = 0; i < word_table.n; i++) {
          int idx = ui.stat_doc.word_order[i];
          double freq = bow_table.bow[ui.stat_doc.idx][idx];
          if (!(freq > 0.0)) {
            break;
          }
          ImGui::Text("%s", word_table.names[idx].c_str());
          ImGui::SameLine();
          double proportion = freq/top_freq;
          float margin = child_width*0.5;
          ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - margin);
          ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f-proportion, 0.2f + proportion, 0.0f, 1.0f)); 
          char text[32];
          snprintf(text, sizeof(text), "%d", (int)freq);
          ImGui::ProgressBar(proportion, ImVec2(margin*0.9, 20), text);
          ImGui::PopStyleColor();
        }
        ImGui::EndChild();
      }
      ImGui::EndChild();
      ImGui::SameLine();
      if(ImGui::BeginChild("Ranking", ImVec2(child_width, child_height), ImGuiChildFlags_None, window_flags)){
        ImGui::SeparatorText("Most similar documents");
        ImGui::BeginChild("Scrollable");
        for (int i = 0; i < doc_table.n; i++) {
          int idx = ui.stat_doc.doc_order[i];
          ImGui::Text("%s %.2lf", doc_table.names[idx].c_str(), ui.stat_doc.scores[idx]);
        }
        ImGui::EndChild();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}
