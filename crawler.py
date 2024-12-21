import scrapy
import requests
import bs4
from time import sleep
import re
import random
import queue
import sys
import os

N_SITES = 10000


downloaded = set()
restart_point = '/wiki/C_(programming_language)'
for site in os.listdir("html"):
    name = "/wiki/"+site.removesuffix(".html")
    downloaded.add(name)
    restart_point = name
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
    with open("html/"+link.removeprefix("/wiki/")+".html", "w") as file:
        file.write(response.text)
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
