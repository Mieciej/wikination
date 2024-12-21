import bs4
import os
sites = os.listdir("html")
for i, site in enumerate(sites):
    with open("html/"+site, "r") as file:
        parsed = bs4.BeautifulSoup(file.read(), "html.parser")
        content = parsed.find('div', {'class': 'mw-body-content'})
        # remove references
        for e in content.find_all('sup', class_="reference"):
            e.decompose()
        # remove edit links
        for e in content.find_all('span', class_="mw-editsection"):
            e.decompose()
        with open("text/"+site.removesuffix(".html"), "w") as out_file:
            out_file.write(content.text)
    print(f"{i+1}/{len(sites)}")

