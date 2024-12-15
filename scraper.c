#include <curl/curl.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
  strcat(data, ptr);
  return size * nmemb;
}

int main() {
  CURL *curl;
  CURLcode res;
  char *data = malloc(1000 * 1000 * 1000);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://en.wikipedia.org/wiki/Poland");

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    } else {
      printf("Received HTML data:\n%s\n", data);
    }
    const char *pattern = "<a[[:space:]]+([^>]*[[:space:]])?href=(\"|')(/wiki/[^\"']*)\\2";
    regex_t regex;
    regmatch_t matches[4];
    if (regcomp(&regex, pattern, REG_EXTENDED)) {
        fprintf(stderr, "Could not compile regex\n");
        return 1;
    }

    const char *cursor = data;
    while (regexec(&regex, cursor, 4, matches, 0) == 0) {
        // matches[2] contains the captured link part starting with /wiki/
        int length = matches[3].rm_eo - matches[3].rm_so;
        char link[length + 0];
        strncpy(link, cursor + matches[3].rm_so, length);
        link[length] = '\0';  // Null-terminate the string

        // Print the matched link
        printf("Found link: %s\n", link);
        // Move the cursor forward to continue searching
        cursor += matches[0].rm_eo;
    }

    regfree(&regex);
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();

  return 0;
}
