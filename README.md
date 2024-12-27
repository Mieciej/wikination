## Build instructions

First populate the database:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python crawler.py
```

The commands above will take a few minutes to create the database.

Then download and install wikination dependencies:

```bash
sudo apt install sqlite3 libsqlite3-dev libsdl2-dev
git clone https://github.com/ocornut/imgui
cd imgui && git checkout v1.91.6 && cd ..
```

Build wikination:

```bash
make
```

You can then start the wikination executable.
