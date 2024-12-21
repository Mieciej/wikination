import nltk
import os
from nltk.stem import WordNetLemmatizer
from nltk.tokenize import word_tokenize

nltk.download('punkt', download_dir="./nltk_data")
nltk.download('punkt_tab', download_dir="./nltk_data")
nltk.download('wordnet', download_dir="./nltk_data")
nltk.data.path.append("./nltk_data")

addons = [".", ",", ";", ":", "!", "?", "-", "_", "(", ")", "[", "]", "{", "}", "\"", "'", "‘", "’", "“", "”", "/", "\\", "|", "@", "#", "$", "%", "^", "&", "*", "+", "=", "~", "`", "<", ">", "…", "'s", "``", "''"]
stopwords = [
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
]
single_letter_languages = ["c", "r", "j", "k", "q", "s"]

nltk.download('stopwords', download_dir="./nltk_data")

wordnet = WordNetLemmatizer()
def is_number(s):
    return s.isdigit() or (s.startswith('-') and s[1:].isdigit())

sites = os.listdir("text")
unique_words = set()
for i, site in enumerate(sites):
    with open("text/"+site) as file:
        tokens = word_tokenize(file.read())
        with open("lemma/"+site, "w") as out_file:
            for token in tokens:
                token = token.lower()
                if token not in stopwords and token not in addons and not (len(token)==1 and token not in single_letter_languages) and not (is_number(token) and len(token) !=4):
                    lemma = wordnet.lemmatize(token)
                    out_file.write(lemma+"\n")
                    unique_words.add(lemma)
    print(f"{i+1}/{len(sites)}")

with open("lemma/dict.txt", "w") as dict_file:
    dict_file.write(str(len(unique_words))+'\n')
    for word in sorted(unique_words):
        dict_file.write(word+'\n')
