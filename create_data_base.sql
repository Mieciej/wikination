CREATE TABLE IF NOT EXISTS words (
    word_id INTEGER PRIMARY KEY AUTOINCREMENT,  
    word TEXT NOT NULL UNIQUE,
    frequency INTEGER
);

CREATE TABLE IF NOT EXISTS documents (
    doc_id INTEGER PRIMARY KEY AUTOINCREMENT,
    doc_name TEXT,
    doc_text TEXT
);


CREATE TABLE IF NOT EXISTS bag_of_words (
    doc_id INTEGER,
    word_id INTEGER,
    frequency INTEGER,
    PRIMARY KEY (doc_id, word_id),
    FOREIGN KEY (doc_id) REFERENCES documents(doc_id) ON DELETE CASCADE,
    FOREIGN KEY (word_id) REFERENCES words(word_id) ON DELETE CASCADE
);

