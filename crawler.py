import scrapy
import requests
import bs4
from time import sleep
import re
import random
import queue
import sys
import os
import sqlite3
import nltk
from nltk.stem import WordNetLemmatizer
from nltk.tokenize import word_tokenize

N_SITES = 10

con = sqlite3.connect("bow.db")
cur = con.cursor()
res = cur.execute("SELECT doc_name from documents")
downloaded = [x[0] for x in res.fetchall()]
if downloaded:
    restart_point = downloaded[-1]
else:
    restart_point = '/wiki/C_(programming_language)'
downloaded = set(downloaded)
queue = [""] * N_SITES
queue[0] = restart_point
front = 0
rear = 1
visited = set()
visited.add(restart_point)
print("Already downloaded:", len(downloaded))
while front!=rear:
    link = queue[front]
    front+=1
    print(link)
    response = requests.get('https://en.wikipedia.org'+link)
    if response.status_code != 200:
        print(f"Failed to scrape {link} (response.status_code)!", file=sys.stderr)
        continue

    doc_name = link
    cur.execute("INSERT INTO  documents (doc_name, doc_text) VALUES (?,?)", (doc_name, response.text))
    parsed = bs4.BeautifulSoup(response.text, "html.parser")
    links = parsed.find_all('a', attrs={'href': re.compile(r'^/wiki/[^:/#]+$')})
    for new_link in links:
        l = new_link['href']
        if rear < N_SITES and l not in visited and l not in downloaded:
            queue[rear] = l
            visited.add(l)
            rear+=1
    print(str(front)+"/"+str(N_SITES))
    sleep(random.random()*2)


nltk.download('punkt', download_dir="./nltk_data")
nltk.download('punkt_tab', download_dir="./nltk_data")
nltk.download('wordnet', download_dir="./nltk_data")
nltk.data.path.append("./nltk_data")
addons = {".", ",", ";", ":", "!", "?", "-", "_", "(", ")", "[", "]", "{", "}", "\"", "'", "‘", "’", "“", "”", "/", "\\", "|", "@", "#", "$", "%", "^", "&", "*", "+", "=", "~", "`", "<", ">", "…", "'s", "``", "''"}
stopwords = {
    "a", "about", "above", "after", "again", "against", "all", "am", "an", "and", "any", "are", 
    "aren't", "as", "at", "be", "because", "been", "before", "being", "below", "between", "both", 
    "but", "by", "can", "can't", "cannot", "could", "couldn't", "did", "didn't", "do", "does", 
    "doesn't", "doing", "don't", "down", "during", "each", "few", "for", "from", "further", "had", 
    "hadn't", "has", "hasn't", "have", "haven't", "having", "he", "he'd", "he'll", "he's", "her", 
    "here", "here's", "hers", "herself", "him", "himself", "his", "how", "how's", "i", "i'd", 
    "i'll", "i'm", "i've", "if", "in", "into", "is", "isn't", "it", "it's", "its", "itself", "let's", 
    "me", "more", "most", "mustn't", "my", "myself", "no", "nor", "not", "of", "off", "on", "once", 
    "only", "or", "other", "ought", "our", "ours", "ourselves", "out", "over", "own", "same", "shan't", 
    "she", "she'd", "she'll", "she's", "should", "shouldn't", "so", "some", "such", "than", "that", 
    "that's", "the", "their", "theirs", "them", "themselves", "then", "there", "there's", "these", 
    "they", "they'd", "they'll", "they're", "they've", "this", "those", "through", "to", "too", "under", 
    "until", "up", "very", "was", "wasn't", "we", "we'd", "we'll", "we're", "we've", "were", "weren't", 
    "what", "what's", "when", "when's", "where", "where's", "which", "while", "who", "who's", "whom", 
    "why", "why's", "with", "won't", "would", "wouldn't", "you", "you'd", "you'll", "you're", "you've", 
    "your", "yours", "yourself", "yourselves"
}
single_letter_languages = ["c", "r", "j", "k", "q", "s"]


wordnet = WordNetLemmatizer()
def is_number(s):
    return s.isdigit() or (s.startswith('-') and s[1:].isdigit())

res = cur.execute("""SELECT t1.doc_id, t1.doc_text FROM documents t1 
                    LEFT JOIN bag_of_words t2 ON t2.doc_id = t1.doc_id
                    WHERE t2.doc_id IS NULL
                  """  )

documents_to_process = res.fetchall()
for document_to_process in documents_to_process:
    doc_id, doc_text = document_to_process
    print(doc_id)
    parsed = bs4.BeautifulSoup(doc_text, "html.parser")
    # Remove citation marks
    for e in parsed.find_all('sup', class_="reference"):
        e.decompose()
    # Get all paragraphs, while restricting other methods result in a lot of garbage text
    content = parsed.find_all('p')
    bow = dict()
    for p in content:
        tokens = word_tokenize(p.text)
        for token in tokens:
            token = token.lower()
            if token not in stopwords and token not in addons and not (len(token)==1 and token not in single_letter_languages) and not (is_number(token) and len(token) !=4):
                    lemma = wordnet.lemmatize(token)
                    if lemma not in bow:
                        bow[lemma] = 0
                    bow[lemma] += 1

    # update word document frequency
    for word in bow:
        cur.execute("""INSERT INTO words (word, frequency) VALUES (?, 1)
                    ON CONFLICT(word) DO UPDATE SET frequency = frequency + 1""", (word,))
    word_ids = {}
    for word in bow:
        cur.execute("SELECT word_id FROM words WHERE word = ?", (word,))
        word_ids[word] = cur.fetchone()[0]

    for word in bow:
        cur.execute("""INSERT INTO bag_of_words (doc_id, word_id, frequency) 
                    VALUES (?, ?, ?) """,
                    (doc_id, word_ids[word],bow[word]))


con.commit()
con.close()
