import scrapy
import requests
import bs4
from time import sleep
import re
import random
import queue

N_SITES = 100


queue = [""] * N_SITES
queue[0] = '/wiki/Poland'
front = 0
rear = 1
visited = set()
visited.add('/wiki/Poland')
while front!=rear:
    link = queue[front]
    front+=1
    print(link)
    response = requests.get('https://en.wikipedia.org'+link)
    print(response.status_code)
    parsed = bs4.BeautifulSoup(response.text, "html.parser")

    links = parsed.find_all('a', attrs={'href': re.compile(r'^/wiki/[^:]+$')})
    for link in links:
        if rear < N_SITES and link['href'] not in visited:
            queue[rear] = link['href']
            visited.add(link['href'])
            rear+=1
    sleep(random.random()*3)
